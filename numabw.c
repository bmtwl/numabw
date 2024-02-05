#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <pthread.h>
#include <numa.h>

// This can be changed to test skipping groups of CPUs (uses modulus and division inline)
#define SKIP_CPUS 1
#define DATA_FILE "/path/to/your.bigfile"

// Define a structure to hold thread-specific data
typedef struct {
    int thread_id;
    int numa_page_size;
    int numa_node;
    int cpu_count;
    int skip_cpus;
    int* data_buffer;
    long long buffer_size;
} ThreadData;

// Function to be executed by each thread
void* threadFunction(void* arg) {
    ThreadData* data = (ThreadData*)arg;

    if (data->thread_id % data->skip_cpus != 0) { 
       free(data);
       pthread_exit(NULL);
    }

    // Set thread affinity to the specified NUMA node
    cpu_set_t cpu_set;
    CPU_ZERO(&cpu_set);
    CPU_SET((data->numa_node * data->cpu_count) + data->thread_id, &cpu_set);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpu_set);

    struct timeval t0, t1, dt;
    gettimeofday(&t0, NULL);

    int thread_id = data->thread_id;

    // Access data buffer associated with the NUMA node
    int* buffer = data->data_buffer;

    int numa_page_size = data->numa_page_size;
    int numa_node = data->numa_node;
    long long buffer_size = data->buffer_size;

    unsigned long long int muldat=0;

    // Perform some work with the data buffer
    //printf("Thread %d on NUMA node %d working on buffer of size %llu\n", thread_id, numa_node, buffer_size);

    int *workbuf = numa_alloc_onnode(numa_page_size * sizeof(int),numa_node);;

    for (long long l=0; l<(buffer_size/numa_page_size); l++) {
       memcpy(workbuf,buffer+(l*numa_page_size),numa_page_size);
       for (int i=0;i<numa_page_size/sizeof(muldat);++i) {
          muldat += *workbuf+i*sizeof(muldat);
       }
    }

    gettimeofday(&t1, NULL);

    timersub(&t1, &t0, &dt);

    if (dt.tv_sec + dt.tv_usec == 0) { dt.tv_usec = 1; }
    unsigned long int bandwidth  = ((buffer_size*1000000) / (dt.tv_sec * 1000000 + dt.tv_usec))/1048576;

    printf("Exiting thread  %02d on NUMA node %d after %d.%06d sec (%i MB/sec) checksum - %llu\n", thread_id, numa_node, dt.tv_sec, dt.tv_usec, bandwidth, muldat);

    // Clean up and exit
    numa_free(workbuf,numa_page_size * sizeof(int));

    free(data);
    pthread_exit(NULL);
}

int main() {
    // Initialize NUMA library
    numa_set_localalloc();

    // Determine the number of available nodes and cores
    int num_nodes = numa_num_configured_nodes();
    int num_cores = numa_num_configured_cpus();
    int cores_per_node = num_cores/num_nodes;
    int numa_page_size = numa_pagesize();

    long long bufsize;

    printf("%d nodes with %d cpus each (%d cpus total)\n",num_nodes,cores_per_node,num_cores);
    printf("NUMA pagesize is %i bytes\nWork buffer size is %u bits\n",numa_page_size, sizeof(bufsize)*8);
    if (SKIP_CPUS>1) { printf("Skipping %i CPU each iteration\n",SKIP_CPUS-1); }

    // Create data buffers for each NUMA node
    int* data_buffers[num_nodes];
    for (int i = 0; i < num_nodes; ++i) {
       FILE *fp = fopen(DATA_FILE, "r");
       if (fp != NULL) {
          if (fseek(fp, 0L, SEEK_END) == 0) {
             bufsize = ftell(fp);
             if (bufsize == -1) { printf("Error getting file size"); }

             printf("Allocating %llu bytes in buffer %i\n",bufsize ,i);
             data_buffers[i] = numa_alloc_onnode(bufsize * sizeof(int),i);

             if (fseek(fp, 0L, SEEK_SET) != 0) { printf("Error seeking to start"); }
             size_t newLen = fread(data_buffers[i], sizeof(char), bufsize, fp);
             if ( ferror( fp ) != 0 ) {
                printf("Error reading file", stderr);
             }
          }
       } else {
          printf("File %s doesn't exist!\nVerify DATA_FILE in source code before compiling\n\n",DATA_FILE);
          return 1;
       }
    }

   printf("Starting threads\n");

    struct timeval t0, t1, dt;
    gettimeofday(&t0, NULL);

    // Create threads with affinity to their respective NUMA node and data buffer
    pthread_t threads[num_nodes * num_cores];
    for (int i = 0; i < num_nodes; ++i) {
        for (int j = 0; j < cores_per_node; ++j) {
            ThreadData* data = (ThreadData*)malloc(sizeof(ThreadData));
            data->thread_id = j;
            data->numa_node = i;
            data->skip_cpus = SKIP_CPUS;
            data->data_buffer = data_buffers[i];
            data->buffer_size = bufsize;
            data->numa_page_size = numa_page_size;
            data->cpu_count = cores_per_node;

            pthread_create(&threads[i * cores_per_node + j], NULL, threadFunction, (void*)data);
        }
    }

    // Wait for threads to complete
    for (int i = 0; i < num_nodes * cores_per_node; ++i) {
        pthread_join(threads[i], NULL);
    }

    gettimeofday(&t1, NULL);
    timersub(&t1, &t0, &dt);

    // Free allocated data buffers
    for (int i = 0; i < num_nodes; ++i) {
	printf("Freeing %llu bytes in buffer %i\n",bufsize ,i);
        numa_free(data_buffers[i], bufsize  * sizeof(int));
    }

    if (dt.tv_sec + dt.tv_usec == 0) { dt.tv_usec = 1; }
    unsigned int bandwidth  = (((bufsize*1000000) / (dt.tv_sec * 1000000 + dt.tv_usec)) *num_nodes*(cores_per_node/SKIP_CPUS))/1048576;

    printf("Exiting program after %d.%06d sec (%d MB/sec)\n", dt.tv_sec, dt.tv_usec, bandwidth);

    return 0;
}

