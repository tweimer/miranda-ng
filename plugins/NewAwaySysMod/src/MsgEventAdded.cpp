/*
	New Away System - plugin for Miranda IM
	Copyright (C) 2005-2007 Chervov Dmitry
	Copyright (C) 2004-2005 Iksaif Entertainment
	Copyright (C) 2002-2003 Goblineye Entertainment

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "stdafx.h"
#include "Properties.h"

struct
{
	int Status, DisableReplyCtlID, DontShowDialogCtlID;
}
static StatusModeList[] =
{
	{ ID_STATUS_ONLINE, IDC_REPLYDLG_DISABLE_ONL, IDC_MOREOPTDLG_DONTPOPDLG_ONL },
	{ ID_STATUS_AWAY, IDC_REPLYDLG_DISABLE_AWAY, IDC_MOREOPTDLG_DONTPOPDLG_AWAY },
	{ ID_STATUS_NA, IDC_REPLYDLG_DISABLE_NA, IDC_MOREOPTDLG_DONTPOPDLG_NA },
	{ ID_STATUS_OCCUPIED, IDC_REPLYDLG_DISABLE_OCC, IDC_MOREOPTDLG_DONTPOPDLG_OCC },
	{ ID_STATUS_DND, IDC_REPLYDLG_DISABLE_DND, IDC_MOREOPTDLG_DONTPOPDLG_DND },
	{ ID_STATUS_FREECHAT, IDC_REPLYDLG_DISABLE_FFC, IDC_MOREOPTDLG_DONTPOPDLG_FFC },
	{ ID_STATUS_INVISIBLE, IDC_REPLYDLG_DISABLE_INV, IDC_MOREOPTDLG_DONTPOPDLG_INV },
};


class CAutoreplyData
{
public:
	CAutoreplyData(MCONTACT hContact, TCString Reply): hContact(hContact), Reply(Reply) {}

	MCONTACT hContact;
	TCString Reply;
};

// _ad must be allocated using "new CAutoreplyData()"
void __cdecl AutoreplyDelayThread(CAutoreplyData *ad)
{
	Thread_SetName("NewAwaySysMod: AutoreplyDelayThread");

	_ASSERT(ad && ad->hContact && ad->Reply.GetLen());
	char *szProto = Proto_GetBaseAccountName(ad->hContact);
	if (!szProto) {
		_ASSERT(0);
		return;
	}

	T2Utf pszReply(ad->Reply);
	int ReplyLen = (int)mir_strlen(pszReply);
	ProtoChainSend(ad->hContact, PSS_MESSAGE, 0, (LPARAM)pszReply);

	if (g_AutoreplyOptPage.GetDBValueCopy(IDC_REPLYDLG_LOGREPLY)) { // store in the history
		SleepEx(1000, true); // delay before sending the reply, as we need it to be later than the message we're replying to (without this delay, srmm puts the messages in a wrong order)

		DBEVENTINFO dbei = {};
		dbei.eventType = EVENTTYPE_MESSAGE;
		dbei.flags = DBEF_SENT | DBEF_UTF;
		dbei.szModule = szProto;
		dbei.iTimestamp = time(0);
		dbei.cbBlob = ReplyLen;
		dbei.pBlob = pszReply;
		db_event_add(ad->hContact, &dbei);
	}

	delete ad;
}


int IsSRMsgWindowOpen(MCONTACT hContact)
{
	MessageWindowData mwd;
	return !Srmm_GetWindowData(hContact, mwd) && mwd.hwndWindow;
}


#define MAX_REPLY_TIMEDIFF 5 // maximum "age" of an event to remain unfiltered; in seconds
#define MSGWNDOPEN_UNDEFINED (-1)

class CMetacontactEvent
{
public:
	CMetacontactEvent(MCONTACT hMetaContact, uint32_t timestamp, int bMsgWindowIsOpen) : hMetaContact(hMetaContact), timestamp(timestamp), bMsgWindowIsOpen(bMsgWindowIsOpen) {};

	MCONTACT hMetaContact;
	uint32_t timestamp;
	int bMsgWindowIsOpen;
};

TMyArray<CMetacontactEvent> MetacontactEvents;


int MsgEventAdded(WPARAM hContact, LPARAM lParam)
{
	DBEVENTINFO *dbei = (DBEVENTINFO*)lParam;
	if (!hContact)
		return 0;

	if (dbei->flags & DBEF_SENT || (dbei->eventType != EVENTTYPE_MESSAGE && dbei->eventType != EVENTTYPE_FILE))
		return 0;

	if (time(0) - dbei->getUnixtime() > MAX_REPLY_TIMEDIFF) // don't reply to offline messages
		return 0;

	char *szProto = Proto_GetBaseAccountName(hContact);
	if (!szProto)
		return 0;

	uint32_t Flags1 = CallProtoService(szProto, PS_GETCAPS, PFLAGNUM_1, 0);
	if (!(Flags1 & PF1_IMSEND)) // don't reply to protocols that don't support outgoing messages
		return 0;

	int bMsgWindowIsOpen = MSGWNDOPEN_UNDEFINED;
	if (dbei->flags & DBEF_READ) {
		// if it's a subcontact of a metacontact
		MCONTACT hMetaContact = db_mc_getMeta(hContact);
		if (hMetaContact == 0)
			return 0;

		// remove outdated events first
		uint32_t CurTime = time(0);
		int i;
		for (i = MetacontactEvents.GetSize() - 1; i >= 0; i--)
			if (CurTime - MetacontactEvents[i].timestamp > MAX_REPLY_TIMEDIFF)
				MetacontactEvents.RemoveElem(i);

		// we compare only event timestamps, and do not look at the message itself. it's unlikely that there'll be two events from a contact at the same second, so it's a trade-off between speed and reliability
		for (i = MetacontactEvents.GetSize() - 1; i >= 0; i--) {
			if (MetacontactEvents[i].timestamp == dbei->getUnixtime() && MetacontactEvents[i].hMetaContact == hMetaContact) {
				bMsgWindowIsOpen = MetacontactEvents[i].bMsgWindowIsOpen;
				break;
			}
		}
		if (i < 0) {
			_ASSERT(0);
			return 0;
		}
	}

	// ugly workaround for metacontacts, part i; store all metacontacts' events to a temporary array, so we'll be able to get the 'source' protocol when subcontact event happens later. we need the protocol to get its status and per-status settings properly
	if (!mir_strcmp(szProto, META_PROTO)) {
		// remove outdated events first
		uint32_t CurTime = time(0);
		for (int i = MetacontactEvents.GetSize() - 1; i >= 0; i--)
			if (CurTime - MetacontactEvents[i].timestamp > MAX_REPLY_TIMEDIFF)
				MetacontactEvents.RemoveElem(i);

		// add the new event and wait for a subcontact's event
		MetacontactEvents.AddElem(CMetacontactEvent(hContact, dbei->getUnixtime(), IsSRMsgWindowOpen(hContact)));
		return 0;
	}

	int iMode = Proto_GetStatus(szProto);
	int i;
	for (i = _countof(StatusModeList) - 1; i >= 0; i--)
		if (iMode == StatusModeList[i].Status)
			break;

	if (i < 0)
		return 0;

	COptPage AutoreplyOptData(g_AutoreplyOptPage);
	AutoreplyOptData.DBToMem();
	if (dbei->eventType == EVENTTYPE_MESSAGE)
		g_plugin.setWord(hContact, DB_MESSAGECOUNT, g_plugin.getWord(hContact, DB_MESSAGECOUNT, 0) + 1); // increment message counter

	if (AutoreplyOptData.GetValue(StatusModeList[i].DisableReplyCtlID))
		return 0;

	MCONTACT hContactForSettings = hContact; // used to take into account not-on-list contacts when getting contact settings, but at the same time allows to get correct contact info for contacts that are in the DB
	if (hContactForSettings != INVALID_CONTACT_ID && !Contact::OnList(hContactForSettings))
		hContactForSettings = INVALID_CONTACT_ID; // INVALID_HANDLE_VALUE means the contact is not-on-list

	if (!CContactSettings(iMode, hContactForSettings).Autoreply.IncludingParents(szProto) || CContactSettings(iMode, hContactForSettings).Ignore)
		return 0;

	if (AutoreplyOptData.GetValue(IDC_REPLYDLG_ONLYCLOSEDDLGREPLY)) {
		if (bMsgWindowIsOpen && bMsgWindowIsOpen != MSGWNDOPEN_UNDEFINED)
			return 0;

		// we never get here for a metacontact; we did check for metacontact's window earlier, and here we need to check only for subcontact's window
		if (IsSRMsgWindowOpen(hContact))
			return 0;
	}
	if (AutoreplyOptData.GetValue(IDC_REPLYDLG_ONLYIDLEREPLY) && !g_bIsIdle)
		return 0;

	int SendCount = (int)AutoreplyOptData.GetValue(IDC_REPLYDLG_SENDCOUNT);
	if (SendCount != -1 && g_plugin.getByte(hContact, DB_SENDCOUNT, 0) >= SendCount)
		return 0;

	if ((dbei->eventType == EVENTTYPE_MESSAGE && !AutoreplyOptData.GetValue(IDC_REPLYDLG_EVENTMSG)) || 
		 (dbei->eventType == EVENTTYPE_FILE && !AutoreplyOptData.GetValue(IDC_REPLYDLG_EVENTFILE)))
		return 0;

	g_plugin.setByte(hContact, DB_SENDCOUNT, g_plugin.getByte(hContact, DB_SENDCOUNT, 0) + 1);
	GetDynamicStatMsg(hContact); // it updates VarParseData.Message needed for %extratext% in the format
	TCString Reply(*(TCString*)AutoreplyOptData.GetValue(IDC_REPLYDLG_PREFIX));
	if (Reply != nullptr && ServiceExists(MS_VARS_FORMATSTRING) && !g_SetAwayMsgPage.GetDBValueCopy(IDS_SAWAYMSG_DISABLEVARIABLES)) {
		wchar_t *szResult = variables_parse(Reply, VarParseData.Message, hContact);
		if (szResult != nullptr) {
			Reply = szResult;
			mir_free(szResult);
		}
	}
	
	if (Reply.GetLen()) {
		CAutoreplyData *ad = new CAutoreplyData(hContact, Reply);
		mir_forkThread<CAutoreplyData>(AutoreplyDelayThread, ad);
	}
	return 0;
}
