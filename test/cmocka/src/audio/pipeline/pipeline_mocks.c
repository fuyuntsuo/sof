/*
 * Copyright (c) 2018, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Jakub Dabek <jakub.dabek@linux.intel.com>
 */

#include "pipeline_mocks.h"

#include <mock_trace.h>

TRACE_IMPL()

struct ipc *_ipc;

void platform_dai_timestamp(struct comp_dev *dai,
	struct sof_ipc_stream_posn *posn)
{
	(void)dai;
	(void)posn;
}

void schedule_task(struct task *task, uint64_t start, uint64_t deadline)
{
	(void)deadline;
	(void)start;
	(void)task;
}

void schedule_task_complete(struct task *task)
{
	(void)task;
}

void schedule_task_idle(struct task *task, uint64_t deadline)
{
	(void)deadline;
	(void)task;
}

int schedule_task_cancel(struct task *task)
{
	(void)task;
	return 0;
}

void rfree(void *ptr)
{
	(void)ptr;
}

void platform_host_timestamp(struct comp_dev *host,
	struct sof_ipc_stream_posn *posn)
{
	(void)host;
	(void)posn;
}

int ipc_stream_send_xrun(struct comp_dev *cdev,
	struct sof_ipc_stream_posn *posn)
{
	return 0;
}

int arch_cpu_is_core_enabled(int id)
{
	return 1;
}

void cpu_power_down_core(void) { }

void notifier_notify(void) { }

struct ipc_comp_dev *ipc_get_comp(struct ipc *ipc, uint32_t id)
{
	(void)ipc;
	(void)id;

	return NULL;
}

void heap_trace_all(int force)
{
	(void)force;
}
