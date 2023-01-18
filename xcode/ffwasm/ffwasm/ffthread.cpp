//
//  ffthread.c
//  ffwasm
//
//  Created by Jason on 2023/2/1.
//

#include "ffthread.h"
#include <pthread.h>
#include <unistd.h>

static pthread_mutex_t lock;
static pthread_cond_t condition;
static pthread_t decode_thread;

static void *decode_event_thread(void *ctx) {
    while (1) {
        float ret = pthread_mutex_lock(&(lock));
        if (ret == 0) {
            printf("pthread_cond_lock\n");
        }
        printf("pthread_cond_wait\n");
        ret = pthread_cond_wait(&condition, &lock);
        for (int i = 0; i < 10000; i++) {
            printf("%d\t", i);
        }
        pthread_mutex_unlock(&(lock));
    }
    return NULL;
}

void init() {
    pthread_mutex_init(&lock, NULL);
    pthread_cond_init(&condition, NULL);
    int ret = pthread_create(&decode_thread, NULL, decode_event_thread, NULL);
    if (ret != 0) {
        printf("pthread_create error %d\n", ret);
    }

    sleep(1);
    for (int i = 0; i < 10; i++)
    {
        usleep(50);

        float ret = pthread_mutex_lock(&lock);
        if (ret == 0) {
            printf("pthread_mutex_lock1111\n");
        }
        printf("1111 = %d\n", i);
        pthread_cond_signal(&condition);
        pthread_mutex_unlock(&lock);

    }
    
    pthread_join(decode_thread, NULL);
    
}
//    pthread_mutexattr_t mutexAttr;
//    pthread_mutexattr_init(&mutexAttr);
//    pthread_mutexattr_settype(&mutexAttr, PTHREAD_MUTEX_ERRORCHECK); //同线程可多次进入
//
