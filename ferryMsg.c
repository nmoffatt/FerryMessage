#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/time.h>

/* States for captain */
#define DOCKED_LOADING 0
#define SAILING 1
#define DOCKED_UNLOADING 2
#define SAILING_BACK 3
#define IDLE 4

/* Message TYPES */
// car
#define CAR_ARRIVING 1
#define TRUCK_ARRIVING 2
#define CAR_LOADING 3
#define TRUCK_LOADING 4
#define CAR_LOADED 5
#define TRUCK_LOADED 6
#define CAR_UNLOADING 7
#define TRUCK_UNLOADING 8
#define CAR_UNLOADED 9
#define TRUCK_UNLOADED 10
#define START_LOADING 11
#define LOADING_COMPLETE 12
#define LOADING_COMPLETE_ACK 13
#define UNLOADING_COMPLETE 14
#define UNLOADING_COMPLETE_ACK 15
#define FERRY_ARRIVED 16
#define FERRY_ARRIVED_ACK 17
#define TRUCK_ARRIVED 18
#define CAR_ARRIVED 19
#define FERRY_RETURNED 20
#define FERRY_RETURNED_ACK 21
#define CAR_TRAVELING 22
#define TRUCK_TRAVELING 23
#define TERMINATION 24

/* Constants */
#define TOTAL_SPOTS_ON_FERRY 6
#define MAX_TRUCKS 2
#define MAX_LOADS 11
#define CAR_SIZE 1
#define TRUCK_SIZE 2
#define CROSSING_TIME 1000000


/* Redefines the message structure */
typedef struct mymsgbuf
{
    long mtype;
    int id;
} mess_t;

// the four queues we will be using
int qidMessageQ;
int qidLoading;
int qidToCaptainA;
int qidToCaptainB;

// variables for captain and vehicle
mess_t buf;
int length;
int captainState;
int waitingLaneA;
int waitingLaneB;
int msgType;
int fullSpotsOnFerry;

// vehicle variables
int maxTimeToNextArrival;
int truckArrivalProb;
struct timeval startTime;



void cleanUp();

void captain();

int timeChange( const struct timeval startTime );

int init();

int main()
{
    pid_t pid_v;

    captainState = IDLE;

    fullSpotsOnFerry = 0;

    /* queue information */                    
    qidLoading = 0;
    qidToCaptainA = 0;
    qidToCaptainB = 0;
    qidMessageQ = 0;

    length = sizeof(mess_t) - sizeof(long);
    gettimeofday(&startTime, NULL);
    
    printf("Enter the probability the next vehicle is a truck uning an integer between 0 and 100)\n");
    scanf("%d", &truckArrivalProb );
    while(truckArrivalProb < 0 || truckArrivalProb > 100) {
        printf("Probability must be between 0 and 100: ");
        scanf("%d", &truckArrivalProb);
    }
    printf("Enter the maximum interval of time between vehicle arrivals using an integer\n");
    scanf("%d", &maxTimeToNextArrival );
  
    if(init() == 1){
        exit(0);
    }

    // create seperate process for vehicle
    if(!(pid_v = fork())){
        return vehicle();
    }
    // captain process will use existing process
    captain();

    cleanUp();
    
    kill(pid_v, SIGKILL);
    printf("END.\n");
    return 0;
}

 void captain(){
    int loads = 0;
    int trucksOn;
    int k;

    printf("                                            ");
    printf("CAPTAIN PROCESS STARTED.\n");
    
    while (loads <= MAX_LOADS) {
        printf("                                            ");
        printf("LOADING PROCESS HAS STARTED: %d\n", loads);
        buf.mtype = START_LOADING;
        msgsnd(qidMessageQ, &buf, length, 0);
        msgrcv(qidLoading, &buf, length, START_LOADING, 0);

        // new arrivals will be put in a different lane
        if(waitingLaneA == qidToCaptainA ) {
            waitingLaneA = qidToCaptainB;
        }
        else {
            waitingLaneA = qidToCaptainA;
        }

        captainState = DOCKED_LOADING;
	printf("                                            ");
        printf("NOW LOADING.\n");
        trucksOn = 0;
        fullSpotsOnFerry = 0;
        // begin loading trucks
        for ( k = 0; k < MAX_TRUCKS; k++ ) {
            if(msgrcv(qidLoading, &buf, length, TRUCK_ARRIVING, IPC_NOWAIT) != -1) {
		printf("                                            ");
                printf("TRUCK %d LOADING.\n", buf.id);
                fullSpotsOnFerry+=TRUCK_SIZE;
                trucksOn++;
                buf.mtype = TRUCK_LOADING;
                msgsnd(qidMessageQ, &buf, length, 0);
            }
        }

        // begin loading cars
        for( k = 0; k < TOTAL_SPOTS_ON_FERRY; k++ ) {
            if(msgrcv(qidLoading, &buf, length, CAR_ARRIVING, IPC_NOWAIT) != -1) {
		printf("                                            ");
                printf("CAR %d LOADING.\n", buf.id);
                fullSpotsOnFerry+=CAR_SIZE;
                buf.mtype = CAR_LOADING;
                msgsnd(qidMessageQ, &buf, length, 0);
                if(fullSpotsOnFerry == 6) break;
            }
        }

        // begin load from late arrivals
        while( fullSpotsOnFerry < TOTAL_SPOTS_ON_FERRY ) {
            if( trucksOn == MAX_TRUCKS || fullSpotsOnFerry == TOTAL_SPOTS_ON_FERRY-1 ){
                if(msgrcv(waitingLaneA, &buf, length, CAR_ARRIVING, IPC_NOWAIT) != -1){
                    buf.mtype = CAR_LOADING;
                    fullSpotsOnFerry+=CAR_SIZE;
                    msgsnd(qidMessageQ, &buf, length, 0);
		    printf("                                            ");
                    printf("LATE CAR %d LOADING.\n", buf.id);        
                }     
            }
            else {
                if(msgrcv(waitingLaneA, &buf, length, TRUCK_ARRIVING, IPC_NOWAIT) != -1){
                    buf.mtype = TRUCK_LOADING;
                    fullSpotsOnFerry+=TRUCK_SIZE;
                    trucksOn++;
                    msgsnd(qidMessageQ, &buf, length, 0);
		    printf("                                            ");
                    printf("LATE TRUCK %d LOADING.\n", buf.id);
                } 
            }
            
            if(fullSpotsOnFerry == TOTAL_SPOTS_ON_FERRY) break;
        }
        printf("                                            ");
        printf("WAITING FOR LOADING COMPLETION.\n");

        fullSpotsOnFerry = 0;
        while( fullSpotsOnFerry < TOTAL_SPOTS_ON_FERRY) {    
            if(msgrcv(qidLoading, &buf, length, CAR_LOADED, IPC_NOWAIT) != -1 ) {
		printf("                                            ");
                printf("CAR %d HAS BEEN LOADED.\n", buf.id);
                fullSpotsOnFerry+=CAR_SIZE; 
                buf.mtype = CAR_TRAVELING;
                msgsnd(qidMessageQ, &buf, length, 0);
            }
            if(msgrcv(qidLoading, &buf, length, TRUCK_LOADED, IPC_NOWAIT) != -1 ) {
		printf("                                            ");
                printf("TRUCK %d HAS BEEN LOADED.\n", buf.id);
                fullSpotsOnFerry+=TRUCK_SIZE; 
                buf.mtype = TRUCK_TRAVELING;
                msgsnd(qidMessageQ, &buf, length, 0);
            }
        }
	printf("                                            ");
        printf("FERRY FULL.\n");


        // signal loading is complete
        buf.mtype = LOADING_COMPLETE;
        msgsnd(qidMessageQ, &buf, length, 0);
        msgrcv(qidLoading, &buf, length, LOADING_COMPLETE_ACK, 0);
	printf("                                            ");
        printf("MESSAGED TO VEHICLES FERRY IS GOING TO SAIL.\n");

        // begin voyage accross the water
        captainState = SAILING;
	printf("                                            ");
        printf("SAILING.\n");
        usleep(CROSSING_TIME);
	printf("                                            ");
        printf("REACHED OTHER SIDE.");
        buf.mtype = FERRY_ARRIVED;
        msgsnd(qidMessageQ, &buf, length, 0);
        msgrcv(qidLoading, &buf, length, FERRY_ARRIVED_ACK, 0);

        // begin unloading
        captainState = DOCKED_UNLOADING;
	printf("                                            ");
        printf("UNLOADING FERRY.\n");
        fullSpotsOnFerry = 0;
        while( fullSpotsOnFerry < TOTAL_SPOTS_ON_FERRY) {    
            if(msgrcv(qidLoading, &buf, length, CAR_ARRIVED, IPC_NOWAIT) != -1 ) {
		printf("                                            ");
                printf("CAR %d UNLOADING.\n", buf.id);
                fullSpotsOnFerry+=CAR_SIZE; 
                buf.mtype = CAR_UNLOADING;
                msgsnd(qidMessageQ, &buf, length, 0);
            }
            if(msgrcv(qidLoading, &buf, length, TRUCK_ARRIVED, IPC_NOWAIT) != -1 ) {
		printf("                                            ");
                printf("TRUCK %d UNLOADING.\n", buf.id);
                fullSpotsOnFerry+=TRUCK_SIZE; 
                buf.mtype = TRUCK_UNLOADING;
                msgsnd(qidMessageQ, &buf, length, 0);
            }
        }

        // check vehicles are unloaded
        fullSpotsOnFerry = 0;
        while( fullSpotsOnFerry < TOTAL_SPOTS_ON_FERRY) {    
            if(msgrcv(qidLoading, &buf, length, CAR_UNLOADED, IPC_NOWAIT) != -1 ) {
		printf("                                            ");
                printf("CAR %d UNLOADED.\n", buf.id);
                    fullSpotsOnFerry+=CAR_SIZE; 
            }
            if(msgrcv(qidLoading, &buf, length, TRUCK_UNLOADED, IPC_NOWAIT) != -1 ) {
                printf("TRUCK %d UNLOADED.\n", buf.id);
                fullSpotsOnFerry+=TRUCK_SIZE; 
            }
        }
	printf("                                            ");
        printf("FERRY EMPTY.\n");

        // signal unloading complete
        buf.mtype = UNLOADING_COMPLETE;
        captainState = SAILING_BACK;
	printf("                                            ");
        printf("MESSAGED TO VEHICLES FERRY SAILING BACK.\n");
        msgsnd(qidMessageQ, &buf, length, 0);
        msgrcv(qidLoading, &buf, length, UNLOADING_COMPLETE_ACK, 0);

        /* Wait while the ferry crosses the river */
	printf("                                            ");
        printf("SAILING BACK.\n");
        usleep(CROSSING_TIME);
	printf("                                            ");
        printf("RETURNED TO LOADING DOCK.\n");

        // prepare for another load
        buf.mtype = FERRY_RETURNED;
        captainState = DOCKED_LOADING;
	printf("                                            ");
        printf("FERRY READY TO LOAD.\n");
        msgsnd(qidMessageQ, &buf, length, 0);
        msgrcv(qidLoading, &buf, length, FERRY_RETURNED_ACK, 0);

        printf("                                            ");
        printf("END LOAD %d\n", loads);
        loads++;
    }
}

int vehicle(){
    int elapsed = 0;
    int count = 0;
    int lastArrivalTime = 0;
    int isTruck = 0;
    printf("VEHICLE PROCESS  %d\n", qidMessageQ);
        srand (time(0));
        while(1) { 
            // end loop if captain is done
            if(msgrcv(qidMessageQ, &buf, length, TERMINATION, IPC_NOWAIT) != -1 ) {
                msgsnd(qidLoading, &buf, length, 0);
                break;
            }

            // create a new vehicle
            elapsed = timeChange(startTime);
            if(elapsed >= lastArrivalTime) {
                if(lastArrivalTime > 0 ) { 
                    isTruck = rand()% 100;
                    if(isTruck > truckArrivalProb ) {
                        /* This is a car */
                        buf.mtype = CAR_ARRIVING;
                        buf.id = count; 
                        printf("CAR %d ARRIVED.\n", buf.id);
                    }
                    else {
                        /* This is a truck */
                        buf.mtype = TRUCK_ARRIVING;
                        buf.id = count;
                        printf("TRUCK %d ARRIVED.\n", buf.id);
                    }
                    msgsnd(waitingLaneA, &buf, length, 0);
                    usleep(5);
                    }
                lastArrivalTime += rand()% maxTimeToNextArrival;
                count++;
            }

            // check if it is time to load
            if(msgrcv(qidMessageQ, &buf, length, START_LOADING, IPC_NOWAIT) != -1 ) {
                captainState = DOCKED_LOADING;
                printf("CAPTAIN LOADING.\n");
                // switch lanes, so we know who comes late
                waitingLaneB = waitingLaneA;
                if(waitingLaneA == qidToCaptainA ) {
                    waitingLaneA = qidToCaptainB;
                }
                else {
                    waitingLaneA = qidToCaptainA;
                }

                while( msgrcv(waitingLaneB, &buf, length, 0, IPC_NOWAIT) != -1) {
                    msgsnd(qidLoading, &buf, length, 0);
                }

                buf.mtype = START_LOADING;
                msgsnd(qidLoading, &buf, length, 0);
            }

    
            // loading vehicles
            if(captainState == DOCKED_LOADING) {    
                if(msgrcv(qidMessageQ, &buf, length, CAR_LOADING, IPC_NOWAIT) != -1) {
                    buf.mtype = CAR_LOADED;
                    printf("CAR %d LOADED.\n", buf.id);
                    msgsnd(qidLoading, &buf, length, 0);
                }
                if(msgrcv(qidMessageQ, &buf, length, TRUCK_LOADING, IPC_NOWAIT) != -1 ) {
                    buf.mtype = TRUCK_LOADED;
                    printf("TRUCK %d LOADED.\n", buf.id);
                    msgsnd(qidLoading, &buf, length, 0);
                }

                if(msgrcv(qidMessageQ, &buf, length, LOADING_COMPLETE, IPC_NOWAIT) != -1 ) {
                    buf.mtype = LOADING_COMPLETE_ACK;
                    captainState = SAILING;
                    printf("CAPTAIN STATE SAILING.\n");
                    msgsnd(qidLoading, &buf, length, 0);
                }
            }

            // check is ferry has arrived
            if(msgrcv(qidMessageQ, &buf, length, FERRY_ARRIVED, IPC_NOWAIT) != -1 ) {
                captainState = DOCKED_UNLOADING;
                fullSpotsOnFerry = 0;
                printf("CAPTAIN STATE DOCKED_UNLOADING.\n");
                buf.mtype = FERRY_ARRIVED_ACK;
                msgsnd(qidLoading, &buf, length, 0);
            }

            // check if captain is unloading
            if(captainState == DOCKED_UNLOADING ) {    
                if(fullSpotsOnFerry < TOTAL_SPOTS_ON_FERRY ) {    
                    if(msgrcv(qidMessageQ, &buf, length, CAR_TRAVELING, IPC_NOWAIT) != -1 ) {
                        fullSpotsOnFerry+=CAR_SIZE; 
                        buf.mtype = CAR_ARRIVED;
                        msgsnd(qidLoading, &buf, length, 0);
                    }
                    if(msgrcv(qidMessageQ, &buf, length, TRUCK_TRAVELING, IPC_NOWAIT) != -1 ) {
                        fullSpotsOnFerry+=TRUCK_SIZE; 
                        buf.mtype = TRUCK_ARRIVED;
                        msgsnd(qidLoading, &buf, length, 0);
                    }
                }

                if(msgrcv(qidMessageQ, &buf, length, CAR_UNLOADING, IPC_NOWAIT) != -1) {
                    buf.mtype = CAR_UNLOADED;
                    printf("CAR %d UNLOADED.\n", buf.id);
                    msgsnd(qidLoading, &buf, length, 0);
                }

                if(msgrcv(qidMessageQ, &buf, length, TRUCK_UNLOADING, IPC_NOWAIT) != -1 ) {
                    buf.mtype = TRUCK_UNLOADED;
                    printf("TRUCK %d UNLOADED.\n", buf.id);
                    msgsnd(qidLoading, &buf, length, 0);
                }

                if(msgrcv(qidMessageQ, &buf, length, UNLOADING_COMPLETE, IPC_NOWAIT) != -1 ) {
                    buf.mtype = UNLOADING_COMPLETE_ACK;
                    captainState = SAILING_BACK;
                    printf("CAPTAIN STATE SAILING_BACK\n");
                    msgsnd(qidLoading, &buf, length, 0);
                }
            }

            if(msgrcv(qidMessageQ, &buf, length, FERRY_RETURNED, IPC_NOWAIT) != -1 ) {
                captainState = DOCKED_LOADING;
                printf("CAPTAIN STATE LOADING\n");
                buf.mtype = FERRY_RETURNED_ACK;
                msgsnd(qidLoading, &buf, length, 0);
            }

        }
        exit(0);
}

void cleanUp(){
    printf("BEGIN CLEANUP\n");
    buf.mtype = TERMINATION;
    msgsnd(qidMessageQ, &buf, length, 0);
    msgrcv(qidLoading, &buf, length, TERMINATION, 0);
    msgctl(qidLoading, IPC_RMID, 0);
    msgctl(qidToCaptainA, IPC_RMID, 0);
    msgctl(qidToCaptainB, IPC_RMID, 0);
    msgctl(qidMessageQ, IPC_RMID, 0);
    printf("CLEANUP SUCCESSFUL\n");
}

int init(){
    printf("INIT.\n");
    qidLoading = msgget(IPC_PRIVATE, IPC_CREAT | IPC_EXCL | 0660);
    if(qidLoading == -1) {
        printf("LOADING QUEUE NOT CREATED.\n");
        return 1;
    }
    printf("QID LOADING QUEUE %d \n", qidToCaptainA); 

   
    qidToCaptainA = msgget(IPC_PRIVATE, IPC_CREAT | IPC_EXCL | 0660);
    if(qidToCaptainA <= 0) {
        printf("ToCaptainA NOT CREATED.\n");
        msgctl(qidLoading, IPC_RMID, 0);
        return 1;
    }
    printf("QID ToCaptainA %d \n", qidToCaptainA); 
   
    qidMessageQ = msgget(IPC_PRIVATE, IPC_CREAT | IPC_EXCL | 0660);
    if(qidMessageQ <= 0) {
        printf("MESSAGE QUEUE NOT CREATED.\n");
        msgctl(qidLoading, IPC_RMID, 0);
        msgctl(qidToCaptainA, IPC_RMID, 0);
        return 1;
    }
    printf("QID MESSAGE QUEUE %d \n", qidMessageQ); 
    
    qidToCaptainB = msgget(IPC_PRIVATE, IPC_CREAT | IPC_EXCL | 0660);
    if(qidToCaptainB <= 0) {
        printf("ToCaptainB NOT CREATED.\n");
        msgctl(qidLoading, IPC_RMID, 0);
        msgctl(qidToCaptainA, IPC_RMID, 0);
        msgctl(qidMessageQ, IPC_RMID, 0);
        return 1;
    }
    printf("QID ToCaptainB %d \n", qidToCaptainA); 

    waitingLaneA = qidToCaptainA;
    printf("INIT COMPLETE.\n");
}
    
int timeChange( const struct timeval startTime )
{
    struct timeval nowTime;
    long int elapsed;
    int elapsedTime;

    gettimeofday(&nowTime, NULL);
    elapsed = (nowTime.tv_sec - startTime.tv_sec) * 1000000
            + (nowTime.tv_usec - startTime.tv_usec);
    elapsedTime = elapsed / 1000;
    return elapsedTime;
}
