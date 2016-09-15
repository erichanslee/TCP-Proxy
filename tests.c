#include <pthread.h>
 #include <stdio.h>
 #include <stdlib.h>

 #define NUM_THREADS  3
 #define TCOUNT 3
 #define COUNT_LIMIT 3

int     count = 0;
int     thread_ids[3] = {0,1,2};
pthread_mutex_t mutexes[NUM_THREADS];
pthread_cond_t count_threshold_cv[NUM_THREADS];


void *watch_count(void *t) 
{
	long my_id = (long)t;

	printf("Starting watch_count(): thread %ld\n", my_id);

   /*
   Lock mutex and wait for signal.  Note that the pthread_cond_wait 
   routine will automatically and atomically unlock mutex while it waits. 
   Also, note that if COUNT_LIMIT is reached before this routine is run by
   the waiting thread, the loop will be skipped to prevent pthread_cond_wait
   from never returning. 
   */
   pthread_mutex_lock(&mutexes[my_id]);
   printf("watch_count(): thread %ld Condition signal received.\n", my_id);
   pthread_mutex_unlock(&mutexes[my_id]);
   pthread_exit(NULL);
}

int main (int argc, char *argv[])
{
	int i, rc;
	pthread_t threads[NUM_THREADS];

   /* Initialize mutex and condition variable objects */
	for(i = 0; i < NUM_THREADS; i++){
		pthread_mutex_init(&mutexes[i], NULL);
		pthread_mutex_lock(&mutexes[i]);
		pthread_cond_init (&count_threshold_cv[i], NULL);
		pthread_create(&threads[i], NULL, watch_count, (void *)i);
	}

	sleep(1);

	for (i=0; i<TCOUNT; i++) {
		count++;
	}
	pthread_mutex_unlock(&mutexes[0]);
	printf("count = %d  Thread Signaled.\n", count);
	sleep(1);

	for (i=0; i<TCOUNT; i++) {
		count++;
	}
	pthread_mutex_unlock(&mutexes[1]);
	printf("count = %d  Thread Signaled.\n", count);
	sleep(1);

	for (i=0; i<TCOUNT; i++) {
		count++;
	}
	pthread_mutex_unlock(&mutexes[2]);
	printf("count = %d  Thread Signaled.\n", count);
	sleep(1);


   /* Clean up and exit */
	for(int i = 0; i < NUM_THREADS; i++){
		pthread_mutex_destroy(&mutexes[i]);
		pthread_cond_destroy(&count_threshold_cv[i]);
		pthread_exit(NULL);
	}

} 