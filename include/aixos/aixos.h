#ifndef AIXOS_H
#define AIXOS_H
#include "aixos/version.h"
#include "aixos/types.h"
#include "aixos/abi.h"
#include "aixos/arch/arch.h"
#include "aixos/task.h"
#include "aixos/sem.h"
#include "aixos/mutex.h"
#include "aixos/mq.h"
#include "aixos/event.h"
#include "aixos/pipe.h"
#include "aixos/timer.h"
#include "aixos/heap.h"
#include "aixos/mempool.h"
#include "aixos/mpu.h"
#include "aixos/notify.h"
#include "aixos/trace.h"
#include "aixos/crash.h"
#include "aixos/microkernel.h"

void aixos_object_init(void);
void aixos_sched_init(void);
#endif
