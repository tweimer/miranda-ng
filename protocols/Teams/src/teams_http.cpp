/*
Copyright (C) 2012-25 Miranda NG team (https://miranda-ng.org)

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation version 2
of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "stdafx.h"

AsyncHttpRequest::AsyncHttpRequest(int type, SkypeHost host, LPCSTR url, MTHttpRequestHandler pFunc) :
	m_host(host)
{
	switch (host) {
	case HOST_API:       m_szUrl = "api.skype.com"; break;
	case HOST_PEOPLE:    m_szUrl = "people.skype.com/v2"; break;
	case HOST_CONTACTS:  m_szUrl = "edge.skype.com/pcs/contacts/v2"; break;
	case HOST_GRAPH:     m_szUrl = "skypegraph.skype.com"; break;
	case HOST_LOGIN:     m_szUrl = "login.microsoftonline.com"; break;
	case HOST_TEAMS:     m_szUrl = "teams.live.com"; break;

	case HOST_DEFAULT:
		AddHeader("MS-IC3-Product", "Sfl");
		m_szUrl = "msgapi.teams.live.com/v1";
		break;
	}

	AddHeader("User-Agent", NETLIB_USER_AGENT);

	if (url)
		m_szUrl.Append(url);
	m_pFunc = pFunc;
	flags = NLHRF_HTTP11 | NLHRF_SSL | NLHRF_DUMPASTEXT;
	requestType = type;
}

void AsyncHttpRequest::AddAuthentication(CTeamsProto *ppro)
{
	AddHeader("Authentication", CMStringA("skypetoken=") + ppro->m_szSkypeToken);
}

void AsyncHttpRequest::AddRegister(CTeamsProto *ppro)
{
	AddHeader("RegistrationToken", CMStringA("registrationToken=") + ppro->m_szToken);
}

/////////////////////////////////////////////////////////////////////////////////////////

void CTeamsProto::StartQueue()
{
	if (!m_isTerminated)
		return;

	m_isTerminated = false;
	if (m_hRequestQueueThread == nullptr)
		ForkThread(&CTeamsProto::WorkerThread);
}

void CTeamsProto::StopQueue()
{
	m_isTerminated = true;

	if (m_hRequestQueueThread)
		m_hRequestQueueEvent.Set();
}

void CTeamsProto::PushRequest(AsyncHttpRequest *request)
{
	if (m_isTerminated)
		return;

	if (request->m_host == HOST_TEAMS) {
		if (!request->FindHeader("Authorization"))
			request->AddHeader("Authorization", "Bearer " + m_szAccessToken);
		request->AddHeader("Accept", "application/json");
	}

	{
		mir_cslock lock(m_requestQueueLock);
		m_requests.insert(request);
	}
	m_hRequestQueueEvent.Set();
}

/////////////////////////////////////////////////////////////////////////////////////////

MHttpResponse* CTeamsProto::DoSend(AsyncHttpRequest *pReq)
{
	if (pReq->m_host != HOST_OTHER)
		pReq->m_szUrl.Insert(0, ((pReq->flags & NLHRF_SSL) ? "https://" : "http://"));

	if (!pReq->m_szParam.IsEmpty()) {
		switch (pReq->requestType) {
		case REQUEST_PUT:
		case REQUEST_POST:
			if (!pReq->FindHeader("Content-Type")) {
				if (pReq->m_szParam[0] == '[' || pReq->m_szParam[0] == '{')
					pReq->AddHeader("Content-Type", "application/json; charset=UTF-8");
				else
					pReq->AddHeader("Content-Type", "application/x-www-form-urlencoded");
			}
		}
	}

	switch (pReq->m_host) {
	case HOST_API:
	case HOST_PEOPLE:
	case HOST_CONTACTS:
		if (m_szSkypeToken)
			pReq->AddHeader("X-Skypetoken", m_szSkypeToken);

		pReq->AddHeader("Accept", "application/json");
		pReq->AddHeader("Origin", "https://web.skype.com");
		pReq->AddHeader("Referer", "https://web.skype.com/");
		break;

	case HOST_GRAPH:
		if (m_szSkypeToken)
			pReq->AddHeader("X-Skypetoken", m_szSkypeToken);
		pReq->AddHeader("Accept", "application/json");
		break;

	case HOST_LOGIN:
		#ifndef _DEBUG
			pReq->flags |= NLHRF_NODUMP;
		#endif
		break;

	case HOST_DEFAULT:
		if (m_szToken)
			pReq->AddRegister(this);
		pReq->AddHeader("Accept", "application/json, text/javascript");
		break;
	}

	debugLogA("Send request to %s", pReq->m_szUrl.c_str());

	return Netlib_HttpTransaction(m_hNetlibUser, pReq);
}

void CTeamsProto::Execute(AsyncHttpRequest *item)
{
	NLHR_PTR response(DoSend(item));
	if (item->m_pFunc != nullptr)
		(this->*item->m_pFunc)(response, item);
	m_requests.remove(item);
	delete item;
}

void CTeamsProto::WorkerThread(void*)
{
	m_hRequestQueueThread = GetCurrentThread();

	while (true) {
		m_hRequestQueueEvent.Wait();
		if (m_isTerminated)
			break;

		while (true) {
			AsyncHttpRequest *item = nullptr;
			{
				mir_cslock lock(m_requestQueueLock);

				if (m_requests.getCount() == 0)
					break;

				item = m_requests[0];
				m_requests.remove(0);
			}
			if (item != nullptr)
				Execute(item);
		}
	}

	m_hRequestQueueThread = nullptr;
}
