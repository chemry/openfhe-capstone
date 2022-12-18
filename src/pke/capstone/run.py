import subprocess

size = [100, 500, 1000, 2000, 4000, 8000]
size2 = [512, 1024, 4096, 8192, 16384, 32768]
lr_bfv_path = "/afs/andrew.cmu.edu/usr19/cmingrui/private/OpenFHE/openfhe-development/build/bin/examples/pke/linear-regression"
lr_ckks_path = "/afs/andrew.cmu.edu/usr19/cmingrui/private/OpenFHE/openfhe-development/build/bin/examples/pke/linear-regression-ckks"


def check(prog, size):
    sum = 0
    t = 1
    for i in range(t):
        res = subprocess.check_output([prog, str(size)]).decode('utf-8')
        # print(res)
        res1 = int(res.split("\n")[0])
        sum += res1
        inter, coef = eval(res.split(':')[-1])
        print(inter, coef)

    return sum // t

print("bfv")
for s in size:
    res = check(lr_bfv_path, s)
    print(s, res)
    
print("ckks")
for s in size2:
    res = check(lr_ckks_path, s)
    print(s, res)
