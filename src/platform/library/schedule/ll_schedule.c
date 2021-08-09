// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>

#include <sof/audio/component.h>
#include <sof/schedule/task.h>
#include <stdint.h>
#include <sof/schedule/schedule.h>
#include <sof/schedule/ll_schedule.h>
#include <sof/lib/wait.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <poll.h>

 /* scheduler testbench definition */

/* 77de2074-828c-4044-a40b-420b72749e8b */
DECLARE_SOF_UUID("ll-schedule", ll_sched_uuid, 0x77de2074, 0x828c, 0x4044,
		 0xa4, 0x0b, 0x42, 0x0b, 0x72, 0x74, 0x9e, 0x8b);

DECLARE_TR_CTX(ll_tr, SOF_UUID(ll_sched_uuid), LOG_LEVEL_INFO);

struct ll_vcore {
	struct list_item list; /* list of tasks in priority queue */
	pthread_mutex_t list_mutex;
	pthread_t thread_id;
	pthread_cond_t systick_cond;
	pthread_mutex_t systick_mutex;
	int vcore_ready;
};

inline void ts_add_ns(struct timespec* ts, uint64_t nsecs)
{
    uint64_t ns = ts->tv_sec * (uint64_t)1000000000L + ts->tv_nsec + nsecs;
    ts->tv_sec = ns / (uint64_t)1000000000L;
    ts->tv_nsec = ns - ts->tv_sec * (uint64_t)1000000000L;
}

inline void ts_nearest(struct timespec* ts, uint64_t nsecs, uint64_t nearest)
{
    uint64_t ns = (ts->tv_sec * (uint64_t)1000000000L + ts->tv_nsec + nsecs) / nearest * nearest;
    ts->tv_sec = ns / (uint64_t)1000000000L;
    ts->tv_nsec = ns - ts->tv_sec * (uint64_t)1000000000L;
}

static void *ll_thread(void *data)
{
	struct ll_vcore *vc = data;
	struct timespec tv;
	struct list_item *tlist, *tlist_;
	struct task *task;

	pthread_cond_init(&vc->systick_cond, NULL);
	clock_gettime(CLOCK_REALTIME, &tv);

	/* schedule on a milisecond boundary to the neareast ms from now */
	ts_nearest(&tv, 1000000, 1000000);

	while (1) {

		/* wait for the next tick */
		pthread_mutex_lock(&vc->systick_mutex);
		pthread_cond_timedwait(&vc->systick_cond, &vc->systick_mutex, &tv);

		/* wait for next millisecond */
		ts_add_ns(&tv, 1000000);
		pthread_mutex_lock(&vc->list_mutex);

		/* list empty then return */
		if (list_is_empty(&vc->list)) {
			pthread_mutex_unlock(&vc->list_mutex);
			break;
		}

		/* iterate through the task list */
		list_for_item_safe(tlist, tlist_, &vc->list) {
			task = container_of(tlist, struct task, list);

			if (task->state == SOF_TASK_STATE_QUEUED) {
				pthread_mutex_unlock(&vc->list_mutex);
				task->ops.run(task->data);
				pthread_mutex_lock(&vc->list_mutex);
			}
		}

		pthread_mutex_unlock(&vc->list_mutex);
		pthread_mutex_unlock(&vc->systick_mutex);
	}

	vc->vcore_ready = 0;
	return NULL;
}

static int schedule_ll_task_complete(void *data, struct task *task)
{
	struct ll_vcore *vc = data;

	pthread_mutex_lock(&vc->list_mutex);
	list_item_del(&task->list);
	task->state = SOF_TASK_STATE_COMPLETED;
	pthread_mutex_unlock(&vc->list_mutex);

	return 0;
}

/* schedule task */
static int schedule_ll_task(void *data, struct task *task, uint64_t start,
			      uint64_t period)
{
	struct ll_vcore *vc = data;
	int err;

	/* add task to list */
	pthread_mutex_lock(&vc->list_mutex);
	list_item_prepend(&task->list, &vc->list);
	task->state = SOF_TASK_STATE_QUEUED;
	pthread_mutex_unlock(&vc->list_mutex);

	/* is vcore thread running ? */
	if (!vc->vcore_ready) {
		err = pthread_create(&vc->thread_id, NULL,
					ll_thread, &vc[task->core]);
		if (err < 0) {
			return err;
		}

		vc->vcore_ready = 1;
	}

	return 0;
}

static void ll_scheduler_free(void *data)
{
	free(data);
}

static int schedule_ll_task_cancel(void *data, struct task *task)
{
	struct ll_vcore *vc = data;

	pthread_mutex_lock(&vc->list_mutex);
	if (task->state == SOF_TASK_STATE_QUEUED) {
		/* delete task */
		task->state = SOF_TASK_STATE_CANCEL;
		list_item_del(&task->list);
	}
	pthread_mutex_unlock(&vc->list_mutex);

	return 0;
}

static int schedule_ll_task_free(void *data, struct task *task)
{
	struct ll_vcore *vc = data;

	pthread_mutex_lock(&vc->list_mutex);
	task->state = SOF_TASK_STATE_FREE;
	pthread_mutex_unlock(&vc->list_mutex);

	return 0;
}

static struct scheduler_ops schedule_ll_ops = {
	.schedule_task		= schedule_ll_task,
	.schedule_task_running	= NULL,
	.schedule_task_complete = schedule_ll_task_complete,
	.reschedule_task	= NULL,
	.schedule_task_cancel	= schedule_ll_task_cancel,
	.schedule_task_free	= schedule_ll_task_free,
	.scheduler_free		= ll_scheduler_free,
};

int schedule_task_init_ll(struct task *task,
			  const struct sof_uuid_entry *uid, uint16_t type,
			  uint16_t priority, enum task_state (*run)(void *data),
			  void *data, uint16_t core, uint32_t flags)
{
	int ret = 0;

	ret = schedule_task_init(task, uid, SOF_SCHEDULE_LL_TIMER, 0, run,
				 data, core, flags);
	if (ret < 0)
		return ret;

	return 0;
}

/* initialize scheduler */
int scheduler_init_ll(struct ll_schedule_domain *domain)
{
	struct ll_vcore *vcore;
	int i;

	tr_info(&ll_tr, "ll_scheduler_init()");


	vcore = calloc(sizeof(*vcore), CONFIG_CORE_COUNT);
	if (!vcore)
		return -ENOMEM;

	for (i = 0; i < CONFIG_CORE_COUNT; i++) {
		list_init(&vcore[i].list);
	}

	scheduler_init(SOF_SCHEDULE_LL_TIMER, &schedule_ll_ops, vcore);

	return 0;
}
