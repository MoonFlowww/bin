import random
import math
import numpy as np
import matplotlib.pyplot as plt
from sklearn.linear_model import RANSACRegressor
from sklearn.preprocessing import PolynomialFeatures
from sklearn.pipeline import make_pipeline
from sklearn.metrics import mean_absolute_error
from skopt import gp_minimize
from skopt.space import Integer, Real

def sinusoidal_growth(i):
    return math.sin(i) + math.log(i + 1) + 5

def exponential_decay(i):
    return math.exp(-i / 200) * 30 + 5

def polynomial_curve(i):
    return 0.00001 * i**2 + math.sin(i / 20) + 5

def piecewise_function(i):
    return (math.sin(i / 10) * 5 + 5) if i % 400 < 200 else (math.cos(i / 10) * 5 + 10)

def chaotic_oscillation(i):
    return math.sin(i / 5) * math.log(i + 1) + 5

functions = [sinusoidal_growth, exponential_decay, polynomial_curve, piecewise_function, chaotic_oscillation]
labels = ["Sinusoidal Growth", "Exponential Decay", "Polynomial Curve", "Piecewise Function", "Chaotic Oscillation"]

def generate_data(func):
    x = np.arange(1000)
    y = np.array([func(i) for i in x])
    for i in range(len(y)):
        if random.random() <= 0.15:
            sign = 1 if random.random() < 0.5 else -1
            y[i] += sign * math.sin(random.random()) * 2**3
    return x.reshape(-1, 1), y

def build_model(degree, max_trials, residual_threshold):
    return make_pipeline(
        PolynomialFeatures(degree),
        RANSACRegressor(max_trials=max_trials, residual_threshold=residual_threshold, random_state=48)
    )

def optimize_model(X, y):
    def objective(params):
        degree, max_trials, residual_threshold = params
        model = build_model(int(degree), int(max_trials), residual_threshold)
        model.fit(X, y)
        y_pred = model.predict(X)
        return mean_absolute_error(y, y_pred)

    search_space = [
        Integer(2, 5, name="degree"),
        Integer(50, 200, name="max_trials"),
        Real(0.5, 2.0, name="residual_threshold")
    ]

    result = gp_minimize(objective, search_space, n_calls=15, random_state=48)
    return build_model(int(result.x[0]), int(result.x[1]), result.x[2])

fig, axes = plt.subplots(3, 2, figsize=(18, 12))
axes = axes.flatten()

for idx, func in enumerate(functions):
    ax = axes[idx]

    X, y = generate_data(func)

    model = optimize_model(X, y)
    model.fit(X, y)
    y_pred_ransac = model.predict(X)

    inlier_mask = model.named_steps['ransacregressor'].inlier_mask_
    outlier_mask = ~inlier_mask

    ax.set_facecolor("black")
    ax.scatter(X[inlier_mask], y[inlier_mask], color='yellow', label='Data (Inliers)', alpha=0.7, s=20)
    ax.scatter(X[outlier_mask], y[outlier_mask], color='grey', label='Outliers', alpha=0.8, s=20)

    x_fine = np.linspace(X.min(), X.max(), 1000).reshape(-1, 1)
    y_pred_fine = model.predict(x_fine)
    ax.plot(x_fine, y_pred_fine, color='red', linewidth=2, label='Optimized Regression')

    ax.set_title(labels[idx], fontsize=14)
    ax.set_xlabel('Index', fontsize=12)
    ax.set_ylabel('Value', fontsize=12)
    ax.legend()

fig.delaxes(axes[-1])
plt.suptitle("Optimized Robust Regression for Different Mathematical Behaviors", fontsize=16, color="white")
plt.tight_layout()
plt.show()
