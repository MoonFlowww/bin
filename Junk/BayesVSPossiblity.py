import numpy as np
import matplotlib.pyplot as plt

# benchmark:
# - Same simulated HMM world for both methods
# - Same known structure given to both: state means mus, transitions A_nom
# - Each method gets 1 hyperparameter tuned on validation data:
#     Bayes: sigma_assumed (Gaussian likelihood width)
#     Possibility: gamma (possibility "similarity" width)
# - Evaluate:
#     (a) state estimation accuracy
#     (b) uncertainty quality:
#         * ECE (calibration of the max-score confidence)
#         * Coverage vs average set size for thresholded confidence sets

rng = np.random.default_rng(11)

K = 3
mus = np.array([-2.0, 0.0, 2.0])
sigma_true = 1.0

A_true = np.array([
    [0.85, 0.10, 0.05],
    [0.10, 0.80, 0.10],
    [0.05, 0.15, 0.80],
], dtype=float)

# "Given" to both methods (change to test transition misspecification)
A_nom = A_true.copy()

def simulate_hmm(Nseq, T, A, mus, sigma, outlier_rate=0.0, outlier_scale=6.0, drift_sigma=0.0, rng=None):
    if rng is None:
        rng = np.random.default_rng()
    K = len(mus)
    S = np.zeros((Nseq, T), dtype=int)
    Y = np.zeros((Nseq, T), dtype=float)

    # global drift over time (shared across sequences)
    drift = np.zeros(T)
    for t in range(1, T):
        drift[t] = drift[t-1] + rng.normal(0.0, drift_sigma)

    S[:, 0] = rng.integers(0, K, size=Nseq)
    Y[:, 0] = rng.normal(mus[S[:, 0]] + drift[0], sigma, size=Nseq)

    for t in range(1, T):
        probs = A[S[:, t-1], :]
        u = rng.random(Nseq)
        cdf = np.cumsum(probs, axis=1)
        S[:, t] = (u[:, None] > cdf).sum(axis=1)

        mean = mus[S[:, t]] + drift[t]
        base = rng.normal(mean, sigma, size=Nseq)

        # outliers (sensor glitches)
        o = rng.random(Nseq) < outlier_rate
        if o.any():
            base[o] = rng.normal(0.0, outlier_scale, size=o.sum())

        Y[:, t] = base

    return S, Y

def calibration_curve(conf, correct, n_bins=12):
    edges = np.linspace(0.0, 1.0, n_bins + 1)
    b = np.digitize(conf, edges) - 1
    avg_conf = np.full(n_bins, np.nan)
    acc = np.full(n_bins, np.nan)
    counts = np.zeros(n_bins, dtype=int)
    for i in range(n_bins):
        m = b == i
        counts[i] = m.sum()
        if counts[i] > 0:
            avg_conf[i] = conf[m].mean()
            acc[i] = correct[m].mean()
    return avg_conf, acc, counts

def ece(conf, correct, n_bins=12):
    avg_conf, acc, counts = calibration_curve(conf, correct, n_bins=n_bins)
    Ntot = counts.sum()
    return sum((n/Ntot) * abs(a - c) for c, a, n in zip(avg_conf, acc, counts) if n > 0)

# Bayes filter (FWD algo)
def bayes_filter_posteriors(Y, A, mus, sigma_assumed):
    Nseq, T = Y.shape
    K = len(mus)
    belief = np.full((Nseq, K), 1.0 / K)
    posts = np.zeros((Nseq, T, K), dtype=float)

    for t in range(T):
        if t > 0:
            belief = belief @ A
        y = Y[:, t][:, None]
        L = np.exp(-0.5 * ((y - mus[None, :]) / sigma_assumed) ** 2) / sigma_assumed
        post = belief * L
        post = post / post.sum(axis=1, keepdims=True)
        posts[:, t, :] = post
        belief = post
    return posts

# Possibility filter (max-min prediction + min update)
Pi_T = A_nom / A_nom.max(axis=1, keepdims=True)

def poss_filter_distributions(Y, Pi_T, mus, gamma):
    Nseq, T = Y.shape
    K = len(mus)
    pi = np.ones((Nseq, K))
    pis = np.zeros((Nseq, T, K), dtype=float)

    for t in range(T):
        if t > 0:
            pi_pred = np.max(np.minimum(pi[:, :, None], Pi_T[None, :, :]), axis=1)
        else:
            pi_pred = pi

        y = Y[:, t][:, None]
        pi_obs = np.exp(-0.5 * ((y - mus[None, :]) / gamma) ** 2)
        pi_obs = pi_obs / pi_obs.max(axis=1, keepdims=True)

        pi_post = np.minimum(pi_pred, pi_obs)
        m = pi_post.max(axis=1, keepdims=True)
        pi = np.divide(pi_post, m, out=np.zeros_like(pi_post), where=m > 0)

        pis[:, t, :] = pi
    return pis

def eval_probability(posts, S_true, alphas):
    Nseq, T, K = posts.shape
    p = posts.reshape(-1, K)
    s = S_true.reshape(-1)

    pred = p.argmax(axis=1)
    conf = p.max(axis=1)
    corr = (pred == s)
    acc = corr.mean()
    cal = ece(conf, corr)

    cover, size = [], []
    for a in alphas:
        mask = p >= a
        empty = ~mask.any(axis=1)
        if empty.any():
            mask[empty, pred[empty]] = True
        cover.append(mask[np.arange(mask.shape[0]), s].mean())
        size.append(mask.sum(axis=1).mean())
    return acc, cal, np.array(cover), np.array(size)

def eval_possibility(pis, S_true, alphas):
    Nseq, T, K = pis.shape
    pi = pis.reshape(-1, K)
    s = S_true.reshape(-1)

    # For ECE only, map pi -> prob-like scores q = pi / sum(pi)
    denom = pi.sum(axis=1, keepdims=True)
    q = np.divide(pi, denom, out=np.full_like(pi, 1.0 / K), where=denom > 0)

    pred = q.argmax(axis=1)
    conf = q.max(axis=1)
    corr = (pred == s)
    acc = corr.mean()
    cal = ece(conf, corr)

    cover, size = [], []
    pi_argmax = pi.argmax(axis=1)
    for a in alphas:
        mask = pi >= a
        empty = ~mask.any(axis=1)
        if empty.any():
            mask[empty, pi_argmax[empty]] = True
        cover.append(mask[np.arange(mask.shape[0]), s].mean())
        size.append(mask.sum(axis=1).mean())
    return acc, cal, np.array(cover), np.array(size)

def tune_bayes(Y_val, S_val, A, mus, grid, alphas, lam=0.5):
    best = None
    for sigma in grid:
        posts = bayes_filter_posteriors(Y_val, A, mus, sigma)
        acc, cal, *_ = eval_probability(posts, S_val, alphas)
        score = acc - lam * cal
        if (best is None) or (score > best["score"]):
            best = {"sigma": sigma, "score": score}
    return best

def tune_poss(Y_val, S_val, Pi_T, mus, grid, alphas, lam=0.5):
    best = None
    for gamma in grid:
        pis = poss_filter_distributions(Y_val, Pi_T, mus, gamma)
        acc, cal, *_ = eval_possibility(pis, S_val, alphas)
        score = acc - lam * cal
        if (best is None) or (score > best["score"]):
            best = {"gamma": gamma, "score": score}
    return best

def run_setting(outlier_rate, drift_sigma, Ntr=800, Nv=800, Nte=2000, T=25, lam=0.5):
    S_val, Y_val = simulate_hmm(Nv, T, A_true, mus, sigma_true, outlier_rate=outlier_rate, drift_sigma=drift_sigma, rng=rng)
    S_te,  Y_te  = simulate_hmm(Nte, T, A_true, mus, sigma_true, outlier_rate=outlier_rate, drift_sigma=drift_sigma, rng=rng)

    alphas = np.linspace(0.1, 0.9, 9)
    grid = np.linspace(0.4, 2.5, 30)

    bayes_best = tune_bayes(Y_val, S_val, A_nom, mus, grid, alphas, lam=lam)
    poss_best  = tune_poss(Y_val, S_val, Pi_T, mus, grid, alphas, lam=lam)

    posts_te = bayes_filter_posteriors(Y_te, A_nom, mus, bayes_best["sigma"])
    pis_te   = poss_filter_distributions(Y_te, Pi_T, mus, poss_best["gamma"])

    acc_b, ece_b, cov_b, sz_b = eval_probability(posts_te, S_te, alphas)
    acc_p, ece_p, cov_p, sz_p = eval_possibility(pis_te, S_te, alphas)

    return {
        "alphas": alphas,
        "bayes": {"sigma": bayes_best["sigma"], "acc": acc_b, "ece": ece_b, "cov": cov_b, "sz": sz_b},
        "poss":  {"gamma": poss_best["gamma"], "acc": acc_p, "ece": ece_p, "cov": cov_p, "sz": sz_p},
    }

# outliers
outlier_sweep = np.array([0.0, 0.05, 0.10, 0.15, 0.20])
drift_fixed = 0.03
results_out = [run_setting(r, drift_fixed, lam=0.5) for r in outlier_sweep]

plt.figure(figsize=(7, 4.5))
plt.plot(outlier_sweep, [r["bayes"]["acc"] for r in results_out], marker="o", label="Bayes accuracy")
plt.plot(outlier_sweep, [r["poss"]["acc"] for r in results_out],  marker="o", label="Possibility accuracy")
plt.xlabel("Outlier rate"); plt.ylabel("State estimation accuracy")
plt.title("Accuracy vs outliers (each method tuned on validation)")
plt.legend(); plt.grid(True); plt.show()

plt.figure(figsize=(7, 4.5))
plt.plot(outlier_sweep, [r["bayes"]["ece"] for r in results_out], marker="o", label="Bayes ECE (max prob)")
plt.plot(outlier_sweep, [r["poss"]["ece"] for r in results_out],  marker="o", label="Possibility ECE (pi→q)")
plt.xlabel("Outlier rate"); plt.ylabel("ECE (lower is better)")
plt.title("Calibration-style metric vs outliers")
plt.legend(); plt.grid(True); plt.show()

drift_sweep = np.array([0.0, 0.02, 0.05, 0.08, 0.12])
outlier_fixed = 0.10
results_dr = [run_setting(outlier_fixed, d, lam=0.5) for d in drift_sweep]

plt.figure(figsize=(7, 4.5))
plt.plot(drift_sweep, [r["bayes"]["acc"] for r in results_dr], marker="o", label="Bayes accuracy")
plt.plot(drift_sweep, [r["poss"]["acc"] for r in results_dr],  marker="o", label="Possibility accuracy")
plt.xlabel("Drift sigma"); plt.ylabel("State estimation accuracy")
plt.title("Accuracy vs nonstationarity (drift)")
plt.legend(); plt.grid(True); plt.show()

plt.figure(figsize=(7, 4.5))
plt.plot(drift_sweep, [r["bayes"]["ece"] for r in results_dr], marker="o", label="Bayes ECE (max prob)")
plt.plot(drift_sweep, [r["poss"]["ece"] for r in results_dr],  marker="o", label="Possibility ECE (pi→q)")
plt.xlabel("Drift sigma"); plt.ylabel("ECE (lower is better)")
plt.title("Calibration-style metric vs drift")
plt.legend(); plt.grid(True); plt.show()

stress = run_setting(outlier_rate=0.15, drift_sigma=0.05, lam=0.5)
plt.figure(figsize=(7, 5))
plt.plot(stress["bayes"]["sz"], stress["bayes"]["cov"], marker="o", label=f"Bayes (sigma*={stress['bayes']['sigma']:.2f})")
plt.plot(stress["poss"]["sz"],  stress["poss"]["cov"],  marker="o", label=f"Possibility (gamma*={stress['poss']['gamma']:.2f})")
plt.xlabel("Average set size"); plt.ylabel("Coverage (true state in set)")
plt.title("Uncertainty sets: coverage vs size (stress setting)")
plt.legend(); plt.grid(True); plt.show()

print("Stress setting (outlier=0.15, drift=0.05):")
print(f"  Bayes sigma*={stress['bayes']['sigma']:.3f}, acc={stress['bayes']['acc']:.3f}, ECE={stress['bayes']['ece']:.3f}")
print(f"  Poss  gamma*={stress['poss']['gamma']:.3f}, acc={stress['poss']['acc']:.3f}, ECE={stress['poss']['ece']:.3f}")
