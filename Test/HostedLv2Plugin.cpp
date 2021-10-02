#include "HostedLv2Plugin.h"
#include "Lv2Exception.h"


using namespace TwoPlay;


HostedLv2Plugin::~HostedLv2Plugin()
{
	if (instance != NULL)
	{
		descriptor->cleanup(instance);
		instance = NULL;
	}
	for (auto i = inputControls.begin(); i != inputControls.end(); ++i)
	{
		if (*i)
		{
			delete (*i);
		}
	}
	for (auto i = outputControls.begin(); i != outputControls.end(); ++i)
	{
		if (*i)
		{
			delete (*i);
		}
	}
	for (auto i = inputAtomStreams.begin(); i < inputAtomStreams.end(); ++i)
	{
		i->FreeBuffer();
	}
	inputAtomStreams.resize(0);

	for (auto i = outputAtomStreams.begin(); i < outputAtomStreams.end(); ++i)
	{
		i->FreeBuffer();
	}

	outputAtomStreams.resize(0);


	inputControls.resize(0);
	host = NULL;

}

void HostedLv2Plugin::Instantiate(const LV2_Descriptor* descriptor, const char* resourcePath) noexcept(false)
{
	this->instance = descriptor->instantiate(descriptor, host->GetSampleRate(), resourcePath, host->GetFeatures());
	this->descriptor = descriptor;

	
}



void HostedLv2Plugin::SetPortType(int port, PortType portType)
{
	if (portTypes.size() <= port)
	{
		portTypes.resize(port + 1);
	}
	portTypes[port] = portType;

	switch (portType)
	{
	case PortType::InputControl:
	{
		InputControl* inputControl = new InputControl();
		if (this->inputControls.size() <= port)
		{
			this->inputControls.resize(port + 1);
		}
		this->inputControls[port] = inputControl;
		ConnectPort(port, inputControl->GetLv2Data());
	}
	break;
	case PortType::InputAudio:
	{
		if (this->ioBuffers.size() <= port)
		{
			this->ioBuffers.resize(port + 1);
		}
		float* buffer = new float[host->GetAudioBufferSize()];
		this->ioBuffers[port] = buffer;
		ConnectPort(port, buffer);
	}
	break;
	case PortType::OutputControl:
	{
		if (this->outputControls.size() <= port)
		{
			this->outputControls.resize(port + 1);
		}
		auto outputControl = new OutputControl();
		this->outputControls[port] = outputControl;
		ConnectPort(port, outputControl->GetLv2Data());
	}
	break;
	case PortType::OutputAudio:
	{
		if (this->ioBuffers.size() <= port) this->ioBuffers.resize(port + 1);
		float* buffer = new float[host->GetAudioBufferSize()];
		this->ioBuffers[port] = buffer;
		ConnectPort(port, buffer);
	}
	break;
	case PortType::OutputAtomStream:
	case PortType::InputAtomStream:
		SetPortType(port, portType, 4096);
		break;
	default:
		throw Lv2Exception("Unknown port type.");
	}
}

void HostedLv2Plugin::SetPortType(int port, PortType portType, uint32_t bufferSize)
{
	switch (portType)
	{
	case PortType::InputAtomStream:
	{
		char* buffer = new char[bufferSize];
		LV2_Atom* pAtom = (LV2_Atom*)(void*)buffer;
		pAtom->size = bufferSize - 8;
		pAtom->type = uris.ridAtomSequence;
		inputAtomStreams.push_back(AtomStreamEntry(port,pAtom,bufferSize));
		ConnectPort(port, buffer);
	}
	break;
	case PortType::OutputAtomStream:
	{
		char* buffer = new char[bufferSize];
		LV2_Atom* pAtom = (LV2_Atom*)(void*)buffer;
		pAtom->size = bufferSize - 8;
		pAtom->type = uris.ridAtomSequence;
		inputAtomStreams.push_back(AtomStreamEntry(port, pAtom, bufferSize));
		ConnectPort(port, buffer);
	}
	break;

	default:
		throw Lv2Exception("bufferSize not valid for this port type.");
	}

}


void HostedLv2Plugin::ConnectPort(int port, void* dataLocation)
{
	descriptor->connect_port(instance, port,dataLocation);
}


void HostedLv2Plugin::SetPortType(int port, PortType portType, float defaultValue,float minValue, float maxValue)
{
	if (portTypes.size() <= port)
	{
		portTypes.resize(port + 1);
	}
	portTypes[port] = portType;
	switch (portType)
	{
	case PortType::InputControl:
	{
		if (this->inputControls.size() <= port)
		{
			this->inputControls.resize(port + 1);
		}
		auto inputControl = new RangedInputControl(defaultValue, minValue, maxValue);
		this->inputControls[port] = inputControl;
		ConnectPort(port, inputControl->GetLv2Data());
		break;
	}
	default:
		throw Lv2Exception("Invalid argument");
	}
}


void HostedLv2Plugin::Activate()
{
	this->descriptor->activate(this->instance);
}

void HostedLv2Plugin::PrepareAtomPorts()
{
	for (auto i = inputAtomStreams.begin(); i != inputAtomStreams.end(); ++i)
	{
		(*i).buffer->type = uris.ridAtomSequence;
		(*i).buffer->size = 0;

	}
	for (auto i = outputAtomStreams.begin(); i != outputAtomStreams.end(); ++i)
	{
		(*i).buffer->type = uris.ridAtomSequence; 
		(*i).buffer->size = i->size - 8;

	}
}
void HostedLv2Plugin::Run(uint32_t samples)
{
	this->descriptor->run(this->instance,samples);

}
void HostedLv2Plugin::Deactivate()
{
	this->descriptor->deactivate(this->instance);
}
