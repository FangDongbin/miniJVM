

#ifndef _GARBAGE_H
#define _GARBAGE_H

#include "../utils/hashtable.h"
#include "../utils/hashset.h"
#include "../utils/linkedlist.h"
#include "jvm.h"
#include "jvm_util.h"
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

//回收线程
extern s64 GARBAGE_PERIOD_MS;//
extern Collector *collector;


//每个线程一个回收站，线程多了就是灾难
struct _Collector {
    //
    Hashset *objs; //key=mem_ptr, value=我被别人引用的列表
    Hashset *objs_holder; //法外之地，防回收的持有器，放入其中的对象及其引用的其他对象不会被回收
    //
    pthread_t _garbage_thread;//垃圾回收线程
    ThreadLock garbagelock;

    //
    LinkedList *operation_cache;
    ArrayList *runtime_refer_copy;
    //
    s64 _garbage_count;
    u8 _garbage_thread_status;
    u8 flag_refer;
};

enum {
    GARBAGE_THREAD_NORMAL,
    GARBAGE_THREAD_PAUSE,
    GARBAGE_THREAD_STOP,
    GARBAGE_THREAD_DEAD,
};

typedef struct _GarbageOp {
    c8 op_type;
    __refer val;
} GarbageOp;

enum {
    GARBAGE_OP_REG,
    GARBAGE_OP_HOLD,
    GARBAGE_OP_RELEASE,
};

void *collect_thread_run(void *para);

void garbage_thread_lock(void);

void garbage_thread_unlock(void);

void garbage_thread_stop(void);

void garbage_thread_pause(void);

void garbage_thread_resume(void);

void garbage_thread_wait(void);

void garbage_thread_timedwait(s64 ms);

void garbage_thread_notify(void);

//其他函数

s32 garbage_collector_create(void);

void garbage_collector_destory(void);

s64 garbage_collect(void);

s32 garbage_is_alive(__refer sonPtr);

void garbage_refer_hold(__refer ref);

void garbage_refer_release(__refer ref);

s32 garbage_refer_reg(__refer ref);

#ifdef __cplusplus
}
#endif

#endif //_GARBAGE_H
