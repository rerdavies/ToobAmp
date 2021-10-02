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


