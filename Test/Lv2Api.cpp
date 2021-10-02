#include "Test.h"
#include "Lv2Api.h"
#include <stdio.h>
#include <filesystem>
#include "Lv2Exception.h"


#ifdef _MSC_VER
// Windows implementation

extern "C" {
	typedef struct { } *HMODULE;
	extern HMODULE LoadLibraryA(const char* szPath);
	extern void* GetProcAddress(HMODULE, const char* szPath);

	extern int64_t GetModuleFileNameA(
		HMODULE hModule,
		char*   lpFilename,
		int64_t  nSize
	);
}

static std::filesystem::path GetExecutablePath()
{
	char szName[512];
	GetModuleFileNameA(NULL, szName, sizeof(szName) / sizeof(szName[0]));
	return szName;
}

std::string LocateLv2Plugin(const char* pluginName)
{
	std::filesystem::path path = GetExecutablePath().parent_path();

	std::filesystem::path dllPath = path / pluginName;

	if (std::filesystem::exists(dllPath))
	{
		return dllPath.string();
	}
	dllPath = path / (std::string(pluginName) + ".dll");
	if (std::filesystem::exists(dllPath))
	{
		return dllPath.string();
	}
	dllPath = path.parent_path() / pluginName / (std::string(pluginName) + ".dll");
	if (std::filesystem::exists(dllPath))
	{
		return dllPath.string();
	}

	throw Lv2Exception("Plugin not found.");

}

FN_LV2_ENTRY* LoadLv2Plugin(const char* name)
{
	HMODULE hModule;



	hModule = LoadLibraryA(name);
	if (hModule == NULL)
	{
		throw "Can't load library.";
	}

	void* pfn = GetProcAddress(hModule,"lv2_descriptor");
	if (pfn == NULL)
	{
		throw "Can't get proc address.";
	}
	return (FN_LV2_ENTRY*)pfn;

}
#else
// linux implementation
#include <unistd.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <dlfcn.h>

static std::filesystem::path GetExecutablePath()
{
	char dest[PATH_MAX +1];
	memset(dest, 0, sizeof(dest)); // readlink does not null terminate!
	if (readlink("/proc/self/exe", dest, PATH_MAX) == -1) {
		perror("readlink");
	}
	return dest;
}

std::string LocateLv2Plugin(const char* pluginName)
{
	std::string libName = "lib" + std::string(pluginName) + ".so";

	std::filesystem::path resourcePath = GetExecutablePath().parent_path();

	std::filesystem::path libPath = resourcePath / libName;

	if (std::filesystem::exists(libPath))
	{
		return libPath;
	}
	libPath = resourcePath.parent_path() / pluginName / libName;
	if (std::filesystem::exists(libPath))
	{
		return libPath;
	}

	throw Lv2Exception("Plugin not found.");

}

FN_LV2_ENTRY* LoadLv2Plugin(const char* name)
{
	std::string libName(name);

	if (!std::filesystem::exists(name))
	{
		libName = LocateLv2Plugin(name);
	}
	void* handle = dlopen(libName.c_str(), RTLD_LAZY);
	if (handle == NULL)
	{
		std::string dlError(dlerror());
		throw Lv2Exception(("Failed to load library." + dlError).c_str());
	}

	void *pfn = dlsym(handle,"lv2_descriptor");
	if (pfn == NULL)
	{
		throw "Can't get proc address.";
	}
	return (FN_LV2_ENTRY*)pfn;

}
#endif

