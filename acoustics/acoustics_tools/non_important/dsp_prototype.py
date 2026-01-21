import numpy as np
import matplotlib.pyplot as plt

def adc_oversampling(signal,s):
    N = len(signal)
    n = len(signal)//s
    if N%s != 0:
        n += 1

    new_signal = np.zeros(n)

    for i in range(n):
        if N-i*s < s:
            new_signal[i] = np.average(signal[i*s:N])
        else:
            new_signal[i] = np.average(signal[i*s:i*s+s])

    return new_signal

def single_freq_DFT(signal,freq,dt):
    n = len(signal)
    R = 0
    I = 0
    for i in range(n):
        R += np.cos(2*np.pi*i*freq*dt)*signal[i]
        I += -np.sin(2*np.pi*i*freq*dt)*signal[i]

    amplitude = 2*np.sqrt(R**2+I**2)/n
    phase = np.atan2(I,R)

    #print("Amplitude",amplitude)
    #print("Phase",phase)

    return amplitude,phase

def TDOA_calculate(times):
    time_differences = []
    for i in range(1,len(times)):
        time_differences.append(times[i]-times[0])

    return time_differences

if __name__ == "__main__":
    f = 30000#29000

    time = np.linspace(0,1/(f),20*int(1/f * 1000000))
    dt = time[1]
    data = []
    true_phase = []
    calculated_phase = []
    amplitudes = []
    noise_freq = 2*np.pi*np.random.normal(70000,100000)
    for i in range(5):
        phase = 2*np.pi*np.random.random()
        #print(phase)
        true_phase.append(phase)
        signal = np.zeros(len(time))
        signal += np.sin(time*f*2*np.pi + phase)
        #single_freq_DFT(signal,f,dt)
        signal += np.sin(time*noise_freq + phase)*0.01
        signal += np.random.normal(0,0.01,len(signal))
        signal += np.random.random(len(signal))*0.1
        signal = adc_oversampling(signal,8)
        A, phi = single_freq_DFT(signal,30000,dt*8)
        #print(A)
        print()
        amplitudes.append(float(A))
        calculated_phase.append(phi)
        data.append(signal)
        id = np.arange(len(signal))
        plt.plot(id,data[i])

    if np.average(amplitudes) > 0.75:
        print(TDOA_calculate(true_phase))
        print(TDOA_calculate(calculated_phase))
    
    plt.show()