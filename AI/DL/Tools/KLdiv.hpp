#pragma once
#define NOMINMAX
#include <matplot/matplot.h>
#include <Eigen/Dense>
#include <algorithm>
#include <stdexcept>
#include <iostream>
#include <numeric>
#include <vector>
#include <cmath>

#define PI 3.141592653589793
#define EPSILON 1e-10 // Small value for numerical stability

namespace plt = matplot;

template <typename T>
struct DataStruct {
    std::vector<double> x; // Data points
    std::vector<double> y; // Probability densities
};

template <typename T>
class KDE {
private:
    std::vector<T> data;
    double bandwidth;

public:
    KDE(const std::vector<T>& data) : data(data) {
        bandwidth = computeOptimalBandwidth();
    }

    double GaussKernel(double u) const {
        return (1.0 / std::sqrt(2 * PI)) * std::exp(-0.5 * u * u);
    }

    double computeOptimalBandwidth() const {
        if (data.empty()) throw std::runtime_error("KDE Error: Empty dataset.");

        double sigma = std::sqrt(std::inner_product(data.begin(), data.end(), data.begin(), 0.0) / data.size());
        return 1.06 * sigma * std::pow(data.size(), -1.0 / 5.0); // Silverman's rule
    }

    std::vector<double> compute(const std::vector<double>& x_vals) const {
        size_t n = data.size();
        if (n == 0) throw std::runtime_error("KDE Error: Empty dataset.");

        std::vector<double> kde_estimates(x_vals.size(), 0.0);
        for (size_t j = 0; j < x_vals.size(); ++j) {
            double x = x_vals[j];
            double sum = 0.0;
            for (const double& xi : data)
                sum += GaussKernel((x - xi) / bandwidth);
            kde_estimates[j] = sum / (n * bandwidth);
        }
        return kde_estimates;
    }
};

double interpolate(const std::vector<double>& x, const std::vector<double>& y, double x_new) {
    size_t n = x.size();
    if (n < 2) throw std::runtime_error("Interpolation Error: Not enough points.");
    if (x_new <= x[0]) return y[0];
    if (x_new >= x[n - 1]) return y[n - 1];

    for (size_t i = 1; i < n; ++i) {
        if (x_new < x[i]) {
            double x0 = x[i - 1], x1 = x[i];
            double y0 = y[i - 1], y1 = y[i];
            return y0 + (y1 - y0) * (x_new - x0) / (x1 - x0);
        }
    }
    return y[n - 1];
}

double KLDiv(const DataStruct<double>& P, const DataStruct<double>& Q) {
    if (P.x.size() != P.y.size() || Q.x.size() != Q.y.size())
        throw std::invalid_argument("KLDiv Error: Mismatched x and y sizes.");
    if (P.y.empty() || Q.y.empty())
        throw std::runtime_error("KLDiv Error: Empty distributions.");

    double sumP = std::accumulate(P.y.begin(), P.y.end(), 0.0);
    double sumQ = std::accumulate(Q.y.begin(), Q.y.end(), 0.0);

    std::vector<double> P_norm(P.y.size()), Q_interp(P.y.size());
    for (size_t i = 0; i < P.y.size(); ++i) {
        P_norm[i] = P.y[i] / sumP;
        Q_interp[i] = interpolate(Q.x, Q.y, P.x[i]) / sumQ + EPSILON;
    }

    double kl_div = 0.0;
    for (size_t i = 0; i < P.y.size(); ++i) {
        if (P_norm[i] > 0)
            kl_div += P_norm[i] * std::log(P_norm[i] / Q_interp[i]);
    }
    return kl_div;
}

double JSDiv(const DataStruct<double>& P, const DataStruct<double>& Q) {
    DataStruct<double> M;
    M.x = P.x;
    M.y.resize(P.y.size());

    double sumP = std::accumulate(P.y.begin(), P.y.end(), 0.0);
    double sumQ = std::accumulate(Q.y.begin(), Q.y.end(), 0.0);

    for (size_t i = 0; i < P.y.size(); ++i) {
        double Q_interp = interpolate(Q.x, Q.y, P.x[i]) / sumQ + EPSILON;
        double P_norm = P.y[i] / sumP + EPSILON;
        M.y[i] = 0.5 * (P_norm + Q_interp);
    }

    return 0.5 * (KLDiv(P, M) + KLDiv(Q, M));
}


static double KnownSensibilityScore(const std::vector<double>& P_data, const std::vector<double>& Q_data, double Cost, bool useJS = false) {
    if (Cost <= 0) throw std::invalid_argument("Cost must be positive.");

    double min_x = std::min(*std::min_element(P_data.begin(), P_data.end()), *std::min_element(Q_data.begin(), Q_data.end()));
    double max_x = std::max(*std::max_element(P_data.begin(), P_data.end()), *std::max_element(Q_data.begin(), Q_data.end()));

    std::vector<double> x_vals;
    for (double x = min_x; x <= max_x; x += 0.1) x_vals.push_back(x); // Finer granularity

    KDE<double> kde_P(P_data), kde_Q(Q_data);
    DataStruct<double> P{ x_vals, kde_P.compute(x_vals) };
    DataStruct<double> Q{ x_vals, kde_Q.compute(x_vals) };

    double divergence = useJS ? JSDiv(P, Q) : KLDiv(P, Q);
    return Cost / (divergence + EPSILON);
}



/*
std::vector<double> P_data = {1.0, 2.0, 2.5, 3.0, 4.2, 5.1};
std::vector<double> Q_data = {1.5, 2.2, 2.8, 3.5, 4.0, 5.5};

double sensitivityScore = own::KnownSensibilityScoring(P_data, Q_data, 10.0, true);
std::cout << "Sensitivity Score: " << sensitivityScore << std::endl;

*/
