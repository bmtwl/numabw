# numabw
Bandwidth Testing tool for NUMA systems

This tool will autodetect the NUMA nodes and their associated CPU cores, load a large file (set with a #DEFINE in the c file) into each node, and transfer/operate on it from one thread per CPU to try to determine the maximum memory bandwidth available to the system

It differs from other bandwidth testers in that it attempts to saturate bandwidth on a single dataset across all threads to simulate things like LLM inference.

Edit the #DEFINE DATA_FILE and compile with:
gcc numabw.c -o numabw -pthread -lnuma -O3 -march=native

For more speed, you could use -Ofast and -march=youractualarch (eg. -march=znver4)

There is also a #DEFINE option to skip CPUS. The default value of 1 uses all CPUs, 2 would be every other CPU, 3 every third CPU, etc
