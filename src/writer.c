
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include "shared.h"

int read_number(const char *prompt, int *number)
{
    char line[100];
    char *end;

    printf("%s", prompt);
    fflush(stdout);

    if (fgets(line, sizeof(line), stdin) == NULL)
        return 0;

    long value = strtol(line, &end, 10);

    while (*end == ' ' || *end == '\t')
        end++;

    if (end == line || (*end != '\n' && *end != '\0'))
        return -1;

    *number = (int)value;
    return 1;
}

int main(void)
{
    /* Start a fresh session.
       Stop old reader terminals before restarting the writer. */
    shm_unlink(SHM_NAME);

    int fd = shm_open(SHM_NAME, O_CREAT | O_EXCL | O_RDWR, 0600);

    if (fd == -1) {
        perror("shm_open");
        return 1;
    }

    if (ftruncate(fd, sizeof(SharedData)) == -1) {
        perror("ftruncate");
        close(fd);
        shm_unlink(SHM_NAME);
        return 1;
    }

    SharedData *db = mmap(NULL, sizeof(SharedData),
                          PROT_READ | PROT_WRITE,
                          MAP_SHARED, fd, 0);

    if (db == MAP_FAILED) {
        perror("mmap");
        close(fd);
        shm_unlink(SHM_NAME);
        return 1;
    }

    memset(db, 0, sizeof(SharedData));

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);

    if (pthread_mutex_init(&db->lock, &attr) != 0) {
        fprintf(stderr, "Mutex initialization failed.\n");
        pthread_mutexattr_destroy(&attr);
        munmap(db, sizeof(SharedData));
        close(fd);
        shm_unlink(SHM_NAME);
        return 1;
    }

    pthread_mutexattr_destroy(&attr);

    printf("\n===== STUDENT MARKS WRITER =====\n");
    printf("Enter roll number 0 to stop.\n");

    while (1) {
        int roll, marks;
        char subject[SUBJECT_LEN];
        char answer[20];

        int result = read_number("\nEnter roll number: ", &roll);

        if (result == 0)
            break;

        if (result != 1 || roll < 0 || roll > 999999) {
            printf("Invalid roll number. Try again.\n");
            continue;
        }

        if (roll == 0)
            break;

        printf("Enter subject: ");
        fflush(stdout);

        if (fgets(subject, sizeof(subject), stdin) == NULL)
            break;

        /* Remove the newline from the subject. */
        char *newline = strchr(subject, '\n');

        if (newline != NULL) {
            *newline = '\0';
        } else if (!feof(stdin)) {
            int ch;
            while ((ch = getchar()) != '\n' && ch != EOF) {}
            printf("Subject is too long. Try again.\n");
            continue;
        }

        if (subject[0] == '\0') {
            printf("Subject cannot be empty.\n");
            continue;
        }

        pthread_mutex_lock(&db->lock);

        int found = -1;

        /* Find a record with the same roll number and subject. */
        for (int i = 0; i < db->count; i++) {
            if (db->students[i].roll_no == roll &&
                strcmp(db->students[i].subject, subject) == 0) {
                found = i;
                break;
            }
        }

        if (found >= 0) {
            printf("Current marks: %d\n", db->students[found].marks);
            printf("Do you want to update marks? (y/n): ");
            fflush(stdout);

            if (fgets(answer, sizeof(answer), stdin) == NULL) {
                pthread_mutex_unlock(&db->lock);
                break;
            }

            if (answer[0] != 'y' && answer[0] != 'Y') {
                printf("Marks remain unchanged.\n");
                pthread_mutex_unlock(&db->lock);
                continue;
            }

            result = read_number("Enter new marks (0-100): ", &marks);

            if (result != 1 || marks < 0 || marks > 100) {
                printf("Invalid marks. Update cancelled.\n");
                pthread_mutex_unlock(&db->lock);
                continue;
            }

            int previous = db->students[found].marks;

            db->students[found].old_marks = previous;
            db->students[found].marks = marks;
            db->version++;

            printf("Marks updated: %d -> %d\n", previous, marks);

        } else {
            if (db->count >= MAX_STUDENTS) {
                printf("Database is full.\n");
                pthread_mutex_unlock(&db->lock);
                continue;
            }

            result = read_number("Enter marks (0-100): ", &marks);

            if (result != 1 || marks < 0 || marks > 100) {
                printf("Invalid marks. Record not added.\n");
                pthread_mutex_unlock(&db->lock);
                continue;
            }

            Student *s = &db->students[db->count];

            s->roll_no = roll;
            snprintf(s->subject, sizeof(s->subject), "%s", subject);
            s->old_marks = marks;
            s->marks = marks;

            db->count++;
            db->version++;

            printf("New student subject record added.\n");
        }

        pthread_mutex_unlock(&db->lock);

        printf("Shared database updated. Readers will see the change.\n");
    }

    printf("\nWriter stopped. Stop both readers before restarting.\n");

    pthread_mutex_destroy(&db->lock);
    munmap(db, sizeof(SharedData));
    close(fd);
    shm_unlink(SHM_NAME);

    return 0;
}

