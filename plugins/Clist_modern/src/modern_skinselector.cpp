/*

Miranda NG: the free IM client for Microsoft* Windows*

Copyright (C) 2012-25 Miranda NG team (https://miranda-ng.org),
Copyright (c) 2000-08 Miranda ICQ/IM project,
all portions of this codebase are copyrighted to the people
listed in contributors.txt.

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

///// structures and services to manage modern skin objects (mask mechanism)

//#include "windows.h"
#include "stdafx.h"

/// IMPLEMENTATIONS

int SkinSelector_DeleteMask(MODERNMASK *mm)
{
	if (!mm->pl_Params) return 0;
	for (int i = 0; i < (int)mm->dwParamCnt; i++) {
		mir_free(mm->pl_Params[i].szName);
		mir_free(mm->pl_Params[i].szValue);
	}
	mir_free(mm->pl_Params);
	return 1;
}

uint32_t mod_CalcHash(const char *szStr)
{
	uint32_t hash = 0;
	int shift = 0;
	for (int i = 0; szStr[i]; i++) {
		hash ^= szStr[i] << shift;
		if (shift > 24) hash ^= (szStr[i] >> (32 - shift)) & 0x7F;
		shift = (shift + 5) & 0x1F;
	}
	return hash;
}

static int AddModernMaskToList(MODERNMASK *mm, LISTMODERNMASK * mmTemplateList)
{
	if (!mmTemplateList || !mm) return -1;
	mmTemplateList->pl_Masks = (MODERNMASK *)mir_realloc(mmTemplateList->pl_Masks, sizeof(MODERNMASK)*(mmTemplateList->dwMaskCnt + 1));
	memmove(&(mmTemplateList->pl_Masks[mmTemplateList->dwMaskCnt]), mm, sizeof(MODERNMASK));
	mmTemplateList->dwMaskCnt++;
	return mmTemplateList->dwMaskCnt - 1;
}

int ClearMaskList(LISTMODERNMASK * mmTemplateList)
{
	if (!mmTemplateList) return -1;
	if (!mmTemplateList->pl_Masks) return -1;
	for (int i = 0; i < (int)mmTemplateList->dwMaskCnt; i++)
		SkinSelector_DeleteMask(&(mmTemplateList->pl_Masks[i]));
	mir_free_and_nil(mmTemplateList->pl_Masks);
	mmTemplateList->dwMaskCnt = 0;
	return 0;
}

static int DeleteMaskByItID(uint32_t mID, LISTMODERNMASK *mmTemplateList)
{
	if (!mmTemplateList) return -1;
	if (mID >= mmTemplateList->dwMaskCnt) return -1;
	if (mmTemplateList->dwMaskCnt == 1) {
		SkinSelector_DeleteMask(&(mmTemplateList->pl_Masks[0]));
		mir_free_and_nil(mmTemplateList->pl_Masks);
		mmTemplateList->pl_Masks = nullptr;
		mmTemplateList->dwMaskCnt--;
	}
	else {
		SkinSelector_DeleteMask(&(mmTemplateList->pl_Masks[mID]));
		MODERNMASK *newAlocation = (MODERNMASK *)mir_alloc(sizeof(MODERNMASK)*mmTemplateList->dwMaskCnt - 1);
		memcpy(newAlocation, mmTemplateList->pl_Masks, sizeof(MODERNMASK)*(mID + 1));
		for (uint32_t i = mID; i < mmTemplateList->dwMaskCnt - 1; i++) {
			newAlocation[i] = mmTemplateList->pl_Masks[i + 1];
			newAlocation[i].dwMaskId = i;
		}
		mir_free_and_nil(mmTemplateList->pl_Masks);
		mmTemplateList->pl_Masks = newAlocation;
		mmTemplateList->dwMaskCnt--;
	}
	return mmTemplateList->dwMaskCnt;
}


static int ExchangeMasksByID(uint32_t mID1, uint32_t mID2, LISTMODERNMASK * mmTemplateList)
{
	if (!mmTemplateList) return 0;
	if (mID1 >= mmTemplateList->dwMaskCnt) return 0;
	if (mID2 >= mmTemplateList->dwMaskCnt) return 0;
	if (mID1 == mID2) return 0;
	{
		MODERNMASK mm = mmTemplateList->pl_Masks[mID1];
		mmTemplateList->pl_Masks[mID1] = mmTemplateList->pl_Masks[mID2];
		mmTemplateList->pl_Masks[mID2] = mm;
	}
	return 1;
}

int SortMaskList(LISTMODERNMASK * mmList)
{
	uint32_t pos = 1;
	if (mmList->dwMaskCnt < 2) return 0;
	do {
		if (mmList->pl_Masks[pos].dwMaskId < mmList->pl_Masks[pos - 1].dwMaskId) {
			ExchangeMasksByID(pos, pos - 1, mmList);
			pos--;
			if (pos < 1)
				pos = 1;
		}
		else pos++;
	} while (pos < mmList->dwMaskCnt);

	return 1;
}

enum
{
	EXCEPTION_EQUAL,
	EXCEPTION_NOT_EQUAL = 1,
	EXCEPTION_WILD = 2,
};

static BOOL _GetParamValue(char *szText, unsigned int &start, unsigned int length, char* &param, unsigned int& paramlen, char* &value, unsigned int &valuelen, int &except)
{
	char *curChar = szText + start;
	char *lastChar = szText + length;

	enum { STATE_PARAM, STATE_VALUE };
	int state = STATE_PARAM;
	if (start >= length) return FALSE;

	paramlen = 0;
	valuelen = 0;
	value = nullptr;
	param = nullptr;

	except = EXCEPTION_EQUAL;
	param = curChar;


	BOOL exitLoop = false;
	while (!exitLoop) {
		switch (*curChar) {
		case '^':
			if (state == STATE_VALUE) break;
			except |= EXCEPTION_NOT_EQUAL;
			exitLoop = TRUE;
			// fall through
		case '=':
			if (state == STATE_VALUE) break;
			// search value end
			paramlen = curChar - param;
			exitLoop = TRUE;
			break;

		case ',':
		default:
			if (*curChar != ',' && curChar < lastChar) break;
			if (state == STATE_PARAM) {
				// no parameter name only value
				value = param;
				param = nullptr;
				paramlen = 0;
				state = STATE_VALUE;
			}
			exitLoop = TRUE;
			break;
		case '*': case '?':
			if (state == STATE_PARAM) break;
			except |= EXCEPTION_WILD;
			break;
		}
		if (exitLoop) {
			if (state == STATE_PARAM) {
				paramlen = curChar - param;
				state = STATE_VALUE;
				curChar++; //skip Sign
				value = curChar;
				exitLoop = FALSE;
			}
			else if (state == STATE_VALUE) {
				valuelen = curChar - value;
			}
		}
		else {
			curChar++;
		}
	}


	start = curChar - szText + 1;
	// skip spaces
	if (value && valuelen) {
		while (*value == ' ' || *value == '\t') {
			value++;
			valuelen--;
		}
		while (*(value + valuelen - 1) == ' ' || *(value + valuelen - 1) == '\t')
			valuelen--;
	}

	if (param && paramlen) {
		while (*param == ' ' || *param == '\t') {
			param++;
			paramlen--;
		}
		while (*(param + paramlen - 1) == ' ' || *(param + paramlen - 1) == '\t')
			paramlen--;
	}

	return (value || param);
}

static int ParseToModernMask(MODERNMASK *mm, char *szText)
{
	if (!mm || !szText) return -1;

	unsigned int textLen = (unsigned)mir_strlen(szText);
	uint8_t curParam = 0;

	unsigned int startPos = 0;
	char *pszParam;
	char *pszValue;
	unsigned int paramlen;
	unsigned int valuelen;
	int except;

	while (_GetParamValue(szText, startPos, textLen, pszParam, paramlen, pszValue, valuelen, except)) {
		MASKPARAM param = { 0 };
		if (except & EXCEPTION_NOT_EQUAL)
			param.bMaskParamFlag = MPF_NOT_EQUAL;
		else
			param.bMaskParamFlag = MPF_EQUAL;

		// Get param name
		if (pszParam && paramlen) {
			param.szName = mir_strndup(pszParam, paramlen);
			param.dwId = mod_CalcHash(param.szName);
		}
		else { // ParamName = 'Module'
			param.szName = mir_strdup("Module");
			param.dwId = mod_CalcHash(param.szName);
		}

		param.szValue = mir_strndup(pszValue, valuelen);

		if (!(except & EXCEPTION_WILD)) {
			param.dwValueHash = mod_CalcHash(param.szValue);
			param.bMaskParamFlag |= MPF_HASHED;
		}
		if (curParam >= mm->dwParamCnt) {
			mm->pl_Params = (MASKPARAM*)mir_realloc(mm->pl_Params, (mm->dwParamCnt + 1)*sizeof(MASKPARAM));
			mm->dwParamCnt++;
		}
		mm->pl_Params[curParam] = param;
		curParam++;
	}
	return 0;
};

static BOOL CompareModernMask(MODERNMASK *mmValue, MODERNMASK *mmTemplate)
{
	//TODO
	BOOL res = TRUE;
	uint8_t pVal = 0, pTemp = 0;
	while (pTemp < mmTemplate->dwParamCnt && pVal < mmValue->dwParamCnt) {
		// find pTemp parameter in mValue
		uint32_t vh, ph;
		BOOL finded = 0;
		MASKPARAM p = mmTemplate->pl_Params[pTemp];
		ph = p.dwId;
		vh = p.dwValueHash;
		pVal = 0;
		if (p.bMaskParamFlag& MPF_HASHED)  //compare by hash
			while (pVal < mmValue->dwParamCnt && mmValue->pl_Params[pVal].bMaskParamFlag != 0) {
				if (mmValue->pl_Params[pVal].dwId == ph) {
					if (mmValue->pl_Params[pVal].dwValueHash == vh) { finded = 1; break; }
					else { finded = 0; break; }
				}
				pVal++;
			}
		else
			while (mmValue->pl_Params[pVal].bMaskParamFlag != 0) {
				if (mmValue->pl_Params[pVal].dwId == ph) {
					if (wildcmp(mmValue->pl_Params[pVal].szValue, p.szValue)) { finded = 1; break; }
					else { finded = 0; break; }
				}
				pVal++;
			}
		if (!((finded && !(p.bMaskParamFlag&MPF_DIFF)) || (!finded && (p.bMaskParamFlag&MPF_DIFF)))) {
			res = FALSE; break;
		}
		pTemp++;
	}
	return res;
};

BOOL CompareStrWithModernMask(char *szValue, MODERNMASK *mmTemplate)
{
	MODERNMASK mmValue = {};
	if (!ParseToModernMask(&mmValue, szValue)) {
		BOOL res = CompareModernMask(&mmValue, mmTemplate);
		SkinSelector_DeleteMask(&mmValue);
		return res;
	}
	else return 0;
};

// AddingMask
int AddStrModernMaskToList(uint32_t maskID, char *szStr, char *objectName, LISTMODERNMASK *mmTemplateList)
{
	if (!szStr || !mmTemplateList)
		return -1;

	MODERNMASK mm = {};
	if (ParseToModernMask(&mm, szStr))
		return -1;

	mm.bObjectFound = FALSE;
	mm.szObjectName = mir_strdup(objectName);
	//mm.pObject = (void*) ske_FindObjectByName(objectName, OT_ANY, (SKINOBJECTSLIST*) pObjectList);
	mm.dwMaskId = maskID;
	return AddModernMaskToList(&mm, mmTemplateList);
}

SKINOBJECTDESCRIPTOR* skin_FindObjectByMask(MODERNMASK *mm, LISTMODERNMASK *mmTemplateList)
{
	for (uint32_t i = 0; i < mmTemplateList->dwMaskCnt; i++)
		if (CompareModernMask(mm, &(mmTemplateList->pl_Masks[i])))
			return (SKINOBJECTDESCRIPTOR*)mmTemplateList->pl_Masks[i].pObject;

	return nullptr;
}

SKINOBJECTDESCRIPTOR* skin_FindObjectByRequest(char *szValue, LISTMODERNMASK *mmTemplateList)
{
	if (!mmTemplateList)
		mmTemplateList = g_SkinObjectList.pMaskList;

	if (!mmTemplateList)
		return nullptr;

	MODERNMASK mm = {};
	ParseToModernMask(&mm, szValue);
	SKINOBJECTDESCRIPTOR *res = skin_FindObjectByMask(&mm, mmTemplateList);
	SkinSelector_DeleteMask(&mm);
	return res;
}

wchar_t* GetParamNT(char *string, wchar_t *buf, int buflen, uint8_t paramN, char Delim, BOOL SkipSpaces)
{
	char *ansibuf = (char*)mir_alloc(buflen / sizeof(wchar_t));
	GetParamN(string, ansibuf, buflen / sizeof(wchar_t), paramN, Delim, SkipSpaces);
	MultiByteToWideChar(CP_UTF8, 0, ansibuf, -1, buf, buflen);
	mir_free(ansibuf);
	return buf;
}

wchar_t* GetParamN(wchar_t *string, wchar_t *buf, int buflen, uint8_t paramN, wchar_t Delim, BOOL SkipSpaces)
{
	size_t i = 0, start = 0, CurentCount = 0, len;
	while (i < mir_wstrlen(string)) {
		if (string[i] == Delim) {
			if (CurentCount == paramN) break;
			start = i + 1;
			CurentCount++;
		}
		i++;
	}
	if (CurentCount == paramN) {
		if (SkipSpaces) { //remove spaces
			while (string[start] == ' ' && (int)start < mir_wstrlen(string))
				start++;
			while (i > 1 && string[i - 1] == ' ' && i > (int)start)
				i--;
		}
		len = ((int)(i - start) < buflen) ? i - start : buflen;
		mir_wstrncpy(buf, string + start, len);
		buf[len] = '\0';
	}
	else buf[0] = '\0';
	return buf;
}

char* GetParamN(char *string, char *buf, int buflen, uint8_t paramN, char Delim, BOOL SkipSpaces)
{
	size_t i = 0, start = 0, CurentCount = 0, len;
	while (i < mir_strlen(string)) {
		if (string[i] == Delim) {
			if (CurentCount == paramN) break;
			start = i + 1;
			CurentCount++;
		}
		i++;
	}
	if (CurentCount == paramN) {
		if (SkipSpaces) { //remove spaces
			while (string[start] == ' ' && (int)start < mir_strlen(string))
				start++;
			while (i > 1 && string[i - 1] == ' ' && i > (int)start)
				i--;
		}
		len = ((int)(i - start) < buflen) ? i - start + 1 : buflen;
		mir_strncpy(buf, string + start, len);
		buf[len] = '\0';
	}
	else buf[0] = '\0';
	return buf;
}

// Parse DB string and add buttons
int RegisterButtonByParce(char * ObjectName, char * Params)
{
	char buf[255];
	GetParamN(Params, buf, _countof(buf), 0, ',', 0);

	// Push type
	char buf2[20] = { 0 };
	char pServiceName[255] = { 0 };
	char pStatusServiceName[255] = { 0 };
	int Left, Top, Right, Bottom;
	int MinWidth, MinHeight;
	char TL[9] = { 0 };
	wchar_t Hint[250] = { 0 };
	char Section[250] = { 0 };
	char Type[250] = { 0 };

	uint32_t alingnto;
	int a = ((int)!mir_strcmpi(buf, "Switch")) * 2;

	GetParamN(Params, pServiceName, _countof(pServiceName), 1, ',', 0);
	// if (a) GetParamN(Params,pStatusServiceName, sizeof(pStatusServiceName),a+1,',',0);
	Left = atoi(GetParamN(Params, buf2, _countof(buf2), a + 2, ',', 0));
	Top = atoi(GetParamN(Params, buf2, _countof(buf2), a + 3, ',', 0));
	Right = atoi(GetParamN(Params, buf2, _countof(buf2), a + 4, ',', 0));
	Bottom = atoi(GetParamN(Params, buf2, _countof(buf2), a + 5, ',', 0));
	GetParamN(Params, TL, _countof(TL), a + 6, ',', 0);

	MinWidth = atoi(GetParamN(Params, buf2, _countof(buf2), a + 7, ',', 0));
	MinHeight = atoi(GetParamN(Params, buf2, _countof(buf2), a + 8, ',', 0));
	GetParamNT(Params, Hint, _countof(Hint), a + 9, ',', 0);
	if (a) {
		GetParamN(Params, Section, _countof(Section), 2, ',', 0);
		GetParamN(Params, Type, _countof(Type), 3, ',', 0);
	}
	alingnto = ((TL[0] == 'R') ? SBF_ALIGN_TL_RIGHT : 0)
		+ ((TL[0] == 'C') ? SBF_ALIGN_TL_HCENTER : 0)
		+ ((TL[1] == 'B') ? SBF_ALIGN_TL_BOTTOM : 0)
		+ ((TL[1] == 'C') ? SBF_ALIGN_TL_VCENTER : 0)
		+ ((TL[2] == 'R') ? SBF_ALIGN_BR_RIGHT : 0)
		+ ((TL[2] == 'C') ? SBF_ALIGN_BR_HCENTER : 0)
		+ ((TL[3] == 'B') ? SBF_ALIGN_BR_BOTTOM : 0)
		+ ((TL[3] == 'C') ? SBF_ALIGN_BR_VCENTER : 0)
		+ ((TL[4] == 'I') ? SBF_CALL_ON_PRESS : 0);
	if (a)
		return ModernSkinButton_AddButton(g_clistApi.hwndContactList, ObjectName + 1, pServiceName, pStatusServiceName, "\0", Left, Top, Right, Bottom, alingnto, TranslateW(Hint), Section, Type, MinWidth, MinHeight);
	return ModernSkinButton_AddButton(g_clistApi.hwndContactList, ObjectName + 1, pServiceName, pStatusServiceName, "\0", Left, Top, Right, Bottom, alingnto, TranslateW(Hint), nullptr, nullptr, MinWidth, MinHeight);
}

//Parse DB string and add object
// Params is:
// Glyph,None
// Glyph,Solid, < ColorR>, < ColorG>, < ColorB>, < Alpha>
// Glyph,Image,Filename,(TileBoth|TileVert|TileHor|StretchBoth), < MarginLeft>, < MarginTop>, < MarginRight>, < MarginBottom>, < Alpha>
int RegisterObjectByParce(char *ObjectName, char *Params)
{
	if (!ObjectName || !Params) return 0;

	SKINOBJECTDESCRIPTOR obj = { 0 };
	char buf[250];
	obj.szObjectID = mir_strdup(ObjectName);
	GetParamN(Params, buf, _countof(buf), 0, ',', 0);
	if (!mir_strcmpi(buf, "Glyph"))
		obj.bType = OT_GLYPHOBJECT;
	else if (!mir_strcmpi(buf, "Font"))
		obj.bType = OT_FONTOBJECT;

	if (obj.bType == OT_GLYPHOBJECT) {
		GLYPHOBJECT gl = { 0 };
		GetParamN(Params, buf, _countof(buf), 1, ',', 0);
		if (!mir_strcmpi(buf, "Solid")) {
			// Solid
			gl.Style = ST_BRUSH;
			int r = atoi(GetParamN(Params, buf, _countof(buf), 2, ',', 0));
			int g = atoi(GetParamN(Params, buf, _countof(buf), 3, ',', 0));
			int b = atoi(GetParamN(Params, buf, _countof(buf), 4, ',', 0));
			gl.dwAlpha = atoi(GetParamN(Params, buf, _countof(buf), 5, ',', 0));
			gl.dwColor = RGB(r, g, b);
		}
		else if (!mir_strcmpi(buf, "Image")) {
			// Image
			gl.Style = ST_IMAGE;
			gl.szFileName = mir_strdup(GetParamN(Params, buf, _countof(buf), 2, ',', 0));
			gl.dwLeft = atoi(GetParamN(Params, buf, _countof(buf), 4, ',', 0));
			gl.dwTop = atoi(GetParamN(Params, buf, _countof(buf), 5, ',', 0));
			gl.dwRight = atoi(GetParamN(Params, buf, _countof(buf), 6, ',', 0));
			gl.dwBottom = atoi(GetParamN(Params, buf, _countof(buf), 7, ',', 0));
			gl.dwAlpha = atoi(GetParamN(Params, buf, _countof(buf), 8, ',', 0));
			GetParamN(Params, buf, _countof(buf), 3, ',', 0);
			if (!mir_strcmpi(buf, "TileBoth")) gl.FitMode = FM_TILE_BOTH;
			else if (!mir_strcmpi(buf, "TileVert")) gl.FitMode = FM_TILE_VERT;
			else if (!mir_strcmpi(buf, "TileHorz")) gl.FitMode = FM_TILE_HORZ;
			else gl.FitMode = 0;
		}
		else if (!mir_strcmpi(buf, "Fragment")) {
			// Image
			gl.Style = ST_FRAGMENT;
			gl.szFileName = mir_strdup(GetParamN(Params, buf, _countof(buf), 2, ',', 0));

			gl.clipArea.x = atoi(GetParamN(Params, buf, _countof(buf), 3, ',', 0));
			gl.clipArea.y = atoi(GetParamN(Params, buf, _countof(buf), 4, ',', 0));
			gl.szclipArea.cx = atoi(GetParamN(Params, buf, _countof(buf), 5, ',', 0));
			gl.szclipArea.cy = atoi(GetParamN(Params, buf, _countof(buf), 6, ',', 0));

			gl.dwLeft = atoi(GetParamN(Params, buf, _countof(buf), 8, ',', 0));
			gl.dwTop = atoi(GetParamN(Params, buf, _countof(buf), 9, ',', 0));
			gl.dwRight = atoi(GetParamN(Params, buf, _countof(buf), 10, ',', 0));
			gl.dwBottom = atoi(GetParamN(Params, buf, _countof(buf), 11, ',', 0));
			gl.dwAlpha = atoi(GetParamN(Params, buf, _countof(buf), 12, ',', 0));
			GetParamN(Params, buf, _countof(buf), 7, ',', 0);
			if (!mir_strcmpi(buf, "TileBoth")) gl.FitMode = FM_TILE_BOTH;
			else if (!mir_strcmpi(buf, "TileVert")) gl.FitMode = FM_TILE_VERT;
			else if (!mir_strcmpi(buf, "TileHorz")) gl.FitMode = FM_TILE_HORZ;
			else gl.FitMode = 0;
		}
		else gl.Style = ST_SKIP; // None

		obj.Data = &gl;
		int res = ske_AddDescriptorToSkinObjectList(&obj, nullptr);
		replaceStr(obj.szObjectID, nullptr);
		replaceStr(gl.szFileName, nullptr);
		return res;
	}

	return 0;
}


int SkinDrawGlyphMask(HDC hdc, RECT *rcSize, RECT *rcClip, MODERNMASK *ModernMask)
{
	if (!ModernMask) return 0;

	SKINDRAWREQUEST rq;
	rq.hDC = hdc;
	rq.rcDestRect = *rcSize;
	rq.rcClipRect = *rcClip;
	strncpy_s(rq.szObjectID, "Masked draw", _TRUNCATE);
	return ske_Service_DrawGlyph((WPARAM)&rq, (LPARAM)ModernMask);
}
