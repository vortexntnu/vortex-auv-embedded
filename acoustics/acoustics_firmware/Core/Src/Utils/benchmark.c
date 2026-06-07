/*
 * benchmark.c
 *
 *  Created on: 22. mar. 2026
 *      Author: vikin
 */


// --- Benchmark globals ---
volatile uint32_t mdma_cycles;
volatile uint32_t memcpy_cycles;
volatile uint32_t detection_cycles;
volatile uint32_t full_cycles;
volatile uint32_t mdma_transfer_cycles;

float mdma_us;
float memcpy_us;
float detection_us;
float full_us;
float mdma_transfer_us;

void Benchmark(void) {
    uint32_t t_start;
    uint64_t accumulator;
    uint8_t target_block = fast_get_detection_block_pos();
    uint8_t processing_half = !mdma_half;
    const uint32_t BENCHMARK_N = 512;

    accumulator = 0;
    uint64_t accumulator2 = 0;
    for(int i = 0; i < (int)BENCHMARK_N; i++){
        target_block = fast_get_detection_block_pos();
        processing_half = !mdma_half;
        // --- Test 1: MDMA ---
        t_start = DWT_CYCCNT;
        MDMA_CopyBlock(hydrophone_buffers[0][target_block],detection_buffer[mdma_half]);
        accumulator += (DWT_CYCCNT - t_start)/BENCHMARK_N;
		while(mdma_done_flag == false) __NOP();
		accumulator2 += (DWT_CYCCNT - t_start)/BENCHMARK_N;
		mdma_done_flag = false;
    }
    mdma_cycles = accumulator;
    mdma_transfer_cycles = accumulator2;


    // --- Test 2: memcpy ---
    // Wait for MDMA to finish first to avoid bus contention
    accumulator = 0;
    for(int i = 0; i < (int)BENCHMARK_N; i++){
        target_block = fast_get_detection_block_pos();
        processing_half = !mdma_half;
		t_start = DWT_CYCCNT;
		memcpy(detection_buffer[processing_half], hydrophone_buffers[0][target_block], 128);
		accumulator += (DWT_CYCCNT - t_start)/BENCHMARK_N;
    }
    memcpy_cycles = accumulator;

    accumulator = 0;
    for(int i = 0; i < (int)BENCHMARK_N; i++){
        target_block = fast_get_detection_block_pos();
        processing_half = !mdma_half;
		t_start = DWT_CYCCNT;
		signal_present(processing_half);
		accumulator += (DWT_CYCCNT - t_start)/BENCHMARK_N;
	}
	detection_cycles = accumulator;

    accumulator = 0;
    for(int i = 0; i < (int)BENCHMARK_N; i++){
    t_start = DWT_CYCCNT;
    {
		uint8_t target_block = fast_get_detection_block_pos();
		uint8_t processing_half = !mdma_half;
		MDMA_CopyBlock(hydrophone_buffers[0][target_block],detection_buffer[mdma_half]);
		signal_present(processing_half);
		while(mdma_done_flag == false) {
			__NOP();
		}
		mdma_done_flag = false;
    }
    accumulator += (DWT_CYCCNT - t_start)/BENCHMARK_N;
    }
    full_cycles = accumulator;

    mdma_us   = mdma_cycles   / 480.0f;
    memcpy_us = memcpy_cycles / 480.0f;
    detection_us = detection_cycles / 480.0f;
    full_us = full_cycles / 480.0f;
    mdma_transfer_us = mdma_transfer_cycles / 480.0f;

}
