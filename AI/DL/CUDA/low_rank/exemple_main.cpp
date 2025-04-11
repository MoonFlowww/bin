#include <iostream>
#include <chrono>
#include <cstdlib>
#include <cmath>
#include "CUDA_HUB.cuh"

void fillMatrix(float* matrix, int rows, int cols) {
    for (int i = 0; i < rows * cols; ++i) {
        matrix[i] = static_cast<float>(rand()) / RAND_MAX;
    }
}

int main() {
    int devices = Devices();
    if (devices == 0)
        return 0;

    int m = 1; // Number of input samples (batch size)
    int k = 768; // Number of input features (neurons in the previous layer)
    int n = 126; // Number of neurons in the current layer


    float* X = new float[m * k]; // input
    float* W = new float[k * n]; // weights
    float* B = new float[m * n]; // bias
    float* R = new float[m * n]; // result

    fillMatrix(X, m, k);
    fillMatrix(W, k, n);
    fillMatrix(B, m, n); // Fill bias matrix

    auto t1 = std::chrono::high_resolution_clock::now();
    matrixMultiplication(X, W, R, m, k, n); // A, B, Result, A[M;K], B[K;N]
    matrixAddition(R, B, R, m, n);
    matrixSubtraction(R, W, R, m, n);

    auto t2 = std::chrono::high_resolution_clock::now();
    auto cd = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1);

    std::cout << "\n\n\aFeedforward operation completed in " << round(cd.count()/10.0)/100.0 << " ms" << std::endl;

    std::cout << "\n\n\nOutput: " << std::endl;
    for (int i = 0; i < m; ++i) {
        for (int j = 0; j < n; ++j) {
            std::cout << R[i * n + j] << " ";
        }
        std::cout << std::endl;
    }

    delete[] X;
    delete[] W;
    delete[] B;
    delete[] R;

    return 0;
}

