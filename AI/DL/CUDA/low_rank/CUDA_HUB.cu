#include <iostream>
#include <cuda_runtime.h>
#include <device_launch_parameters.h>

#include "CUDA_HUB.cuh"
#include "kernel.cuh"

inline void HANDLE_ERROR(cudaError_t err) {
    if (err != cudaSuccess) {
        std::cerr << "CUDA Error: " << cudaGetErrorString(err) << std::endl;
        exit(-1);
    }
}

int getCoresPerSM(int major, int minor) {
    // Defines the number of CUDA cores per multiprocessor for different architectures
    struct SMtoCores {
        int SM;
        int Cores;
    };

    SMtoCores nGpuArchCoresPerSM[] = {
        {0x30, 192}, // Kepler
        {0x32, 192}, // Kepler
        {0x35, 192}, // Kepler
        {0x37, 192}, // Kepler
        {0x50, 128}, // Maxwell
        {0x52, 128}, // Maxwell
        {0x53, 128}, // Maxwell
        {0x60, 64},  // Pascal
        {0x61, 128}, // Pascal
        {0x62, 128}, // Pascal
        {0x70, 64},  // Volta
        {0x72, 64},  // Volta
        {0x75, 64},  // Turing
        {0x80, 64},  // Ampere
        {0x86, 128}, // Ampere
        {0x90, 128}, // Ada Lovelace (RTX 40 series)
        {-1, -1}
    };

    int index = 0;
    while (nGpuArchCoresPerSM[index].SM != -1) {
        if (nGpuArchCoresPerSM[index].SM == ((major << 4) + minor)) {
            return nGpuArchCoresPerSM[index].Cores;
        }
        index++;
    }
    //std::cerr << "Unknown GPU architecture" << std::endl;
    return -1;
}

int Devices() {
    int deviceCount = 0;
    cudaError_t error_id = cudaGetDeviceCount(&deviceCount);
    HANDLE_ERROR(error_id);
    if (deviceCount == 0)
        printf("There are no available device(s) that support CUDA\n");
    else {
        
        std::cout << "\n\033[1m\033[37m*~~~~~~~~~~~~~~GPUs~~~~~~~~~~~~~*\033[0m" << std::endl;
        for (int GPU = 0; GPU < deviceCount; ++GPU) {
            cudaSetDevice(GPU);
            cudaDeviceProp deviceProp;
            cudaGetDeviceProperties(&deviceProp, GPU);
            int coresPerSM = getCoresPerSM(deviceProp.major, deviceProp.minor);
            int totalCores = coresPerSM > 0 ? coresPerSM * deviceProp.multiProcessorCount : 0;
            std::cout << "  \033[1m\033[37m" << GPU + 1 << "> " << deviceProp.name << "\033[0m\n"
                << "     |-> \033[1m\033[37m" << deviceProp.multiProcessorCount << "\033[0m Multi-Processors\n"
                << "     |-> ";
            if (totalCores > 0) {
                std::cout << "\033[1m\033[37m" << totalCores;
            }else std::cout << "\033[1;90m___";
            std::cout << "\033[0m CUDA Cores\n"
                << "     |-> \033[1m\033[37m" << deviceProp.maxThreadsPerMultiProcessor << "\033[0m Threads/MP\n"
                << "         ~> \033[1m\033[37m" << deviceProp.multiProcessorCount * deviceProp.maxThreadsPerMultiProcessor << "\033[0m Total fake Threads" << std::endl;
        }
    }
    std::cout << "\033[1m\033[37m*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*\033[0m\n" << std::endl;
    return deviceCount;
}

// --------------------------------- LINEAR ---------------------------------------


void matrixAddition(float* host_A, float* host_B, float* host_C, int rows, int cols) {
    float* dev_A, * dev_B, * dev_C;
    size_t size = rows * cols * sizeof(float);

    HANDLE_ERROR(cudaMalloc(&dev_A, size));
    HANDLE_ERROR(cudaMalloc(&dev_B, size));
    HANDLE_ERROR(cudaMalloc(&dev_C, size));

    HANDLE_ERROR(cudaMemcpy(dev_A, host_A, size, cudaMemcpyHostToDevice));
    HANDLE_ERROR(cudaMemcpy(dev_B, host_B, size, cudaMemcpyHostToDevice));

    dim3 threads(16, 16);
    dim3 blocks((cols + 15) / 16, (rows + 15) / 16);
    addition <<<blocks, threads >>> (dev_A, dev_B, dev_C, rows, cols);
    HANDLE_ERROR(cudaGetLastError());
    HANDLE_ERROR(cudaDeviceSynchronize());

    HANDLE_ERROR(cudaMemcpy(host_C, dev_C, size, cudaMemcpyDeviceToHost));

    cudaFree(dev_A);
    cudaFree(dev_B);
    cudaFree(dev_C);
}

void matrixSubtraction(float* host_A, float* host_B, float* host_C, int rows, int cols) {
    float* dev_A, * dev_B, * dev_C;
    size_t size = rows * cols * sizeof(float);

    HANDLE_ERROR(cudaMalloc(&dev_A, size));
    HANDLE_ERROR(cudaMalloc(&dev_B, size));
    HANDLE_ERROR(cudaMalloc(&dev_C, size));

    HANDLE_ERROR(cudaMemcpy(dev_A, host_A, size, cudaMemcpyHostToDevice));
    HANDLE_ERROR(cudaMemcpy(dev_B, host_B, size, cudaMemcpyHostToDevice));

    dim3 threads(16, 16);
    dim3 blocks((cols + 15) / 16, (rows + 15) / 16);
    addition <<<blocks, threads >>> (dev_A, dev_B, dev_C, rows, cols);
    HANDLE_ERROR(cudaGetLastError());
    HANDLE_ERROR(cudaDeviceSynchronize());

    HANDLE_ERROR(cudaMemcpy(host_C, dev_C, size, cudaMemcpyDeviceToHost));

    cudaFree(dev_A);
    cudaFree(dev_B);
    cudaFree(dev_C);
}

// --------------------------------- NON LINEAR ---------------------------------------
void matrixMultiplication(float* host_X, float* host_W, float* host_R, int m, int k, int n) {
    float* dev_X, * dev_W, * dev_R;
    size_t sizeX = m * k * sizeof(float);
    size_t sizeW = k * n * sizeof(float);
    size_t sizeR = m * n * sizeof(float);

    HANDLE_ERROR(cudaMalloc(&dev_X, sizeX));
    HANDLE_ERROR(cudaMalloc(&dev_W, sizeW));
    HANDLE_ERROR(cudaMalloc(&dev_R, sizeR));

    HANDLE_ERROR(cudaMemcpy(dev_X, host_X, sizeX, cudaMemcpyHostToDevice));
    HANDLE_ERROR(cudaMemcpy(dev_W, host_W, sizeW, cudaMemcpyHostToDevice));

    dim3 threads(16, 16);
    dim3 blocks((n + 15) / 16, (m + 15) / 16);
    multiplication <<<blocks, threads >>> (dev_X, dev_W, dev_R, m, k, n);
    HANDLE_ERROR(cudaGetLastError());
    HANDLE_ERROR(cudaDeviceSynchronize());

    HANDLE_ERROR(cudaMemcpy(host_R, dev_R, sizeR, cudaMemcpyDeviceToHost));

    cudaFree(dev_X);
    cudaFree(dev_W);
    cudaFree(dev_R);
}

void matrixDivision(float* host_X, float* host_W, float* host_R, int m, int k, int n) {
    float* dev_X, * dev_W, * dev_R;
    size_t sizeX = m * k * sizeof(float);
    size_t sizeW = k * n * sizeof(float);
    size_t sizeR = m * n * sizeof(float);

    HANDLE_ERROR(cudaMalloc(&dev_X, sizeX));
    HANDLE_ERROR(cudaMalloc(&dev_W, sizeW));
    HANDLE_ERROR(cudaMalloc(&dev_R, sizeR));

    HANDLE_ERROR(cudaMemcpy(dev_X, host_X, sizeX, cudaMemcpyHostToDevice));
    HANDLE_ERROR(cudaMemcpy(dev_W, host_W, sizeW, cudaMemcpyHostToDevice));

    dim3 threads(16, 16);
    dim3 blocks((n + 15) / 16, (m + 15) / 16);
    multiplication << <blocks, threads >> > (dev_X, dev_W, dev_R, m, k, n);
    HANDLE_ERROR(cudaGetLastError());
    HANDLE_ERROR(cudaDeviceSynchronize());

    HANDLE_ERROR(cudaMemcpy(host_R, dev_R, sizeR, cudaMemcpyDeviceToHost));

    cudaFree(dev_X);
    cudaFree(dev_W);
    cudaFree(dev_R);
}


