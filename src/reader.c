
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include "shared.h"

int main(void)
{
    int fd = shm_open(SHM_NAME, O_RDWR, 0600);

    if (fd == -1) {
        perror("Cannot open database - start writer first");
        return 1;
    }

    SharedData *db = mmap(NULL, sizeof(SharedData),
                          PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (db == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return 1;
    }

    unsigned int last_version = 0;
    int first_display = 1;

    printf("\n===== STUDENT MARKS READER =====\n");
    printf("Waiting for student records and updates...\n");

    while (1) {
        Student snapshot[MAX_STUDENTS];
        int count;
        unsigned int version;

        pthread_mutex_lock(&db->lock);

        count = db->count;
        version = db->version;

        if (count > 0) {
            memcpy(snapshot, db->students, count * sizeof(Student));
        }

        pthread_mutex_unlock(&db->lock);

        if (first_display || version != last_version) {
            printf("\n%-10s %-20s %-12s %-12s\n",
                   "Roll No.", "Subject", "Old Marks", "New Marks");
            printf("--------------------------------------------------------\n");

            if (count == 0) {
                printf("No student records yet.\n");
            } else {
                for (int i = 0; i < count; i++) {
                    printf("%-10d %-20s %-12d %-12d\n",
                           snapshot[i].roll_no,
                           snapshot[i].subject,
                           snapshot[i].old_marks,
                           snapshot[i].marks);
                }
            }

            printf("Waiting for next update...\n");

            last_version = version;
            first_display = 0;
        }

        usleep(200000);
    }

    /* Stop this reader with Ctrl+C. */
    munmap(db, sizeof(SharedData));
    close(fd);
    return 0;
}

