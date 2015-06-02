/*
 * File:  	barbers.c
 * Date:   	04.2011
 * Author:  Tomas Valek, xvalek02@stud.fit.vutbr.cz
 * Project: IOS project number 2 - Synchronization of proceses
 * Desc.:   Implement of modified variant of barber sleeping problem using semaphores.
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


//Definuji si jmena pro semafory:
#define SEM_NAME_CEKARNA "/xvalek02_cekarna"
#define SEM_NAME_VYPIS "/xvalek02_vypis"
#define SEM_NAME_HOLIC_SPI "/xvalek02_holic_spi"
#define SEM_NAME_HOLICOVA_ZIDLE "/xvalek02_holicova_zidle"
#define SEM_NAME_HOLIC_DOSTRIHAL "/xvalek02_holic_dostrihal"
#define SEM_NAME_HOLIC_POKRACUJ "/xvalek02_ready_vypisy"

//TODO codestyle upravit
//TODO dopsat README
//TODO promenne prepsat do english
//TODO upravit makefile

typedef struct
	{//struktura pro parametry programu
		unsigned int pocet_zidli;			//pocet zidli v cekarne
		unsigned int genc;					//interval generovani zakazniku		TODO jaky interval?
		unsigned int genb;					//interval obsluhy zakaznika
		unsigned int pocet_zakazniku;		//pocet zakazniku, kteri maji prijit
		char *soubor;						//soubor
	}P_parametry;

typedef struct
	{//sdilena struktura
		unsigned int aktualni_zakaznik;		//aktualne zpracovavany zakaznik
		unsigned int poradi_operace;		//poradi aktualni provadene operace
		unsigned int volne_zidle;			//pocet volnych zidli
	}S_sdilena_data;

//globalni promene:
sem_t *xvalek02_cekarna;

sem_t *xvalek02_vypis;

sem_t *xvalek02_holic_spi;

sem_t *xvalek02_holicova_zidle;

sem_t *xvalek02_holic_dostrihal;

sem_t *xvalek02_ready_vypisy;

FILE *vyst_soubor;

S_sdilena_data * sdilena_data;


void znic_semafory(sem_t *xvalek02_cekarna, sem_t *xvalek02_vypis, sem_t *xvalek02_holic_spi, sem_t *xvalek02_holicova_zidle, sem_t *xvalek02_holic_dostrihal, sem_t *xvalek02_ready_vypisy )
	{ //funkce odstrani vsechny vytvorene semafory ze systemu
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

int konvertuj (char *parametr)
{//funkce prekonvertuje parametr na cislo

	int cislo = atoi(parametr);
	if ( cislo > 0 ) 	return cislo;
	else if ( cislo == 0 )
	{//musime jeste zkontrolovat zda byla zadana 0 nebo chybny parametr
		int kontrola = 0;
		kontrola = strcmp("0", parametr);
		if ( kontrola == cislo )	return cislo;
		else if ( kontrola < 0 )	return UINT_MAX;
	}
return UINT_MAX;
}

int over_parametry(int argc, char *argv[], P_parametry *p)
	{//overime spravnost parametru
		if (argc != 6) return 1;

		p->pocet_zidli = konvertuj(argv[1]);
		p->genc = konvertuj(argv[2]);
		p->genb = konvertuj(argv[3]);
		p->pocet_zakazniku = konvertuj(argv[4]);
		p->soubor = argv[5];

		if (p->pocet_zidli == UINT_MAX || p->genc == UINT_MAX || p->genb == UINT_MAX || p->pocet_zakazniku == UINT_MAX || strlen(p->soubor) == UINT_MAX)
		{//strlen nam vrati pocet znaku v stringu
		 //UINT_MAX v nasem pripade indikuje chybu z konverze
			return 1;
		} 
		else return 0;
	}

unsigned int sdilena_pamet(int pam, int n)
	{
		unsigned int hodnota;
		unsigned int *pamet;
		pamet = shmat(pam, NULL, 0);
		hodnota = (*pamet);
		if (n == -1) *pamet = 1;
		else if (n == 1) (*pamet)++;
		shmdt(pamet);
		return hodnota;
	}

void ukonceni_holice(int signum)
{//uklid
	if (vyst_soubor != stdout) fclose(vyst_soubor);
	sem_close(xvalek02_cekarna);
	sem_close(xvalek02_vypis);
	sem_close(xvalek02_holic_spi);
	sem_close(xvalek02_holicova_zidle);
	sem_close(xvalek02_holic_dostrihal);
	sem_close(xvalek02_ready_vypisy);
	shmdt((void*)sdilena_data);
	fflush(stdout);
	exit(signum);
}

/****************HLAVNI PROGRAM****************/
int main(int argc, char *argv[])
{
	signal(SIGUSR1, ukonceni_holice);

	srand(time(0));//kvuli rand()
	P_parametry p;
	if ( over_parametry(argc, argv, &p) == 1 )
	{//overime spravne zadane parametry
		fprintf(stderr,"Spatne zadane argumenty.\n");
		return 1;
	}

	//vytvorime vsechny potrebne semafory:
	xvalek02_cekarna = sem_open(SEM_NAME_CEKARNA, O_CREAT | O_EXCL, 0600, p.pocet_zidli);

	xvalek02_vypis = sem_open(SEM_NAME_VYPIS, O_CREAT | O_EXCL, 0600, 1);

	xvalek02_holic_spi = sem_open(SEM_NAME_HOLIC_SPI, O_CREAT | O_EXCL, 0600, 0);

	xvalek02_holicova_zidle = sem_open(SEM_NAME_HOLICOVA_ZIDLE, O_CREAT | O_EXCL, 0600, 0);

	xvalek02_holic_dostrihal = sem_open(SEM_NAME_HOLIC_DOSTRIHAL, O_CREAT | O_EXCL, 0600, 0);

	xvalek02_ready_vypisy = sem_open(SEM_NAME_HOLIC_POKRACUJ, O_CREAT | O_EXCL, 0600, 0);

	if ( xvalek02_cekarna == SEM_FAILED || xvalek02_vypis == SEM_FAILED || xvalek02_holic_spi == SEM_FAILED || xvalek02_holicova_zidle == SEM_FAILED  || xvalek02_holic_dostrihal == SEM_FAILED || xvalek02_ready_vypisy == SEM_FAILED )
	{//pokud se nepodari vytvorit jeden ze semaforu
		znic_semafory(xvalek02_cekarna, xvalek02_vypis, xvalek02_holic_spi, xvalek02_holicova_zidle, xvalek02_holic_dostrihal, xvalek02_ready_vypisy );
		perror("sem_open() chyba");
		return 1;
	}

	unsigned int i = 1; //pouzito v cyklu pro vytvareni poctu zakazniku
	int pamet1; // jmena sdilenych pameti

	//vystupni soubor:
	if (strcmp(p.soubor,"-") == 0) vyst_soubor = stdout;
	else
	{
		vyst_soubor = fopen(p.soubor, "w");
		if (vyst_soubor == NULL)
		{//kontrola zda se vytvoril soubor
			znic_semafory(xvalek02_cekarna, xvalek02_vypis, xvalek02_holic_spi, xvalek02_holicova_zidle, xvalek02_holic_dostrihal, xvalek02_ready_vypisy);
			perror("fopen() chyba");
			return 1;
		}
	}

	//generujeme IPC klice:
	key_t klic1 = ftok(argv[0],0);
	if ( klic1 == -1 )
	{//jeden z klicu se nevytvoril
		if (vyst_soubor != stdout) fclose(vyst_soubor);
		znic_semafory(xvalek02_cekarna, xvalek02_vypis, xvalek02_holic_spi, xvalek02_holicova_zidle, xvalek02_holic_dostrihal, xvalek02_ready_vypisy);
		perror("ftok() chyba");
		return 1;
	}

	//inicializujeme sdilenou pamet:
	pamet1 = shmget(klic1, sizeof(S_sdilena_data), IPC_CREAT | 0600);
	if ( pamet1 == -1 )
	{//chyba
		if ( pamet1 == -1 ) shmctl(pamet1,IPC_RMID, NULL);
		if (vyst_soubor != stdout) fclose(vyst_soubor);
		znic_semafory(xvalek02_cekarna, xvalek02_vypis, xvalek02_holic_spi, xvalek02_holicova_zidle, xvalek02_holic_dostrihal, xvalek02_ready_vypisy);
		perror("shmget() chyba");
		return 1;
	}


	//pripojime sdilenou pamet
	sdilena_data = (S_sdilena_data*)shmat(pamet1, NULL, 0);
	if ( sdilena_data == NULL)
	{//chyba
		if (vyst_soubor != stdout) fclose(vyst_soubor);
		znic_semafory(xvalek02_cekarna, xvalek02_vypis, xvalek02_holic_spi, xvalek02_holicova_zidle, xvalek02_holic_dostrihal, xvalek02_ready_vypisy);
		shmdt((void*)sdilena_data);
		shmctl(pamet1,IPC_RMID, NULL);
		perror("shmat() chyba");
		return 1;
	}

	sdilena_data->aktualni_zakaznik = 1;
	sdilena_data->poradi_operace = 1;
	sdilena_data->volne_zidle = p.pocet_zidli;

	pid_t id = fork();

	if ( id == -1 )
	{//neporadilo se vytvorit proces
		if (vyst_soubor != stdout) fclose(vyst_soubor);
		shmdt((void*)sdilena_data);
		shmctl(pamet1,IPC_RMID, NULL);
		znic_semafory(xvalek02_cekarna, xvalek02_vypis, xvalek02_holic_spi, xvalek02_holicova_zidle, xvalek02_holic_dostrihal, xvalek02_ready_vypisy);
		perror("fork() chyba");
		exit(1);
	}

	else if ( id == 0 )
	{//proces holice

		while (1)
		{//cyklus holice

			sem_wait(xvalek02_vypis);
			fprintf(vyst_soubor, "%u: barber: checks\n", sdilena_data->poradi_operace++);//holic zkontroluje cekarnu
			fflush(vyst_soubor);
			sem_post(xvalek02_vypis);

			sem_wait(xvalek02_holic_spi);//holic jde spat

			sem_wait(xvalek02_vypis);
			fprintf(vyst_soubor, "%u: barber: ready\n", sdilena_data->poradi_operace++);//holic je pripraven
			fflush(vyst_soubor);
			sem_post(xvalek02_vypis);

			sem_post(xvalek02_ready_vypisy);//semafor aby byl ready holic pred ready zakaznik

			sem_post(xvalek02_holicova_zidle);//povolime zakaznikovi si sednou do kresla

			usleep(rand() % ((p.genb*1000)+1)); //cas obsluhy holice dany parametrem

			sem_wait(xvalek02_vypis);
			fprintf(vyst_soubor, "%u: barber: finished\n", sdilena_data->poradi_operace++);//ceka nez holic zkontroluje cekarnu
			fflush(vyst_soubor);
			sem_post(xvalek02_vypis);

			sem_post(xvalek02_holic_dostrihal);//ceka nez ho holic dostriha

		}

		return 0;
	}

	for ( i = 1 ; i <= p.pocet_zakazniku ; i++ )
	{//tvorim zakazniky
		usleep(rand() % ((p.genc*1000)+1)); //cas vytvoreni zakaznika
		pid_t zakaznik = fork();
		
		if ( zakaznik == -1 )
		{//neporadilo se vytvorit proces
			perror("fork() chyba");
			break;
		}

		if ( zakaznik > 0 ) continue;

		sem_wait(xvalek02_vypis);
		fprintf(vyst_soubor, "%u: customer %u: created\n", sdilena_data->poradi_operace++, i);
		fflush(vyst_soubor);
		sem_post(xvalek02_vypis);

		if (sem_trywait(xvalek02_cekarna) == -1 && errno == EAGAIN)
		{//nepodarilo se vstoupit do cekarny
			sem_wait(xvalek02_vypis);
			fprintf(vyst_soubor, "%u: customer %u: refused\n", sdilena_data->poradi_operace++, i);
			fflush(vyst_soubor);
			sem_post(xvalek02_vypis);

			//uklid
			if (vyst_soubor != stdout) fclose(vyst_soubor);
			sem_close(xvalek02_cekarna);
			sem_close(xvalek02_vypis);
			sem_close(xvalek02_holic_spi);
			sem_close(xvalek02_holicova_zidle);
			sem_close(xvalek02_holic_dostrihal);
			sem_close(xvalek02_ready_vypisy);
			shmdt((void*)sdilena_data);
			exit(0);
		}

		//dostal jsem se do cekarny:
		sem_wait(xvalek02_vypis);
		fprintf(vyst_soubor, "%u: customer %u: enters\n", sdilena_data->poradi_operace++, i);
		fflush(vyst_soubor);
		sem_post(xvalek02_vypis);

		//zakaznik zbudi holice:
		sem_post(xvalek02_holic_spi);

		//1 zakaznik se posadi:
		sem_wait(xvalek02_holicova_zidle);
		sem_post(xvalek02_cekarna);

		sem_wait(xvalek02_ready_vypisy);

		sem_wait(xvalek02_vypis);
		fprintf(vyst_soubor, "%u: customer %u: ready\n", sdilena_data->poradi_operace++, i);
		fflush(vyst_soubor);
		sem_post(xvalek02_vypis);

		sem_wait(xvalek02_holic_dostrihal);

		//uvolni holicovu zidli dalsimu zakaznikovi:
		sem_post(xvalek02_holicova_zidle);

		sem_wait(xvalek02_vypis);
		fprintf(vyst_soubor, "%u: customer %u: served\n", sdilena_data->poradi_operace++, i);
		fflush(vyst_soubor);
		sem_post(xvalek02_vypis);
	
		//uklid
		if (vyst_soubor != stdout) fclose(vyst_soubor);
		sem_close(xvalek02_cekarna);
		sem_close(xvalek02_vypis);
		sem_close(xvalek02_holic_spi);
		sem_close(xvalek02_holicova_zidle);
		sem_close(xvalek02_holic_dostrihal);
		sem_close(xvalek02_ready_vypisy);
		shmdt((void*)sdilena_data);
		fflush(stdout);
		exit(0);
	}

	for ( i-- ; i != 0 ; i--  )
	{//ukoncujeme zakazniky
		wait(NULL);
	}

	kill(id, SIGUSR1);

	waitpid(id, NULL, 0);
	znic_semafory(xvalek02_cekarna, xvalek02_vypis, xvalek02_holic_spi, xvalek02_holicova_zidle, xvalek02_holic_dostrihal, xvalek02_ready_vypisy);
	shmdt((void*)sdilena_data);
	shmctl(pamet1,IPC_RMID, NULL);
	if (vyst_soubor != stdout) fclose(vyst_soubor);
	return 0;
}
