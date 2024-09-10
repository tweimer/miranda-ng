#include <Windows.h>

#include "newpluginapi.h"

struct CMPlugin : public PLUGIN<CMPlugin>
{
};

static BOOL WINAPI RawDllMain(HINSTANCE hInstance, uint32_t, LPVOID)
{
	g_plugin.setInst(hInstance);
	return TRUE;
}

extern "C" _pfnCrtInit _pRawDllMain = &RawDllMain;
