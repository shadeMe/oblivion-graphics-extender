#include <assert.h>

#include "TextureManager.h"
#include "TextureIOHook.hpp"
#include "windows.h"
#include "obse_common/SafeWrite.h"
#include "GlobalSettings.h"

#include "D3D9.hpp"
#include "D3D9Device.hpp"
#include "D3D9Identifiers.hpp"

#include "Hooking/detours/detours.h"
#include "Hooking/D3DX.hpp"

#if	defined(OBGE_LOGGING) || defined(OBGE_DEVLING) || defined(OBGE_GAMMACORRECTION)

std::map<std::string, IDirect3DBaseTexture9 *> textureFiles;
CRITICAL_SECTION textureLock;

/* ------------------------------------------------------------------------------------------------- */

#if	defined(OBGE_LOGGING) || defined(OBGE_DEVLING)
const char *findTexture(IDirect3DBaseTexture9 *tex) {
  static char buf[256];
  buf[0] = '\0';

  /* search loaded texture files */
  std::map<std::string, IDirect3DBaseTexture9 *>::iterator TFile = textureFiles.begin();
  while (TFile != textureFiles.end()) {
    if ((*TFile).second == tex) {
      std::string str = (*TFile).first;
      const char *ptr = str.data(), *ofs;

      if ((ofs = strstr(ptr, "Textures\\")) ||
	  (ofs = strstr(ptr, "textures\\")) ||
	  (ofs = strstr(ptr, "textures/"))) {
	sprintf(buf, "%s", ofs + 9);
      }
      else
	sprintf(buf, "%s", ptr);

      return buf;
    }

    TFile++;
  }

  return "unknown";
}
#endif

/* ------------------------------------------------------------------------------------------------- */

class Anonymous {
public:
	bool TrackLoadTextureFile(char *texture, void *renderer, void *flags);
};

/* 00760DA0 == NiDX9SourceTextureData_LoadTextureFile */

bool (__thiscall Anonymous::* LoadTextureFile)(char *, void *, void *)/* =
	(void * (__stdcall *)(char *))00760DA0*/;
bool (__thiscall Anonymous::* TrackLoadTextureFile)(char *, void *, void *)/* =
	(void * (__stdcall *)(char *))00760DA0*/;

bool Anonymous::TrackLoadTextureFile(char *texture, void *renderer, void *flags) {
	EnterCriticalSection(&textureLock);

	lastOBGEDirect3DBaseTexture9 = NULL;

	bool r = (this->*LoadTextureFile)(texture, renderer, flags);

	if (r && lastOBGEDirect3DBaseTexture9) {
	  textureFiles[texture] = lastOBGEDirect3DBaseTexture9;

#ifdef OBGE_GAMMACORRECTION
	  /* remember DeGamma for this kind of texture */
//	  if (DeGamma) {
	    if (!strchr(texture, '_'))
	    if (!strstr(texture, "Menu") &&
		!strstr(texture, "menu")) {
	      static const bool PotDeGamma = true;

	      lastOBGEDirect3DBaseTexture9->SetPrivateData(GammaGUID, &PotDeGamma, sizeof(PotDeGamma), 0);
//	    }
	  }
#endif
	}

	_DMESSAGE("Texture load: %s (%s)", texture, r ? "success" : "failed");

	LeaveCriticalSection(&textureLock);
	return r;
}

/* ------------------------------------------------------------------------------------------------- */

void CreateTextureIOHook(void) {
	/* GetTextureBinary */
	*((int *)&LoadTextureFile) = 0x00760DA0;
	TrackLoadTextureFile = &Anonymous::TrackLoadTextureFile;

        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
	DetourAttach(&(PVOID&)LoadTextureFile, *((PVOID *)&TrackLoadTextureFile));
        LONG error = DetourTransactionCommit();

        if (error == NO_ERROR) {
		_MESSAGE("Detoured LoadTextureFile(); succeeded");
        }
        else {
		_MESSAGE("Detoured LoadTextureFile(); failed");
        }

	InitializeCriticalSection(&textureLock);

	return;
}

#endif
