#include "shared.h"

void print_struct(){
    printf("%d\n", settings.time_unit);
    printf("%d %d\n", settings.takeoff_duration, settings.takeoff_interval);
    printf("%d %d\n", settings.landing_duration, settings.landing_interval);
    printf("%d %d\n", settings.holding_min_duration, settings.holding_max_duration);
    printf("%d\n", settings.max_departures_on_system);
    printf("%d\n", settings.max_arrivals_on_system);
}

int read_config_file(){
    FILE * file;
    file = fopen(FILENAME, "r");
    if(file==NULL){
        return -1;
    }
    char line[MAX_TEXT];
    if(fscanf(file,"%d", &settings.time_unit)!=1){
        fclose(file);
        return -1;
    }

    fgets(line,MAX_TEXT,file);
    if(fscanf(file,"%d, %d", &settings.takeoff_duration, &settings.takeoff_interval)!=2){
        fclose(file);
        return -1;
    }
    fgets(line,MAX_TEXT,file);
    if(fscanf(file,"%d, %d",&settings.landing_duration, &settings.landing_interval)!=2){
        fclose(file);
        return -1;
    }
    fgets(line,MAX_TEXT,file);
    if(fscanf(file,"%d, %d", &settings.holding_min_duration, &settings.holding_max_duration)!=2){
        fclose(file);
        return -1;
    }
    fgets(line,MAX_TEXT,file);
    if(fscanf(file,"%d", &settings.max_departures_on_system)!=1){
        fclose(file);
        return -1;
    }
    fgets(line,MAX_TEXT,file);
    if(fscanf(file,"%d",&settings.max_arrivals_on_system)!=1){
        fclose(file);
        return -1;
    }
    fclose(file);
    return 1;
}

void init_stats(){
    sem_wait(sem_stats);
    sharedMemoryStats->average_time_take_off = 0;
    sharedMemoryStats->average_waiting_time_landing = 0;
    sharedMemoryStats->flights_redirectionated = 0;
    sharedMemoryStats->flights_rejected_by_control_tower = 0;
    sharedMemoryStats->holding_maneuvers_landing = 0 ;
    sharedMemoryStats->holding_maneuvers_may_day = 0;
    sharedMemoryStats->total_flights_created  = 0;
    sharedMemoryStats->total_flights_landed = 0;
    sharedMemoryStats->total_flights_taken_off = 0;
    sem_post(sem_stats);
}

void show_stats(int sig){
    sem_wait(sem_stats);
	printf("\n\nStatistics:\n\n");
	printf("average_time_take_off: %f\n",sharedMemoryStats->average_time_take_off);
	printf("average_waiting_time_landing: %f\n",sharedMemoryStats ->average_waiting_time_landing);
	printf("Total flights_redirectionated: %d\n",sharedMemoryStats ->flights_redirectionated);
	printf("Total flights_rejected_by_control_tower: %d\n",sharedMemoryStats ->flights_rejected_by_control_tower);
	printf("total_flights_created: %d\n",sharedMemoryStats ->total_flights_created);
    printf("total_flights_landed: %d\n",sharedMemoryStats ->total_flights_landed);
	printf("total_flights_taken_off: %d\n",sharedMemoryStats ->total_flights_taken_off);
    sem_post(sem_stats);
}

void init_semaphores(){

    if ((sem_stats = sem_open("/sem_stats", O_CREAT, 0644, 1)) == SEM_FAILED) {
    	perror("sem_init(): Failed to initialize stats semaphore");
    	exit(EXIT_FAILURE);
    }
    if ((sem_log = sem_open("/sem_log", O_CREAT, 0644, 1)) == SEM_FAILED) {
    	perror("sem_init(): Failed to initialize log semaphore");
    	exit(EXIT_FAILURE);
    }
}

void create_shared_memory(){
    //Initiating Shared memory for Statistics
	shmidStats = shmget(IPC_PRIVATE, sizeof(statistic_t), IPC_CREAT|0666);
    if (shmidStats == -1) {
    	logger("shmget(): Failed to create shared memory for statistics");
    	exit(-1);
    }
    //Attatching shared memory for Statistics
    sharedMemoryStats = shmat(shmidStats, NULL, 0);
    if (*((int *) sharedMemoryStats) == -1) {
    	logger("shmat(): Failed to attach memory for statistics");
    	exit(-1);
    }
}

void create_message_queue(){
    // Message Queue
    msqid = msgget(IPC_PRIVATE, IPC_CREAT|0666);
	if (msqid < -1) {
		logger("msgget(): Failed to create MQ");
		exit(-1);
	}
}

void terminate(int sig){
    logger("Program ended!");

    TERMINATE = 1;
    
    //Shared memory detach
	shmdt(&sharedMemoryStats);

    //Remove shared memory
	shmctl(shmidStats, IPC_RMID, NULL);
    
    //Remove message queue
	msgctl(msqid, IPC_RMID, NULL);

    //Destroy stats semaphore
    if (sem_close(sem_stats) == -1) {
        perror("Error closing sem_stats");
        exit(EXIT_FAILURE);
    }

    //if (sem_unlink("/sem_stats") == -1) {
    //perror("Error unlinking sem_stats");
    //exit(EXIT_FAILURE);
    //}
    
    //Destroy log semaphore
	if (sem_close(sem_log) == -1) {
        perror("Error closing sem_log");
        exit(EXIT_FAILURE);
    }
    
    //if (sem_unlink("/sem_log") == -1) {
    //perror("Error unlinking sem_log");
    //exit(EXIT_FAILURE);
    //}   
    
    //Waits for processes to exit
	while(wait(NULL) > 0);   
}

void init(){
    
    //criar memoria partilhada
    create_shared_memory();

    init_semaphores();
    init_stats();
    
    //criar message queue
    create_message_queue();

    //signals
    signal(SIGUSR1, SIG_IGN);
    signal(SIGINT, terminate);

    //inicializar logs
    init_logs();
    
    //ler ficheiro config.txt
    if(read_config_file()){
        logger("Config read successfully");
    }
    else{
        logger("Error in reading config");
        exit(0);
    }
    
    logger("Program started!");
    
    create_central_process();

    create_pipe();

    int TYPE;
    while(!TERMINATE){
        TYPE = handle_pipe();
        if(TYPE==ARRIVAL){
            create_thread_arrivals(); 
        }
        if(TYPE==DEPARTURE){
            create_thread_departures();
        }
        if(TYPE==-1){
            logger("Error in reading pipe");
        }
    }
}

void create_thread_arrivals(){
    flight_arrival_t * arrival;
    while(flights_arrival != NULL){
        arrival = popFirstArrival(&flights_arrival);
        sleep(time_to_millis(arrival->init));
        if(pthread_create(&(arrival->thread),NULL,flight_arrival,(void*)arrival)!=0){
            logger("Error creating thread arrival flight");
			exit(0);
        }
    }
}

void create_thread_departures(){
    flight_departure_t * departure;
    while(flights_departure != NULL){
        departure = popFirstDeparture(&flights_departure);
        sleep(time_to_millis(departure->init));
        if(pthread_create(&(departure->thread),NULL,flight_departure,(void*)departure)!=0){
            logger("Error creating thread departure flight");
			exit(0);
        }
    }
}


void create_central_process(){
    if(fork() == 0){
        control_tower();
        exit(0);
    }
}

void create_pipe(){
    if((mkfifo(PIPE_NAME, O_CREAT|O_EXCL|0600) < 0) && (errno!=EEXIST)){
        logger("Error creating pipe");
        exit(0);
    }
}

int handle_pipe(){
    int TYPE = 0;
    if((fd_pipe = open(PIPE_NAME, O_RDONLY))<0){
        logger("Error opening named pipe.");
        exit(0);
    }
    FD_ZERO(&read_set);
    FD_SET(fd_pipe,&read_set);
    if(select(fd_pipe+1,&read_set,NULL,NULL,NULL) > 0){
        if(FD_ISSET(fd_pipe,&read_set)){
            char buffer[MAX_TEXT];
            int n = 0;
            char c = 0;
            int error = 0;
            do{
                if(read(fd_pipe,&c,1) <= 0){
                    error=-1;
                    return error;
                }
                if(c != '\n'){
                    buffer[n++] = c;
                }
            }while(c!='\n');
            if(error==-1){
                return error;
            }
            logger(buffer);
            TYPE = parse_request(buffer);
            close(fd_pipe);
        }
    }
    return TYPE;
}

int parse_request(char *str){
    char *buffer;
    buffer = strtok(str, " ");
    int VALID_COMMAND = 0;
    if(strcmp(buffer, "DEPARTURE")==0){
        flight_departure_t *flight = (flight_departure_t*)malloc(sizeof(flight_departure_t));
        strcpy(flight->name, strtok(NULL," "));
        strtok(NULL," ");
        flight->init= atoi(strtok(NULL," "));
        strtok(NULL," ");
        flight->takeoff = atoi(strtok(NULL," "));
        if(((flight->takeoff)-(flight->init))>=0){
            if((count_total_departures() + 1) <= settings.max_departures_on_system){
                append_to_list_departures(flight);
                print_list_departures();
                return DEPARTURE;
            }else{
                logger("You have exceed the total number of departures for this airport");
            }
        }else{
            logger("Takeoff time is less than init time");
        }
        VALID_COMMAND = 1;
    }else if(strcmp(buffer, "ARRIVAL")==0){
        flight_arrival_t *flight = (flight_arrival_t*)malloc(sizeof(flight_arrival_t));
        strcpy(flight->name, strtok(NULL," "));
        strtok(NULL," ");
        flight->init = atoi(strtok(NULL," "));
        strtok(NULL," ");
        flight->eta = atoi(strtok(NULL," "));
        strtok(NULL," ");
        flight->fuel = atoi(strtok(NULL," "));
        if(((flight->fuel)-(flight->eta))>=0){
            if((count_total_arrivals() + 1) <= settings.max_arrivals_on_system){
                append_to_list_arrivals(flight);
                print_list_arrivals();
                return ARRIVAL;
            }else{
                logger("You have exceed the total number of arrivals for this airport");
            }
        }else{
            logger("Plane doesn't have enough fuel to get to the airport");
        }
         VALID_COMMAND = 1;
    }if(!VALID_COMMAND){
        logger("Invalid command!");
    }
    return 0;
}


int main(){
    struct timespec start_time = get_current_time();
    sleep(10);
    struct timespec end_time = get_current_time();
    int time_diffe = time_difference(start_time,end_time);
    printf("diferença = %d\n",time_to_millis(time_diffe));
    init();
    
    return 0;
}

