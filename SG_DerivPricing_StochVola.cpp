// white board with formulas : https://www.tldraw.com/s/v2_c_HWbZwoIxgZLkzhmjECUAx?d=v243.-467.3623.1925.Owoa9ULqPO6J_H-hGiDK-
#include <iostream>
#include <random>
#include <vector>
#include <cmath>
#include <functional>
#define M_SQRT1_2 0.70710678118654752440  // 1/sqrt(2)

std::vector<double> price = { 20352.02, 20232.87, 20066.96, 20383.64, 20361.47, 20324.04, 20190.42, 20174.05 };


double deterministic(int t) {
    return price[t];
}


double M() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<> N(0.0, 1.0);
    return N(gen);
}


double U_G(double theta, double gamma, int t, double x, double alpha, bool derivative = false, bool verbose = false) {
    double term1 = 0.0, term2 = 0.0;
    double psi = deterministic(t) / 1000.0;
    double Mx = derivative ? x : M(); 

    auto Ufunc = [theta, gamma, psi, &term1, &term2](double Mx) {
        term1 = 3 * psi * Mx;
        term2 = 3 * std::pow(gamma, 2) * std::pow(psi, 2);
        return theta * theta * exp(term1 - term2);
        };

    auto gfunc = [gamma, psi](double x, double t) {
        double psi_t = psi; 
        double term1 = 3 * psi_t * x;
        double term2 = 3 * std::pow(gamma, 2) * std::pow(psi_t, 2);
        return exp(term1 - term2);
        };


    double result = Ufunc(Mx);

    if (derivative) {
        int num_intervals = 100;
        double T = 1.0;
        double dt = T / num_intervals;
        double integral = 0.0;

        for (int i = 0; i < num_intervals; ++i) { // integral
            double t_i = i * dt;
            double t_ip1 = (i + 1) * dt;
            integral += 0.5 * (gfunc(x, t_i) + gfunc(x, t_ip1)) * dt;
        }

        result = alpha * sqrt(integral);
    }

    if (verbose) {
        std::cout << "t = " << t << ", psi = " << psi << std::endl;
        std::cout << "Term1 (3 * psi * M()) = " << term1 << std::endl;
        std::cout << "Term2 (3 * gamma^2 * psi^2) = " << term2 << std::endl;
        std::cout << "Result after exp: " << result << std::endl;
    }

    return result;
}


double phi(double x) {
    return 0.5 * erfc(-x * M_SQRT1_2);
}


double BS(double S, double K, double r, double T, double sigma) { // black sholes classic
    double d1 = (log(S / K) + (r + 0.5 * sigma * sigma) * T) / (sigma * sqrt(T));
    double d2 = d1 - sigma * sqrt(T);
    return S * phi(d1) - K * exp(-r * T) * phi(d2);
}

int main() {
    double theta = 0.5;  // θ
    double gamma = 0.1;  // γ
    double alpha = 0.5;
    double strike = 20000.0;
    double r = 0.03;  //rf
    double T = 1.0;  // Time to maturity

    std::vector<double> r_vec;
    for (int i = 0; i < price.size(); ++i) {
        r_vec.push_back(U_G(theta, gamma, i, 0, alpha, false, true));
        std::cout << "--------------------------\n" << std::endl;
    }

    double g_result = U_G(theta, gamma, 0, 1.0, alpha, true, true);
    std::cout << "Final result for g(x, T): " << g_result << std::endl;


    double S_t = price[0];  // Initial price
    double sigma = g_result;  // vola derivate (g)
    double option_price = BS(S_t, strike, r, T, sigma);
    std::cout << "Black-Scholes call option price: " << option_price << std::endl;

    return 0;
}
