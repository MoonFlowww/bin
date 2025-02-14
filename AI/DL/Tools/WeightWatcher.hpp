#ifndef WEIGHTWATCHER_HPP
#define WEIGHTWATCHER_HPP

#include <vector>
#include <iostream>
#include <Eigen/Dense>
#include <cmath>
#include <algorithm>
#include <numeric>
#include "matplot/matplot.h"

namespace WeightWatcher {

    void Guide() {
        std::cout << "\nWeightWatcher Usage Guide:\n";
        std::cout << "Empirical Spectral Density (ESD):\n";
        std::cout << "  - A bulk distribution suggests randomness, while deviations indicate structure.\n";
        std::cout << "Marchenko-Pastur (MP) Distribution Fit:\n";
        std::cout << "  - If eigenvalues exceed MP bounds, the matrix contains meaningful information.\n";
        std::cout << "Heavy-Tailed Behavior:\n";
        std::cout << "  - A high maximum eigenvalue compared to MP bounds suggests strong structure.\n";
        std::cout << "Rank Computation:\n";
        std::cout << "  - Higher rank indicates a richer representation of information.\n";
        std::cout << "Entropy Calculation:\n";
        std::cout << "  - Higher entropy means evenly distributed information, lower entropy suggests compression.\n";
        std::cout << "-------------------------------------------------------------\n\n";
    }

    void Interpretability(double lambda_min, double lambda_max, bool heavyTailed, int rank, double entropy) {
        std::cout << "\nInterpretability Guide:\n";
        if (lambda_max > 5.0) {
            std::cout << "- The model exhibits strong structure with significant information content.\n";
        }
        else if (lambda_max > 2.0) {
            std::cout << "- The model is well-structured with a mix of randomness and meaningful information.\n";
        }
        else {
            std::cout << "- The model appears mostly random with low structural complexity.\n";
        }

        if (heavyTailed) {
            std::cout << "- The weight matrix has heavy-tailed properties, suggesting power-law distributed eigenvalues and potential memorization capacity.\n";
        }
        else {
            std::cout << "- The matrix follows MP distribution, indicating a more generic structure.\n";
        }

        if (rank > 100) {
            std::cout << "- The model has a high rank, suggesting high complexity and expressiveness.\n";
        }
        else if (rank > 50) {
            std::cout << "- The model has moderate complexity with a balance between compression and capacity.\n";
        }
        else {
            std::cout << "- The model has low complexity, possibly suffering from rank collapse.\n";
        }

        if (entropy > 2.0) {
            std::cout << "- The entropy is high, indicating diverse and distributed information storage.\n";
        }
        else {
            std::cout << "- The entropy is low, meaning the model might be overcompressed or losing important features.\n";
        }
        std::cout << "-------------------------------------------------------------\n\n";
    }

    std::vector<double> computeESD(const Eigen::MatrixXd& matrix);
    bool fitMPDistribution(const Eigen::MatrixXd& matrix, double& lambda_min, double& lambda_max);
    bool isHeavyTailed(const Eigen::MatrixXd& matrix, double threshold = 2.5);
    int computeRank(const Eigen::MatrixXd& matrix);
    double computeEntropy(const Eigen::MatrixXd& matrix);
    std::vector<double> getEigenvalues(const Eigen::MatrixXd& matrix);

    std::vector<double> computeESD(const Eigen::MatrixXd& matrix) {
        Eigen::JacobiSVD<Eigen::MatrixXd> svd(matrix, Eigen::ComputeThinU | Eigen::ComputeThinV);
        Eigen::VectorXd singularValues = svd.singularValues();
        return std::vector<double>(singularValues.data(), singularValues.data() + singularValues.size());
    }

    bool fitMPDistribution(const Eigen::MatrixXd& matrix, double& lambda_min, double& lambda_max) {
        double q = static_cast<double>(matrix.rows()) / matrix.cols();
        double sigma2 = matrix.squaredNorm() / (matrix.rows() * matrix.cols());
        lambda_min = sigma2 * pow(1 - sqrt(1 / q), 2);
        lambda_max = sigma2 * pow(1 + sqrt(1 / q), 2);
        return true;
    }

    bool isHeavyTailed(const Eigen::MatrixXd& matrix, double threshold) {
        std::vector<double> eigenvalues = getEigenvalues(matrix);
        double maxEigen = *std::max_element(eigenvalues.begin(), eigenvalues.end());
        return maxEigen > threshold;
    }

    int computeRank(const Eigen::MatrixXd& matrix) {
        Eigen::JacobiSVD<Eigen::MatrixXd> svd(matrix);
        return (svd.singularValues().array() > 1e-10).count();
    }

    double computeEntropy(const Eigen::MatrixXd& matrix) {
        Eigen::JacobiSVD<Eigen::MatrixXd> svd(matrix);
        Eigen::VectorXd singularValues = svd.singularValues();
        double entropy = 0.0;
        double sum = singularValues.sum();
        for (int i = 0; i < singularValues.size(); ++i) {
            if (singularValues(i) > 0) {
                double p = singularValues(i) / sum;
                entropy -= p * log(p);
            }
        }
        return entropy;
    }

    std::vector<double> getEigenvalues(const Eigen::MatrixXd& matrix) {
        Eigen::JacobiSVD<Eigen::MatrixXd> svd(matrix, Eigen::ComputeThinU | Eigen::ComputeThinV);
        Eigen::VectorXd singularValues = svd.singularValues();
        return std::vector<double>(singularValues.data(), singularValues.data() + singularValues.size());
    }

    void Analysis(const Eigen::MatrixXd& matrix, bool InterpretabilityGuide = false) {
        auto esd = computeESD(matrix);
        double lambda_min, lambda_max;
        fitMPDistribution(matrix, lambda_min, lambda_max);
        bool heavyTailed = isHeavyTailed(matrix);
        int rank = computeRank(matrix);
        double entropy = computeEntropy(matrix);

        std::cout << "Empirical Spectral Density computed." << std::endl;
        std::cout << "MP Distribution Bounds: [" << lambda_min << ", " << lambda_max << "]" << std::endl;
        std::cout << "Heavy-Tailed: " << (heavyTailed ? "Yes" : "No") << std::endl;
        std::cout << "Rank: " << rank << std::endl;
        std::cout << "Entropy: " << entropy << std::endl;
        if (InterpretabilityGuide) Interpretability(lambda_min, lambda_max, heavyTailed, rank, entropy);
        matplot::plot(esd);
        matplot::title("Empirical Spectral Density");
        matplot::show();
    }

} // namespace WeightWatcher

#endif // WEIGHTWATCHER_HPP
