/*
 * File:  	barbers.c
 * Date:   	04.2011
 * Author:  Tomas Valek, xvalek02@stud.fit.vutbr.cz
 * Project: IOS project number 2 - Synchronization of proceses
 * Desc.:   Implement of modified variant of barber sleeping problem using semaphores
 *			and shared memory. Prevent to deadlock and starvation.
 * Params:  
 *	./barbers Q genc genb N F
 *	Q - number of chairs in waiting room
 *	genc - interval of generate customers [ms]
 *	genb - interval of generate of time service [ms]
 *	N - number of customers
 *	F - output filename
*/

#include <stdio.h>		//standard input/output library functions
#include <stdlib.h>		//general funciton of language C
#include <limits.h>		//UINT_MAX
#include <errno.h>		//perror
#include <sys/types.h>	//fork
#include <unistd.h>		//fork
#include <semaphore.h>	//semaphor
#include <sys/wait.h>	//wait
#include <fcntl.h>		//files
#include <string.h>		//strlen
#include <sys/shm.h>	//shmat-connect of share memory, shmget-allocate segment of shared memory
#include <sys/ipc.h>	//ftok-generate IPC key, shmget-allocate segment of shared memory
#include <time.h>		//srand
#include <signal.h>		//signals

//Definition name of semaphores:
#define SEM_NAME_CEKARNA "/xvalek02_cekarna"
#define SEM_NAME_VYPIS "/xvalek02_vypis"
#define SEM_NAME_HOLIC_SPI "/xvalek02_holic_spi"
#define SEM_NAME_HOLICOVA_ZIDLE "/xvalek02_holicova_zidle"
#define SEM_NAME_HOLIC_DOSTRIHAL "/xvalek02_holic_dostrihal"
#define SEM_NAME_HOLIC_POKRACUJ "/xvalek02_ready_vypisy"

//TODO magicka cisla
//TODO codestyle upravit
//TODO dopsat README
//TODO promenne prepsat do english

typedef struct {
//Params of application
		unsigned int numberOfChairs;		//number of chairs in waiting room
		unsigned int genc;					//interval of generate customers [ms]
		unsigned int genb;					//interval of generate of time service [ms]
		unsigned int numberOfCustomers;		//number of customers
		char *file;							//output file
} P_param;

typedef struct {
//shared struct
		unsigned int currentCustomer;		//current processed customer
		unsigned int orderOfOperations;		//order of operation
		unsigned int numberOfFreeChairs;	//number of free chairs for new customers
} SSharedData;

//global variables:
sem_t *xvalek02_cekarna;
sem_t *xvalek02_vypis;
sem_t *xvalek02_holic_spi;
sem_t *xvalek02_holicova_zidle;
sem_t *xvalek02_holic_dostrihal;
sem_t *xvalek02_ready_vypisy;
FILE *outputFile;
SSharedData * sharedData;


//Release all semaphores.
void cleanSemaphores(sem_t *xvalek02_cekarna, sem_t *xvalek02_vypis, sem_t *xvalek02_holic_spi, sem_t *xvalek02_holicova_zidle, sem_t *xvalek02_holic_dostrihal, sem_t *xvalek02_ready_vypisy ) {
		sem_close(xvalek02_cekarna);
		sem_unlink(SEM_NAME_CEKARNA);

		sem_close(xvalek02_vypis);
		sem_unlink(SEM_NAME_VYPIS);

		sem_close(xvalek02_holic_spi);
		sem_unlink(SEM_NAME_HOLIC_SPI);

		sem_close(xvalek02_holicova_zidle);
		sem_unlink(SEM_NAME_HOLICOVA_ZIDLE);

		sem_close(xvalek02_holic_dostrihal);
		sem_unlink(SEM_NAME_HOLIC_DOSTRIHAL);

		sem_close(xvalek02_ready_vypisy);
		sem_unlink(SEM_NAME_HOLIC_POKRACUJ);
}

//Char's parameter to number parameter.
int convert(char *param) {

	int number = atoi(param);
	if ( number > 0 )
		return number;
	else if ( number == 0 ) {
		int check = strcmp("0", param);
		if ( check == number )
			return number;
		else if ( check < 0 )
			return UINT_MAX;
	}

	return UINT_MAX;
}

//Verify parameters.
int verifyParameters(int argc, char *argv[], P_param *p) {
		if (argc != 6)
			return 1;

		p->numberOfChairs = convert(argv[1]);
		p->genc = convert(argv[2]);
		p->genb = convert(argv[3]);
		p->numberOfCustomers = convert(argv[4]);
		p->file = argv[5];

		if (p->numberOfChairs == UINT_MAX || p->genc == UINT_MAX || p->genb == UINT_MAX || p->numberOfCustomers == UINT_MAX || strlen(p->file) == UINT_MAX)
			return 1;

	return 0;
}

//Clean.
void clean(int signum) {
	if (outputFile != stdout)
		fclose(outputFile);

	sem_close(xvalek02_cekarna);
	sem_close(xvalek02_vypis);
	sem_close(xvalek02_holic_spi);
	sem_close(xvalek02_holicova_zidle);
	sem_close(xvalek02_holic_dostrihal);
	sem_close(xvalek02_ready_vypisy);

	shmdt((void*)sharedData);
	fflush(stdout);
	exit(signum);
}

/****************MAIN****************/
int main(int argc, char *argv[]){

	signal(SIGUSR1, clean);

	srand(time(0));
	P_param p;

	//verify correct parameters
	if ( verifyParameters(argc, argv, &p) == 1 ) {
		fprintf(stderr,"Bad parameters.\n");
		return 1;
	}

	//create semaphores:
	xvalek02_cekarna = sem_open(SEM_NAME_CEKARNA, O_CREAT | O_EXCL, 0600, p.numberOfChairs);

	xvalek02_vypis = sem_open(SEM_NAME_VYPIS, O_CREAT | O_EXCL, 0600, 1);

	xvalek02_holic_spi = sem_open(SEM_NAME_HOLIC_SPI, O_CREAT | O_EXCL, 0600, 0);

	xvalek02_holicova_zidle = sem_open(SEM_NAME_HOLICOVA_ZIDLE, O_CREAT | O_EXCL, 0600, 0);

	xvalek02_holic_dostrihal = sem_open(SEM_NAME_HOLIC_DOSTRIHAL, O_CREAT | O_EXCL, 0600, 0);

	xvalek02_ready_vypisy = sem_open(SEM_NAME_HOLIC_POKRACUJ, O_CREAT | O_EXCL, 0600, 0);

	//create semaphores failed
	if ( xvalek02_cekarna == SEM_FAILED || xvalek02_vypis == SEM_FAILED || xvalek02_holic_spi == SEM_FAILED || xvalek02_holicova_zidle == SEM_FAILED  || xvalek02_holic_dostrihal == SEM_FAILED || xvalek02_ready_vypisy == SEM_FAILED ){
		cleanSemaphores(xvalek02_cekarna, xvalek02_vypis, xvalek02_holic_spi, xvalek02_holicova_zidle, xvalek02_holic_dostrihal, xvalek02_ready_vypisy );
		perror("sem_open() fail.");
		return 1;
	}

	unsigned int i = 1; 	//used in cycle for create of number of customer
	int sharedMemory;

	//output file:
	if (strcmp(p.file,"-") == 0)
		outputFile = stdout;
	else {
		outputFile = fopen(p.file, "w");

		//check is file created correct
		if (outputFile == NULL)	{
			cleanSemaphores(xvalek02_cekarna, xvalek02_vypis, xvalek02_holic_spi, xvalek02_holicova_zidle, xvalek02_holic_dostrihal, xvalek02_ready_vypisy);
			perror("fopen() fail.");
			return 1;
		}
	}

	//generating of IPC keys:
	key_t IPCkey = ftok(argv[0], 0);

	//once of keys did not created
	if ( IPCkey == -1 ) {
		if (outputFile != stdout)
			fclose(outputFile);
		cleanSemaphores(xvalek02_cekarna, xvalek02_vypis, xvalek02_holic_spi, xvalek02_holicova_zidle, xvalek02_holic_dostrihal, xvalek02_ready_vypisy);
		perror("ftok() fail.");
		return 1;
	}

	//init of shared memory:
	sharedMemory = shmget(IPCkey, sizeof(SSharedData), IPC_CREAT | 0600);
	if ( sharedMemory == -1 ) {//error
		if ( sharedMemory == -1 )
			shmctl(sharedMemory,IPC_RMID, NULL);

		if (outputFile != stdout)
			fclose(outputFile);

		cleanSemaphores(xvalek02_cekarna, xvalek02_vypis, xvalek02_holic_spi, xvalek02_holicova_zidle, xvalek02_holic_dostrihal, xvalek02_ready_vypisy);
		perror("shmget() fail.");
		return 1;
	}

	//connect shared memory
	sharedData = (SSharedData*)shmat(sharedMemory, NULL, 0);
	if ( sharedData == NULL) {//error
		if (outputFile != stdout)
			fclose(outputFile);

		cleanSemaphores(xvalek02_cekarna, xvalek02_vypis, xvalek02_holic_spi, xvalek02_holicova_zidle, xvalek02_holic_dostrihal, xvalek02_ready_vypisy);
		shmdt((void*)sharedData);
		shmctl(sharedMemory,IPC_RMID, NULL);
		perror("shmat() fail.");
		return 1;
	}

	sharedData->currentCustomer = 1;
	sharedData->orderOfOperations = 1;
	sharedData->numberOfFreeChairs = p.numberOfChairs;

	pid_t id = fork();

	if ( id == -1 )	{ //fork fail
		if (outputFile != stdout)
			fclose(outputFile);

		shmdt((void*)sharedData);
		shmctl(sharedMemory,IPC_RMID, NULL);
		cleanSemaphores(xvalek02_cekarna, xvalek02_vypis, xvalek02_holic_spi, xvalek02_holicova_zidle, xvalek02_holic_dostrihal, xvalek02_ready_vypisy);
		perror("fork() fail.");
		exit(1);
	} else if ( id == 0 ) {//process of barber

		while (1) {//cycle of barber

			sem_wait(xvalek02_vypis);
			fprintf(outputFile, "%u: barber: checks\n", sharedData->orderOfOperations++); 	//holic checks waiting room
			fflush(outputFile);
			sem_post(xvalek02_vypis);

			sem_wait(xvalek02_holic_spi);													//barber goes sleep

			sem_wait(xvalek02_vypis);
			fprintf(outputFile, "%u: barber: ready\n", sharedData->orderOfOperations++);	//barber is ready
			fflush(outputFile);
			sem_post(xvalek02_vypis);

			sem_post(xvalek02_ready_vypisy);	//semaphore, because "barber ready" must be earlier than "customer ready"

			sem_post(xvalek02_holicova_zidle);	//customer can sit to barber's chair

			usleep(rand() % ((p.genb*1000)+1)); //barber is cutting customer

			sem_wait(xvalek02_vypis);
			fprintf(outputFile, "%u: barber: finished\n", sharedData->orderOfOperations++);//waiting for barber checks waiting room
			fflush(outputFile);
			sem_post(xvalek02_vypis);

			sem_post(xvalek02_holic_dostrihal); //waiting for barber cuts
		}

		return 0;
	}

	for ( i = 1 ; i <= p.numberOfCustomers ; i++ ) { //creating of customers
		usleep(rand() % ((p.genc*1000)+1)); //customer's creating time
		pid_t customer = fork();
		
		//customer creating fail
		if ( customer == -1 ) {
			perror("fork() fail.");
			break;
		}

		if ( customer > 0 )
			continue;

		sem_wait(xvalek02_vypis);
		fprintf(outputFile, "%u: customer %u: created\n", sharedData->orderOfOperations++, i);
		fflush(outputFile);
		sem_post(xvalek02_vypis);

		//unable to enter waiting room
		if (sem_trywait(xvalek02_cekarna) == -1 && errno == EAGAIN) {
			sem_wait(xvalek02_vypis);
			fprintf(outputFile, "%u: customer %u: refused\n", sharedData->orderOfOperations++, i);
			fflush(outputFile);
			sem_post(xvalek02_vypis);

			//clean
			if (outputFile != stdout)
				fclose(outputFile);

			sem_close(xvalek02_cekarna);
			sem_close(xvalek02_vypis);
			sem_close(xvalek02_holic_spi);
			sem_close(xvalek02_holicova_zidle);
			sem_close(xvalek02_holic_dostrihal);
			sem_close(xvalek02_ready_vypisy);
			shmdt((void*)sharedData);
			exit(0);
		}

		//customer is in waiting room
		sem_wait(xvalek02_vypis);
		fprintf(outputFile, "%u: customer %u: enters\n", sharedData->orderOfOperations++, i);
		fflush(outputFile);
		sem_post(xvalek02_vypis);

		//customer wakes up barber:
		sem_post(xvalek02_holic_spi);

		//1 customer allocate chair :D:
		sem_wait(xvalek02_holicova_zidle);
		sem_post(xvalek02_cekarna);

		sem_wait(xvalek02_ready_vypisy);

		sem_wait(xvalek02_vypis);
		fprintf(outputFile, "%u: customer %u: ready\n", sharedData->orderOfOperations++, i);
		fflush(outputFile);
		sem_post(xvalek02_vypis);

		sem_wait(xvalek02_holic_dostrihal);

		//release barber's chair to next customer:
		sem_post(xvalek02_holicova_zidle);

		sem_wait(xvalek02_vypis);
		fprintf(outputFile, "%u: customer %u: served\n", sharedData->orderOfOperations++, i);
		fflush(outputFile);
		sem_post(xvalek02_vypis);
	
		//clean
		if (outputFile != stdout)
			fclose(outputFile);
		sem_close(xvalek02_cekarna);
		sem_close(xvalek02_vypis);
		sem_close(xvalek02_holic_spi);
		sem_close(xvalek02_holicova_zidle);
		sem_close(xvalek02_holic_dostrihal);
		sem_close(xvalek02_ready_vypisy);
		shmdt((void*)sharedData);
		fflush(stdout);
		exit(0);
	}

	//customers ending of
	for ( i-- ; i != 0 ; i--  )	{
		wait(NULL);
	}

	kill(id, SIGUSR1);

	waitpid(id, NULL, 0);
	cleanSemaphores(xvalek02_cekarna, xvalek02_vypis, xvalek02_holic_spi, xvalek02_holicova_zidle, xvalek02_holic_dostrihal, xvalek02_ready_vypisy);
	shmdt((void*)sharedData);
	shmctl(sharedMemory,IPC_RMID, NULL);
	if (outputFile != stdout)
		fclose(outputFile);
	return 0;
}
