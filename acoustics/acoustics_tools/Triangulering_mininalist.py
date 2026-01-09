import numpy as np
import matplotlib.pyplot as plt

S1 = np.array([0,0,0])
S2 = np.array([0.2,0,0])
S3 = np.array([0,0.2,0])
S4 = np.array([0,0,0.2])
S5 = np.array([-0.2,-0.2,-0.2])
S = np.array([S1,S2,S3,S4,S5])

Q = np.array([20,0,0])

T = np.linalg.norm(S - Q, axis=1)

T = np.linalg.norm(S - Q, axis=1)

T = (T[:]-np.min(T))

A = np.concat((2 * (S[1:] - S[0]),np.reshape(2*(T[1:]-T[0]),(4,1))),axis=1)

B = np.sum(S[1:]**2,axis=1) - np.sum(S[0]**2)+ T[0]**2 - T[1:]**2

""" Ainv = np.linalg.inv(A)
C = np.matmul(Ainv,B) """

Q_est = np.linalg.lstsq(A, B, rcond=None)[0]

print(Q_est)

print(np.reshape(2*(T[1:]-T[0]),(4,1)))
print(2 * (S[1:] - S[0]))
print(A)