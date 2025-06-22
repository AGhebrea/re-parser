from pwn import *

def genData(filename, size):
    n = 8;
    g = cyclic_gen(n=n);
    f = open(filename, "w");
    if(size % n != 0):
        size = n * (size // n + 1);
    f.write(str(g.get(size), "ascii"));
    f.close();

def findData(data, size):
    n = 8;
    g = cyclic_gen(n=n);
    if(size % n != 0):
        size = n * (size // n + 1);
    g.get(size);
    print(g.find(data));


# genData("./sample_1.txt", 660);
# genData("./sample_2.txt", 1320);
# genData("./sample_3.txt", 6600);
# genData("./sample_4.txt", 10000);

findData("aaaaaamv", 10000);
findData("aaaaaabh", 10000);