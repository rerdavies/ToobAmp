#pragma once

#include "Lv2Host.h"
#include <vector>
#include "InputControl.h"
#include "OutputControl.h"
#include "lv2/core/lv2.h"
#include "Lv2Exception.h"


namespace TwoPlay {

	enum class PortType {
		InputAudio,
		InputControl,
		InputAtomStream,
		OutputAudio,
		OutputControl,
		OutputAtomStream
	};

	class HostedLv2Plugin {
	private:
		class AtomStreamEntry {
		public:
			int port;
			LV2_Atom* buffer;
			uint32_t size;
		public:
			AtomStreamEntry()
			{
				this->port = - 1;
				this->buffer = NULL;
				this->size = 0;
			}
			AtomStreamEntry(int port, LV2_Atom* buffer, uint32_t size)
			{
				this->port = port;
				this->buffer = buffer;
				this->size = size;
			}
			void FreeBuffer()
			{
				if (buffer != NULL)
				{
					delete[](char*)buffer;
					buffer = NULL;
				}
			}
			~AtomStreamEntry() {
				buffer = NULL;
			}
		};

		struct Uris {
		public:
			void Map(Lv2Host* host)
			{
				ridAtomSequence = host->MapURI(LV2_ATOM__Sequence);
			}

			LV2_URID ridAtomSequence;
		};
		Uris uris;
		std::vector <float*> ioBuffers;
		std::vector <InputControl*> inputControls;
		std::vector <OutputControl*> outputControls;
		std::vector <AtomStreamEntry> inputAtomStreams;
		std::vector <AtomStreamEntry> outputAtomStreams;
		const LV2_Descriptor* descriptor;
		LV2_Handle instance;

		std::vector<PortType> portTypes;
		Lv2Host* host;

		void ConnectPort(int port, void* data);

	private:
		friend class Lv2Host;

		HostedLv2Plugin(Lv2Host* host)
		{
			this->host = host;
			uris.Map(host);
		}
		virtual ~HostedLv2Plugin();

		void Instantiate(const LV2_Descriptor* descriptor, const char* resourcePath) noexcept(false); // throws(Lv2Exception)
	public:
		void Activate();

		void PrepareAtomPorts();
		void Run(uint32_t samples);
		void Deactivate();

	public:
		int GetAudioBufferSize() {
			return host->GetAudioBufferSize();
		}
		void SetPortType(int port, PortType portType);

		void SetPortType(int port, PortType portType, uint32_t bufferSize);

		void SetPortType(int port, PortType portType, float defaultValue, float minValue, float maxValue);

		const float* GetOutputAudio(int port)
		{
			return ioBuffers[port];
		}

		float* GetInputAudio(int port) {
			return ioBuffers[port];
		}

		void SetControl(int control, float value)
		{
			inputControls[control]->SetValue(value);
		}
	};
};