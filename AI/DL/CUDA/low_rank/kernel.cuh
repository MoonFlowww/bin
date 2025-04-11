#pragma once
//linear
__global__ void addition(float* X, float* B, float* R, int rows, int cols);
__global__ void subtraction(float* X, float* B, float* R, int rows, int cols);

//nlinear
__global__ void multiplication(float* A, float* B, float* R, int m, int k, int n);
__global__ void division(float* A, float* B, float* R, int m, int k, int n);

