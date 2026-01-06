import numpy as np
import matplotlib.pyplot as plt

S1 = np.array([0,0,0])
S2 = np.array([0.2,0,0])
S3 = np.array([0,0.2,0])
S4 = np.array([0,0,0.2])
S5 = np.array([-0.2,-0.2,-0.2])
S = np.array([S1,S2,S3,S4,S5])

Q = np.array([20,0,0])#np.array([12.4,-24.1,-15.48])#

def rotz(theta,minQ):
    minQ = np.reshape(minQ,(3,1))
    rotMat = np.matrix([
        [np.cos(theta),-np.sin(theta),0],
        [np.sin(theta),np.cos(theta),0],
        [0,0,0]
    ])
    return np.reshape(np.matmul(rotMat,minQ),3)

def roty(theta,minQ):
    minQ = np.reshape(minQ,(3,1))
    rotMat = np.matrix([
        [np.cos(theta),0,-np.sin(theta)],
        [0,0,0],
        [np.sin(theta),0,np.cos(theta)]
    ])
    return np.reshape(np.matmul(rotMat,minQ),3)

ztheta = np.linspace(0,np.pi,20)
ytheta = np.linspace(0,2*np.pi,40)

Qlist = []

""" for phi in ztheta:
    for psy in ytheta:
        Q_ = roty() """

z = []
y = []
x = np.linspace(0,20,100)

T = np.linalg.norm(S - Q, axis=1)

n = 1000

v = 1500
for avvik in x:
    minSum = []
    for i in range(n):
        T = np.linalg.norm(S - Q, axis=1)/1500 + np.ones(5)*129471 
        T = T + np.random.normal(0,avvik*10**(-9),5)#np.array([0,1,2,3,4])*avvik*10**(-9)#
        #print("T",T[:]-np.min(T))
        T = (T[:]-np.min(T))*1500

        A = np.concat((2 * (S[1:] - S[0]),np.reshape(2*(T[1:]-T[0]),(4,1))),axis=1)

        B = np.sum(S[1:]**2,axis=1) - np.sum(S[0]**2)+ T[0]**2 - T[1:]**2

        """ Ainv = np.linalg.inv(A)
        C = np.matmul(Ainv,B) """

        Q_est = np.linalg.lstsq(A, B, rcond=None)[0]
        minSum.append(Q_est)


    mean = np.mean(minSum,axis=0)
    std = np.var(minSum,axis=0)
    std = np.sqrt(std)

    y.append(mean[0])
    z.append(std[0])

print("T",T)
print()
print("A",A)
print()
print("B",B)
print()
print("S",S)
""" print()
print(Ainv)
print()
print("C",C)
print()
print("Q_est",Q_est)
print()
print("gjennomsnitt",mean)
print()
print("standardavvik",std) """

plt.plot(x,y)
plt.show()

plt.plot(x,z)
plt.show()
