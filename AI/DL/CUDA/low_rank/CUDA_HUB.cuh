#ifndef CUDA_HUB_CUH
#define CUDA_HUB_CUH
#include "cuda_runtime.h"

inline void HANDLE_ERROR(cudaError_t err);
int Devices();
int getCoresPerSM(int major, int minor);


//Linear
void matrixAddition(float* host_X, float* host_B, float* host_R, int rows, int cols);
void matrixSubtraction(float* host_X, float* host_B, float* host_R, int rows, int cols);



//nLinear
void matrixMultiplication(float* host_X, float* host_W, float* host_R, int m, int k, int n);
void matrixDivision(float* host_X, float* host_W, float* host_R, int m, int k, int n);

#endif // CUDA_HUB_CUH
