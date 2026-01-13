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


def TDOA_solve(r,t,c):

    #Generate linear system on form Ap = b so that we can solve for p
    n = len(t)
    A = []
    b = []

    for i in range(1,n):
        Ai = 2*(r[0]-r[i])
        bi = c**2*(t[i]**2-t[0]**2) + np.dot(r[0],r[0]) - np.dot(r[i],r[i])

        A.append(Ai)
        b.append(bi)

    A = np.array(A)
    b = np.array(b)

    #Use least squares to make a system that is easier to solve and fixes other things

    AT = A.T #Transpose
    M = np.matmul(AT,A) + np.identity(3)*10**-6
    y = np.matmul(AT,b)

    #now we solve Mp = y wher M is Semi positive definite and symetrical
    #for this we use cholesky
    
    L = np.linalg.cholesky(M)
    LT = L.T

    #solve L*L^T * p = y by first solving L*x = y for x, then solving L^T * p = L*x for p
    #since L and L^T are triangular this is trivial

    x = np.zeros(3)

    for i in range(3):
        x[i] = (y[i]-np.dot(L[i][:i],x[:i]))/L[i][i]

    p = np.zeros(3)

    for i in range(2,-1,-1):
        p[i] = (x[i]-np.dot(LT[i][i:],p[i:]))/LT[i][i]
    
    return p

# Hydrophone positions
R = np.array([
    [0,0,0],
    [1,0,0],
    [0,1,0],
    [0,0,1],
    [1,1,1]
])

# True source
s_true = np.array([10, 10, 10])

c = 1500.0  # speed of sound
n = 1000
N = 1

error_list = np.zeros(n)

for i in range(n):
    distances = np.zeros(N)
    for j in range(N):

        # Generate arrival times
        t = np.linalg.norm(R - s_true, axis=1) / c

        # Add noise
        t += np.random.normal(0, i*10**-5, size=len(t))

        #solve
        p = TDOA_solve(R,t,c)
        distances[j] = np.linalg.norm(s_true-p)
    #print(np.std(distances))
    error_list[i] = np.average(distances)

#print(p)
index = np.arange(len(error_list))
plt.plot(index,error_list)
plt.show()