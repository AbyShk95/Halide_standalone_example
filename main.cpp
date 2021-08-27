#include <assert.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <chrono>
#include <algorithm>
#include <cmath>
#include "dsp/generated/resize.h"

void cpuTest(int iterations) {
    int inputW = 1024;
    int inputH = 1024;
    int inputC = 4;

    int outputW = 512;
    int outputH = 512;
    int outputC = 4;

    uint8_t* inputData = new uint8_t[inputW * inputH * inputC];
    uint8_t* outputData = new uint8_t[outputW * outputH * outputC];
    
    /**
     *  Interleaved RGBA input
     */
    for (int i = 0; i < inputW * inputH * inputC; i += 4) {
        inputData[i]     = 235; // R Channel
        inputData[i + 1] = 35;  // G Channel
        inputData[i + 2] = 178; // B Channel
        inputData[i + 3] = 125; // A Channel
    }

    float x_ratio = inputW / (float) outputW;
    float y_ratio = inputH / (float) outputH;

    uint32_t* x_offset = new uint32_t[outputW];

    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; i++) {
        // NN algorithm
        for (int x = 0; x < outputW; x++) {
            x_offset[x] = std::min((int) floor(x * x_ratio), inputW - 1);
        }

        for (int y = 0; y < outputH; y++) {
            int y_offset = std::min((int) floor(y * y_ratio), inputH - 1);

            unsigned int srcOffset  = y_offset * inputW * inputC;
            unsigned int dstOffset  = y * outputW * outputC;

            uint32_t* S = (uint32_t*) &inputData[srcOffset];
            uint32_t* D = (uint32_t*) &outputData[dstOffset];

            for (int x = 0; x < outputW; x ++) {
                D[x] = S[x_offset[x]];
            }
        }
    }
    
    auto end = std::chrono::high_resolution_clock::now();

    double timeTaken = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    timeTaken = static_cast<double>(timeTaken / (float)iterations);

    /**
     *  Output Verification
     */ 
    for (int i = 0; i < outputW * outputH * outputC; i += 4) {
        if ((outputData[i] != 235) || (outputData[i + 1] != 35) || (outputData[i + 2] != 178) || (outputData[i + 3] != 125)) {
            printf("Error: CPU output is wrong at i = %d\n", i);
            delete[] inputData;
            delete[] outputData;
            delete[] x_offset;
            return;
        }
    }

    printf("Time taken for CPU computation is %f us\n", timeTaken);

    delete[] inputData;
    delete[] outputData;
    delete[] x_offset;

    return;
}

int main(int argc, char **argv) {
    #pragma weak remote_session_control
    if (remote_session_control) { 
        struct remote_rpc_control_unsigned_module data; 
        data.enable = 1; 
        data.domain = CDSP_DOMAIN_ID; 
        remote_session_control(DSPRPC_CONTROL_UNSIGNED_MODULE, (void*)&data, sizeof(data)); 
    }
    /** 
     *  DSP Halide: Set iterations;
     */
    unsigned int timeTaken;
    int iterations = 100;
    if (resize_dspHalide_run(iterations, &timeTaken) == 0) {
        printf("Time taken for Halide execution is %d us\n", timeTaken);
    } else {
        printf("Halide failure");
    }
    
    // Run CPU version
    cpuTest(iterations);

    return 0;
}
