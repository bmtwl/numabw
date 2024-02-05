# numabw
Bandwidth Testing tool for NUMA systems

This tool will autodetect the NUMA nodes and their associated CPU cores, load a large file (set with a #DEFINE in the c file) into each node, and transfer/operate on it from one thread per CPU to try to determine the maximum memory bandwidth available to the system

Edit the filename and compile with:
gcc numabw.c -o numabw -pthread -lnuma -O3 -march=native

For more speed, you could use -Ofast and -march=youractualarch (eg. -march=znver4)
