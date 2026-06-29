#include "aixos/posix.h"
#include "aixos/arch/arch.h"
#include "kernel/heap_internal.h"
#include "kernel/sched.h"
#include "config/aixos_cfg.h"
#include "config/string.h"

typedef struct posix_thread {
    struct posix_thread *next;
    aixos_pthread_t thread;
    aixos_handle_t completion;
    void *(*start_routine)(void *);
    void *argument;
    void *result;
    uint8_t detached;
    uint8_t completed;
    uint8_t joining;
} posix_thread_t;

typedef struct {
    uint8_t used;
    uint32_t generation;
    void (*destructor)(void *);
} posix_key_t;

typedef struct {
    aixos_handle_t native;
    uint32_t generation;
    uint32_t next_deadline;
    uint32_t interval_ticks;
    int overrun;
    aixos_sigevent_t event;
    uint8_t used;
    uint8_t active;
} posix_timer_t;

static posix_thread_t *threads;
static posix_key_t keys[AIXOS_CFG_POSIX_KEYS];
static posix_timer_t timers[AIXOS_CFG_POSIX_TIMERS];
static int fallback_errno;

int *aixos_posix_errno_location(void)
{
    return g_cur_task != NULL ? &g_cur_task->posix_errno : &fallback_errno;
}

static int map_error(int result)
{
    switch (result) {
        case AIXOS_OK: return 0;
        case AIXOS_ERR_TIMEOUT: return AIXOS_ETIMEDOUT;
        case AIXOS_ERR_BUSY: return AIXOS_EBUSY;
        case AIXOS_ERR_NOMEM: return AIXOS_ENOMEM;
        case AIXOS_ERR_AGAIN: return AIXOS_EAGAIN;
        case AIXOS_ERR_CONTEXT:
        case AIXOS_ERR_INVAL:
        case AIXOS_ERR_LOCKED:
        case AIXOS_ERR_OVERFLOW:
        case AIXOS_ERR_CORRUPT:
        default: return AIXOS_EINVAL;
    }
}

static posix_thread_t *thread_find(aixos_pthread_t thread)
{
    posix_thread_t *current;
    for (current = threads; current != NULL; current = current->next) {
        if (current->thread == thread) {
            return current;
        }
    }
    return NULL;
}

static void thread_remove(posix_thread_t *thread)
{
    posix_thread_t **link = &threads;
    while (*link != NULL) {
        if (*link == thread) {
            *link = thread->next;
            return;
        }
        link = &(*link)->next;
    }
}

static void run_key_destructors(void)
{
    aixos_tcb_t *tcb = g_cur_task;
    uint32_t iteration;
    if (tcb == NULL) {
        return;
    }
    for (iteration = 0U; iteration < 4U; iteration++) {
        uint32_t key;
        int called = 0;
        for (key = 0U; key < AIXOS_CFG_POSIX_KEYS; key++) {
            void *value = tcb->posix_values[key];
            if (keys[key].used != 0U &&
                tcb->posix_key_generations[key] == keys[key].generation &&
                keys[key].destructor != NULL &&
                value != NULL) {
                tcb->posix_values[key] = NULL;
                keys[key].destructor(value);
                called = 1;
            }
        }
        if (called == 0) {
            break;
        }
    }
}

static void thread_complete(posix_thread_t *thread, void *result)
{
    aixos_arch_flags_t flags;
    int detached;
    run_key_destructors();
    flags = aixos_arch_int_disable();
    if (thread->completed == 0U) {
        thread->result = result;
        thread->completed = 1U;
    }
    detached = thread->detached != 0U;
    aixos_arch_int_restore(flags);
    (void)aixos_sem_post(thread->completion);
    if (detached) {
        flags = aixos_arch_int_disable();
        thread_remove(thread);
        aixos_arch_int_restore(flags);
        (void)aixos_sem_delete(thread->completion);
        aixos_free(thread);
    }
}

static void thread_entry(void *argument)
{
    posix_thread_t *thread = (posix_thread_t *)argument;
    void *result = thread->start_routine(thread->argument);
    thread_complete(thread, result);
}

int aixos_pthread_attr_init(aixos_pthread_attr_t *attr)
{
    if (attr == NULL) return AIXOS_EINVAL;
    attr->stack_size = AIXOS_CFG_DEFAULT_STACK_SIZE;
    attr->priority = 1;
    attr->detach_state = AIXOS_PTHREAD_CREATE_JOINABLE;
    attr->sched_policy = 0;
    attr->inheritsched = AIXOS_PTHREAD_INHERIT_SCHED;
    return 0;
}

int aixos_pthread_attr_destroy(aixos_pthread_attr_t *attr)
{
    if (attr == NULL) return AIXOS_EINVAL;
    memset(attr, 0, sizeof(*attr));
    return 0;
}

int aixos_pthread_attr_setstacksize(aixos_pthread_attr_t *attr, size_t size)
{
    if (attr == NULL || size < AIXOS_CFG_MIN_TASK_STACK_SIZE) {
        return AIXOS_EINVAL;
    }
    attr->stack_size = size;
    return 0;
}

int aixos_pthread_attr_getstacksize(const aixos_pthread_attr_t *attr,
                                    size_t *size)
{
    if (attr == NULL || size == NULL) return AIXOS_EINVAL;
    *size = attr->stack_size;
    return 0;
}

int aixos_pthread_attr_setdetachstate(aixos_pthread_attr_t *attr, int state)
{
    if (attr == NULL || (state != AIXOS_PTHREAD_CREATE_JOINABLE &&
                         state != AIXOS_PTHREAD_CREATE_DETACHED)) {
        return AIXOS_EINVAL;
    }
    attr->detach_state = state;
    return 0;
}

int aixos_pthread_attr_getdetachstate(const aixos_pthread_attr_t *attr,
                                      int *state)
{
    if (attr == NULL || state == NULL) return AIXOS_EINVAL;
    *state = attr->detach_state;
    return 0;
}

int aixos_pthread_attr_setschedprio(aixos_pthread_attr_t *attr, int priority)
{
    if (attr == NULL || priority < 0 ||
        priority >= AIXOS_CFG_MAX_PRIORITY) {
        return AIXOS_EINVAL;
    }
    attr->priority = priority;
    return 0;
}

int aixos_pthread_attr_getschedprio(const aixos_pthread_attr_t *attr,
                                    int *priority)
{
    if (attr == NULL || priority == NULL) return AIXOS_EINVAL;
    *priority = attr->priority;
    return 0;
}

int aixos_pthread_attr_setschedpolicy(aixos_pthread_attr_t *attr, int policy)
{
    if (attr == NULL) return AIXOS_EINVAL;
    attr->sched_policy = policy;
    return 0;
}

int aixos_pthread_attr_getschedpolicy(const aixos_pthread_attr_t *attr,
                                      int *policy)
{
    if (attr == NULL || policy == NULL) return AIXOS_EINVAL;
    *policy = attr->sched_policy;
    return 0;
}

int aixos_pthread_attr_setinheritsched(aixos_pthread_attr_t *attr,
                                       int inheritsched)
{
    if (attr == NULL) return AIXOS_EINVAL;
    if (inheritsched != AIXOS_PTHREAD_INHERIT_SCHED &&
        inheritsched != AIXOS_PTHREAD_EXPLICIT_SCHED) {
        return AIXOS_EINVAL;
    }
    attr->inheritsched = inheritsched;
    return 0;
}

int aixos_pthread_attr_getinheritsched(const aixos_pthread_attr_t *attr,
                                       int *inheritsched)
{
    if (attr == NULL || inheritsched == NULL) return AIXOS_EINVAL;
    *inheritsched = attr->inheritsched;
    return 0;
}

int aixos_pthread_create(aixos_pthread_t *thread_out,
                         const aixos_pthread_attr_t *attr,
                         void *(*start_routine)(void *), void *arg)
{
    aixos_pthread_attr_t defaults;
    posix_thread_t *thread;
    aixos_arch_flags_t flags;
    if (thread_out == NULL || start_routine == NULL) {
        return AIXOS_EINVAL;
    }
    if (attr == NULL) {
        (void)aixos_pthread_attr_init(&defaults);
        attr = &defaults;
    }
    if (attr->stack_size < AIXOS_CFG_MIN_TASK_STACK_SIZE ||
        attr->priority < 0 || attr->priority >= AIXOS_CFG_MAX_PRIORITY ||
        (attr->detach_state != AIXOS_PTHREAD_CREATE_JOINABLE &&
         attr->detach_state != AIXOS_PTHREAD_CREATE_DETACHED)) {
        return AIXOS_EINVAL;
    }
    thread = (posix_thread_t *)aixos_kernel_malloc(sizeof(*thread));
    if (thread == NULL) return AIXOS_EAGAIN;
    memset(thread, 0, sizeof(*thread));
    thread->completion = aixos_sem_create(0);
    if (thread->completion == AIXOS_HANDLE_INVALID) {
        aixos_free(thread);
        return AIXOS_EAGAIN;
    }
    thread->start_routine = start_routine;
    thread->argument = arg;
    thread->detached =
        (uint8_t)(attr->detach_state == AIXOS_PTHREAD_CREATE_DETACHED);
    thread->thread = aixos_task_create("pthread", thread_entry, thread,
                                       attr->stack_size, attr->priority);
    if (thread->thread == AIXOS_HANDLE_INVALID) {
        (void)aixos_sem_delete(thread->completion);
        aixos_free(thread);
        return AIXOS_EAGAIN;
    }
    flags = aixos_arch_int_disable();
    thread->next = threads;
    threads = thread;
    aixos_arch_int_restore(flags);
    *thread_out = thread->thread;
    return 0;
}

int aixos_pthread_join(aixos_pthread_t thread_id, void **value)
{
    posix_thread_t *thread;
    aixos_arch_flags_t flags;
    int result;
    if (thread_id == aixos_task_self()) return AIXOS_EINVAL;
    flags = aixos_arch_int_disable();
    thread = thread_find(thread_id);
    if (thread == NULL || thread->detached != 0U ||
        thread->joining != 0U) {
        aixos_arch_int_restore(flags);
        return thread == NULL ? AIXOS_ESRCH : AIXOS_EINVAL;
    }
    thread->joining = 1U;
    aixos_arch_int_restore(flags);
    result = aixos_sem_wait(thread->completion, UINT32_MAX);
    if (result != AIXOS_OK) {
        flags = aixos_arch_int_disable();
        thread->joining = 0U;
        aixos_arch_int_restore(flags);
        return map_error(result);
    }
    if (value != NULL) *value = thread->result;
    flags = aixos_arch_int_disable();
    thread_remove(thread);
    aixos_arch_int_restore(flags);
    (void)aixos_sem_delete(thread->completion);
    aixos_free(thread);
    return 0;
}

int aixos_pthread_detach(aixos_pthread_t thread_id)
{
    posix_thread_t *thread;
    aixos_arch_flags_t flags = aixos_arch_int_disable();
    thread = thread_find(thread_id);
    if (thread == NULL) {
        aixos_arch_int_restore(flags);
        return AIXOS_ESRCH;
    }
    if (thread->detached != 0U || thread->joining != 0U) {
        aixos_arch_int_restore(flags);
        return AIXOS_EINVAL;
    }
    thread->detached = 1U;
    if (thread->completed != 0U) {
        thread_remove(thread);
        aixos_arch_int_restore(flags);
        (void)aixos_sem_delete(thread->completion);
        aixos_free(thread);
        return 0;
    }
    aixos_arch_int_restore(flags);
    return 0;
}

aixos_pthread_t aixos_pthread_self(void)
{
    return aixos_task_self();
}

int aixos_pthread_equal(aixos_pthread_t left, aixos_pthread_t right)
{
    return left == right;
}

void aixos_pthread_exit(void *value)
{
    posix_thread_t *thread;
    aixos_arch_flags_t flags = aixos_arch_int_disable();
    thread = thread_find(aixos_task_self());
    aixos_arch_int_restore(flags);
    if (thread != NULL) {
        thread_complete(thread, value);
    }
    (void)aixos_task_delete(aixos_task_self());
#ifdef AIXOS_HOST_TEST
    return;
#else
    for (;;) {}
#endif
}

int aixos_pthread_getschedprio(aixos_pthread_t thread, int *priority)
{
    aixos_task_info_t info;
    if (priority == NULL) return AIXOS_EINVAL;
    if (aixos_task_get_info(thread, &info) != AIXOS_OK) {
        return AIXOS_ESRCH;
    }
    *priority = info.priority;
    return 0;
}

int aixos_pthread_setschedprio(aixos_pthread_t thread, int priority)
{
    return map_error(aixos_task_set_priority(thread, priority));
}

int aixos_sched_yield(void)
{
    return map_error(aixos_task_yield());
}

int aixos_sched_get_priority_min(int policy)
{
    (void)policy;
    return 0;
}

int aixos_sched_get_priority_max(int policy)
{
    (void)policy;
    return AIXOS_CFG_MAX_PRIORITY - 1;
}

int aixos_pthread_once(aixos_pthread_once_t *once_control,
                       void (*init_routine)(void))
{
    if (once_control == NULL || init_routine == NULL) {
        return AIXOS_EINVAL;
    }
    for (;;) {
        aixos_arch_flags_t flags = aixos_arch_int_disable();
        if (once_control->state == 2U) {
            aixos_arch_int_restore(flags);
            return 0;
        }
        if (once_control->state == 0U) {
            once_control->state = 1U;
            aixos_arch_int_restore(flags);
            init_routine();
            flags = aixos_arch_int_disable();
            once_control->state = 2U;
            aixos_arch_int_restore(flags);
            return 0;
        }
        aixos_arch_int_restore(flags);
        (void)aixos_task_yield();
    }
}

int aixos_pthread_key_create(aixos_pthread_key_t *key,
                             void (*destructor)(void *))
{
    uint32_t index;
    aixos_arch_flags_t flags;
    if (key == NULL) return AIXOS_EINVAL;
    flags = aixos_arch_int_disable();
    for (index = 0U; index < AIXOS_CFG_POSIX_KEYS; index++) {
        if (keys[index].used == 0U) {
            keys[index].used = 1U;
            keys[index].generation++;
            if (keys[index].generation == 0U) {
                keys[index].generation = 1U;
            }
            keys[index].destructor = destructor;
            *key = index;
            aixos_arch_int_restore(flags);
            return 0;
        }
    }
    aixos_arch_int_restore(flags);
    return AIXOS_EAGAIN;
}

int aixos_pthread_key_delete(aixos_pthread_key_t key)
{
    aixos_arch_flags_t flags;
    if (key >= AIXOS_CFG_POSIX_KEYS) return AIXOS_EINVAL;
    flags = aixos_arch_int_disable();
    if (keys[key].used == 0U) {
        aixos_arch_int_restore(flags);
        return AIXOS_EINVAL;
    }
    keys[key].used = 0U;
    keys[key].destructor = NULL;
    aixos_arch_int_restore(flags);
    return 0;
}

void *aixos_pthread_getspecific(aixos_pthread_key_t key)
{
    if (g_cur_task == NULL || key >= AIXOS_CFG_POSIX_KEYS ||
        keys[key].used == 0U ||
        g_cur_task->posix_key_generations[key] != keys[key].generation) {
        return NULL;
    }
    return g_cur_task->posix_values[key];
}

int aixos_pthread_setspecific(aixos_pthread_key_t key, const void *value)
{
    if (g_cur_task == NULL || key >= AIXOS_CFG_POSIX_KEYS ||
        keys[key].used == 0U) {
        return AIXOS_EINVAL;
    }
    g_cur_task->posix_values[key] = (void *)value;
    g_cur_task->posix_key_generations[key] = keys[key].generation;
    return 0;
}

int aixos_pthread_mutexattr_init(aixos_pthread_mutexattr_t *attr)
{
    if (attr == NULL) return AIXOS_EINVAL;
    attr->type = AIXOS_PTHREAD_MUTEX_RECURSIVE;
    attr->protocol = AIXOS_PTHREAD_PRIO_INHERIT;
    attr->prio_ceiling = 0;
    return 0;
}

int aixos_pthread_mutexattr_destroy(aixos_pthread_mutexattr_t *attr)
{
    if (attr == NULL) return AIXOS_EINVAL;
    memset(attr, 0, sizeof(*attr));
    return 0;
}

int aixos_pthread_mutexattr_settype(aixos_pthread_mutexattr_t *attr, int type)
{
    if (attr == NULL) return AIXOS_EINVAL;
    if (type != AIXOS_PTHREAD_MUTEX_NORMAL &&
        type != AIXOS_PTHREAD_MUTEX_RECURSIVE &&
        type != AIXOS_PTHREAD_MUTEX_ERRORCHECK) {
        return AIXOS_EINVAL;
    }
    attr->type = type;
    return 0;
}

int aixos_pthread_mutexattr_gettype(const aixos_pthread_mutexattr_t *attr,
                                    int *type)
{
    if (attr == NULL || type == NULL) return AIXOS_EINVAL;
    *type = attr->type;
    return 0;
}

int aixos_pthread_mutexattr_setprotocol(aixos_pthread_mutexattr_t *attr,
                                        int protocol)
{
    if (attr == NULL) return AIXOS_EINVAL;
    if (protocol != AIXOS_PTHREAD_PRIO_NONE &&
        protocol != AIXOS_PTHREAD_PRIO_INHERIT &&
        protocol != AIXOS_PTHREAD_PRIO_PROTECT) {
        return AIXOS_EINVAL;
    }
    attr->protocol = protocol;
    return 0;
}

int aixos_pthread_mutexattr_getprotocol(
    const aixos_pthread_mutexattr_t *attr, int *protocol)
{
    if (attr == NULL || protocol == NULL) return AIXOS_EINVAL;
    *protocol = attr->protocol;
    return 0;
}

int aixos_pthread_mutexattr_setprioceiling(
    aixos_pthread_mutexattr_t *attr, int prioceiling)
{
    if (attr == NULL) return AIXOS_EINVAL;
    if (prioceiling < 0 || prioceiling >= AIXOS_CFG_MAX_PRIORITY) {
        return AIXOS_EINVAL;
    }
    attr->prio_ceiling = prioceiling;
    return 0;
}

int aixos_pthread_mutexattr_getprioceiling(
    const aixos_pthread_mutexattr_t *attr, int *prioceiling)
{
    if (attr == NULL || prioceiling == NULL) return AIXOS_EINVAL;
    *prioceiling = attr->prio_ceiling;
    return 0;
}

int aixos_pthread_mutex_init(aixos_pthread_mutex_t *mutex,
                             const aixos_pthread_mutexattr_t *attr)
{
    if (mutex == NULL) return AIXOS_EINVAL;
    mutex->native = aixos_mutex_create();
    if (mutex->native == AIXOS_HANDLE_INVALID) return AIXOS_EAGAIN;
    mutex->type = (attr != NULL) ? (uint8_t)attr->type :
                                   AIXOS_PTHREAD_MUTEX_NORMAL;
    mutex->owner = AIXOS_HANDLE_INVALID;
    mutex->lock_count = 0U;
    mutex->protocol = (attr != NULL) ? (uint8_t)attr->protocol :
                                       AIXOS_PTHREAD_PRIO_NONE;
    mutex->prio_ceiling = (attr != NULL) ? attr->prio_ceiling : 0;
    return 0;
}

int aixos_pthread_mutex_destroy(aixos_pthread_mutex_t *mutex)
{
    if (mutex == NULL) return AIXOS_EINVAL;
    return map_error(aixos_mutex_delete(mutex->native));
}

int aixos_pthread_mutex_lock(aixos_pthread_mutex_t *mutex)
{
    aixos_pthread_t self;
    int result;
    if (mutex == NULL) return AIXOS_EINVAL;
    self = aixos_pthread_self();
    if (mutex->type == AIXOS_PTHREAD_MUTEX_ERRORCHECK) {
        if (mutex->owner == self) return AIXOS_EDEADLK;
    } else if (mutex->type == AIXOS_PTHREAD_MUTEX_RECURSIVE) {
        if (mutex->owner == self) {
            mutex->lock_count++;
            return 0;
        }
    }
    result = aixos_mutex_lock(mutex->native, UINT32_MAX);
    if (result == AIXOS_OK) {
        mutex->owner = self;
        mutex->lock_count = 1U;
    }
    return map_error(result);
}

int aixos_pthread_mutex_trylock(aixos_pthread_mutex_t *mutex)
{
    aixos_pthread_t self;
    int result;
    if (mutex == NULL) return AIXOS_EINVAL;
    self = aixos_pthread_self();
    if (mutex->type == AIXOS_PTHREAD_MUTEX_ERRORCHECK) {
        if (mutex->owner == self) return AIXOS_EDEADLK;
    } else if (mutex->type == AIXOS_PTHREAD_MUTEX_RECURSIVE) {
        if (mutex->owner == self) {
            mutex->lock_count++;
            return 0;
        }
    }
    result = aixos_mutex_lock(mutex->native, 0U);
    if (result == AIXOS_OK) {
        mutex->owner = self;
        mutex->lock_count = 1U;
    }
    return map_error(result);
}

static int relative_timeout(const aixos_timespec_t *absolute,
                            uint32_t *timeout_ms)
{
    aixos_timespec_t now;
    int64_t seconds;
    int64_t nanoseconds;
    uint64_t milliseconds;
    if (absolute == NULL || timeout_ms == NULL ||
        absolute->tv_sec < 0 || absolute->tv_nsec < 0 ||
        absolute->tv_nsec >= 1000000000) {
        return AIXOS_EINVAL;
    }
    (void)aixos_clock_gettime(AIXOS_CLOCK_MONOTONIC, &now);
    seconds = absolute->tv_sec - now.tv_sec;
    nanoseconds = (int64_t)absolute->tv_nsec - now.tv_nsec;
    if (nanoseconds < 0) {
        seconds--;
        nanoseconds += 1000000000;
    }
    if (seconds < 0) {
        *timeout_ms = 0U;
        return 0;
    }
    milliseconds = (uint64_t)seconds * 1000U +
                   ((uint64_t)nanoseconds + 999999U) / 1000000U;
    *timeout_ms = milliseconds >= UINT32_MAX ?
                  UINT32_MAX - 1U : (uint32_t)milliseconds;
    return 0;
}

int aixos_pthread_mutex_timedlock(aixos_pthread_mutex_t *mutex,
                                  const aixos_timespec_t *absolute)
{
    uint32_t timeout;
    int result;
    aixos_pthread_t self;
    if (mutex == NULL) return AIXOS_EINVAL;
    result = relative_timeout(absolute, &timeout);
    if (result != 0) return result;
    self = aixos_pthread_self();
    if (mutex->type == AIXOS_PTHREAD_MUTEX_ERRORCHECK) {
        if (mutex->owner == self) return AIXOS_EDEADLK;
    } else if (mutex->type == AIXOS_PTHREAD_MUTEX_RECURSIVE) {
        if (mutex->owner == self) {
            mutex->lock_count++;
            return 0;
        }
    }
    result = aixos_mutex_lock(mutex->native, timeout);
    if (result == AIXOS_OK) {
        mutex->owner = self;
        mutex->lock_count = 1U;
    }
    return map_error(result);
}

int aixos_pthread_mutex_unlock(aixos_pthread_mutex_t *mutex)
{
    if (mutex == NULL) return AIXOS_EINVAL;
    if (mutex->type == AIXOS_PTHREAD_MUTEX_ERRORCHECK ||
        mutex->type == AIXOS_PTHREAD_MUTEX_RECURSIVE) {
        if (mutex->owner != aixos_pthread_self()) return AIXOS_EPERM;
    }
    if (mutex->type == AIXOS_PTHREAD_MUTEX_RECURSIVE &&
        mutex->lock_count > 1U) {
        mutex->lock_count--;
        return 0;
    }
    mutex->owner = AIXOS_HANDLE_INVALID;
    mutex->lock_count = 0U;
    return map_error(aixos_mutex_unlock(mutex->native));
}

int aixos_pthread_cond_init(aixos_pthread_cond_t *condition,
                            const aixos_pthread_condattr_t *attr)
{
    if (condition == NULL ||
        (attr != NULL && attr->clock_id != AIXOS_CLOCK_MONOTONIC)) {
        return AIXOS_EINVAL;
    }
    condition->semaphore = aixos_sem_create(0);
    condition->waiters = 0U;
    return condition->semaphore == AIXOS_HANDLE_INVALID ?
           AIXOS_EAGAIN : 0;
}

int aixos_pthread_condattr_init(aixos_pthread_condattr_t *attr)
{
    if (attr == NULL) return AIXOS_EINVAL;
    attr->clock_id = AIXOS_CLOCK_MONOTONIC;
    return 0;
}

int aixos_pthread_condattr_destroy(aixos_pthread_condattr_t *attr)
{
    if (attr == NULL) return AIXOS_EINVAL;
    attr->clock_id = -1;
    return 0;
}

int aixos_pthread_condattr_setclock(aixos_pthread_condattr_t *attr,
                                    int clock_id)
{
    if (attr == NULL) return AIXOS_EINVAL;
    if (clock_id != AIXOS_CLOCK_MONOTONIC) return AIXOS_ENOTSUP;
    attr->clock_id = clock_id;
    return 0;
}

int aixos_pthread_condattr_getclock(const aixos_pthread_condattr_t *attr,
                                    int *clock_id)
{
    if (attr == NULL || clock_id == NULL) return AIXOS_EINVAL;
    *clock_id = attr->clock_id;
    return 0;
}

int aixos_pthread_cond_destroy(aixos_pthread_cond_t *condition)
{
    if (condition == NULL || condition->waiters != 0U) {
        return condition == NULL ? AIXOS_EINVAL : AIXOS_EBUSY;
    }
    return map_error(aixos_sem_delete(condition->semaphore));
}

static int cond_wait_common(aixos_pthread_cond_t *condition,
                            aixos_pthread_mutex_t *mutex,
                            uint32_t timeout)
{
    aixos_arch_flags_t flags;
    int result;
    if (condition == NULL || mutex == NULL) return AIXOS_EINVAL;
    flags = aixos_arch_int_disable();
    condition->waiters++;
    aixos_arch_int_restore(flags);
    result = aixos_pthread_mutex_unlock(mutex);
    if (result != 0) {
        flags = aixos_arch_int_disable();
        condition->waiters--;
        aixos_arch_int_restore(flags);
        return result;
    }
    result = aixos_sem_wait(condition->semaphore, timeout);
    if (result != AIXOS_OK) {
        flags = aixos_arch_int_disable();
        if (condition->waiters != 0U) condition->waiters--;
        aixos_arch_int_restore(flags);
    }
    {
        int lock_result = aixos_pthread_mutex_lock(mutex);
        if (lock_result != 0) return lock_result;
    }
    return map_error(result);
}

int aixos_pthread_cond_wait(aixos_pthread_cond_t *condition,
                            aixos_pthread_mutex_t *mutex)
{
    return cond_wait_common(condition, mutex, UINT32_MAX);
}

int aixos_pthread_cond_timedwait(aixos_pthread_cond_t *condition,
                                 aixos_pthread_mutex_t *mutex,
                                 const aixos_timespec_t *absolute)
{
    uint32_t timeout;
    int result = relative_timeout(absolute, &timeout);
    return result != 0 ? result :
           cond_wait_common(condition, mutex, timeout);
}

int aixos_pthread_cond_signal(aixos_pthread_cond_t *condition)
{
    aixos_arch_flags_t flags;
    int result = AIXOS_OK;
    if (condition == NULL) return AIXOS_EINVAL;
    flags = aixos_arch_int_disable();
    if (condition->waiters != 0U) {
        condition->waiters--;
        result = aixos_sem_post(condition->semaphore);
    }
    aixos_arch_int_restore(flags);
    return map_error(result);
}

int aixos_pthread_cond_broadcast(aixos_pthread_cond_t *condition)
{
    uint32_t count;
    int result = AIXOS_OK;
    aixos_arch_flags_t flags;
    if (condition == NULL) return AIXOS_EINVAL;
    flags = aixos_arch_int_disable();
    count = condition->waiters;
    condition->waiters = 0U;
    while (count-- != 0U) {
        result = aixos_sem_post(condition->semaphore);
        if (result != AIXOS_OK) break;
    }
    aixos_arch_int_restore(flags);
    return map_error(result);
}

int aixos_pthread_rwlockattr_init(aixos_pthread_rwlockattr_t *attr)
{
    if (attr == NULL) return AIXOS_EINVAL;
    attr->process_shared = AIXOS_PTHREAD_PROCESS_PRIVATE;
    return 0;
}

int aixos_pthread_rwlockattr_destroy(aixos_pthread_rwlockattr_t *attr)
{
    if (attr == NULL) return AIXOS_EINVAL;
    attr->process_shared = -1;
    return 0;
}

int aixos_pthread_rwlock_init(aixos_pthread_rwlock_t *lock,
                              const aixos_pthread_rwlockattr_t *attr)
{
    int result;
    if (lock == NULL ||
        (attr != NULL &&
         attr->process_shared != AIXOS_PTHREAD_PROCESS_PRIVATE)) {
        return attr != NULL &&
               attr->process_shared == AIXOS_PTHREAD_PROCESS_SHARED ?
               AIXOS_ENOTSUP : AIXOS_EINVAL;
    }
    memset(lock, 0, sizeof(*lock));
    lock->writer = AIXOS_HANDLE_INVALID;
    {
        uint32_t index;
        for (index = 0U; index < AIXOS_CFG_POSIX_RWLOCK_READERS; index++) {
            lock->reader_threads[index] = AIXOS_HANDLE_INVALID;
        }
    }
    result = aixos_pthread_mutex_init(&lock->mutex, NULL);
    if (result != 0) return result;
    result = aixos_pthread_cond_init(&lock->readers_condition, NULL);
    if (result != 0) {
        (void)aixos_pthread_mutex_destroy(&lock->mutex);
        return result;
    }
    result = aixos_pthread_cond_init(&lock->writers_condition, NULL);
    if (result != 0) {
        (void)aixos_pthread_cond_destroy(&lock->readers_condition);
        (void)aixos_pthread_mutex_destroy(&lock->mutex);
    }
    return result;
}

int aixos_pthread_rwlock_destroy(aixos_pthread_rwlock_t *lock)
{
    int result;
    if (lock == NULL) return AIXOS_EINVAL;
    result = aixos_pthread_mutex_trylock(&lock->mutex);
    if (result != 0) return AIXOS_EBUSY;
    if (lock->readers != 0U || lock->waiting_writers != 0U ||
        lock->writer != AIXOS_HANDLE_INVALID) {
        (void)aixos_pthread_mutex_unlock(&lock->mutex);
        return AIXOS_EBUSY;
    }
    (void)aixos_pthread_mutex_unlock(&lock->mutex);
    result = aixos_pthread_cond_destroy(&lock->writers_condition);
    if (result != 0) return result;
    result = aixos_pthread_cond_destroy(&lock->readers_condition);
    if (result != 0) return result;
    return aixos_pthread_mutex_destroy(&lock->mutex);
}

static int rwlock_reader_slot(aixos_pthread_rwlock_t *lock,
                              aixos_pthread_t thread, int allocate)
{
    uint32_t index;
    int free_slot = -1;
    for (index = 0U; index < AIXOS_CFG_POSIX_RWLOCK_READERS; index++) {
        if (lock->reader_threads[index] == thread) {
            return (int)index;
        }
        if (free_slot < 0 &&
            lock->reader_threads[index] == AIXOS_HANDLE_INVALID) {
            free_slot = (int)index;
        }
    }
    return allocate != 0 ? free_slot : -1;
}

static int rwlock_rdlock_common(aixos_pthread_rwlock_t *lock,
                                const aixos_timespec_t *absolute,
                                int try_only)
{
    int result;
    int reader_slot;
    aixos_pthread_t self;
    if (lock == NULL) return AIXOS_EINVAL;
    result = aixos_pthread_mutex_lock(&lock->mutex);
    if (result != 0) return result;
    self = aixos_pthread_self();
    reader_slot = rwlock_reader_slot(lock, self, 0);
    if (try_only != 0 &&
        (lock->writer != AIXOS_HANDLE_INVALID ||
         (lock->waiting_writers != 0U && reader_slot < 0))) {
        (void)aixos_pthread_mutex_unlock(&lock->mutex);
        return AIXOS_EBUSY;
    }
    while (lock->writer != AIXOS_HANDLE_INVALID ||
           (lock->waiting_writers != 0U && reader_slot < 0)) {
        result = absolute == NULL ?
                 aixos_pthread_cond_wait(&lock->readers_condition,
                                         &lock->mutex) :
                 aixos_pthread_cond_timedwait(&lock->readers_condition,
                                              &lock->mutex, absolute);
        if (result != 0) {
            (void)aixos_pthread_mutex_unlock(&lock->mutex);
            return result;
        }
    }
    if (lock->readers == UINT32_MAX) {
        (void)aixos_pthread_mutex_unlock(&lock->mutex);
        return AIXOS_EAGAIN;
    }
    reader_slot = rwlock_reader_slot(lock, self, 1);
    if (reader_slot < 0 ||
        lock->reader_counts[reader_slot] == UINT32_MAX) {
        (void)aixos_pthread_mutex_unlock(&lock->mutex);
        return AIXOS_EAGAIN;
    }
    lock->reader_threads[reader_slot] = self;
    lock->reader_counts[reader_slot]++;
    lock->readers++;
    return aixos_pthread_mutex_unlock(&lock->mutex);
}

int aixos_pthread_rwlock_rdlock(aixos_pthread_rwlock_t *lock)
{
    return rwlock_rdlock_common(lock, NULL, 0);
}

int aixos_pthread_rwlock_tryrdlock(aixos_pthread_rwlock_t *lock)
{
    return rwlock_rdlock_common(lock, NULL, 1);
}

int aixos_pthread_rwlock_timedrdlock(aixos_pthread_rwlock_t *lock,
                                     const aixos_timespec_t *absolute)
{
    if (absolute == NULL) return AIXOS_EINVAL;
    return rwlock_rdlock_common(lock, absolute, 0);
}

static int rwlock_wrlock_common(aixos_pthread_rwlock_t *lock,
                                const aixos_timespec_t *absolute,
                                int try_only)
{
    int result;
    aixos_pthread_t self;
    if (lock == NULL) return AIXOS_EINVAL;
    result = aixos_pthread_mutex_lock(&lock->mutex);
    if (result != 0) return result;
    self = aixos_pthread_self();
    if (lock->writer == self || rwlock_reader_slot(lock, self, 0) >= 0) {
        (void)aixos_pthread_mutex_unlock(&lock->mutex);
        return AIXOS_EBUSY;
    }
    if (try_only != 0 &&
        (lock->writer != AIXOS_HANDLE_INVALID || lock->readers != 0U)) {
        (void)aixos_pthread_mutex_unlock(&lock->mutex);
        return AIXOS_EBUSY;
    }
    lock->waiting_writers++;
    while (lock->writer != AIXOS_HANDLE_INVALID || lock->readers != 0U) {
        result = absolute == NULL ?
                 aixos_pthread_cond_wait(&lock->writers_condition,
                                         &lock->mutex) :
                 aixos_pthread_cond_timedwait(&lock->writers_condition,
                                              &lock->mutex, absolute);
        if (result != 0) {
            lock->waiting_writers--;
            (void)aixos_pthread_mutex_unlock(&lock->mutex);
            return result;
        }
    }
    lock->waiting_writers--;
    lock->writer = self;
    return aixos_pthread_mutex_unlock(&lock->mutex);
}

int aixos_pthread_rwlock_wrlock(aixos_pthread_rwlock_t *lock)
{
    return rwlock_wrlock_common(lock, NULL, 0);
}

int aixos_pthread_rwlock_trywrlock(aixos_pthread_rwlock_t *lock)
{
    return rwlock_wrlock_common(lock, NULL, 1);
}

int aixos_pthread_rwlock_timedwrlock(aixos_pthread_rwlock_t *lock,
                                     const aixos_timespec_t *absolute)
{
    if (absolute == NULL) return AIXOS_EINVAL;
    return rwlock_wrlock_common(lock, absolute, 0);
}

int aixos_pthread_rwlock_unlock(aixos_pthread_rwlock_t *lock)
{
    int result;
    int reader_slot;
    aixos_pthread_t self;
    if (lock == NULL) return AIXOS_EINVAL;
    result = aixos_pthread_mutex_lock(&lock->mutex);
    if (result != 0) return result;
    self = aixos_pthread_self();
    reader_slot = rwlock_reader_slot(lock, self, 0);
    if (lock->writer == self) {
        lock->writer = AIXOS_HANDLE_INVALID;
    } else if (reader_slot >= 0 &&
               lock->reader_counts[reader_slot] != 0U) {
        lock->reader_counts[reader_slot]--;
        if (lock->reader_counts[reader_slot] == 0U) {
            lock->reader_threads[reader_slot] = AIXOS_HANDLE_INVALID;
        }
        lock->readers--;
    } else {
        (void)aixos_pthread_mutex_unlock(&lock->mutex);
        return AIXOS_EPERM;
    }
    if (lock->writer == AIXOS_HANDLE_INVALID && lock->readers == 0U &&
        lock->waiting_writers != 0U) {
        result = aixos_pthread_cond_signal(&lock->writers_condition);
    } else if (lock->writer == AIXOS_HANDLE_INVALID &&
               lock->waiting_writers == 0U) {
        result = aixos_pthread_cond_broadcast(&lock->readers_condition);
    }
    {
        int unlock_result = aixos_pthread_mutex_unlock(&lock->mutex);
        return result != 0 ? result : unlock_result;
    }
}

int aixos_pthread_barrierattr_init(aixos_pthread_barrierattr_t *attr)
{
    if (attr == NULL) return AIXOS_EINVAL;
    attr->process_shared = AIXOS_PTHREAD_PROCESS_PRIVATE;
    return 0;
}

int aixos_pthread_barrierattr_destroy(aixos_pthread_barrierattr_t *attr)
{
    if (attr == NULL) return AIXOS_EINVAL;
    attr->process_shared = -1;
    return 0;
}

int aixos_pthread_barrier_init(aixos_pthread_barrier_t *barrier,
                               const aixos_pthread_barrierattr_t *attr,
                               unsigned int count)
{
    int result;
    if (barrier == NULL || count == 0U ||
        (attr != NULL &&
         attr->process_shared != AIXOS_PTHREAD_PROCESS_PRIVATE)) {
        return attr != NULL &&
               attr->process_shared == AIXOS_PTHREAD_PROCESS_SHARED ?
               AIXOS_ENOTSUP : AIXOS_EINVAL;
    }
    memset(barrier, 0, sizeof(*barrier));
    barrier->threshold = count;
    result = aixos_pthread_mutex_init(&barrier->mutex, NULL);
    if (result != 0) return result;
    result = aixos_pthread_cond_init(&barrier->condition, NULL);
    if (result != 0) {
        (void)aixos_pthread_mutex_destroy(&barrier->mutex);
    }
    return result;
}

int aixos_pthread_barrier_destroy(aixos_pthread_barrier_t *barrier)
{
    int result;
    if (barrier == NULL) return AIXOS_EINVAL;
    result = aixos_pthread_mutex_trylock(&barrier->mutex);
    if (result != 0) return AIXOS_EBUSY;
    if (barrier->count != 0U) {
        (void)aixos_pthread_mutex_unlock(&barrier->mutex);
        return AIXOS_EBUSY;
    }
    (void)aixos_pthread_mutex_unlock(&barrier->mutex);
    result = aixos_pthread_cond_destroy(&barrier->condition);
    if (result != 0) return result;
    return aixos_pthread_mutex_destroy(&barrier->mutex);
}

int aixos_pthread_barrier_wait(aixos_pthread_barrier_t *barrier)
{
    uint32_t generation;
    int result;
    if (barrier == NULL || barrier->threshold == 0U) {
        return AIXOS_EINVAL;
    }
    result = aixos_pthread_mutex_lock(&barrier->mutex);
    if (result != 0) return result;
    generation = barrier->generation;
    barrier->count++;
    if (barrier->count == barrier->threshold) {
        barrier->count = 0U;
        barrier->generation++;
        result = aixos_pthread_cond_broadcast(&barrier->condition);
        (void)aixos_pthread_mutex_unlock(&barrier->mutex);
        return result != 0 ? result : AIXOS_PTHREAD_BARRIER_SERIAL_THREAD;
    }
    while (generation == barrier->generation) {
        result = aixos_pthread_cond_wait(&barrier->condition,
                                         &barrier->mutex);
        if (result != 0) {
            (void)aixos_pthread_mutex_unlock(&barrier->mutex);
            return result;
        }
    }
    return aixos_pthread_mutex_unlock(&barrier->mutex);
}

int aixos_sem_posix_init(aixos_sem_posix_t *sem, unsigned int value)
{
    if (sem == NULL || value > AIXOS_CFG_SEM_MAX_COUNT) {
        return AIXOS_EINVAL;
    }
    *sem = aixos_sem_create((int)value);
    return *sem == AIXOS_HANDLE_INVALID ? AIXOS_EAGAIN : 0;
}

int aixos_sem_posix_destroy(aixos_sem_posix_t *sem)
{
    return sem == NULL ? AIXOS_EINVAL :
           map_error(aixos_sem_delete(*sem));
}

int aixos_sem_posix_wait(aixos_sem_posix_t *sem)
{
    return sem == NULL ? AIXOS_EINVAL :
           map_error(aixos_sem_wait(*sem, UINT32_MAX));
}

int aixos_sem_posix_trywait(aixos_sem_posix_t *sem)
{
    return sem == NULL ? AIXOS_EINVAL :
           map_error(aixos_sem_wait(*sem, 0U));
}

int aixos_sem_posix_timedwait(aixos_sem_posix_t *sem,
                              const aixos_timespec_t *absolute)
{
    uint32_t timeout;
    int result = relative_timeout(absolute, &timeout);
    return result != 0 || sem == NULL ?
           (result != 0 ? result : AIXOS_EINVAL) :
           map_error(aixos_sem_wait(*sem, timeout));
}

int aixos_sem_posix_post(aixos_sem_posix_t *sem)
{
    return sem == NULL ? AIXOS_EINVAL :
           map_error(aixos_sem_post(*sem));
}

int aixos_sem_posix_getvalue(aixos_sem_posix_t *sem, int *value)
{
    int result;
    if (sem == NULL || value == NULL) return AIXOS_EINVAL;
    result = aixos_sem_get_count(*sem);
    if (result < 0) return map_error(result);
    *value = result;
    return 0;
}

int aixos_clock_gettime(int clock_id, aixos_timespec_t *time)
{
    uint64_t ticks;
    uint64_t nanoseconds;
    aixos_sched_stats_t stats;
    if (time == NULL || clock_id != AIXOS_CLOCK_MONOTONIC) {
        return clock_id == AIXOS_CLOCK_REALTIME ?
               AIXOS_ENOTSUP : AIXOS_EINVAL;
    }
    aixos_sched_stats_snapshot(&stats);
    ticks = stats.total_ticks;
    nanoseconds = (ticks % AIXOS_CFG_SYSTICK_HZ) *
                  UINT64_C(1000000000) / AIXOS_CFG_SYSTICK_HZ;
    time->tv_sec = (int64_t)(ticks / AIXOS_CFG_SYSTICK_HZ);
    time->tv_nsec = (int32_t)nanoseconds;
    return 0;
}

int aixos_clock_getres(int clock_id, aixos_timespec_t *resolution)
{
    if (resolution == NULL || clock_id != AIXOS_CLOCK_MONOTONIC) {
        return clock_id == AIXOS_CLOCK_REALTIME ?
               AIXOS_ENOTSUP : AIXOS_EINVAL;
    }
    resolution->tv_sec = 0;
    resolution->tv_nsec = (int32_t)(UINT32_C(1000000000) /
                                    AIXOS_CFG_SYSTICK_HZ);
    return 0;
}

int aixos_nanosleep(const aixos_timespec_t *request,
                    aixos_timespec_t *remaining)
{
    uint64_t milliseconds;
    int result;
    if (request == NULL || request->tv_sec < 0 || request->tv_nsec < 0 ||
        request->tv_nsec >= 1000000000) {
        return AIXOS_EINVAL;
    }
    milliseconds = (uint64_t)request->tv_sec * 1000U +
                   ((uint64_t)request->tv_nsec + 999999U) / 1000000U;
    if (milliseconds > UINT32_MAX) return AIXOS_EINVAL;
    result = aixos_task_sleep((uint32_t)milliseconds);
    if (remaining != NULL) {
        remaining->tv_sec = 0;
        remaining->tv_nsec = 0;
    }
    return map_error(result);
}

int aixos_clock_nanosleep(int clock_id, int absolute,
                          const aixos_timespec_t *request)
{
    uint32_t timeout;
    if (clock_id != AIXOS_CLOCK_MONOTONIC) return AIXOS_ENOTSUP;
    if (absolute == 0) return aixos_nanosleep(request, NULL);
    if (relative_timeout(request, &timeout) != 0) return AIXOS_EINVAL;
    return map_error(aixos_task_sleep(timeout));
}

static int timespec_valid(const aixos_timespec_t *value)
{
    return value != NULL && value->tv_sec >= 0 && value->tv_nsec >= 0 &&
           value->tv_nsec < 1000000000;
}

static int timespec_to_ms(const aixos_timespec_t *value, uint32_t *ms)
{
    uint64_t milliseconds;
    if (!timespec_valid(value) || ms == NULL) {
        return AIXOS_EINVAL;
    }
    milliseconds = (uint64_t)value->tv_sec * 1000U +
                   ((uint64_t)value->tv_nsec + 999999U) / 1000000U;
    if (milliseconds > UINT32_MAX) {
        return AIXOS_EINVAL;
    }
    *ms = (uint32_t)milliseconds;
    return 0;
}

static void ms_to_timespec(uint32_t ms, aixos_timespec_t *value)
{
    value->tv_sec = (int64_t)(ms / 1000U);
    value->tv_nsec = (int32_t)((ms % 1000U) * 1000000U);
}

static uint32_t ticks_to_ms(uint32_t ticks)
{
    uint64_t milliseconds =
        ((uint64_t)ticks * 1000U + AIXOS_CFG_SYSTICK_HZ - 1U) /
        AIXOS_CFG_SYSTICK_HZ;
    return milliseconds > UINT32_MAX ? UINT32_MAX :
           (uint32_t)milliseconds;
}

static posix_timer_t *timer_from_handle(aixos_timer_posix_t handle)
{
    uint32_t index;
    posix_timer_t *timer;
    if (handle < 0) {
        return NULL;
    }
    index = AIXOS_HANDLE_IDX(handle);
    if (index >= AIXOS_CFG_POSIX_TIMERS) {
        return NULL;
    }
    timer = &timers[index];
    if (timer->used == 0U ||
        timer->generation != AIXOS_HANDLE_GEN(handle)) {
        return NULL;
    }
    return timer;
}

static void timer_snapshot(posix_timer_t *timer, aixos_itimerspec_t *value)
{
    uint32_t remaining_ticks = 0U;
    if (timer->active != 0U) {
        uint32_t now = aixos_get_tick();
        if ((int32_t)(timer->next_deadline - now) > 0) {
            remaining_ticks = timer->next_deadline - now;
        }
    }
    ms_to_timespec(ticks_to_ms(remaining_ticks), &value->value);
    ms_to_timespec(ticks_to_ms(timer->interval_ticks), &value->interval);
}

static void posix_timer_expired(void *argument)
{
    posix_timer_t *timer = (posix_timer_t *)argument;
    aixos_sigevent_t event;
    uint32_t restart_ms = 0U;
    aixos_arch_flags_t flags = aixos_arch_int_disable();
    if (timer->used == 0U || timer->active == 0U) {
        aixos_arch_int_restore(flags);
        return;
    }
    {
        uint32_t now = aixos_get_tick();
        timer->overrun = 0;
        if (timer->interval_ticks != 0U) {
            if ((int32_t)(now - timer->next_deadline) >= 0) {
                uint32_t late = now - timer->next_deadline;
                uint32_t missed = late / timer->interval_ticks;
                timer->overrun =
                    missed > INT32_MAX ? INT32_MAX : (int)missed;
            }
            timer->next_deadline = now + timer->interval_ticks;
            restart_ms = ticks_to_ms(timer->interval_ticks);
        } else {
            timer->active = 0U;
        }
        event = timer->event;
    }
    aixos_arch_int_restore(flags);

    if (restart_ms != 0U) {
        (void)aixos_timer_start(timer->native, restart_ms);
    } else {
        (void)aixos_timer_stop(timer->native);
    }
    if (event.notify == AIXOS_SIGEV_THREAD && event.function != NULL) {
        event.function(event.value);
    }
}

int aixos_timer_posix_create(int clock_id, const aixos_sigevent_t *event,
                             aixos_timer_posix_t *handle)
{
    posix_timer_t *timer = NULL;
    aixos_arch_flags_t flags;
    uint32_t index;
    if (clock_id != AIXOS_CLOCK_MONOTONIC || handle == NULL ||
        (event != NULL &&
         event->notify != AIXOS_SIGEV_NONE &&
         event->notify != AIXOS_SIGEV_THREAD) ||
        (event != NULL && event->notify == AIXOS_SIGEV_THREAD &&
         event->function == NULL)) {
        return clock_id == AIXOS_CLOCK_REALTIME ?
               AIXOS_ENOTSUP : AIXOS_EINVAL;
    }

    flags = aixos_arch_int_disable();
    for (index = 0U; index < AIXOS_CFG_POSIX_TIMERS; index++) {
        if (timers[index].used == 0U) {
            timer = &timers[index];
            timer->generation++;
            if (timer->generation == 0U) {
                timer->generation = 1U;
            }
            timer->used = 1U;
            timer->native = AIXOS_HANDLE_INVALID;
            timer->active = 0U;
            timer->interval_ticks = 0U;
            timer->overrun = 0;
            memset(&timer->event, 0, sizeof(timer->event));
            if (event != NULL) {
                timer->event = *event;
            } else {
                timer->event.notify = AIXOS_SIGEV_NONE;
            }
            break;
        }
    }
    aixos_arch_int_restore(flags);
    if (timer == NULL) {
        return AIXOS_EAGAIN;
    }

    timer->native = aixos_timer_create(
        "ptimer", AIXOS_TIMER_PERIODIC, posix_timer_expired, timer);
    if (timer->native == AIXOS_HANDLE_INVALID) {
        flags = aixos_arch_int_disable();
        timer->used = 0U;
        aixos_arch_int_restore(flags);
        return AIXOS_EAGAIN;
    }
    *handle = AIXOS_HANDLE_MAKE(index, timer->generation);
    return 0;
}

int aixos_timer_posix_delete(aixos_timer_posix_t handle)
{
    posix_timer_t *timer;
    aixos_handle_t native;
    aixos_arch_flags_t flags = aixos_arch_int_disable();
    timer = timer_from_handle(handle);
    if (timer == NULL) {
        aixos_arch_int_restore(flags);
        return AIXOS_EINVAL;
    }
    native = timer->native;
    timer->used = 0U;
    timer->active = 0U;
    timer->native = AIXOS_HANDLE_INVALID;
    aixos_arch_int_restore(flags);
    return map_error(aixos_timer_delete(native));
}

int aixos_timer_posix_gettime(aixos_timer_posix_t handle,
                              aixos_itimerspec_t *value)
{
    posix_timer_t *timer;
    aixos_arch_flags_t flags;
    if (value == NULL) {
        return AIXOS_EINVAL;
    }
    flags = aixos_arch_int_disable();
    timer = timer_from_handle(handle);
    if (timer == NULL) {
        aixos_arch_int_restore(flags);
        return AIXOS_EINVAL;
    }
    timer_snapshot(timer, value);
    aixos_arch_int_restore(flags);
    return 0;
}

int aixos_timer_posix_settime(aixos_timer_posix_t handle, int flags_value,
                              const aixos_itimerspec_t *value,
                              aixos_itimerspec_t *old_value)
{
    posix_timer_t *timer;
    uint32_t initial_ms;
    uint32_t interval_ms;
    uint32_t initial_ticks;
    uint32_t interval_ticks;
    int result;
    aixos_arch_flags_t interrupt_flags;
    if (value == NULL ||
        (flags_value != 0 && flags_value != AIXOS_TIMER_ABSTIME) ||
        !timespec_valid(&value->value) ||
        !timespec_valid(&value->interval)) {
        return AIXOS_EINVAL;
    }
    result = timespec_to_ms(&value->interval, &interval_ms);
    if (result != 0) {
        return result;
    }
    if (value->value.tv_sec == 0 && value->value.tv_nsec == 0) {
        initial_ms = 0U;
    } else if (flags_value == AIXOS_TIMER_ABSTIME) {
        aixos_timespec_t now;
        int64_t seconds;
        int64_t nanoseconds;
        uint64_t delta_ns;
        result = aixos_clock_gettime(AIXOS_CLOCK_MONOTONIC, &now);
        if (result != 0) {
            return result;
        }
        seconds = value->value.tv_sec - now.tv_sec;
        nanoseconds = (int64_t)value->value.tv_nsec - now.tv_nsec;
        if (nanoseconds < 0) {
            seconds--;
            nanoseconds += 1000000000;
        }
        if (seconds < 0) {
            initial_ms = 1U;
        } else {
            delta_ns = (uint64_t)seconds * 1000000000U +
                       (uint64_t)nanoseconds;
            if (delta_ns == 0U) {
                initial_ms = 1U;
            } else if (delta_ns > (uint64_t)UINT32_MAX * 1000000U) {
                return AIXOS_EINVAL;
            } else {
                initial_ms =
                    (uint32_t)((delta_ns + 999999U) / 1000000U);
            }
        }
    } else {
        result = timespec_to_ms(&value->value, &initial_ms);
        if (result != 0) {
            return result;
        }
    }
    initial_ticks = aixos_ms_to_ticks(initial_ms);
    interval_ticks = aixos_ms_to_ticks(interval_ms);
    if ((initial_ms != 0U && initial_ticks == UINT32_MAX) ||
        (interval_ms != 0U && interval_ticks == UINT32_MAX)) {
        return AIXOS_EINVAL;
    }

    interrupt_flags = aixos_arch_int_disable();
    timer = timer_from_handle(handle);
    if (timer == NULL) {
        aixos_arch_int_restore(interrupt_flags);
        return AIXOS_EINVAL;
    }
    if (old_value != NULL) {
        timer_snapshot(timer, old_value);
    }
    (void)aixos_timer_stop(timer->native);
    timer->active = 0U;
    timer->interval_ticks = interval_ticks;
    timer->overrun = 0;
    if (initial_ms != 0U) {
        timer->next_deadline = aixos_get_tick() + initial_ticks;
        result = aixos_timer_start(timer->native, initial_ms);
        if (result == AIXOS_OK) {
            timer->active = 1U;
        }
    } else {
        result = AIXOS_OK;
    }
    aixos_arch_int_restore(interrupt_flags);
    return map_error(result);
}

int aixos_timer_posix_getoverrun(aixos_timer_posix_t handle)
{
    posix_timer_t *timer;
    int overrun;
    aixos_arch_flags_t flags = aixos_arch_int_disable();
    timer = timer_from_handle(handle);
    if (timer == NULL) {
        aixos_arch_int_restore(flags);
        return -AIXOS_EINVAL;
    }
    overrun = timer->overrun;
    aixos_arch_int_restore(flags);
    return overrun;
}

aixos_mqd_t aixos_mq_posix_open(long max_messages, long message_size)
{
    if (max_messages <= 0 || message_size <= 0) {
        return AIXOS_HANDLE_INVALID;
    }
    return aixos_mq_create((size_t)max_messages, (size_t)message_size);
}

int aixos_mq_posix_close(aixos_mqd_t queue)
{
    return map_error(aixos_mq_delete(queue));
}

int aixos_mq_posix_send(aixos_mqd_t queue, const char *message,
                        size_t length, unsigned int priority)
{
    return map_error(aixos_mq_send_priority(queue, message, length,
                                            priority, UINT32_MAX));
}

int aixos_mq_posix_timedsend(aixos_mqd_t queue, const char *message,
                             size_t length, unsigned int priority,
                             const aixos_timespec_t *absolute)
{
    uint32_t timeout;
    int result;
    result = relative_timeout(absolute, &timeout);
    return result != 0 ? result :
           map_error(aixos_mq_send_priority(queue, message, length,
                                            priority, timeout));
}

int aixos_mq_posix_receive(aixos_mqd_t queue, char *message,
                           size_t capacity, unsigned int *priority)
{
    size_t length = 0U;
    uint32_t native_priority = 0U;
    int result = aixos_mq_recv_priority(queue, message, capacity, &length,
                                        &native_priority, UINT32_MAX);
    if (result != AIXOS_OK) return -map_error(result);
    if (priority != NULL) *priority = native_priority;
    return (int)length;
}

int aixos_mq_posix_timedreceive(aixos_mqd_t queue, char *message,
                                size_t capacity, unsigned int *priority,
                                const aixos_timespec_t *absolute)
{
    uint32_t timeout;
    uint32_t native_priority = 0U;
    size_t length = 0U;
    int result = relative_timeout(absolute, &timeout);
    if (result != 0) return -result;
    result = aixos_mq_recv_priority(queue, message, capacity, &length,
                                    &native_priority, timeout);
    if (result != AIXOS_OK) return -map_error(result);
    if (priority != NULL) *priority = native_priority;
    return (int)length;
}

int aixos_mq_posix_getattr(aixos_mqd_t queue, aixos_mq_attr_t *attr)
{
    aixos_mq_info_t info;
    int result;
    if (attr == NULL) return AIXOS_EINVAL;
    result = aixos_mq_get_info(queue, &info);
    if (result != AIXOS_OK) return map_error(result);
    attr->mq_flags = 0;
    attr->mq_maxmsg = (long)info.max_messages;
    attr->mq_msgsize = (long)info.message_size;
    attr->mq_curmsgs = (long)info.current_messages;
    return 0;
}

int aixos_mq_posix_setattr(aixos_mqd_t queue,
                           const aixos_mq_attr_t *new_attr,
                           aixos_mq_attr_t *old_attr)
{
    if (new_attr == NULL && old_attr == NULL) return AIXOS_EINVAL;
    if (old_attr != NULL) {
        int result = aixos_mq_posix_getattr(queue, old_attr);
        if (result != 0) return result;
    }
    /* Only mq_flags (O_NONBLOCK) can be changed; we do not support it. */
    return 0;
}

int aixos_mq_posix_notify(aixos_mqd_t queue,
                           const aixos_sigevent_t *event)
{
    (void)queue;
    (void)event;
    return AIXOS_ENOSYS;
}

#ifdef AIXOS_HOST_TEST
int aixos_posix_test_complete(aixos_pthread_t thread_id, void *value)
{
    posix_thread_t *thread;
    aixos_arch_flags_t flags = aixos_arch_int_disable();
    thread = thread_find(thread_id);
    if (thread == NULL || thread->completed != 0U) {
        aixos_arch_int_restore(flags);
        return AIXOS_ESRCH;
    }
    thread->result = value;
    thread->completed = 1U;
    aixos_arch_int_restore(flags);
    return map_error(aixos_sem_post(thread->completion));
}

int aixos_posix_test_run_thread(aixos_pthread_t thread_id)
{
    posix_thread_t *thread;
    aixos_tcb_t *tcb;
    aixos_tcb_t *saved;
    aixos_arch_flags_t flags = aixos_arch_int_disable();
    thread = thread_find(thread_id);
    if (thread == NULL || thread->completed != 0U) {
        aixos_arch_int_restore(flags);
        return AIXOS_ESRCH;
    }
    aixos_arch_int_restore(flags);

    tcb = aixos_tcb_from_handle(thread_id);
    if (tcb == NULL) {
        return AIXOS_ESRCH;
    }
    saved = g_cur_task;
    aixos_test_set_current(tcb);
    thread_entry(thread);
    aixos_test_set_current(saved);
    return 0;
}
#endif
