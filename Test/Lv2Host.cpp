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
