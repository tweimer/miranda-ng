/*

IEView Plugin for Miranda IM
Copyright (C) 2005-2010  Piotr Piastucki

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "stdafx.h"

// tabsrmm stuff

#define MWF_LOG_SHOWNICK 512
#define MWF_LOG_SHOWTIME 1024
#define MWF_LOG_SHOWSECONDS 2048
#define MWF_LOG_SHOWDATES 4096
#define MWF_LOG_NEWLINE 8192
#define MWF_LOG_INDENT 16384
#define MWF_LOG_RTL 32768
#define MWF_LOG_UNDERLINE 65536
#define MWF_LOG_SWAPNICK 131072
#define MWF_LOG_SHOWICONS 262144

#define MWF_LOG_INDENTWITHTABS 1048576
#define MWF_LOG_SYMBOLS 0x200000
#define MWF_LOG_TEXTFORMAT 0x2000000
#define MWF_LOG_GRID 0x4000000
#define MWF_LOG_INDIVIDUALBKG 0x8000000

#define MWF_DIVIDERWANTED 0x40000000
#define MWF_LOG_GROUPMODE 0x80000000
#define MWF_LOG_LONGDATES 64
#define MWF_LOG_USERELATIVEDATES 1

#define MWF_SHOW_URLEVENTS 1
#define MWF_SHOW_FILEEVENTS 2
#define MWF_SHOW_INOUTICONS 4
#define MWF_SHOW_EMPTYLINEFIX 8
#define MWF_SHOW_MICROLF 16
#define MWF_SHOW_MARKFOLLOWUPTS 32

#define SRMSGMOD_T "Tab_SRMsg"
#define TABSRMM_FONTMODULE "TabSRMM_Fonts"

#define SRMSGSET_SHOWURLS          "ShowURLs"
#define SRMSGSET_SHOWFILES         "ShowFiles"
#define SRMSGSET_SHOWSTATUSCHANGES "ShowFiles"

#define MWF_LOG_DEFAULT (MWF_LOG_SHOWTIME | MWF_LOG_SHOWNICK | MWF_LOG_SHOWDATES)

#define FONTF_BOLD   1
#define FONTF_ITALIC 2
#define FONTF_UNDERLINE 4

#define FONT_NUM 19

static const char *classNames[] = {
	".messageOut", ".miscOut", ".messageIn", ".miscIn", ".nameOut", ".timeOut", ".nameIn", ".timeIn",
	".hMessageOut", ".hMiscOut", ".hMessageIn", ".hMiscIn", ".hNameOut", ".hTimeOut", ".hNameIn", ".hTimeIn",
	".inputArea", ".statusChange", ".dividers"
};

TabSRMMHTMLBuilder::TabSRMMHTMLBuilder()
{
	setLastEventType(-1);
	setLastEventTime(time(0));
	lastEventTime = time(0);
	startedTime = time(0);
}

bool TabSRMMHTMLBuilder::isDbEventShown(uint32_t dwFlags, const DB::EventInfo &dbei)
{
	switch (dbei.eventType) {
	case EVENTTYPE_MESSAGE:
		return 1;
	case EVENTTYPE_FILE:
		if (dwFlags & MWF_SHOW_FILEEVENTS) return 1;
		break;

	case EVENTTYPE_CONTACTS:
	case EVENTTYPE_ADDED:
	case EVENTTYPE_AUTHREQUEST:
		return 0;
	default:
		return dbei.isSrmm();
	}
	return 1;
}

bool TabSRMMHTMLBuilder::isDbEventShown(const DB::EventInfo &dbei)
{
	uint32_t dwFlags2 = db_get_b(0, SRMSGMOD_T, SRMSGSET_SHOWURLS, 0) ? MWF_SHOW_URLEVENTS : 0;
	dwFlags2 |= db_get_b(0, SRMSGMOD_T, SRMSGSET_SHOWFILES, 0) ? MWF_SHOW_FILEEVENTS : 0;
	return isDbEventShown(dwFlags2, dbei);
}

void TabSRMMHTMLBuilder::loadMsgDlgFont(int i, LOGFONTA * lf, COLORREF * colour)
{
	char str[32];
	int style;
	DBVARIANT dbv;
	if (colour) {
		mir_snprintf(str, "Font%dCol", i);
		*colour = db_get_dw(0, TABSRMM_FONTMODULE, str, 0x000000);
	}
	if (lf) {
		HDC hdc = GetDC(nullptr);
		mir_snprintf(str, "Font%dSize", i);
		lf->lfHeight = (char)db_get_b(0, TABSRMM_FONTMODULE, str, 10);
		ReleaseDC(nullptr, hdc);

		lf->lfWidth = 0;
		lf->lfEscapement = 0;
		lf->lfOrientation = 0;
		mir_snprintf(str, "Font%dSty", i);
		style = db_get_b(0, TABSRMM_FONTMODULE, str, 0);
		lf->lfWeight = style & FONTF_BOLD ? FW_BOLD : FW_NORMAL;
		lf->lfItalic = style & FONTF_ITALIC ? 1 : 0;
		lf->lfUnderline = style & FONTF_UNDERLINE ? 1 : 0;
		lf->lfStrikeOut = 0;
		mir_snprintf(str, "Font%dSet", i);
		lf->lfCharSet = db_get_b(0, TABSRMM_FONTMODULE, str, DEFAULT_CHARSET);
		lf->lfOutPrecision = OUT_DEFAULT_PRECIS;
		lf->lfClipPrecision = CLIP_DEFAULT_PRECIS;
		lf->lfQuality = DEFAULT_QUALITY;
		lf->lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
		mir_snprintf(str, "Font%d", i);
		if (db_get_s(0, TABSRMM_FONTMODULE, str, &dbv))
			strncpy_s(lf->lfFaceName, "Verdana", _TRUNCATE);
		else {
			strncpy_s(lf->lfFaceName, dbv.pszVal, _TRUNCATE);
			db_free(&dbv);
		}
	}
}

char* TabSRMMHTMLBuilder::timestampToString(uint32_t dwFlags, time_t check, int isGroupBreak)
{
	static char szResult[512];
	const char *szFormat;
	if (!isGroupBreak || !(dwFlags & MWF_LOG_SHOWDATES)) {
		szFormat = (dwFlags & MWF_LOG_SHOWSECONDS) ? "s" : "t";
		szResult[0] = '\0';
	}
	else {
		time_t now = time(0);
		struct tm tm_now = *localtime(&now);
		struct tm tm_today = tm_now;
		tm_today.tm_hour = tm_today.tm_min = tm_today.tm_sec = 0;
		time_t today = mktime(&tm_today);

		if (dwFlags & MWF_LOG_USERELATIVEDATES && check >= today) {
			szFormat = (dwFlags & MWF_LOG_SHOWSECONDS) ? "s" : "t";
			strncpy_s(szResult, Translate("Today"), _TRUNCATE);
			mir_strcat(szResult, ", ");
		}
		else if (dwFlags & MWF_LOG_USERELATIVEDATES && check > (today - 86400)) {
			szFormat = (dwFlags & MWF_LOG_SHOWSECONDS) ? "s" : "t";
			strncpy_s(szResult, Translate("Yesterday"), _TRUNCATE);
			mir_strcat(szResult, ", ");
		}
		else {
			if (dwFlags & MWF_LOG_LONGDATES)
				szFormat = (dwFlags & MWF_LOG_SHOWSECONDS) ? "D s" : "D t";
			else
				szFormat = (dwFlags & MWF_LOG_SHOWSECONDS) ? "d s" : "d t";
			szResult[0] = '\0';
		}
	}

	char str[80];
	TimeZone_ToString(check, szFormat, str, _countof(str));
	mir_strncat(szResult, str, _countof(szResult) - mir_strlen(szResult));
	mir_strncpy(szResult, ptrA(mir_utf8encode(szResult)), 500);
	return szResult;
}

void TabSRMMHTMLBuilder::buildHead(IEView *view, IEVIEWEVENT *event)
{
	LOGFONTA lf;
	COLORREF color;

	ProtocolSettings *protoSettings = getSRMMProtocolSettings(event->hContact);
	if (protoSettings == nullptr)
		return;

	if (protoSettings->getSRMMMode() == Options::MODE_TEMPLATE) {
		buildHeadTemplate(view, event, protoSettings);
		return;
	}

	CMStringA str;
	if (protoSettings->getSRMMMode() == Options::MODE_CSS) {
		auto *externalCSS = protoSettings->getSRMMCssFilename();
		if (wcsncmp(externalCSS, L"http://", 7))
			str.AppendFormat("<html><head><link rel=\"stylesheet\" href=\"file://%S\"/></head><body class=\"body\">\n", externalCSS);
		else
			str.AppendFormat("<html><head><link rel=\"stylesheet\" href=\"%S\"/></head><body class=\"body\">\n", externalCSS);
	}
	else {
		HDC hdc = GetDC(nullptr);
		int logPixelSY = GetDeviceCaps(hdc, LOGPIXELSY);
		ReleaseDC(nullptr, hdc);
		uint32_t dwFlags = db_get_dw(0, SRMSGMOD_T, "mwflags", MWF_LOG_DEFAULT);
		str.Append("<html><head><style type=\"text/css\">\n");
		COLORREF inColor, outColor;
		COLORREF bkgColor = db_get_dw(0, TABSRMM_FONTMODULE, "BkgColour", 0xFFFFFF);
		bkgColor = (((bkgColor & 0xFF) << 16) | (bkgColor & 0xFF00) | ((bkgColor & 0xFF0000) >> 16));
		COLORREF gridColor = db_get_dw(0, TABSRMM_FONTMODULE, "hgrid", 0xFFFFFF);
		gridColor = (((gridColor & 0xFF) << 16) | (gridColor & 0xFF00) | ((gridColor & 0xFF0000) >> 16));
		if (dwFlags & MWF_LOG_INDIVIDUALBKG) {
			inColor = db_get_dw(0, TABSRMM_FONTMODULE, "inbg", RGB(224, 224, 224));
			outColor = db_get_dw(0, TABSRMM_FONTMODULE, "outbg", RGB(224, 224, 224));
			inColor = (((inColor & 0xFF) << 16) | (inColor & 0xFF00) | ((inColor & 0xFF0000) >> 16));
			outColor = (((outColor & 0xFF) << 16) | (outColor & 0xFF00) | ((outColor & 0xFF0000) >> 16));
		}
		else inColor = outColor = bkgColor;

		if (protoSettings->getSRMMFlags() & Options::LOG_IMAGE_ENABLED)
			str.AppendFormat(".body {margin: 0px; text-align: left; background-attachment: %s; background-color: #%06X;  background-image: url('%s'); overflow: auto;}\n",
				protoSettings->getSRMMFlags() & Options::LOG_IMAGE_SCROLL ? "scroll" : "fixed", bkgColor, protoSettings->getSRMMBackgroundFilename());
		else
			str.AppendFormat(".body {margin: 0px; text-align: left; background-color: #%06X; overflow: auto;}\n", bkgColor);

		str.Append(".link {color: #0000FF; text-decoration: underline;}\n");
		str.Append(".img {vertical-align: middle;}\n");
		if (protoSettings->getSRMMFlags() & Options::LOG_IMAGE_ENABLED) {
			str.Append(".divIn {padding-left: 2px; padding-right: 2px; word-wrap: break-word;}\n");
			str.Append(".divOut {padding-left: 2px; padding-right: 2px; word-wrap: break-word;}\n");
			str.AppendFormat(".divInGrid {padding-left: 2px; padding-right: 2px; word-wrap: break-word; border-top: 1px solid #%06X}\n", gridColor);
			str.AppendFormat(".divOutGrid {padding-left: 2px; padding-right: 2px; word-wrap: break-word; border-top: 1px solid #%06X}\n", gridColor);
			str.Append(".divInRTL {text-align: right; direction:RTL; unicode-bidi:embed; padding-left: 2px; padding-right: 2px; word-wrap: break-word;}\n");
			str.Append(".divOutRTL {text-align: right; direction:RTL; unicode-bidi:embed; padding-left: 2px; padding-right: 2px; word-wrap: break-word;}\n");
			str.AppendFormat(".divInGridRTL {text-align: right; direction:RTL; unicode-bidi:embed; padding-left: 2px; padding-right: 2px; word-wrap: break-word; border-top: 1px solid #%06X}\n", gridColor);
			str.AppendFormat(".divOutGridRTL {text-align: right; direction:RTL; unicode-bidi:embed; padding-left: 2px; padding-right: 2px; word-wrap: break-word; border-top: 1px solid #%06X}\n", gridColor);
		}
		else {
			str.AppendFormat(".divIn {padding-left: 2px; padding-right: 2px; word-wrap: break-word; background-color: #%06X;}\n", inColor);
			str.AppendFormat(".divOut {padding-left: 2px; padding-right: 2px; word-wrap: break-word; background-color: #%06X;}\n", outColor);
			str.AppendFormat(".divInGrid {padding-left: 2px; padding-right: 2px; word-wrap: break-word; border-top: 1px solid #%06X; background-color: #%06X;}\n", gridColor, inColor);
			str.AppendFormat(".divOutGrid {padding-left: 2px; padding-right: 2px; word-wrap: break-word; border-top: 1px solid #%06X; background-color: #%06X;}\n", gridColor, outColor);
			str.AppendFormat(".divInRTL {text-align: right; direction:RTL; unicode-bidi:embed; padding-left: 2px; padding-right: 2px; word-wrap: break-word; background-color: #%06X;}\n", inColor);
			str.AppendFormat(".divOutRTL {text-align: right; direction:RTL; unicode-bidi:embed; padding-left: 2px; padding-right: 2px; word-wrap: break-word; background-color: #%06X;}\n", outColor);
			str.AppendFormat(".divInGridRTL {text-align: right; direction:RTL; unicode-bidi:embed; padding-left: 2px; padding-right: 2px; word-wrap: break-word; border-top: 1px solid #%06X; background-color: #%06X;}\n", gridColor, inColor);
			str.AppendFormat(".divOutGridRTL {text-align: right; direction:RTL; unicode-bidi:embed; padding-left: 2px; padding-right: 2px; word-wrap: break-word; border-top: 1px solid #%06X; background-color: #%06X;}\n", gridColor, outColor);
		}

		for (int i = 0; i < FONT_NUM; i++) {
			loadMsgDlgFont(i, &lf, &color);
			str.AppendFormat("%s {font-family: %s; font-size: %dpt; font-weight: %s; color: #%06X; %s }\n",
				classNames[i],
				lf.lfFaceName,
				abs((signed char)lf.lfHeight) * 74 / logPixelSY,
				lf.lfWeight >= FW_BOLD ? "bold" : "normal",
				(int)(((color & 0xFF) << 16) | (color & 0xFF00) | ((color & 0xFF0000) >> 16)),
				lf.lfItalic ? "font-style: italic;" : "");
		}
		str.Append("</style></head><body class=\"body\">\n");
	}

	if (!str.IsEmpty())
		view->write(str);

	setLastEventType(-1);
}

time_t TabSRMMHTMLBuilder::getStartedTime()
{
	return startedTime;
}

void TabSRMMHTMLBuilder::appendEventNonTemplate(IEView *view, IEVIEWEVENT *event)
{
	uint32_t today = (uint32_t)time(0);
	today = today - today % 86400;
	uint32_t dwFlags = db_get_dw(0, SRMSGMOD_T, "mwflags", MWF_LOG_DEFAULT);
	uint32_t dwFlags2 = db_get_b(0, SRMSGMOD_T, SRMSGSET_SHOWURLS, 0) ? MWF_SHOW_URLEVENTS : 0;
	dwFlags2 |= db_get_b(0, SRMSGMOD_T, SRMSGSET_SHOWFILES, 0) ? MWF_SHOW_FILEEVENTS : 0;
	dwFlags2 |= db_get_b(0, SRMSGMOD_T, "in_out_icons", 0) ? MWF_SHOW_INOUTICONS : 0;
	dwFlags2 |= db_get_b(0, SRMSGMOD_T, "emptylinefix", 1) ? MWF_SHOW_EMPTYLINEFIX : 0;
	dwFlags2 |= MWF_SHOW_MICROLF;
	dwFlags2 |= db_get_b(0, SRMSGMOD_T, "followupts", 1) ? MWF_SHOW_MARKFOLLOWUPTS : 0;

	IEVIEWEVENTDATA* eventData = event->eventData;
	for (int eventIdx = 0; eventData != nullptr && (eventIdx < event->count || event->count == -1); eventData = eventData->next, eventIdx++) {
		if (eventData->iType == IEED_EVENT_MESSAGE || eventData->iType == IEED_EVENT_FILE || eventData->iType == IEED_EVENT_STATUSCHANGE) {
			bool isGroupBreak = true;
			bool isSent = (eventData->dwFlags & IEEDF_SENT) != 0;
			bool isRTL = (eventData->dwFlags & IEEDF_RTL) != 0;
			int isHistory = (eventData->time < (uint32_t)getStartedTime() && (eventData->dwFlags & IEEDF_READ || eventData->dwFlags & IEEDF_SENT));
			if (dwFlags & MWF_LOG_GROUPMODE && eventData->dwFlags == LOWORD(getLastEventType()) &&
				eventData->iType == IEED_EVENT_MESSAGE && HIWORD(getLastEventType()) == IEED_EVENT_MESSAGE &&
				((eventData->time < today) == (getLastEventTime() < today)) &&
				(((eventData->time < (uint32_t)startedTime) == (getLastEventTime() < (uint32_t)startedTime)) || !(eventData->dwFlags & IEEDF_READ)))
			{
				isGroupBreak = FALSE;
			}

			ptrA szName, szText;
			if (eventData->dwFlags & IEEDF_UNICODE_NICK)
				szName = encodeUTF8(event->hContact, eventData->szNick.w, ENF_NAMESMILEYS, true);
			else
				szName = encodeUTF8(event->hContact, eventData->szNick.a, ENF_NAMESMILEYS, true);

			if (eventData->dwFlags & IEEDF_UNICODE_TEXT)
				szText = encodeUTF8(event->hContact, eventData->szText.w, ENF_ALL, isSent);
			else
				szText = encodeUTF8(event->hContact, eventData->szText.a, event->codepage, ENF_ALL, isSent);

			/* TabSRMM-specific formatting */
			CMStringA str;
			if ((dwFlags & MWF_LOG_GRID) && isGroupBreak && getLastEventType() != -1)
				str.AppendFormat("<div class=\"%s\">", isRTL ? isSent ? "divOutGridRTL" : "divInGridRTL" : isSent ? "divOutGrid" : "divInGrid");
			else
				str.AppendFormat("<div class=\"%s\">", isRTL ? isSent ? "divOutRTL" : "divInRTL" : isSent ? "divOut" : "divIn");

			if (dwFlags & MWF_LOG_SHOWICONS && isGroupBreak) {
				const char *iconFile = "";
				if (eventData->iType == IEED_EVENT_MESSAGE) {
					if (dwFlags2 & MWF_SHOW_INOUTICONS) iconFile = isSent ? "message_out.gif" : "message_in.gif";
					else iconFile = "message.gif";
				}
				else if (eventData->iType == IEED_EVENT_FILE)
					iconFile = "file.gif";
				else if (eventData->iType == IEED_EVENT_STATUSCHANGE)
					iconFile = "status.gif";

				Utils::appendIcon(str, iconFile);
			}
			if ((dwFlags & MWF_LOG_SWAPNICK) && (dwFlags & MWF_LOG_SHOWNICK) && isGroupBreak && (eventData->iType != IEED_EVENT_STATUSCHANGE)) {
				const char *className;
				if (!isHistory)
					className = isSent ? "nameOut" : "nameIn";
				else
					className = isSent ? "hNameOut" : "hNameIn";
				if (dwFlags & MWF_LOG_UNDERLINE)
					str.AppendFormat("<span class=\"%s\"><u>%s%s</span>", className, szName.get(), (dwFlags & MWF_LOG_SHOWTIME) ? " </u>" : "</u>: ");
				else
					str.AppendFormat("<span class=\"%s\">%s%s</span>", className, szName.get(), (dwFlags & MWF_LOG_SHOWTIME) ? " " : ": ");
			}
			if (dwFlags & MWF_LOG_SHOWTIME && (isGroupBreak || dwFlags2 & MWF_SHOW_MARKFOLLOWUPTS)) {
				const char *className;
				if (!isHistory)
					className = isSent ? "timeOut" : "timeIn";
				else
					className = isSent ? "hTimeOut" : "hTimeIn";
				if (dwFlags & MWF_LOG_UNDERLINE)
					str.AppendFormat("<span class=\"%s\"><u>%s%s</span>", className, timestampToString(dwFlags, eventData->time, isGroupBreak),
						(!isGroupBreak || (eventData->iType == IEED_EVENT_STATUSCHANGE) || (dwFlags & MWF_LOG_SWAPNICK) || !(dwFlags & MWF_LOG_SHOWNICK)) ? "</u>: " : " </u>");
				else
					str.AppendFormat("<span class=\"%s\">%s%s</span>", className, timestampToString(dwFlags, eventData->time, isGroupBreak),
						(!isGroupBreak || (eventData->iType == IEED_EVENT_STATUSCHANGE) || (dwFlags & MWF_LOG_SWAPNICK) || !(dwFlags & MWF_LOG_SHOWNICK)) ? ": " : " ");
			}
			if ((eventData->iType == IEED_EVENT_STATUSCHANGE) || ((dwFlags & MWF_LOG_SHOWNICK) && !(dwFlags & MWF_LOG_SWAPNICK) && isGroupBreak)) {
				if (eventData->iType == IEED_EVENT_STATUSCHANGE)
					str.AppendFormat("<span class=\"statusChange\">%s </span>", szName.get());
				else {
					const char *className = "";
					if (!isHistory)
						className = isSent ? "nameOut" : "nameIn";
					else
						className = isSent ? "hNameOut" : "hNameIn";
					if (dwFlags & MWF_LOG_UNDERLINE)
						str.AppendFormat("<span class=\"%s\"><u>%s</u>: </span>", className, szName.get());
					else
						str.AppendFormat("<span class=\"%s\">%s: </span>", className, szName.get());
				}
			}
			if (dwFlags & MWF_LOG_NEWLINE && eventData->iType != IEED_EVENT_STATUSCHANGE && eventData->iType != IEED_EVENT_ERRMSG && isGroupBreak)
				str.Append("<br>");

			const char *className = "";
			if (eventData->iType == IEED_EVENT_MESSAGE) {
				if (!isHistory) className = isSent ? "messageOut" : "messageIn";
				else className = isSent ? "hMessageOut" : "hMessageIn";
			}
			else if (eventData->iType == IEED_EVENT_FILE)
				className = isHistory ? "hMiscIn" : "miscIn";
			else if (eventData->iType == IEED_EVENT_STATUSCHANGE)
				className = "statusChange";

			str.AppendFormat("<span class=\"%s\">%s</span>", className, szText.get());
			str.Append("</div>\n");
			setLastEventType(MAKELONG(eventData->dwFlags, eventData->iType));
			setLastEventTime(eventData->time);

			view->write(str);
		}
	}

	view->documentClose();
}

void TabSRMMHTMLBuilder::appendEvent(IEView *view, IEVIEWEVENT *event)
{
	ProtocolSettings *protoSettings = getSRMMProtocolSettings(event->hContact);
	if (protoSettings == nullptr)
		return;

	if (protoSettings->getSRMMMode() == Options::MODE_TEMPLATE)
		appendEventTemplate(view, event, protoSettings);
	else
		appendEventNonTemplate(view, event);
}
