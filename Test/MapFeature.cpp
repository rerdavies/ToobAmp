#include "MapFeature.h"
#include <mutex>

using namespace TwoPlay;

static LV2_URID mapFn(LV2_URID_Map_Handle handle, const char* uri)
{
	MapFeature* feature = (MapFeature*)(void*)handle;
	return feature->GetUrid(uri);
}

MapFeature::MapFeature()
{
	feature.URI = LV2_URID__map;
	feature.data = &map;
	map.handle = (void*)this;
	map.map = &mapFn;
}

LV2_URID MapFeature::GetUrid(const char* uri)
{

	std::lock_guard<std::mutex> guard(mapMutex);

	LV2_URID result = stdMap[uri];
	if (result == 0)
	{
		stdMap[uri] = ++nextAtom;
		result = nextAtom;
	}
	return result;
}
