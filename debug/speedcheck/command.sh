valgrind --tool=callgrind --dump-instr=yes --simulate-cache=yes --collect-jumps=yes ./bin/executable
# then vizualize w/  kcachegrind