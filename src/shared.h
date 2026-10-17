
#ifndef SHARED_H
#define SHARED_H

#include <pthread.h>

#define SHM_NAME "/student_marks_database"
#define MAX_STUDENTS 100
#define SUBJECT_LEN 50

typedef struct {
    int roll_no;
    char subject[SUBJECT_LEN];
    int old_marks;
    int marks;
} Student;

typedef struct {
    pthread_mutex_t lock;
    int count;
    unsigned int version;
    Student students[MAX_STUDENTS];
} SharedData;

#endif

