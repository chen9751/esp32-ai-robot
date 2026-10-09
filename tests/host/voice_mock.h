#pragma once
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <stdio.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_STATE 1
#define ESP_ERR_NO_MEM 2
#define ESP_ERR_NOT_FOUND 3
#define ESP_ERR_TIMEOUT 4
#define ESP_ERR_INVALID_SIZE 5
#define ESP_LOGE(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
#define ESP_LOGI(...) ((void)0)
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
#define AFE_TYPE_SR 1
#define AFE_MODE_HIGH_PERF 1
#define WAKENET_DETECTED 1
#define pdTRUE 1
#define pdPASS 1
#define portMAX_DELAY UINT32_MAX
#define pdMS_TO_TICKS(x) (x)
typedef uint32_t TickType_t;
typedef struct task {
    pthread_t thread;
    pthread_mutex_t lock;
    pthread_cond_t wake;
    bool notified;
    void (*fn)(void *);
    void *arg;
} *TaskHandle_t;
static _Thread_local TaskHandle_t current_task;
static TaskHandle_t tasks[256];
static int task_count, task_calls, task_fail;
static int alloc_calls, alloc_fail;
static atomic_int live_allocs;
static atomic_bool microphone_error, generate_wake;
static void *tracked_alloc(size_t n) {
    if (++alloc_calls == alloc_fail) return NULL;
    void *p = calloc(1,n); assert(p); ++live_allocs; return p;
}
static void tracked_free(void *p) { if (p) { --live_allocs; free(p); } }
static void *heap_caps_malloc(size_t n, int caps) { (void)caps; return tracked_alloc(n); }
static TickType_t xTaskGetTickCount(void) {
    struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t);
    return (TickType_t)((uint64_t)t.tv_sec*1000 + t.tv_nsec/1000000);
}
static void vTaskDelay(TickType_t n) { usleep(n*1000); }
static unsigned uxTaskGetStackHighWaterMark(void *p) { (void)p; return 4096; }
static TaskHandle_t xTaskGetCurrentTaskHandle(void) { return current_task; }
static void vTaskDelete(TaskHandle_t h) { assert(!h); pthread_exit(NULL); }
static void *task_entry(void *p) { current_task=p; current_task->fn(current_task->arg); return NULL; }
static int xTaskCreate(void (*fn)(void *),const char *name,int stack,void *arg,int priority,TaskHandle_t *out) {
    (void)name; (void)stack; (void)priority;
    if (++task_calls==task_fail) return 0;
    TaskHandle_t t=calloc(1,sizeof(*t)); assert(t);
    pthread_mutex_init(&t->lock,NULL); pthread_cond_init(&t->wake,NULL);
    t->fn=fn;t->arg=arg; *out=t; tasks[task_count++]=t;
    assert(!pthread_create(&t->thread,NULL,task_entry,t)); return pdPASS;
}
static void xTaskNotifyGive(TaskHandle_t t) {
    pthread_mutex_lock(&t->lock); t->notified=true; pthread_cond_signal(&t->wake); pthread_mutex_unlock(&t->lock);
}
static unsigned ulTaskNotifyTake(int clear,TickType_t timeout) {
    (void)clear; (void)timeout; TaskHandle_t t=current_task;
    pthread_mutex_lock(&t->lock);
    while(!t->notified) pthread_cond_wait(&t->wake,&t->lock);
    t->notified=false; pthread_mutex_unlock(&t->lock); return 1;
}
static void reap_tasks(void) {
    for(int i=0;i<task_count;i++) {
        TaskHandle_t t=tasks[i]; pthread_join(t->thread,NULL);
        pthread_mutex_destroy(&t->lock); pthread_cond_destroy(&t->wake); free(t);
    }
    task_count=0;
}
typedef struct { int unused; } srmodel_list_t;
typedef struct { int unused; } afe_config_t;
typedef struct { atomic_int pending; } esp_afe_sr_data_t;
typedef struct { int ret_value,wakeup_state; } afe_fetch_result_t;
static bool audio_service_capture_ready(void) { return true; }
static int audio_service_capture_read(void *p,size_t n) {
    usleep(1000); if (microphone_error) return ESP_FAIL; memset(p,0,n); return ESP_OK;
}
static srmodel_list_t *esp_srmodel_init(const char *p) { (void)p; return tracked_alloc(sizeof(srmodel_list_t)); }
static void esp_srmodel_deinit(srmodel_list_t *p) { tracked_free(p); }
static afe_config_t *afe_config_init(const char *p,srmodel_list_t *m,int type,int mode) {
    (void)p;(void)m;(void)type;(void)mode;return tracked_alloc(sizeof(afe_config_t));
}
static void afe_config_free(afe_config_t *p) { tracked_free(p); }
static esp_afe_sr_data_t *create_afe(afe_config_t *c) { (void)c; return tracked_alloc(sizeof(esp_afe_sr_data_t)); }
static void destroy_afe(esp_afe_sr_data_t *p) { tracked_free(p); }
static int get_samples(esp_afe_sr_data_t *p) { (void)p; return 512; }
static int get_channels(esp_afe_sr_data_t *p) { (void)p; return 1; }
static int feed_afe(esp_afe_sr_data_t *p,const int16_t *pcm) {
    (void)pcm;
    /* One-slot ring deliberately blocks the producer until fetch drains it. */
    while (atomic_load(&p->pending)) usleep(1000);
    atomic_store(&p->pending,1); return 512;
}
static afe_fetch_result_t *fetch_afe(esp_afe_sr_data_t *p,TickType_t timeout) {
    static _Thread_local afe_fetch_result_t r;
    TickType_t start=xTaskGetTickCount();
    while(!atomic_exchange(&p->pending,0)) {
        if(xTaskGetTickCount()-start>=timeout) return NULL;
        usleep(1000);
    }
    r.ret_value=ESP_OK; r.wakeup_state=atomic_exchange(&generate_wake,false)?WAKENET_DETECTED:0;
    return &r;
}
typedef struct {
    esp_afe_sr_data_t *(*create_from_config)(afe_config_t *);
    void (*destroy)(esp_afe_sr_data_t *);
    int (*get_feed_chunksize)(esp_afe_sr_data_t *);
    int (*get_feed_channel_num)(esp_afe_sr_data_t *);
    int (*feed)(esp_afe_sr_data_t *,const int16_t *);
    afe_fetch_result_t *(*fetch_with_delay)(esp_afe_sr_data_t *,TickType_t);
} esp_afe_sr_iface_t;
static const esp_afe_sr_iface_t iface={create_afe,destroy_afe,get_samples,get_channels,feed_afe,fetch_afe};
static const esp_afe_sr_iface_t *esp_afe_handle_from_config(afe_config_t *cfg) { (void)cfg; return &iface; }
