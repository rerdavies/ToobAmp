#include "std.h"

#include "lv2/core/lv2.h"
#include "lv2/log/logger.h"
#include "lv2/uri-map/uri-map.h"
#include "lv2/atom/atom.h"
#include "lv2/atom/forge.h"
#include "lv2/worker/worker.h"
#include "lv2/patch/patch.h"
#include "lv2/parameters/parameters.h"
#include "lv2/units/units.h"
#include "FilterResponse.h"
#include <string>

#include "Lv2Plugin.h"

#include "MidiProcessor.h"
#include "InputPort.h"
#include "OutputPort.h"
#include "ControlDezipper.h"
#include "LsNumerics/Freeverb.hpp"



#define TOOB_FREEVERB_URI "http://two-play.com/plugins/toob-freeverb"
#ifndef TOOB_URI
#define TOOB_URI "http://two-play.com/plugins/toob"
#endif



namespace TwoPlay {

	class ToobFreeverb : public Lv2Plugin {
	private:
		enum class PortId {
			DRYWET = 0,
			ROOMSIZE,
			DAMPING,
			AUDIO_INL,
			AUDIO_INR,
			AUDIO_OUTL,
			AUDIO_OUTR,
		};

		float*dryWet = nullptr;
		float *roomSize= nullptr;
		float *damping = nullptr;
		const float*inL = nullptr;
		const float*inR = nullptr;
		float*outL = nullptr;
		float*outR = nullptr;

		float dryWetValue = -1;
		float roomSizeValue = -1;
		float dampingValue = -1;


		Freeverb freeverb;
		double rate = 44100;
		std::string bundle_path;

		double getRate() { return rate; }
		std::string getBundlePath() { return bundle_path.c_str(); }

	public:
		static Lv2Plugin* Create(double rate,
			const char* bundle_path,
			const LV2_Feature* const* features)
		{
			return new ToobFreeverb(rate, bundle_path, features);
		}
		ToobFreeverb(double rate,
			const char* bundle_path,
			const LV2_Feature* const* features
		);

	public:
		static const char* URI;
	protected:
		virtual void ConnectPort(uint32_t port, void* data);
		virtual void Activate();
		virtual void Run(uint32_t n_samples);
		virtual void Deactivate();

    };

}// namespace TwoPlay