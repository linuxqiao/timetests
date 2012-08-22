/* threadtest.c 
 *		by: john stultz (johnstul@us.ibm.com)
 *		(C) Copyright IBM 2004, 2005, 2006, 2012
 *		Licensed under the GPLv2
 *
 *  To build:
 *	$ gcc threadtest.c -o threadtest -lrt
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 */
#include <stdio.h>
#include <sys/time.h>
#include <pthread.h>


/* serializes shared list access */
pthread_mutex_t list_lock = PTHREAD_MUTEX_INITIALIZER;
/* serializes console output */
pthread_mutex_t print_lock = PTHREAD_MUTEX_INITIALIZER;


#define MAX_THREADS 128
#define LISTSIZE 128

extern char *optarg;

int done = 0;

struct timespec global_list[LISTSIZE];
int listcount = 0;


void checklist(struct timespec* list, int size)
{
	int i,j;
	struct timespec *a,*b;

	/* scan the list */
	for(i=0; i < size-1; i++){
		a = &list[i];
		b = &list[i+1];
		
		/* look for any time inconsistencies */
		if((b->tv_sec <= a->tv_sec)&&
			(b->tv_nsec < a->tv_nsec)){

			/* flag other threads */
			done = 1;

			/*serialize printing to avoid junky output*/
			pthread_mutex_lock(&print_lock);

			/* dump the list */
			printf("\n");
			for(j=0; j< size; j++){
				if(j == i)
					printf("---------------\n");
				printf("%lu:%lu\n", list[j].tv_sec,list[j].tv_nsec);
				if(j == i+1)
					printf("---------------\n");
			}
			printf("FAILED\n");

			pthread_mutex_unlock(&print_lock);
		}
	}
}

/* The shared thread shares a global list
 * that each thread fills while holding the lock.
 * This stresses clock syncronization across cpus. 
 */
void* shared_thread(void* arg)
{
	while(!done){
		/* protect the list */
		pthread_mutex_lock(&list_lock);
		
		/* see if we're ready to check the list */
		if(listcount >= LISTSIZE){
			checklist(global_list, LISTSIZE);
			listcount = 0;
		}
		clock_gettime(CLOCK_MONOTONIC, &global_list[listcount++]);
				
		pthread_mutex_unlock(&list_lock);
	}
}


/* Each independent thread fills in its own
 * list. This stresses clock_gettime() lock contention. 
 */
void* independent_thread(void* arg)
{
	struct timespec my_list[LISTSIZE];
	int count;

	while(!done){
		/* fill the list */
		for(count=0; count < LISTSIZE; count++)
			clock_gettime(CLOCK_MONOTONIC, &my_list[count]);
		checklist(my_list, LISTSIZE);
	}
}


int main(int argc, char** argv)
{
	int thread_count = 1, i;
	time_t start, runtime = 60;

	pthread_t pth[MAX_THREADS];
	int opt;
	void* tret;
	int ret = 0;
	void* (*thread)(void*) = shared_thread;
	

	/* Process arguments */
	while ((opt = getopt(argc, argv, "t:n:i"))!=-1) {
		switch(opt) {
		case 't':
			runtime = atoi(optarg);
			break;
		case 'n':
			thread_count = atoi(optarg);
			break;
		case 'i':
			thread = independent_thread;
			printf("using independent threads\n");
			break;
		default:
			printf("Usage: %s [-t <secs>] [-n <numthreads>] [-i]\n", argv[0]);
			printf("	-t: time to run\n");
			printf("	-n: number of threads\n");
			printf("	-i: use independent threads\n");
			return -1;
		}
	}

	if(thread_count > MAX_THREADS)
		thread_count = MAX_THREADS;
	

	setbuf(stdout, NULL);

	start = time(0);	
	system("date");
	printf("Testing consistency with %i threads for %ld seconds: ", thread_count,runtime);

	/* spawn */
	for(i=0; i < thread_count; i++)
		pthread_create(&pth[i], 0, thread, 0);
	
	while (time(0) < start + runtime) {
		sleep(1);
		if (done) {
			ret = 1;
			system("date");
			goto out;
		}
	}
	printf("PASSED\n");
	done = 1;

out:
	/* wait */
	for(i=0; i< thread_count; i++)
		pthread_join(pth[i],&tret);

	/* die */
	return ret;
}