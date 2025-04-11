#include "cuda_runtime.h"
#include "device_launch_parameters.h"
#include "kernel.cuh"

#include <cstdio>

// --------------------------------- LINEAR ---------------------------------------

__global__ void addition(float* X, float* B, float* R, int rows, int cols) {
    int col = blockIdx.x * blockDim.x + threadIdx.x;
    int row = blockIdx.y * blockDim.y + threadIdx.y;

    if (col < cols && row < rows) {
        R[row * cols + col] = X[row * cols + col] + B[row * cols + col];
    }
}

__global__ void subtraction(float* A, float* B, float* C, int rows, int cols) {
    int col = blockIdx.x * blockDim.x + threadIdx.x;
    int row = blockIdx.y * blockDim.y + threadIdx.y;

    if (col < cols && row < rows) {
        C[row * cols + col] = A[row * cols + col] - B[row * cols + col];
    }
}




// --------------------------------- NON LINEAR ---------------------------------------

__global__ void multiplication(float* X, float* W, float* R, int m, int k, int n) {
    int col = blockIdx.x * blockDim.x + threadIdx.x;
    int row = blockIdx.y * blockDim.y + threadIdx.y;

    if (col < n && row < m) {
        float sum = 0.0f;
        for (int i = 0; i < k; ++i) {
            sum += X[row * k + i] * W[i * n + col];
        }
        R[row * n + col] = sum;
    }
}

__global__ void division(float* X, float* W, float* R, int m, int k, int n) {
    int col = blockIdx.x * blockDim.x + threadIdx.x;
    int row = blockIdx.y * blockDim.y + threadIdx.y;

    if (col < n && row < m) {
        float sum = 0.0f;
        for (int i = 0; i < k; ++i) {
            sum += X[row * k + i] / W[i * n + col];
        }
        R[row * n + col] = sum;
    }
}


