
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

int main(int argc, char* argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <patient_id> <bed_type> <bed_start>\n", argv[0]);
        return 1;
    }
    int patient_id = atoi(argv[1]);
    char* bed_type = argv[2];
    int bed_start = atoi(argv[3]);

    srand(time(NULL) ^ getpid());
    int sleep_min, sleep_max;
    if (bed_type[0] == 'I' && bed_type[1] == 'C') { sleep_min=5; sleep_max=15; }
    else if (bed_type[0] == 'I') { sleep_min=3; sleep_max=10; }
    else { sleep_min=2; sleep_max=8; }
    int duration = sleep_min + rand() % (sleep_max - sleep_min + 1);

    printf("Patient %d arrived, bed %d (%s), starting treatment (%d s)\n", patient_id, bed_start, bed_type, duration);
    sleep(duration);
    printf("Patient %d discharged\n", patient_id);

    int fd = open("/tmp/discharge_fifo", O_WRONLY);
    if (fd >= 0) {
        write(fd, &patient_id, sizeof(int));
        close(fd);
    }
    return 0;
}
