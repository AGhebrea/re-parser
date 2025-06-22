from pwn import *

n = 4;
skip = 8000;

def genData(filename, size):
    global n;
    global skip;
    g = cyclic_gen(n=n);
    f = open(filename, "w");
    if(skip % n != 0):
        skip = n * (skip // n + 1);
    g.get(skip);
    if(size % n != 0):
        size = n * (size // n + 1);
    f.write(str(g.get(size), "ascii"));
    f.close();

def findData(data, size):
    global n;
    global skip;
    g = cyclic_gen(n=n);
    if(skip % n != 0):
        skip = n * (skip // n + 1);
    g.get(skip);
    if(size % n != 0):
        size = n * (size // n + 1);
    g.get(size);
    print(g.find(data));


# genData("./sample_1.txt", 660);
# genData("./sample_2.txt", 1320);
# genData("./sample_3.txt", 6600);
genData("./sample_4.txt", 8200);
# genData("./sample_5.txt", 0x1000 * 0x1002);