#pragma once

#include "lv2/core/lv2.h"

#include "lv2/atom/atom.h"
#include "lv2/atom/util.h"
#include "lv2/core/lv2.h"
#include "lv2/log/log.h"
#include "lv2/log/logger.h"
#include "lv2/midi/midi.h"
#include "lv2/urid/urid.h"
#include "lv2/atom/atom.h"
#include "lv2/worker/worker.h"

#include <map>
#include <string>
#include <mutex>

namespace TwoPlay {
	class ScheduleFeature {

	private:
		LV2_Feature feature;
		LV2_Worker_Schedule schedule;
		std::mutex mapMutex;

	public:
		ScheduleFeature();

		const LV2_Feature* GetFeature()
		{
			return &feature;
		}

		void ScheduleWork(uint32_t     size, const void* data);

	};
}