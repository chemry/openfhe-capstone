OpenFHE - CMU Capstone Linear Regression Implmentation
=====================================

## Introduction
This repository is cloned from the [OpenFHE](https://github.com/openfheorg/openfhe-development) official github repository and modified based on the source. We implemented Linear regression on BFV and CKKS scheme. As well as gradient descent version of OpenFHE with CKKS.

## Installation
On a Ubuntu Linux machine with `cmake` version `3.22.1` and `make` version `4.3`, `gcc` version `11.3.0`, the following command builds successfully.
```
mkdir OpenFHE
cd OpenFHE
git clone https://github.com/chemry/openfhe-capstone.git
cd openfhe-capstone
mkdir build
cd build
cmake ..
make
```
After build, the following binary can be executed to see the results of linear regression outputs:
```
cd ..
./build/bin/examples/pke/linear-regression
./build/bin/examples/pke/linear-regression-ckks
./build/bin/examples/pke/linear-regression-gd-ckks
```

The code are located at [capstone](/src/pke/capstone/).