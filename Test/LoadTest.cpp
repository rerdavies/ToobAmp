#include "Test.h"
#include "LoadTest.h"

#include "Lv2Api.h"
#include "MapFeature.h"
#include "Lv2Host.h"
#include "HostedLv2Plugin.h"
#include <memory>

using namespace TwoPlay;

void LoadTest::Execute()
{
	LinkTest();
	ExecuteInputStage();


} 

void LoadTest::LinkTest()
{
	const LV2_Descriptor* pDescriptor = lv2_descriptor(0);
}

void LoadTest::ExecuteInputStage()
{

	FN_LV2_ENTRY* pfn = LoadLv2Plugin("InputStage");

	Lv2Host host(44100.0, 1024);

	HostedLv2Plugin* plugin(host.CreatePlugin("InputStage", 0));

	// set inputs for InputStage
	plugin->SetPortType(0, PortType::InputControl, 0, -60, 30); // trim
	plugin->SetPortType(1, PortType::InputControl, 120,30,300); // lo cut
	plugin->SetPortType(2, PortType::InputControl, 0,0,25); // Bright
	plugin->SetPortType(3, PortType::InputControl, 1300, 1000, 5000); // Brightf
	plugin->SetPortType(4, PortType::InputControl, 6000, 2000, 13000); // Hi cut
	plugin->SetPortType(5, PortType::InputControl, -80, -80, -20); // Gate T
	plugin->SetPortType(6, PortType::InputControl, 0, 0, 60); // Boost

	plugin->SetPortType(7, PortType::InputAudio);
	plugin->SetPortType(8, PortType::OutputAudio);

	plugin->SetPortType(9, PortType::OutputAtomStream,4096);

	host.Activate();

	host.Run(0);
	host.Run(10);
	plugin->SetControl(4, 12000.0f);
	host.Run(10);

	host.Deactivate();

	host.DeletePlugin(plugin);

}