/*
 *   Copyright (c) 2021 Robin E. R. Davies
 *   All rights reserved.

 *   Permission is hereby granted, free of charge, to any person obtaining a copy
 *   of this software and associated documentation files (the "Software"), to deal
 *   in the Software without restriction, including without limitation the rights
 *   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *   copies of the Software, and to permit persons to whom the Software is
 *   furnished to do so, subject to the following conditions:
 
 *   The above copyright notice and this permission notice shall be included in all
 *   copies or substantial portions of the Software.
 
 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *   SOFTWARE.
 */

#include "ScheduleFeature.h"
#include <mutex>


using namespace TwoPlay;

static LV2_Worker_Status scheduleWorkFn(
	LV2_Worker_Schedule_Handle handle,
	uint32_t                   size,
	const void* data)
{
	ScheduleFeature* feature = (ScheduleFeature*)(void*)handle;
	feature->ScheduleWork(size, data);

	return LV2_WORKER_SUCCESS;


}

ScheduleFeature::ScheduleFeature()
{
	feature.URI = LV2_WORKER__schedule;
	feature.data = &schedule;
	schedule.handle = (void*)this;
	schedule.schedule_work = scheduleWorkFn;

}


void ScheduleFeature::ScheduleWork(
	uint32_t     size,
	const void* data
)
{

}


