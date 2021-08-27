#include "resizeHalide.generator.h"
#include "HAP_farf.h"
#include "HalideRuntime.h"
#include "HalideRuntimeHexagonHost.h"
#include "HAP_perf.h"
#include "hvx_interface.h"
#include  <stdlib.h>
#include "resize.h"

#undef FARF_LOW
#define FARF_LOW 1
#undef FARF_HIGH
#define FARF_HIGH 1

void halide_print(void *user_context, const char *msg) {
    //FARF(HIGH, "halide_print %s\n", msg);
}

void halide_error(void *user_context, const char *msg) {
    //FARF(LOW, "In halide_error\n");
    //halide_print(user_context, msg);
}

int resize_dspHalide_run(int iterations, unsigned int* avg_time) {
    halide_buffer_t input = {0};
    halide_buffer_t output = {0};

    int inputW = 1024;
    int inputH = 1024;
    int inputC = 4;

    int outputW = 512;
    int outputH = 512;
    int outputC = 4;

    float scaleX = inputW / (float) outputW;
    float scaleY = inputH / (float) outputH;

    halide_dimension_t in_shape[3] = {{0, inputW, 1}, {0, inputH, inputW}, {0, inputC, inputW * inputH}};
    halide_dimension_t out_shape[3] = {{0, outputW, 1}, {0, outputH, outputW}, {0, outputC, outputW * outputH}};

    uint8_t* inputData = (uint8_t*) malloc(inputW * inputH * inputC * sizeof(uint8_t));
    
    /**
     *  Planar format RGBA input
     */
    for (int i = 0; i < inputW * inputH; i ++) {
        inputData[i]     = 235; // R Channel
        inputData[i + (inputW * inputH)] = 35;  // G Channel
        inputData[i + (inputW * inputH * 2)] = 178; // B Channel
        inputData[i + (inputW * inputH * 3)] = 125; // A Channel
    }
    
    input.dimensions = 3;
    input.device = 0;
    input.device_interface = NULL;
    input.type.code = halide_type_uint;
    input.type.bits = 8;
    input.type.lanes = 1;
    input.dim = in_shape;

    output.dimensions = 3;
    output.device = 0;
    output.device_interface = NULL;
    output.type.code = halide_type_uint;
    output.type.bits = 8;
    output.type.lanes = 1;
    output.dim = out_shape;

    input.host  = inputData;
    output.host = (uint8_t*) malloc(outputW * outputH * outputC * sizeof(uint8_t));

    power_on_hvx();
    set_hvx_perf_mode_turbo();

    uint64_t start_time = HAP_perf_get_time_us();

    for (int i = 0; i < iterations; i++) {
        // Hardcoding to integer for this case 1024/512 = 2
        resizeHalide(&input, 2, 2, &output);
    }

    uint64_t end_time = HAP_perf_get_time_us();
    (*avg_time) = (unsigned int)((end_time - start_time) / (float) iterations);

    set_hvx_perf_mode_nominal();
    power_off_hvx();

    /**
     *  Output verification
     */
    for (int i = 0; i < outputW * outputH; i ++) {
        if (output.host[i] != 235 || output.host[i + (outputW * outputH)] != 35 || output.host[i + (outputW * outputH * 2)] != 178 || output.host[i + (outputW * outputH * 3)] != 125) {
            FARF(HIGH, "------Error: Failure on verification------ at i = %d; value is %d", i, output.host[i]);
            free(inputData);
            free(output.host);
            return -1;
        }
    }

    FARF(HIGH, "------Verification success------");

    free(inputData);
    free(output.host);

    return 0;
}
