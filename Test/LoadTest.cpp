/*
 *   Copyright (c) 2022 Robin E. R. Davies
 *   All rights reserved.

 *   Permission is hereby granted, free of charge, to any person obtaining a copy
 *   of this software and associated documentation files (the "Software"), to deal
 *   in the Software without restriction, including without limitation the rights
 *   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *   copies of the Software, and to permit persons to whom the Software is
 *   furnished to do so, subject to the following conditions:
 
 *   The above copyright notice and this permission notice shall be included in all
 *   copies or substantial portions of the Software.
 
 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *   SOFTWARE.
 */

#include "Test.h"
#include "LoadTest.h"

#include "Lv2Api.h"
#include "MapFeature.h"
#include "Lv2Host.h"
#include "HostedLv2Plugin.h"
#include <memory>

using namespace toob;

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