/*

Created by Sarthak Gupta
All rights reserved © sarthak-gpt

*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <limits.h>
#include <time.h>


typedef struct Student
{
    int id;
    int helps_received;
    int status; 
    int arrival_time;
} Student;

/*
Status values indicate the following
-1 : Not in Waiting Room
-2 : Inside Waiting Room, but waiting to be processed by Coordinator
-3 : Inside Waiting Room, processed by Coordinator, waiting to be picked by a Tutor
 0 : Indicates that this particular student is being tutored right now
*/

int students; // Total number of Students
int tutors;   // Total number of Tutors
int chairs;   // Total number of Chairs available
int helps;    // Total number of Helps available

Student* student_arr; // to store current attributes of each student
int* tutor_arr;       // simple array to retrieve id of each tutor in the corresponding tutor thread 
int* student_WR_arr_q; // student waiting room array from which the coordinator will process on FCFS basis
int num_student_WR_arr_q;

int* student_being_tutored_by;

int num_free_chairs;
int num_total_requests_to_coordinator;
int num_students_receiving_help_rn;
int num_total_sessions_tutored;
int num_students_finished_all_sessions;
int curr_time;


// sem_t coordinator_semaphore; // semaphore to interact between student and coordinator
// sem_t tutor_semaphore;       // semaphore to interact between coordinator and tutor

pthread_mutex_t lock;        // mutex lock to lock state of all students

// // FOLLOWING CODE CHANGES TO MAKE IT WORK ON MAC OS
sem_t* coordinator_semaphore; // semaphore to interact between student and coordinator
sem_t* tutor_semaphore;       // semaphore to interact between coordinator and tutor



void* student_thread (void* arg){

    int student_id = *(int*)arg;

    while(1){
        if (student_arr[student_id].helps_received >= helps){

            pthread_mutex_lock(&lock);

            num_students_finished_all_sessions++;

            if (num_students_finished_all_sessions == students) {
                // sem_post(&coordinator_semaphore); // sem_post for Unix systems
                sem_post(coordinator_semaphore); // sem_post for MacOS
            }
                
            pthread_mutex_unlock(&lock);

            pthread_exit(NULL);
        }

        pthread_mutex_lock(&lock);
       
        if (num_free_chairs > 0) {

            num_free_chairs--; // student takes a free chair in the waiting room
            num_student_WR_arr_q++;
            student_WR_arr_q[num_student_WR_arr_q - 1] = student_id;

            printf("S: Student %d takes a seat. Empty chairs = %d.\n",
                 student_id, num_free_chairs);

            student_arr[student_id].status = -2;
            student_arr[student_id].arrival_time = curr_time++;
            num_total_requests_to_coordinator++;
            
            // sem_post(&coordinator_semaphore);  // sem_post for Unix
            sem_post(coordinator_semaphore); // For MAC OS

            pthread_mutex_unlock(&lock);

            while (student_arr[student_id].status != -1) {
                sleep(0.2/1000);
            }

            pthread_mutex_lock(&lock);

            int tutor_id = student_being_tutored_by[student_id]; // getting tutor id from student being tutored by array
            printf("S: Student %d received help from Tutor %d.\n", student_id, tutor_id);
            student_being_tutored_by[student_id] = -1; // resetting the value to -1 for that student to indicate it is not being tutored by anyone now
            student_arr[student_id].arrival_time = INT_MAX;

            pthread_mutex_unlock(&lock);


        }

        else {
            //printf("S: Student %d found no empty chair. Will try again later.\n", student_id);
            pthread_mutex_unlock(&lock);

            sleep((float)(rand() % 2000)/1000000);  // make student sleep for 2 milliseconds
        }
        
    }
    return NULL;
}


void* coordinator_thread(){

    while(1) {

        // sem_wait(&coordinator_semaphore); // sem_wait for Unix
        sem_wait(coordinator_semaphore); // sem_wait for Mac OS

        pthread_mutex_lock(&lock);

        if (num_students_finished_all_sessions >= students) {
            for (int i = 0; i < tutors; i++) {
                // sem_post(&tutor_semaphore); // sem_post For UNIX system
                 sem_post(tutor_semaphore); // sem_post for Mac OS system
            }

            pthread_mutex_unlock(&lock);
            pthread_exit(NULL);
        }

        if (num_student_WR_arr_q == 0) {
            pthread_mutex_unlock(&lock);
            continue;
        }

        int student_id = student_WR_arr_q[0];
        num_student_WR_arr_q--;

        for (int i = 0; i < num_student_WR_arr_q; i++)
            student_WR_arr_q[i] = student_WR_arr_q[i+1];
        
        printf("C: Student %d with priority %d added to the queue. Waiting students now = %d. Total requests = %d\n", 
            student_id, student_arr[student_id].helps_received,
            chairs - num_free_chairs, 
            num_total_requests_to_coordinator);
        
        student_arr[student_id].status = -3;

        // sem_post(&tutor_semaphore); // sem_post For Unix 
        sem_post(tutor_semaphore); // sem_post For MAC OS

        pthread_mutex_unlock(&lock);

    }
    
    return NULL;
}


void* tutor_thread(void* arg) {
    int tutor_id = *(int*)arg;
    
    while (1) {
        
        // sem_wait(&tutor_semaphore); // sem_wait for Unix
        sem_wait(tutor_semaphore); // sem_wait for Mac OS

        pthread_mutex_lock(&lock);

        if (num_students_finished_all_sessions >= students) {
            pthread_mutex_unlock(&lock);
            pthread_exit(NULL);
        }
            

        int student_id = -1;
        int min_helps = INT_MAX;
        int min_arrival_time = INT_MAX;
        
        for (int i = 0; i < students; i++) // Finding highest priority student among all student that are waiting to be picked by Tutor
            if (student_arr[i].status == -3){

                if (student_arr[i].helps_received < min_helps) {
                    student_id = i;
                    min_helps = student_arr[i].helps_received;
                    min_arrival_time = student_arr[student_id].arrival_time;
                }
                    
                
                else if (student_arr[i].helps_received == min_helps && student_arr[student_id].arrival_time < min_arrival_time) {
                    student_id = i;
                    min_arrival_time = student_arr[i].arrival_time;
                }
            }


        if(student_id == -1) { // No student present in Waiting Queue for Tutor to pick up
                pthread_mutex_unlock(&lock);
                continue;
            }



        student_arr[student_id].status = 0; // changing status of picked highest priority student to 0 to signify it is being tutored

        num_free_chairs++; // As student leaves WR and enters tutoring area of Tutor tutor_id;
        num_students_receiving_help_rn++;

        pthread_mutex_unlock(&lock);
        
        sleep(0.2 / 1000); // Tutoring happening right now Make this thread sleep for 0.2 milliseconds

        pthread_mutex_lock(&lock);

        // Tutoring finished;
        student_arr[student_id].helps_received++;
        num_students_receiving_help_rn--;
        num_total_sessions_tutored++;

        student_being_tutored_by[student_id] = tutor_id;

        printf("T: Student %d tutored by Tutor %d. Students tutored now = %d. Total sessions tutored = %d\n"
            , student_id, tutor_id, num_students_receiving_help_rn, num_total_sessions_tutored);
        
        student_arr[student_id].status = -1;

        pthread_mutex_unlock(&lock);
        
        
    }
    return NULL;
}


int main(int argc, char* argv[]) {

    srand(time(NULL));

    students = atoi(argv[1]);
    tutors = atoi(argv[2]);
    chairs = atoi(argv[3]);
    helps = atoi(argv[4]);
    
    //printf ("Input given is %d students, %d tutors, %d chairs, %d helps\n", students, tutors, chairs, helps);

    // populating the Student array and it's attributes
    student_arr = (Student*) malloc(students * sizeof(Student));
    for (int i = 0; i < students; i++){
        student_arr[i].id = i;
        student_arr[i].helps_received = 0;
        student_arr[i].status = -1;
        student_arr[i].arrival_time = INT_MAX;
    }


    // populating the tutor array so we can get tutor id in the corresponding tutor thread
    tutor_arr = (int*) malloc(tutors * sizeof(int));
    for (int i = 0; i < tutors; i++)
        tutor_arr[i] = i;


    //initialising Student Waiting Room Queue for Coordinator to process on FCFS basis
    student_WR_arr_q = (int*) malloc(chairs * sizeof(int));
    num_student_WR_arr_q = 0;

    student_being_tutored_by = (int*) malloc(students * sizeof(int));
    for (int i = 0; i < students; i++)
        student_being_tutored_by[i] = -1;

    num_free_chairs = chairs;
    num_total_requests_to_coordinator = 0;
    num_students_receiving_help_rn = 0;
    num_total_sessions_tutored = 0;
    num_students_finished_all_sessions = 0;
    curr_time = 0;


    // Initialising semaphores & mutex
    // sem_init(&coordinator_semaphore, 0, 0);  // sem_init for Unix
    // sem_init(&tutor_semaphore, 0, 0);        // sem_init for Unix

    /* SEM OPEN FOR MACOS*/

    if ((coordinator_semaphore = sem_open("/coordinator_semaphore", O_CREAT, 0644, 0)) == SEM_FAILED) {
        perror("sem_open");
        exit(EXIT_FAILURE);
    }

    if ((tutor_semaphore = sem_open("/tutor_semaphore", O_CREAT, 0644, 0)) == SEM_FAILED) {
        perror("sem_open");
        exit(EXIT_FAILURE);
    }
    /**/

    

    pthread_mutex_init(&lock, NULL);

    // Creating threads for all actors and joining them in the end
    pthread_t student_thread_arr[students];
    pthread_t coordinator_thread_arr;
    pthread_t tutor_thread_arr[tutors];

    for (int i = 0; i < students; i++){
        //printf("Sending student id : %d\n", student_arr[i].id);
        pthread_create(&student_thread_arr[i], NULL, &student_thread, &student_arr[i].id);
    }

    //printf("Creating coordinator thread\n");
    pthread_create(&coordinator_thread_arr, NULL, &coordinator_thread, NULL);

    for (int i = 0; i < tutors; i++){
        //printf("Sending tutor id : %d\n", tutor_arr[i]);
        pthread_create(&tutor_thread_arr[i], NULL, &tutor_thread, &tutor_arr[i]);
    }

    for (int i = 0; i < students; i++) 
        if (pthread_join(student_thread_arr[i], NULL) != 0)
            perror("Failed to join Student thread");


    if (pthread_join(coordinator_thread_arr, NULL) != 0)
        perror("Failed to join Coordinator thread\n");

    for (int i = 0; i < tutors; i++)
        if (pthread_join(tutor_thread_arr[i], NULL))
            perror("Failed to join Tutor thread\n");

    pthread_mutex_destroy(&lock);

    // sem_destroy(&coordinator_semaphore); // sem_destroy for Unix
    // sem_destroy(&tutor_semaphore); // sem_destroy for Unix


    /*  SEM CLOSE FOR MacOS */
    sem_close(coordinator_semaphore);  
    sem_close(tutor_semaphore);
    sem_unlink("/coordinator_semaphore");
    sem_unlink("/tutor_semaphore");
    /**/
    

    return 0;
}