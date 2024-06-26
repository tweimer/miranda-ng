
#ifndef __YAMN_H
#define __YAMN_H

#define VC_EXTRALEAN

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>

#include <newpluginapi.h>
#include <m_clistint.h>
#include <m_contacts.h>
#include <m_database.h>
#include <m_hotkeys.h>
#include <m_icolib.h>
#include <m_langpack.h>
#include <m_netlib.h>
#include <m_options.h>
#include <m_popup.h>
#include <m_protosvc.h>
#include <m_skin.h>
#include <m_timezones.h>

#include <m_toptoolbar.h>
#include <m_kbdnotify.h>
#include <m_folders.h>

#include "mails/decode.h"
#include "mails/mails.h"

#include "account.h"
#include "messages.h"
#include "protoplugin.h"

#include "main.h"
#include "yamn.h"
#include "debug.h"

#include "resource.h"
#include "version.h"

#include "browser/browser.h"

#include "proto/netlib.h"
#include "proto/pop3/pop3.h"
#include "proto/pop3/pop3comm.h"

struct CMPlugin : public PLUGIN<CMPlugin>
{
	CMPlugin();

	CMOption<bool> bForceCheck;
	__forceinline bool CheckFlags() {
		return bForceCheck;
	}

	int Load() override;
	int Unload() override;
};
 
// From services.cpp
void CreateServiceFunctions(void);
void HookEvents(void);

INT_PTR ClistContactDoubleclicked(WPARAM wParam, LPARAM lParam);

extern mir_cs PluginRegCS;
extern SCOUNTER *AccountWriterSO;
extern HANDLE ExitEV;
extern HANDLE WriteToFileEV;

// From debug.cpp
#ifdef _DEBUG
void InitDebug();
void UnInitDebug();
#endif

// From yamn.cpp

// Function every seconds decrements account counter of seconds and checks if they are 0
// If yes, creates a POP3 thread to check account
void CALLBACK TimerProc(HWND, UINT, UINT_PTR, DWORD);

// Function called to check all accounts immidialtelly
// no params
INT_PTR ForceCheckSvc(WPARAM, LPARAM);

// From account.cpp
int InitAccount(CAccount *Which);
void DeInitAccount(CAccount *Which);
void StopSignalFcn(CAccount *Which);
void CodeDecodeString(char *Dest, BOOL Encrypt);
uint32_t FileToMemory(wchar_t *FileName, char **MemFile, char **End);

uint32_t  AddAccountsFromFile(YAMN_PROTOPLUGIN *Plugin, const wchar_t *pwszFilename);
CAccount *CreatePluginAccount(YAMN_PROTOPLUGIN *Plugin);
int       DeleteAccount(YAMN_PROTOPLUGIN *Plugin, CAccount *Which);
void      DeletePluginAccount(CAccount *OldAccount);
CAccount *FindAccountByContact(YAMN_PROTOPLUGIN *Plugin, MCONTACT hContact);
CAccount *FindAccountByName(YAMN_PROTOPLUGIN *Plugin, const char *SearchedAccount);
CAccount *GetNextFreeAccount(YAMN_PROTOPLUGIN *Plugin);
uint32_t  WriteAccountsToFile(YAMN_PROTOPLUGIN *Plugin, const wchar_t *pwszFilename);


uint32_t ReadStringFromMemory(char **Parser, char *End, char **StoreTo);
uint32_t ReadMessagesFromMemory(CAccount *Which, char **Parser, char *End);

uint32_t WriteStringToFile(HANDLE File, char *Source);
uint32_t WriteStringToFileW(HANDLE File, wchar_t *Source);

DWORD WriteMessagesToFile(HANDLE File, CAccount *Which);
DWORD MIR_CDECL WritePOP3Accounts();

void __cdecl DeleteAccountInBackground(void *Which);
int StopAccounts(YAMN_PROTOPLUGIN *Plugin);
int WaitForAllAccounts(YAMN_PROTOPLUGIN *Plugin, BOOL GetAccountBrowserAccess = FALSE);
int DeleteAccounts(YAMN_PROTOPLUGIN *Plugin);

void GetStatusFcn(CAccount *Which, wchar_t *Value);
void SetStatusFcn(CAccount *Which, wchar_t *Value);

INT_PTR UnregisterProtoPlugins();

// From mime.cpp
// void WINAPI ExtractHeaderFcn(char *,int,uint16_t,YAMNMAIL *);	// already in MailExported
struct _tcptable
{
	char *NameBase, *NameSub;
	BOOLEAN isValid;
	unsigned short int CP;
};
extern struct _tcptable CodePageNamesAll[]; // in mime/decode.cpp
extern int CPLENALL;
extern struct _tcptable *CodePageNamesSupp; // in mime/decode.cpp
extern int CPLENSUPP;

extern int PosX, PosY, SizeX, SizeY;
extern int HeadPosX, HeadPosY, HeadSizeX, HeadSizeY, HeadSplitPos;

// From pop3comm.cpp
int RegisterPOP3Plugin(WPARAM, LPARAM);

// From mailbrowser.cpp
void RunMailBrowser(YAMN_MAILBROWSERPARAM *Param);

// From badconnect.cpp
void RunBadConnection(CAccount *acc, UINT_PTR iErrorCode, void *pUserInfo);

// From YAMNopts.cpp
int YAMNOptInitSvc(WPARAM, LPARAM);

// From main.cpp
int PostLoad(WPARAM, LPARAM);    // Executed after all plugins loaded YAMN reads mails from file and notify every protocol it should set its functions
int Shutdown(WPARAM, LPARAM);    // Executed before Miranda is going to shutdown

extern wchar_t UserDirectory[];  // e.g. "F:\WINNT\Profiles\UserXYZ"
extern wchar_t ProfileName[];    // e.g. "majvan"
extern SWMRG *AccountBrowserSO;
extern MWindowList MessageWnds;
extern MWindowList NewMailAccountWnd;
extern bool g_bShutdown;

extern HANDLE hNewMailHook;
extern HCURSOR hCurSplitNS, hCurSplitWE;
extern UINT SecTimer;

// From synchro.cpp
void DeleteMessagesToEndFcn(CAccount *Account, YAMNMAIL *From);

// From mails.cpp
void DeleteMessageFromQueueFcn(YAMNMAIL **From, YAMNMAIL *Which, int mode);
void SetRemoveFlagsInQueueFcn(YAMNMAIL *From, uint32_t FlagsSet, uint32_t FlagsNotSet, uint32_t FlagsToSet, int mode);

void      AppendQueueFcn(YAMNMAIL *first, YAMNMAIL *second);
YAMNMAIL *CreateNewDeleteQueueFcn(YAMNMAIL *From);
int       DeleteAccountMail(YAMN_PROTOPLUGIN *Plugin, YAMNMAIL *OldMail);
void      DeleteMessageFromQueueFcn(YAMNMAIL **From, YAMNMAIL *Which, int mode = 0);
YAMNMAIL *FindMessageByIDFcn(YAMNMAIL *From, char *ID);
void      SynchroMessagesFcn(CAccount *Account, YAMNMAIL **OldQueue, YAMNMAIL **RemovedOld, YAMNMAIL **NewQueue, YAMNMAIL **RemovedNew);
void      TranslateHeaderFcn(char *stream, int len, struct CMimeItem **head);

// From mime.cpp
void ExtractHeader(struct CMimeItem *items, int &CP, struct CHeader *head);
void ExtractShortHeader(struct CMimeItem *items, struct CShortHeader *head);
void DeleteShortHeaderContent(struct CShortHeader *head);
char *ExtractFromContentType(char *ContentType, char *value);
wchar_t *ParseMultipartBody(char *src, char *bond);

// From account.cpp
extern YAMN_PROTOPLUGIN *POP3Plugin;

// from decode.cpp
int DecodeQuotedPrintable(char *Src, char *Dst, int DstLen, BOOL isQ);

void SkipNonSpaces(char *&p);
void SkipSpaces(char *&p);

// From protoplugin.cpp
extern YAMN_PROTOPLUGINQUEUE *FirstProtoPlugin;

extern char *iconDescs[];
extern char *iconNames[];
extern HIMAGELIST CSImages;

int SetProtocolPluginFcnImportFcn(YAMN_PROTOPLUGIN *Plugin, YAMN_PROTOIMPORTFCN *YAMNFcn, YAMN_MAILIMPORTFCN *YAMNMailFcn);

YAMN_PROTOPLUGIN *RegisterProtocolPlugin(YAMN_PROTOREGISTRATION *Registration);

int GetCharsetFromString(char *input, size_t size);
CMStringW ConvertCodedStringToUnicode(char *stream, uint32_t cp, int mode);
extern PVOID TLSCtx;
extern PVOID SSLCtx;

CMStringW GetFileName(wchar_t *pwszPlugin);

#endif
