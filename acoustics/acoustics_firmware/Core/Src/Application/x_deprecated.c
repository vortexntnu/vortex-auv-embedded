/*
 * deprecated.c
 *
 *  Created on: 24. apr. 2026
 *      Author: vikin
 */

//Deprecated:
#define MAX_TROUGHS WORKSPACE_LEN

uint32_t acoustics_find_da_edge_legacy(const float32_t *signal, uint32_t signal_len)
{
	float32_t buf_min;
	uint32_t min_idx;
	arm_min_f32(signal, signal_len, &buf_min, &min_idx);
	float32_t min_depth = buf_min*0.1f; //deepest trough

	float32_t min_prominence;
	arm_std_f32(signal, signal_len, &min_prominence);
	min_prominence *= 0.01;

    uint32_t           trough_idxs[MAX_TROUGHS];
    find_peaks_props_f32_t props[MAX_TROUGHS];
    uint32_t           n_troughs;

    find_peaks_config_f32_t cfg = FIND_PEAKS_CONFIG_F32_DEFAULT;
    cfg.height     = min_depth;
    cfg.prominence = min_prominence;
    cfg.distance   = 5;

    if(find_troughs_f32(signal, signal_len, &cfg, trough_idxs, props, MAX_TROUGHS, &n_troughs) != FIND_PEAKS_OK){
    	Error_Handler();
    }

    // 4. first_peak = np.min(find_peaks_data) → lowest index found
    //    peak_idx is already in ascending index order, so index 0 is the first
    uint32_t first_trough;
    if (n_troughs > 0) {
        first_trough = trough_idxs[0];          // leftmost peak (min index)
    } else {
        arm_min_f32(signal, signal_len, &buf_min, &first_trough);  // fallback: argmin
    }
    return first_trough;
}


void dump_everything(uint16_t workspace_idx, uint8_t valid){
	printf("dump = {\r\n");

	magnitude_output_f32[0] = 0;
	DEBUG_DUMP_ARRAY_NAMED_DICT_F32("magnitude",magnitude_output_f32,DETECTION_FFT_SIZE/2);
	printf(",");

	printf("\t\"raw_mv\" : [\r\n\t");
	for(int i = 0; i < N_HYDROPHONES; i++){
		q15_t* buffer_flat = (q15_t*)hydrophone_buffers[i];
		circ_unwrap_to_f32(buffer_flat ,processing_workspace[i], WORKSPACE_LEN, BUFFER_LEN, (uint32_t)workspace_idx);
		float32_t scalar = 32768.0*ad7606_channel_scaling_factor(&my_ADC, i)*1000.0;
		arm_scale_f32(processing_workspace[i],scalar,processing_workspace[i],WORKSPACE_LEN);
			debug_dump_python_array_f32(processing_workspace[i], WORKSPACE_LEN);
			if(i == N_HYDROPHONES - 1){
				printf("\r\n],\r\n");
			}else{
				printf(",\r\n\t");
			}
	    //normalize
	    scalar = 0;
	    arm_mean_f32(processing_workspace[i], WORKSPACE_LEN, &scalar);   // Step 1: compute mean
	    arm_offset_f32(processing_workspace[i],-scalar,processing_workspace[i],WORKSPACE_LEN); // Step 2: subtract it
	    scalar = 0;
		arm_rms_f32(processing_workspace[i], WORKSPACE_LEN, &scalar);
		scalar = 1/scalar;
		arm_scale_f32(processing_workspace[i],scalar,processing_workspace[i],WORKSPACE_LEN);
		dsp_spike_filter(processing_workspace[i],WORKSPACE_LEN,10);
	}

	printf("\"normalized\" : [\r\n\t");
	for(int i = 0; i < N_HYDROPHONES; i++){
		debug_dump_python_array_f32(processing_workspace[i], WORKSPACE_LEN);
		if(i == N_HYDROPHONES - 1){
			printf("\r\n],\r\n");
		}else{
			printf(",\r\n\t");
		}
	}

	printf("\"cwt\" : [\r\n\t");
	for(int i = 0; i < N_HYDROPHONES; i++){
		cwt_morlet_magnitude_f32(processing_workspace[i], envelope);
		debug_dump_python_array_f32(envelope, WORKSPACE_LEN);
		if(i == N_HYDROPHONES - 1){
			printf("\r\n],\r\n");
		}else{
			printf(",\r\n\t");
		}
	}


	float32_t thresholds[5];

	printf("\"thresholded\" : [\r\n\t");
	for(int i = 0; i < N_HYDROPHONES; i++){
		cwt_morlet_magnitude_f32(processing_workspace[i], envelope);

		float32_t linear_threshold = 0.01;
		uint8_t max_retries = 6;
		uint32_t dead_space = WORKSPACE_LEN/4;
		idxs[i] = 0;
		float32_t threshold;
		while(idxs[i] < dead_space && max_retries--){
			threshold = dsp_min_max_lerp(envelope, WORKSPACE_LEN, linear_threshold, 15, dead_space);
			idxs[i] = dsp_rl_under_threshold_search(envelope,WORKSPACE_LEN, threshold , 3);
			linear_threshold *= 2;
		}

		thresholds[i] = threshold;
		threshold_applier(envelope, WORKSPACE_LEN, threshold);
		debug_dump_python_array_f32(envelope, WORKSPACE_LEN);
		if(i == N_HYDROPHONES - 1){
			printf("\r\n],\r\n");
		}else{
			printf(",\r\n\t");
		}
	}

	printf("\"envelope_edge\" : [\r\n\t");
	for(int i = 0; i < N_HYDROPHONES; i++){
		cwt_morlet_magnitude_f32(processing_workspace[i], envelope);
		hilbert_imag_f32(envelope,envelope_edge);
		debug_dump_python_array_f32(envelope_edge, PROCESSING_FFT_SIZE);
		if(i == N_HYDROPHONES - 1){
			printf("\r\n],\r\n");
		}else{
			printf(",\r\n\t");
		}
	}

	for(int i = 0; i < N_HYDROPHONES; i++){
		cwt_morlet_magnitude_f32(processing_workspace[i], envelope);

		float32_t linear_threshold = 0.01;
		uint8_t max_retries = 6;
		uint32_t dead_space = WORKSPACE_LEN/4;
		idxs[i] = 0;
		while(idxs[i] < dead_space && max_retries--){
			float32_t threshold = dsp_min_max_lerp(envelope, WORKSPACE_LEN, linear_threshold, 15, dead_space);
			idxs[i] = dsp_rl_under_threshold_search(envelope,WORKSPACE_LEN, threshold , 3);
			linear_threshold *= 2;
		}
		times_of_arrival[i] = (float32_t)idxs[i];//*1/SAMPLING_FREQUENCY;
	}

	DEBUG_DUMP_ARRAY_NAMED_DICT_Q15("idxs", (q15_t*)idxs, N_HYDROPHONES);
	printf(",");



	int32_t tdoa_status = 0;
	tdoa_status = TDOA_direction_solve_f32(hydrophone_positions,
										  times_of_arrival,
										  hydrophone_valid,
										  N_HYDROPHONES,
										  direction_of_arrival);

	printf("\"hydrophone_pos\" : [\r\n\t");
	for(int i = 0; i < N_HYDROPHONES; i++){
		debug_dump_python_array_f32(hydrophone_positions[i], 3);
		if(i == N_HYDROPHONES - 1){
			printf("\r\n],\r\n");
		}else{
			printf(",\r\n\t");
		}
	}

	DEBUG_DUMP_ARRAY_NAMED_DICT_F32("thresholds",thresholds, 5);
	printf(",");
	DEBUG_DUMP_ARRAY_NAMED_DICT_F32("direction_of_arrival",direction_of_arrival, 3);
	printf(",");
	printf("\t\"valid\" : %d",valid);

	fflush(stdout);
	printf("}\r\n");
}
