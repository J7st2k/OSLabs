#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

pthread_cond_t cond = PTHREAD_COND_INITIALIZER; 
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER; 

int ready = 0;

void* producer(void* arg) {
    while (1) {
        sleep(1);  
        pthread_mutex_lock(&mutex);  

        if(ready == 1){
            pthread_mutex_unlock(&mutex); 
            printf("Поставщик: событие не обработанно");
            continue;
        }
        
        ready = 1;
        printf("Поставщик: отправлено событие\n");

        pthread_cond_signal(&cond);
        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}

void* consumer(void* arg) {
    while (1) {
        pthread_mutex_lock(&mutex);

        while (ready == 0) {
            pthread_cond_wait(&cond, &mutex);
        }

        ready = 0;
        printf("Потребитель: получено событие\n\n");

        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}

int main() {
    pthread_t producer_thread, consumer_thread;

    pthread_create(&producer_thread, NULL, producer, NULL);
    pthread_create(&consumer_thread, NULL, consumer, NULL);

    pthread_join(producer_thread, NULL);
    pthread_join(consumer_thread, NULL);

    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&cond);

    return 0;
}
