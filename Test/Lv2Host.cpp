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
#include "Lv2Host.h"
#include "HostedLv2Plugin.h"
#include "Lv2Api.h"
#include <filesystem>

using namespace TwoPlay;
using namespace std;


Lv2Host::~Lv2Host()
{
	for (auto i = activePlugins.begin(); i != activePlugins.end(); ++i)
	{
		delete (*i);
	}
	activePlugins.clear();
}
void Lv2Host::DeletePlugin(HostedLv2Plugin* plugin)
{
	for (auto i = activePlugins.begin(); i != activePlugins.end(); ++i)
	{
		if (*i == plugin)
		{
			activePlugins.erase(i);
			break;
		}
	}
	delete plugin;
}

void Lv2Host::Activate()
{
	for (auto plugin : activePlugins)
	{
		plugin->Activate();
	}
}
void Lv2Host::Deactivate()
{
	for (auto plugin : activePlugins)
	{
		plugin->Deactivate();
	}
}
void Lv2Host::Run(int samples)
{
	for (auto plugin : activePlugins)
	{
		plugin->PrepareAtomPorts();
	}
	for (auto plugin : activePlugins)
	{
		plugin->Run(samples);
	}
}



HostedLv2Plugin* Lv2Host::CreatePlugin(const char *libName,int instance)
{
	std::filesystem::path libPath = LocateLv2Plugin(libName);

	FN_LV2_ENTRY* pfn = LoadLv2Plugin(libName);
	if (pfn == NULL) return NULL;

	const LV2_Descriptor *descriptor = pfn(instance);
	if (descriptor == NULL) return NULL;


	HostedLv2Plugin* pResult = new HostedLv2Plugin(this);
	std::filesystem::path bundlePath = libPath.parent_path();
	try {
		pResult->Instantiate(descriptor, bundlePath.string().c_str());
	}
	catch (Lv2Exception e)
	{
		delete pResult;
		throw e;
	}
	activePlugins.push_back(pResult);
	return pResult;


}
