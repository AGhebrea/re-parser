# Notes:
The lexer part, on a 1.1Gb file runs in 0m3.58s on my pc.
Doing a raw read with 4096 on the same file takes 0m0.149s
Obviously we cannot be as fast as the raw read since we go char by char but I still feel like we could improve the code.

# Optimization
Baseline (without compiler optimization) is 
W/O optimization    | W/ optimization   | size
  0m1.842s          | N/A               | 10Mb file
  0m18.623s         | 0m14.043s         | 100Mb file
As a side note, if I rember correctly, GCC with C compiler does a **similar** thing in 1.1Gb in 18 seconds so we are 10 times
worse than the GCC C compiler, which is ok

# Minimal DFA construction
Parsing the benchmark.cfg file took this much time:
We still need to implement two optimizations

real	4m40.470s
user	4m37.626s
sys	0m1.879s