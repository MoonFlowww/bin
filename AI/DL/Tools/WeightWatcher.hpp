#include <iostream>
#include <Eigen/Dense>
#include <cmath>
#include <algorithm>
#include <numeric>


using namespace Eigen;


double* computeSingularValues(const MatrixXd* W, int& size) {
    JacobiSVD<MatrixXd> svd(*W, ComputeThinU | ComputeThinV);
    size = svd.singularValues().size();

    double* singularValues = new double[size];
    for (int i = 0; i < size; ++i) {
        singularValues[i] = svd.singularValues()(i);
    }
    return singularValues;
}

double fitPowerLaw(double* singularValues, int size) {
    if (size == 0) return -1.0;


    std::sort(singularValues, singularValues + size, std::greater<double>());


    int nonZeroCount = 0;
    for (int i = 0; i < size; ++i) {
        if (singularValues[i] > 1e-10) {
            ++nonZeroCount;
        } else break;
    }

    if (nonZeroCount == 0) {
        std::cerr << "No non-zero singular values found." << std::endl;
        return -1.0;
    }

    double* logSingularValues = new double[nonZeroCount];
    double* indices = new double[nonZeroCount];

    for (int i = 0; i < nonZeroCount; ++i) {
        logSingularValues[i] = log10(singularValues[i]);
        indices[i] = log10(i + 1); // Avoid log(0)
    }

    // lin-reg
    double sumX = 0.0, sumY = 0.0, sumXY = 0.0, sumXX = 0.0;
    for (int i = 0; i < nonZeroCount; ++i) {
        sumX += indices[i];
        sumY += logSingularValues[i];
        sumXY += indices[i] * logSingularValues[i];
        sumXX += indices[i] * indices[i];
    }

    double meanX = sumX / nonZeroCount;
    double meanY = sumY / nonZeroCount;
    double numerator = sumXY - nonZeroCount * meanX * meanY;
    double denominator = sumXX - nonZeroCount * meanX * meanX;

    delete[] logSingularValues;
    delete[] indices;

    if (denominator == 0.0) {
        std::cerr << "Denominator is zero, cannot compute alpha." << std::endl;
        return -1.0;
    }

    return -numerator / denominator;
}


double AlphaMetric(const std::vector<std::vector<double>>& weights) {
    int rows = weights.size();
    int cols = weights[0].size();

    MatrixXd* W = new MatrixXd(rows, cols);
    for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < cols; ++j) {
            (*W)(i, j) = weights[i][j];
        }
    }
    int singularSize = 0;
    double* singularValues = computeSingularValues(W, singularSize);

    double alpha = fitPowerLaw(singularValues, singularSize);

    delete W;
    delete[] singularValues;

    return alpha;
}



/*
int main() {
    std::vector<std::vector<double>> weights = {
        {1.0, 2.0, 3.0},
        {4.0, 5.0, 6.0},
        {7.0, 8.0, 9.0}
    };

    double alpha = AlphaMetric(weights);
    if (alpha != -1.0) {
        std::cout << "Alpha Metric: " << alpha << std::endl;
    }
    else {
        std::cout << "Failed to compute alpha." << std::endl;
    }

    return 0;
}
*/

