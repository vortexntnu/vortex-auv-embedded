import fullstack_prototype

fullstack_prototype.main()

""" 
reference_envelope = envelope.copy()

            detected_indices = [None] * 5
            if SNR > 1.0:
                detected_indices[0] = int(np.argmin(envelope_envelope))
                print(f"Hydrophone 1: Detected Index = {detected_indices[0]}")
                for k in range(1, 5):
                    buffer_working_space = working_space(buffers[k], i, block_size)
                    analytic_signal_k = scpy.hilbert(buffer_working_space)
                    envelope_k = np.abs(analytic_signal_k)
                    envelope_analytic_signal_k = scpy.hilbert(envelope_k)
                    envelope_envelope_k = envelope_analytic_signal_k.imag
                    #detected_indices[k] = int(np.argmin(envelope_envelope_k))
                    print(f"Hydrophone {k+1}: Detected Index = {detected_indices[k]}")

                    correlation = scpy.correlate(envelope_k, reference_envelope, mode='full')
                    lags = scpy.correlation_lags(len(envelope_k), len(reference_envelope), mode='full')
                    detected_index = lags[np.argmax(correlation)]
                    detected_indices[k] = int(detected_index + detected_indices[0])
 """