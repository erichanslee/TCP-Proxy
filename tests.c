 #include <pthread.h>
 #include <stdio.h>
 #define NUM_THREADS     5

 void *BusyWork(void *threadid)
 {
	while(1){
		if(*(long*)threadid == 1){
			printf("Success!\n");
			break;
		}
	}
 }

 int main (int argc, char *argv[])
 {
    pthread_t threads[NUM_THREADS];
    int rc, i, t;
	long taskids[NUM_THREADS];

    for(i = 0; i < NUM_THREADS; i ++){
    	taskids[i] = 0;
    }


	for(t=0; t<NUM_THREADS; t++)
	{
	   printf("Creating thread %ld\n", t);
	   rc = pthread_create(&threads[t], NULL, BusyWork, (void *) &taskids[t]);
	}



    /* Last thing that main() should do */
    pthread_exit(NULL);
 }