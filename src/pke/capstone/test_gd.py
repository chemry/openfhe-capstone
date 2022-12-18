# import numpy as np
# from sklearn.linear_model import LinearRegression
import numpy as np

L = 0.0001  # The learning Rate
epochs = 15  # The number of iterations to perform gradient descent
m = 0
c = 0
print("Linear Regression Gradient Descent with Python")
age = []
income = []
n = 256
with open("/afs/andrew.cmu.edu/usr24/jiachend/public/cleaned_small_user_data.csv") as f:
    line = f.readline()
    count = 0
    while count < n:
        line = f.readline()
        if not line or len(line) <= 1:
            break
        line = line.split(',')
        age.append(eval(line[2]))
        income.append(eval(line[3]))
        count += 1
age = age * 100
income = income * 100
age = age[:n]
income = income[:n]

FACTOR = 100

X = np.array(age)
Y = np.array(income)
n = float(n)

for i in range(epochs): 
    Y_pred = m * X + c  # The current predicted value of Y
    Y_dif = Y - Y_pred
    D_m = L * (-2.0/n) * sum(X * Y_dif)  # Derivative wrt m
    D_c = L * (-2.0/n) * sum(Y_dif)  # Derivative wrt c
    m = m - D_m  # Update m
    c = c - D_c  # Update c
    print("iter", i, "m", m, "c", c)

""" m, c
0 441.0486765625 9.713069531250001
1 667.0536549337348 15.368271470611594
2 782.8583340360261 18.943979400516696
3 842.19027747752 21.45402150312248
4 872.5824641671279 23.41793948305007
"""