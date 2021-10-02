#include "MidiProcessor.h"

#include "lv2/atom/atom.h"
#include "lv2/atom/util.h"
#include "lv2/core/lv2.h"
#include "lv2/core/lv2_util.h"
#include "lv2/log/log.h"
#include "lv2/log/logger.h"
#include "lv2/midi/midi.h"
#include "lv2/urid/urid.h"
#include "lv2/atom/atom.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using namespace TwoPlay;


MidiProcessor::MidiProcessor(const LV2_Feature* const* _features, IMidiCallback* callback)
	:ridMidiEvent(0)
{

	this->callback = callback;

	LV2_URID_Map* map = NULL;

	const char* missing = lv2_features_query(
		_features,
		LV2_URID__map, &map, true,
		NULL);
	if (missing) {
		throw "Missing feature: map";
	}
	if (map != NULL)
	{
		ridMidiEvent =
			map->map(map->handle, LV2_MIDI__MidiEvent);
	}
}


void MidiProcessor::ProcessMidiEvents(const LV2_Atom_Sequence* events)
{
	LV2_ATOM_SEQUENCE_FOREACH(events, ev) {
		if (ev->body.type == ridMidiEvent) {
			const uint8_t* const msg = (const uint8_t*)(ev + 1);
			if (callback != NULL)
			{
				callback->OnMidiCommand(msg[0], msg[1], msg[2]);
			}

		}
	}
}

