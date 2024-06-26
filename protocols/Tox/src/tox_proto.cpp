#include "stdafx.h"

CToxProto::CToxProto(const char* protoName, const wchar_t* userName) :
	PROTO<CToxProto>(protoName, userName),
	m_tox(nullptr),
	m_impl(*this),
	hMessageProcess(1)
{
	InitNetlib();

	m_accountName = mir_wstrdup(userName);
	m_defaultGroup = getWStringA(TOX_SETTINGS_GROUP);
	if (m_defaultGroup == nullptr)
		m_defaultGroup = mir_wstrdup(L"Tox");
	
	setAllContactStatuses(ID_STATUS_OFFLINE);

	// avatars
	CreateDirectoryTreeW(GetAvatarPath());

	CreateProtoService(PS_GETAVATARCAPS, &CToxProto::GetAvatarCaps);
	CreateProtoService(PS_GETAVATARINFO, &CToxProto::GetAvatarInfo);
	CreateProtoService(PS_GETMYAVATAR, &CToxProto::GetMyAvatar);
	CreateProtoService(PS_SETMYAVATAR, &CToxProto::SetMyAvatar);

	// menus
	CreateProtoService(PS_MENU_REQAUTH, &CToxProto::OnRequestAuth);
	CreateProtoService(PS_MENU_GRANTAUTH, &CToxProto::OnGrantAuth);

	// nick
	CreateProtoService(PS_SETMYNICKNAME, &CToxProto::SetMyNickname);

	// events
	CreateServiceFunction(MODULE "/GetEventIcon", &CToxProto::EventGetIcon);

	HookProtoEvent(ME_CLIST_PREBUILDCONTACTMENU, &CToxProto::OnPrebuildContactMenu);
	HookProtoEvent(ME_OPT_INITIALISE, &CToxProto::OnOptionsInit);
	HookProtoEvent(ME_PROTO_ACCLISTCHANGED, &CToxProto::OnAccountRenamed);
}

CToxProto::~CToxProto()
{
}

void CToxProto::OnModulesLoaded()
{
	Clist_GroupCreate(0, m_defaultGroup);

	HookProtoEvent(ME_MSG_PRECREATEEVENT, &CToxProto::OnPreCreateMessage);
	HookProtoEvent(ME_USERINFO_INITIALISE, &CToxProto::OnUserInfoInit);

	InitCustomDbEvents();
}

INT_PTR CToxProto::GetCaps(int type, MCONTACT)
{
	switch (type) {
	case PFLAGNUM_1:
		return PF1_IM | PF1_FILE | PF1_AUTHREQ | PF1_MODEMSG | PF1_EXTSEARCH | PF1_SERVERCLIST;
	case PFLAGNUM_2:
		return PF2_ONLINE | PF2_SHORTAWAY | PF2_LIGHTDND;
	case PFLAGNUM_3:
		return PF2_ONLINE | PF2_SHORTAWAY | PF2_LIGHTDND;
	case PFLAGNUM_4:
		return PF4_SINGLEFILEONLY | PF4_SUPPORTTYPING | PF4_AVATARS | PF4_NOAUTHDENYREASON | PF4_FORCEAUTH;
	case PFLAG_UNIQUEIDTEXT:
		return (INT_PTR)L"Tox ID";
	case PFLAG_MAXLENOFMESSAGE:
		return TOX_MAX_MESSAGE_LENGTH;
	}

	return 0;
}

MCONTACT CToxProto::AddToList(int flags, PROTOSEARCHRESULT *psr)
{
	ptrA myAddress(getStringA(TOX_SETTINGS_ID));
	if (strnicmp(psr->id.a, myAddress, TOX_PUBLIC_KEY_SIZE) == 0) {
		ShowNotification(TranslateT("You cannot add yourself to your contact list"), 0);
		return NULL;
	}
	if (MCONTACT hContact = GetContact(psr->id.a)) {
		ShowNotification(TranslateT("Contact already in your contact list"), 0, hContact);
		return NULL;
	}
	return AddContact(psr->id.a, psr->nick.w, psr->email.w, flags & PALF_TEMPORARY);
}

int CToxProto::Authorize(MEVENT hDbEvent)
{
	MCONTACT hContact = GetContactFromAuthEvent(hDbEvent);
	if (hContact == INVALID_CONTACT_ID)
		return 1;

	return OnGrantAuth(hContact, 0);
}

int CToxProto::AuthRecv(MCONTACT, DB::EventInfo &dbei)
{
	return Proto_AuthRecv(m_szModuleName, dbei);
}

int CToxProto::AuthRequest(MCONTACT hContact, const wchar_t *szMessage)
{
	ptrA reason(mir_utf8encodeW(szMessage));
	return OnRequestAuth(hContact, (LPARAM)reason);
}

HANDLE CToxProto::FileAllow(MCONTACT hContact, HANDLE hTransfer, const wchar_t *tszPath)
{
	return OnFileAllow(m_tox, hContact, hTransfer, tszPath);
}

int CToxProto::FileCancel(MCONTACT hContact, HANDLE hTransfer)
{
	return CancelTransfer(hContact, hTransfer);
}

int CToxProto::FileDeny(MCONTACT hContact, HANDLE hTransfer, const wchar_t*)
{
	return FileCancel(hContact, hTransfer);
}

int CToxProto::FileResume(HANDLE hTransfer, int action, const wchar_t *szFilename)
{
	if (hTransfer == nullptr) // file resume canceled
		return 1;

	return OnFileResume(m_tox, hTransfer, action, szFilename);
}

HANDLE CToxProto::SearchAdvanced(HWND owner)
{
	return OnSearchAdvanced(owner);
}

HWND CToxProto::CreateExtendedSearchUI(HWND owner)
{
	return OnCreateExtendedSearchUI(owner);
}

int CToxProto::SendMsg(MCONTACT hContact, MEVENT, const char *msg)
{
	return OnSendMessage(hContact, msg);
}

HANDLE CToxProto::SendFile(MCONTACT hContact, const wchar_t *msg, wchar_t **ppszFiles)
{
	return OnSendFile(m_tox, hContact, msg, ppszFiles);
}

int CToxProto::SetStatus(int iNewStatus)
{
	if (iNewStatus == m_iDesiredStatus)
		return 0;

	iNewStatus = MapStatus(iNewStatus);

	debugLogA(__FUNCTION__": changing status from %i to %i", m_iStatus, iNewStatus);

	int old_status = m_iStatus;
	m_iDesiredStatus = iNewStatus;

	// logout
	if (iNewStatus == ID_STATUS_OFFLINE) {
		m_impl.timerPoll.Stop();

		if (m_tox) {
			UninitToxCore(m_tox);
			tox_kill(m_tox);
			m_tox = nullptr;
		}

		if (!Miranda_IsTerminated()) {
			setAllContactStatuses(ID_STATUS_OFFLINE);

			m_iStatus = m_iDesiredStatus = ID_STATUS_OFFLINE;
			ProtoBroadcastAck(NULL, ACKTYPE_STATUS, ACKRESULT_SUCCESS, (HANDLE)old_status, m_iStatus);

			UpdateStatusMenu(NULL, NULL);
		}

		return 0;
	}

	if (old_status >= ID_STATUS_CONNECTING && old_status < ID_STATUS_OFFLINE)
		return 0;

	// login
	if (old_status == ID_STATUS_OFFLINE && !IsOnline()) {
		m_iStatus = ID_STATUS_CONNECTING;
		ProtoBroadcastAck(NULL, ACKTYPE_STATUS, ACKRESULT_SUCCESS, (HANDLE)old_status, m_iStatus);

		ForkThread(&CToxProto::InitThread);
		return 0;
	}
	
	// change status
	m_iStatus = iNewStatus;
	tox_self_set_status(m_tox, MirandaToToxStatus(iNewStatus));
	ProtoBroadcastAck(NULL, ACKTYPE_STATUS, ACKRESULT_SUCCESS, (HANDLE)old_status, m_iStatus);
	
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////

HANDLE CToxProto::GetAwayMsg(MCONTACT hContact)
{
	if (IsOnline()) {
		ForkThread(&CToxProto::GetStatusMessageAsync, (void*)hContact);
		return (HANDLE)hContact;
	}

	return nullptr;
}

int CToxProto::SetAwayMsg(int, const wchar_t *msg)
{
	if (IsOnline()) {
		T2Utf statusMessage(msg);
		TOX_ERR_SET_INFO error;
		if (!tox_self_set_status_message(m_tox, (uint8_t*)(char*)statusMessage, min(TOX_MAX_STATUS_MESSAGE_LENGTH, mir_strlen(statusMessage)), &error))
			debugLogA(__FUNCTION__": failed to set status status message %s (%d)", msg, error);
	}

	return 0;
}

int CToxProto::UserIsTyping(MCONTACT hContact, int type)
{
	return OnUserIsTyping(hContact, type);
}
