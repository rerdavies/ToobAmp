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
#include <map>
#include <string>
#include <mutex>


namespace TwoPlay {
	class MapFeature {

	private:
		LV2_URID nextAtom = 0;
		LV2_Feature feature;
		LV2_URID_Map map;
		std::map<std::string, LV2_URID> stdMap;
		std::mutex mapMutex;


	public:
		MapFeature();
	public:
		const LV2_Feature* GetFeature()
		{
			return &feature;
		}
		LV2_URID GetUrid(const char* uri);

	};
}