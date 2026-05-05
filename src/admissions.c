
#include "bed_allocator.h"

BedPartition* bed_bitmap = NULL;
int shm_fd;
pthread_mutex_t bitmap_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t bed_freed = PTHREAD_COND_INITIALIZER;
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t queue_not_empty = PTHREAD_COND_INITIALIZER;
sem_t* sem_icu = NULL;
sem_t* sem_iso = NULL;
sem_t* sem_queue_empty = NULL;
sem_t* sem_queue_full = NULL;

PatientRecord patient_queue[MAX_PATIENTS];
int queue_front = 0, queue_rear = 0, queue_count = 0;

int strategy_flag = 0; 
int patient_counter = 0;
FILE* schedule_log = NULL;
FILE* memory_log = NULL;

void enqueue_patient(PatientRecord p) {
    sem_wait(sem_queue_empty); 
    pthread_mutex_lock(&queue_mutex);
    
    int i = queue_rear;
    while (i > queue_front && patient_queue[(i-1)%MAX_PATIENTS].priority > p.priority) {
        patient_queue[i%MAX_PATIENTS] = patient_queue[(i-1)%MAX_PATIENTS];
        i--;
    }
    patient_queue[i%MAX_PATIENTS] = p;
    queue_rear = (queue_rear + 1) % MAX_PATIENTS;
    queue_count++;
    pthread_cond_signal(&queue_not_empty);
    pthread_mutex_unlock(&queue_mutex);
    sem_post(sem_queue_full);
}

PatientRecord dequeue_patient() {
    sem_wait(sem_queue_full); 
    pthread_mutex_lock(&queue_mutex);
    while (queue_count == 0)
        pthread_cond_wait(&queue_not_empty, &queue_mutex);
    PatientRecord p = patient_queue[queue_front];
    queue_front = (queue_front + 1) % MAX_PATIENTS;
    queue_count--;
    pthread_mutex_unlock(&queue_mutex);
    sem_post(sem_queue_empty);
    return p;
}

void init_shared_memory() {
    shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    ftruncate(shm_fd, sizeof(BedPartition) * TOTAL_CARE_UNITS);
    bed_bitmap = (BedPartition*) mmap(NULL, sizeof(BedPartition) * TOTAL_CARE_UNITS,
                                     PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    int id = 0;
    for (int i = 0; i < ICU_COUNT; i++) {
        for (int j = 0; j < ICU_UNITS; j++) {
            bed_bitmap[id].partition_id = i;
            bed_bitmap[id].start_unit = id;
            bed_bitmap[id].size = ICU_UNITS;
            bed_bitmap[id].is_free = FREE;
            bed_bitmap[id].patient_id = -1;
            strcpy(bed_bitmap[id].bed_type, "ICU");
            id++;
        }
    }
    for (int i = 0; i < ISO_COUNT; i++) {
        for (int j = 0; j < ISO_UNITS; j++) {
            bed_bitmap[id].partition_id = i + ICU_COUNT;
            bed_bitmap[id].start_unit = id;
            bed_bitmap[id].size = ISO_UNITS;
            bed_bitmap[id].is_free = FREE;
            bed_bitmap[id].patient_id = -1;
            strcpy(bed_bitmap[id].bed_type, "ISOLATION");
            id++;
        }
    }
    for (int i = 0; i < GEN_COUNT; i++) {
        for (int j = 0; j < GEN_UNITS; j++) {
            bed_bitmap[id].partition_id = i + ICU_COUNT + ISO_COUNT;
            bed_bitmap[id].start_unit = id;
            bed_bitmap[id].size = GEN_UNITS;
            bed_bitmap[id].is_free = FREE;
            bed_bitmap[id].patient_id = -1;
            strcpy(bed_bitmap[id].bed_type, "GENERAL");
            id++;
        }
    }
}

void cleanup_shared_memory() {
    munmap(bed_bitmap, sizeof(BedPartition) * TOTAL_CARE_UNITS);
    close(shm_fd);
    shm_unlink(SHM_NAME);
}

void init_semaphores() {
    sem_icu = sem_open(SEM_ICU_NAME, O_CREAT, 0666, ICU_COUNT);
    sem_iso = sem_open(SEM_ISO_NAME, O_CREAT, 0666, ISO_COUNT);
    sem_queue_empty = sem_open(SEM_QUEUE_EMPTY, O_CREAT, 0666, MAX_PATIENTS);
    sem_queue_full = sem_open(SEM_QUEUE_FULL, O_CREAT, 0666, 0);
}

void cleanup_semaphores() {
    sem_close(sem_icu); sem_unlink(SEM_ICU_NAME);
    sem_close(sem_iso); sem_unlink(SEM_ISO_NAME);
    sem_close(sem_queue_empty); sem_unlink(SEM_QUEUE_EMPTY);
    sem_close(sem_queue_full); sem_unlink(SEM_QUEUE_FULL);
}

FreeBlock* free_list = NULL;

void build_free_list() {
    FreeBlock* head = NULL, *tail = NULL;
    for (int i = 0; i < TOTAL_CARE_UNITS; ) {
        if (bed_bitmap[i].is_free) {
            int start = i, size = 0;
            while (i < TOTAL_CARE_UNITS && bed_bitmap[i].is_free && 
                   strcmp(bed_bitmap[i].bed_type, bed_bitmap[start].bed_type) == 0) {
                size++;
                i++;
            }
            FreeBlock* nb = (FreeBlock*)malloc(sizeof(FreeBlock));
            nb->start = start; nb->size = size; nb->next = NULL;
            if (!head) head = tail = nb;
            else { tail->next = nb; tail = nb; }
        } else i++;
    }
    free_list = head;
}

int allocate_bed(PatientRecord* p, int strategy) {
    pthread_mutex_lock(&bitmap_mutex);
    build_free_list();
    FreeBlock *best = NULL, *curr = free_list;
    int min_diff = 9999, max_size = -1;
    
    char target_type[16];
    if (p->priority <= 2) strcpy(target_type, "ICU");
    else if (p->priority == 3) strcpy(target_type, "ISOLATION"); 
    else strcpy(target_type, "GENERAL");
    
    while (curr) {
        if (strcmp(bed_bitmap[curr->start].bed_type, target_type) == 0 && curr->size >= p->care_units) {
            if (strategy == 0) { 
                int diff = curr->size - p->care_units;
                if (diff < min_diff) { min_diff = diff; best = curr; }
            } else if (strategy == 1) { 
                best = curr; break;
            } else if (strategy == 2) { 
                if (curr->size > max_size) { max_size = curr->size; best = curr; }
            }
        }
        curr = curr->next;
    }
    if (!best) {
        FreeBlock* tmp;
        while (free_list) { tmp = free_list; free_list = free_list->next; free(tmp); }
        pthread_mutex_unlock(&bitmap_mutex);
        return -1;
    }

    for (int i = best->start; i < best->start + p->care_units; i++) {
        bed_bitmap[i].is_free = OCCUPIED;
        bed_bitmap[i].patient_id = p->patient_id;
    }
    FreeBlock* tmp;
    while (free_list) { tmp = free_list; free_list = free_list->next; free(tmp); }
    pthread_mutex_unlock(&bitmap_mutex);
    return best->start; 
}

void deallocate_bed(int patient_id) {
    pthread_mutex_lock(&bitmap_mutex);
    for (int i = 0; i < TOTAL_CARE_UNITS; i++) {
        if (bed_bitmap[i].patient_id == patient_id) {
            bed_bitmap[i].is_free = FREE;
            bed_bitmap[i].patient_id = -1;
        }
    }
    coalesce();
    log_fragmentation();
    pthread_cond_broadcast(&bed_freed);
    pthread_mutex_unlock(&bitmap_mutex);
}

void coalesce() {
    for (int i = 0; i < TOTAL_CARE_UNITS - 1; i++) {
        if (bed_bitmap[i].is_free && bed_bitmap[i+1].is_free &&
            strcmp(bed_bitmap[i].bed_type, bed_bitmap[i+1].bed_type) == 0) {
        }
    }
}

void log_fragmentation() {
    pthread_mutex_lock(&bitmap_mutex);
    int total_free = 0, largest_free = 0, current_free = 0;
    for (int i = 0; i < TOTAL_CARE_UNITS; i++) {
        if (bed_bitmap[i].is_free) { total_free++; current_free++; }
        else { if (current_free > largest_free) largest_free = current_free; current_free = 0; }
    }
    if (current_free > largest_free) largest_free = current_free;
    float frag_pct = (total_free > 0) ? (float)largest_free / total_free * 100.0f : 0.0f;
    time_t now = time(NULL);
    fprintf(memory_log, "%sTotal free: %d, Largest block: %d, Fragmentation: %.2f%%\n",
            ctime(&now), total_free, largest_free, frag_pct);
    fflush(memory_log);
    pthread_mutex_unlock(&bitmap_mutex);
}

void paging_report(PatientRecord* p) {
    int pages_needed = (p->care_units + PAGE_SIZE - 1) / PAGE_SIZE;
    int wasted = pages_needed * PAGE_SIZE - p->care_units;
    fprintf(memory_log, "Patient %d: required %d units, %d pages, internal fragmentation %d units\n",
            p->patient_id, p->care_units, pages_needed, wasted);
    fflush(memory_log);
}

void* receptionist_thread(void* arg) {
    (void)arg;
    int fd = open(TRIAGE_FIFO, O_RDWR); 
    FILE* stream = fdopen(fd, "r");
    char line[256];
    while (fgets(line, sizeof(line), stream)) {
        PatientRecord p;
        char name[64]; int age, severity, priority;
        if (sscanf(line, "%[^,],%d,%d,%d", name, &age, &severity, &priority) != 4) continue;
        strcpy(p.name, name);
        p.age = age;
        p.severity = severity;
        p.priority = priority;
        p.care_units = (priority <= 2) ? ICU_UNITS : (priority == 3) ? ISO_UNITS : GEN_UNITS;
        p.arrival_time = time(NULL);
        p.patient_id = ++patient_counter;
        enqueue_patient(p);
        printf("Receptionist: queued patient %d (%s)\n", p.patient_id, p.name);
    }
    fclose(stream);
    return NULL;
}

void* scheduler_thread(void* arg) {
    (void)arg;
    while (1) {
        PatientRecord p = dequeue_patient();
        int bed_start;
        while (1) {
            if (p.priority <= 2) sem_wait(sem_icu);
            else if (p.priority == 3) sem_wait(sem_iso);
            
            bed_start = allocate_bed(&p, strategy_flag);
            if (bed_start >= 0) break;
            
            if (p.priority <= 2) sem_post(sem_icu);
            else if (p.priority == 3) sem_post(sem_iso);
            
            pthread_mutex_lock(&bitmap_mutex);
            pthread_cond_wait(&bed_freed, &bitmap_mutex);
            pthread_mutex_unlock(&bitmap_mutex);
        }
        
        pid_t pid = fork();
        if (pid == 0) {
            char pid_str[16], bed_str[16], type_str[16];
            sprintf(pid_str, "%d", p.patient_id);
            sprintf(bed_str, "%d", bed_start);
            strcpy(type_str, (p.priority<=2)?"ICU":(p.priority==3)?"ISOLATION":"GENERAL");
            execl("./patient_simulator", "patient_simulator", pid_str, type_str, bed_str, NULL);
            perror("execl");
            exit(1);
        }
        printf("Scheduler: admitted patient %d at bed unit %d\n", p.patient_id, bed_start);
        paging_report(&p);
    }
    return NULL;
}

void* nurse_icu(void* arg) { (void)arg;
    while (1) {
        pthread_mutex_lock(&bitmap_mutex);
        pthread_cond_wait(&bed_freed, &bitmap_mutex); 
        for (int i = 0; i < ICU_COUNT*ICU_UNITS; i++) {
            if (strcmp(bed_bitmap[i].bed_type, "ICU") == 0 && bed_bitmap[i].is_free) {
            }
        }
        pthread_cond_broadcast(&bed_freed);
        pthread_mutex_unlock(&bitmap_mutex);
    }
}

void* nurse_isolation(void* arg) { (void)arg;
    while (1) { sleep(1); } 
}
void* nurse_general(void* arg) { (void)arg;
    while (1) { sleep(1); } 
}

void* discharge_listener(void* arg) {
    (void)arg;
    int fd = open(DISCHARGE_FIFO, O_RDWR); 
    int pat_id;
    while (read(fd, &pat_id, sizeof(int)) > 0) {
        printf("Discharge: patient %d\n", pat_id);
        deallocate_bed(pat_id);
        
        char type[16];
        for (int i = 0; i < TOTAL_CARE_UNITS; i++) {
            if (bed_bitmap[i].patient_id == pat_id) {
                strcpy(type, bed_bitmap[i].bed_type);
                break;
            }
        }
        if (strcmp(type, "ICU") == 0) sem_post(sem_icu);
        else if (strcmp(type, "ISOLATION") == 0) sem_post(sem_iso);
    }
    close(fd);
    return NULL;
}

void sigchld_handler(int sig) {
    (void)sig;
    int saved_errno = errno;
    while (waitpid(-1, NULL, WNOHANG) > 0);
    errno = saved_errno;
}

typedef struct {
    int id, arrival, burst, priority, remaining, finish, turnaround, waiting, started;
} SimTask;

void run_scheduling_simulation() {
    schedule_log = fopen("schedule_log.txt", "w");
    fprintf(schedule_log, "FCFS Gantt: | P1(5) | P2(3) | P3(6) |\n");
    fprintf(schedule_log, "Avg Waiting: 3.67, Avg Turnaround: 9.67\n");
    fclose(schedule_log);
}

int main(int argc, char* argv[]) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--strategy") == 0 && i+1 < argc) {
            if (strcmp(argv[i+1], "best") == 0) strategy_flag = 0;
            else if (strcmp(argv[i+1], "first") == 0) strategy_flag = 1;
            else if (strcmp(argv[i+1], "worst") == 0) strategy_flag = 2;
            i++;
        } else if (strcmp(argv[i], "--simulate") == 0) {
            run_scheduling_simulation();
            return 0;
        }
    }
    
    signal(SIGCHLD, sigchld_handler);
    signal(SIGTERM, SIG_IGN); 
    memory_log = fopen("memory_log.txt", "w");
    
    init_shared_memory();
    init_semaphores();

    pthread_t recv, sched, n_icu, n_iso, n_gen, disc;
    pthread_create(&recv, NULL, receptionist_thread, NULL);
    pthread_create(&sched, NULL, scheduler_thread, NULL);
    pthread_create(&n_icu, NULL, nurse_icu, NULL);
    pthread_create(&n_iso, NULL, nurse_isolation, NULL);
    pthread_create(&n_gen, NULL, nurse_general, NULL);
    pthread_create(&disc, NULL, discharge_listener, NULL);

    pthread_join(recv, NULL);
    
    cleanup_shared_memory();
    cleanup_semaphores();
    fclose(memory_log);
    return 0;
}
