#include "LogFeature.h"
#include <mutex>
#include <cstdio>


using namespace TwoPlay;
using namespace std;



int LogFeature::printfFn(LV2_Log_Handle handle, LV2_URID type, const char* fmt, ...)
{
	va_list va;
	va_start(va, fmt);

	LogFeature* logFeature = (LogFeature*)handle;
	return logFeature->vprintf(type, fmt, va);
}

int LogFeature::vprintfFn(LV2_Log_Handle handle,
	LV2_URID       type,
	const char* fmt,
	va_list        ap)
{
	LogFeature* logFeature = (LogFeature*)handle;
	return logFeature->vprintf(type, fmt, ap);
}

int LogFeature::vprintf(LV2_URID type,const char*fmt, va_list va)
{
	std::lock_guard<std::mutex> guard(logMutex);

	const char* prefix = "";

	if (type == uris.ridError)
	{
		prefix = "Error";
	}
	else if (type == uris.ridWarning)
	{
		prefix = "Warning";
	}
	else if (type == uris.ridNote)
	{
		prefix = "Note";
	}
	else if (type == uris.ridTrace)
	{
		prefix = "Trace";
	}
	char buffer[1024];
	vsnprintf(buffer, sizeof(buffer), fmt, va);

	return fprintf(stderr, "%s: %s",prefix, buffer);
}


LogFeature::LogFeature()
{
	feature.URI = LV2_LOG__log;
	feature.data = &log;
	log.handle = (void*)this;
	log.printf = printfFn;
	log.vprintf = vprintfFn;
}
void LogFeature::Prepare(MapFeature*map)
{
	uris.Map(map);

}

