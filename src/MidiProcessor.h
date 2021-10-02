#pragma once

#include "lv2/core/lv2.h"
#include "lv2/atom/atom.h"

namespace TwoPlay {


	class IMidiCallback {
	public:
		virtual void OnMidiCommand(int cmd0, int cmd1, int cmd2) = 0;
	};

	class MidiProcessor {
	private:
		uint32_t ridMidiEvent;
		IMidiCallback* callback;

	public:
		MidiProcessor(const LV2_Feature* const* _features, IMidiCallback* callback);
		virtual ~MidiProcessor() { }
	protected:
		void ProcessMidiEvents(const LV2_Atom_Sequence* events);

	};
}