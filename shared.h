#if !defined(_SHARED_H_)
#define _SHARED_H_

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <string.h>
#include <semaphore.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/msg.h>
#include <pthread.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <sys/mman.h>
#include <time.h>
#include "simulation_manager.h"
#include "control_tower.h"
#include "flight.h"


#define PIPE_NAME "my_pipe"
#define FILESIZE 4194304 // 4 MB
#define DEBUG
#define MAX 200
#define STRINGSIZE 256
#define MAX_TEXT 1024


typedef struct{
    int time_unit;
    int takeoff_duration, takeoff_interval;
    int landing_duration, landing_interval;
    int holding_min_duration, holding_max_duration;
    int max_departures_on_system;
    int max_arrivals_on_system;
}settings_t;


typedef struct{
    int total_flights_created;
    int total_flights_landed;
    double average_waiting_time_landing;
    int total_flights_taken_off;
    double average_time_take_off;
    double holding_maneuvers_landing;
    double holding_maneuvers_may_day;
    int flights_redirectionated;
    int flights_rejected_by_control_tower;
    sem_t sem_stats; //Semaphore to write statistics

}statistic_t;



settings_t settings;
flight_t* flights = NULL;
//Shared mem
int shmid;
statistic_t * sharedMemory;
//Semaphore
sem_t sem_log;
//Message queue
int msqid;
//central process id
int central_process_pid



#endif