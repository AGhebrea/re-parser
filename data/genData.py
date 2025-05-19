
content = """
(?|.[0...0]+)E
STOP
(^*|*+^/)
"(^")*"
(0|[1...9][0...9]*)
(?|+|-)([0...9])
abababa
(a)
"(^("|\\n))*

"""

print("Overwrite sample1.cfg ? (Press <ENTER>)")
input();

outputfile = open("./sample1.cfg", "w");
# size = 1024**3;
size = (1024**2) * 100;
times = int((size) / len(content)) + 1;

print(f"writing {times} times");
for _ in range(times):
    outputfile.write(content);

outputfile.close();