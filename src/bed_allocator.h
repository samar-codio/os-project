
#ifndef BED_ALLOCATOR_H
#define BED_ALLOCATOR_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>

#define ICU_COUNT       4
#define ISO_COUNT       4
#define GEN_COUNT       12
#define ICU_UNITS       3
#define ISO_UNITS       2
#define GEN_UNITS       1
#define TOTAL_CARE_UNITS (ICU_COUNT*ICU_UNITS + ISO_COUNT*ISO_UNITS + GEN_COUNT*GEN_UNITS)

#define MAX_NAME        64
#define MAX_PATIENTS    100
#define PAGE_SIZE       2

#define SHM_NAME        "/bed_bitmap_shm"
#define SEM_ICU_NAME    "/sem_icu"
#define SEM_ISO_NAME    "/sem_iso"
#define SEM_QUEUE_EMPTY "/sem_queue_empty"
#define SEM_QUEUE_FULL  "/sem_queue_full"
#define DISCHARGE_FIFO  "/tmp/discharge_fifo"
#define TRIAGE_FIFO     "/tmp/triage_fifo"

#define FREE     1
#define OCCUPIED 0

typedef struct {
    int patient_id;
    char name[MAX_NAME];
    int age;
    int severity;
    int priority;
    int care_units;
    time_t arrival_time;
} PatientRecord;

typedef struct {
    int partition_id;
    int start_unit;
    int size;               
    int is_free;
    int patient_id;         
    char bed_type[16];      
} BedPartition;

typedef struct FreeBlock {
    int start;
    int size;
    struct FreeBlock* next;
} FreeBlock;

typedef struct {
    int page_id;
    int patient_id;         
} PageTableEntry;

extern BedPartition* bed_bitmap;   
extern pthread_mutex_t bitmap_mutex;
extern pthread_cond_t bed_freed;
extern pthread_mutex_t queue_mutex;
extern pthread_cond_t queue_not_empty;
extern sem_t* sem_icu;
extern sem_t* sem_iso;
extern sem_t* sem_queue_empty;
extern sem_t* sem_queue_full;

void* receptionist_thread(void* arg);
void* scheduler_thread(void* arg);
void* nurse_icu(void* arg);
void* nurse_isolation(void* arg);
void* nurse_general(void* arg);
void* discharge_listener(void* arg);
void init_shared_memory();
void cleanup_shared_memory();
void init_semaphores();
void cleanup_semaphores();
int allocate_bed(PatientRecord* p, int strategy);
void deallocate_bed(int patient_id);
void coalesce();
void log_fragmentation();
void run_scheduling_simulation();
void sigchld_handler(int sig);
#endif
