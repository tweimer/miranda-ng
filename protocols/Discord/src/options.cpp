/*
Copyright © 2016-22 Miranda NG team

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "stdafx.h"

/////////////////////////////////////////////////////////////////////////////////////////

class CDiscardAccountOptions : public CDiscordDlgBase
{
	ptrW m_wszOldGroup;
	CCtrlEdit m_edGroup, m_edUserName, m_edPassword;
	CCtrlCheck chkUseChats, chkHideChats, chkUseGroups, chkDeleteMsgs, chkDeleteUsers;
	CCtrlButton btnLogout;

public:
	CDiscardAccountOptions(CDiscordProto *ppro, int iDlgID, bool bFullDlg) :
		CDiscordDlgBase(ppro, iDlgID),
		btnLogout(this, IDC_LOGOUT),
		m_edGroup(this, IDC_GROUP),
		m_edUserName(this, IDC_USERNAME),
		m_edPassword(this, IDC_PASSWORD),
		chkUseChats(this, IDC_USEGUILDS),
		chkHideChats(this, IDC_HIDECHATS),
		chkUseGroups(this, IDC_USEGROUPS),
		chkDeleteMsgs(this, IDC_DELETE_MSGS),
		chkDeleteUsers(this, IDC_DELETE_CONTACTS),
		m_wszOldGroup(mir_wstrdup(ppro->m_wszDefaultGroup))
	{
		btnLogout.OnClick = Callback(this, &CDiscardAccountOptions::onClick_Logout);

		CreateLink(m_edGroup, ppro->m_wszDefaultGroup);
		CreateLink(m_edUserName, ppro->m_wszEmail);
		if (bFullDlg) {
			CreateLink(chkUseChats, ppro->m_bUseGroupchats);
			CreateLink(chkHideChats, ppro->m_bHideGroupchats);
			CreateLink(chkUseGroups, ppro->m_bUseGuildGroups);
			CreateLink(chkDeleteMsgs, ppro->m_bSyncDeleteMsgs);
			CreateLink(chkDeleteUsers, ppro->m_bSyncDeleteUsers);

			chkUseChats.OnChange = Callback(this, &CDiscardAccountOptions::onChange_GroupChats);
		}
	}

	bool OnInitDialog() override
	{
		if (m_proto->getMStringA(DB_KEY_TOKEN).IsEmpty())
			btnLogout.Disable();
		else {
			m_edUserName.Disable();
			m_edPassword.Disable();
		}

		ptrW buf(m_proto->getWStringA(DB_KEY_PASSWORD));
		if (buf)
			m_edPassword.SetText(buf);
		return true;
	}

	bool OnApply() override
	{
		if (mir_wstrcmp(m_proto->m_wszDefaultGroup, m_wszOldGroup))
			Clist_GroupCreate(0, m_proto->m_wszDefaultGroup);

		ptrW buf(m_edPassword.GetText());
		m_proto->setWString(DB_KEY_PASSWORD, buf);
		return true;
	}

	void onClick_Logout(CCtrlButton *)
	{
		auto *pReq = new AsyncHttpRequest(m_proto, REQUEST_POST, "/auth/logout", &CDiscordProto::OnReceiveLogout);
		pReq->pUserInfo = this;
		m_proto->Push(pReq);

		CallService(MS_KS_ENABLEPROTOCOL, FALSE, LPARAM(m_proto->m_szModuleName));
	}

	void onLogout()
	{
		m_edUserName.Enable();
		m_edPassword.Enable();
	}

	void onChange_GroupChats(CCtrlCheck*)
	{
		bool bEnabled = chkUseChats.GetState();
		chkHideChats.Enable(bEnabled);
		chkUseGroups.Enable(bEnabled);
	}
};

void CDiscordProto::OnReceiveLogout(MHttpResponse *, AsyncHttpRequest *pReq)
{
	delSetting(DB_KEY_TOKEN);
	m_szAccessToken = 0;
	ShutdownSession();

	auto *pDlg = (CDiscardAccountOptions *)pReq->pUserInfo;
	pDlg->onLogout();
}

/////////////////////////////////////////////////////////////////////////////////////////

MWindow CDiscordProto::OnCreateAccMgrUI(MWindow hwndParent)
{
	CDiscardAccountOptions *pDlg = new CDiscardAccountOptions(this, IDD_OPTIONS_ACCMGR, false);
	pDlg->SetParent(hwndParent);
	pDlg->Create();
	return pDlg->GetHwnd();
}

int CDiscordProto::OnOptionsInit(WPARAM wParam, LPARAM)
{
	OPTIONSDIALOGPAGE odp = {};
	odp.szTitle.w = m_tszUserName;
	odp.flags = ODPF_UNICODE;
	odp.szGroup.w = LPGENW("Network");

	odp.position = 1;
	odp.szTab.w = LPGENW("Account");
	odp.pDialog = new CDiscardAccountOptions(this, IDD_OPTIONS_ACCOUNT, true);
	g_plugin.addOptions(wParam, &odp);
	return 0;
}
