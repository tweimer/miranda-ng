/*

Miranda NG: the free IM client for Microsoft* Windows*

Copyright (C) 2012-25 Miranda NG team (https://miranda-ng.org),
Copyright (c) 2000-12 Miranda IM project,
all portions of this codebase are copyrighted to the people
listed in contributors.txt.

 Copyright 2000 Alexandre Julliard of Wine project
 (UTF-8 conversion routines)

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

/* number of following bytes in sequence based on first byte value (for bytes above 0x7f) */
static const char utf8_length[128] =
{
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0x80-0x8f */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0x90-0x9f */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0xa0-0xaf */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0xb0-0xbf */
	0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 0xc0-0xcf */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 0xd0-0xdf */
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, /* 0xe0-0xef */
	3, 3, 3, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0  /* 0xf0-0xff */
};

/* first byte mask depending on UTF-8 sequence length */
static const unsigned char utf8_mask[4] = { 0x7f, 0x1f, 0x0f, 0x07 };

/* minimum Unicode value depending on UTF-8 sequence length */
static const unsigned int utf8_minval[4] = { 0x0, 0x80, 0x800, 0x10000 };

/* get the next char value taking surrogates into account */
static unsigned int getSurrogateValue(const wchar_t *src, unsigned int srclen)
{
	if (src[0] >= 0xd800 && src[0] <= 0xdfff) { /* surrogate pair */
		if (src[0] > 0xdbff || /* invalid high surrogate */
			srclen <= 1 ||     /* missing low surrogate */
			src[1] < 0xdc00 || src[1] > 0xdfff) /* invalid low surrogate */
			return 0;
		return 0x10000 + ((src[0] & 0x3ff) << 10) + (src[1] & 0x3ff);
	}
	return src[0];
}

/* query necessary dst length for src string */
static int mir_utf8len(const wchar_t *src, unsigned int srclen)
{
	int len;
	unsigned int val;

	for (len = 0; srclen; srclen--, src++) {
		if (*src < 0x80) { /* 0x00-0x7f: 1 byte */
			len++;
			continue;
		}
		if (*src < 0x800) { /* 0x80-0x7ff: 2 bytes */
			len += 2;
			continue;
		}
		if (!(val = getSurrogateValue(src, srclen)))
			return -2;

		if (val < 0x10000)  /* 0x800-0xffff: 3 bytes */
			len += 3;
		else { /* 0x10000-0x10ffff: 4 bytes */
			len += 4;
			src++;
			srclen--;
		}
	}
	return len;
}

MIR_CORE_DLL(int) mir_utf8lenW(const wchar_t *src)
{
	if (src == nullptr)
		return 0;

	return mir_utf8len(src, (int)wcslen(src));
}

/* wide char to UTF-8 string conversion */
/* return -1 on dst buffer overflow, -2 on invalid input char */
int Ucs2toUtf8(const wchar_t *src, int srclen, char *dst, int dstlen)
{
	int len;

	for (len = dstlen; srclen; srclen--, src++) {
		wchar_t ch = *src;
		unsigned int val;

		if (ch < 0x80) { /* 0x00-0x7f: 1 byte */
			if (!len--) return -1;  /* overflow */
			*dst++ = ch;
			continue;
		}

		if (ch < 0x800) { /* 0x80-0x7ff: 2 bytes */
			if ((len -= 2) < 0) return -1;  /* overflow */
			dst[1] = 0x80 | (ch & 0x3f);
			ch >>= 6;
			dst[0] = 0xc0 | ch;
			dst += 2;
			continue;
		}

		if (!(val = getSurrogateValue(src, srclen)))
			return -2;

		if (val < 0x10000) { /* 0x800-0xffff: 3 bytes */
			if ((len -= 3) < 0) return -1;  /* overflow */
			dst[2] = 0x80 | (val & 0x3f);
			val >>= 6;
			dst[1] = 0x80 | (val & 0x3f);
			val >>= 6;
			dst[0] = 0xe0 | val;
			dst += 3;
		}
		else { /* 0x10000-0x10ffff: 4 bytes */
			if ((len -= 4) < 0) return -1;  /* overflow */
			dst[3] = 0x80 | (val & 0x3f);
			val >>= 6;
			dst[2] = 0x80 | (val & 0x3f);
			val >>= 6;
			dst[1] = 0x80 | (val & 0x3f);
			val >>= 6;
			dst[0] = 0xf0 | val;
			dst += 4;
			src++;
			srclen--;
		}
	}
	return dstlen - len;
}

/* helper for the various utf8 mbstowcs functions */
static unsigned int decodeUtf8Char(unsigned char ch, const char **str, const char *strend)
{
	unsigned int len = utf8_length[ch - 0x80];
	unsigned int res = ch & utf8_mask[len];
	const char *end = *str + len;

	if (end > strend) return ~0;
	switch (len) {
	case 3:
		if ((ch = end[-3] ^ 0x80) >= 0x40) break;
		res = (res << 6) | ch;
		(*str)++;

	case 2:
		if ((ch = end[-2] ^ 0x80) >= 0x40) break;
		res = (res << 6) | ch;
		(*str)++;

	case 1:
		if ((ch = end[-1] ^ 0x80) >= 0x40) break;
		res = (res << 6) | ch;
		(*str)++;
		if (res < utf8_minval[len]) break;
		return res;
	}
	return ~0;
}

/* query necessary dst length for src string */
static int Utf8toUcs2Len(const char *src, size_t srclen)
{
	int ret = 0;
	unsigned int res;
	const char *srcend = src + srclen;

	while (src < srcend) {
		unsigned char ch = *src++;
		if (ch < 0x80) { /* special fast case for 7-bit ASCII */
			ret++;
			continue;
		}
		if ((res = decodeUtf8Char(ch, &src, srcend)) <= 0x10ffff) {
			if (res > 0xffff) ret++;
			ret++;
		}
		else return -2;  /* bad char */
		/* otherwise ignore it */
	}
	return ret;
}

/* UTF-8 to wide char string conversion */
/* return -1 on dst buffer overflow, -2 on invalid input char */
MIR_CORE_DLL(int) Utf8toUcs2(const char *src, size_t srclen, wchar_t *dst, size_t dstlen)
{
	unsigned int res;
	const char *srcend = src + srclen; // including trailing zero
	wchar_t *dstend = dst + dstlen;

	while ((dst < dstend) && (src < srcend)) {
		unsigned char ch = *src++;
		if (ch < 0x80) { /* special fast case for 7-bit ASCII */
			*dst++ = ch;
			continue;
		}

		if ((res = decodeUtf8Char(ch, &src, srcend)) <= 0xffff)
			*dst++ = res;
		else if (res <= 0x10ffff) { /* we need surrogates */
			if (dst == dstend - 1)
				return -1;  /* overflow */
			res -= 0x10000;
			*dst++ = 0xd800 | (res >> 10);
			*dst++ = 0xdc00 | (res & 0x3ff);
		}
		else return -2;  /* bad char */
	}

	if (src < srcend)
		return -1;  /* overflow */

	return (int)(dstlen - (dstend - dst));
}

/////////////////////////////////////////////////////////////////////////////////////////
// mir_utf8decode - converts UTF8-encoded string to the UCS2/MBCS format

#ifdef _MSC_VER
MIR_CORE_DLL(char*) mir_utf8decodecp(char *str, int codepage, wchar_t **ucs2)
{
	bool needs_free = false;
	wchar_t* tempBuf = nullptr;
	if (ucs2)
		*ucs2 = nullptr;

	if (str == nullptr)
		return nullptr;

	size_t len = strlen(str);
	if (len < 2) {
		if (ucs2 != nullptr) {
			*ucs2 = tempBuf = (wchar_t*)mir_alloc((len + 1) * sizeof(wchar_t));
			MultiByteToWideChar(codepage, 0, str, (int)len, tempBuf, (int)len);
			tempBuf[len] = 0;
		}
		return str;
	}

	int destlen = Utf8toUcs2Len(str, len);
	if (destlen < 0)
		return nullptr;

	if (ucs2 == nullptr) {
		__try {
			tempBuf = (wchar_t*)alloca((destlen + 1) * sizeof(wchar_t));
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			tempBuf = nullptr;
			needs_free = true;
		}
	}

	if (tempBuf == nullptr) {
		tempBuf = (wchar_t*)mir_alloc((destlen + 1) * sizeof(wchar_t));
		if (tempBuf == nullptr)
			return nullptr;
	}

	Utf8toUcs2(str, len, tempBuf, destlen);
	tempBuf[destlen] = 0;
	WideCharToMultiByte(codepage, 0, tempBuf, -1, str, (int)len + 1, "?", nullptr);

	if (ucs2)
		*ucs2 = tempBuf;
	else if (needs_free)
		mir_free(tempBuf);

	return str;
}

MIR_CORE_DLL(char*) mir_utf8decode(char *str, wchar_t **ucs2)
{
	return mir_utf8decodecp(str, Langpack_GetDefaultCodePage(), ucs2);
}
#endif

MIR_CORE_DLL(wchar_t*) mir_utf8decodeW(const char *str)
{
	if (str == nullptr)
		return nullptr;

	size_t len = strlen(str);

	int destlen = Utf8toUcs2Len(str, len);
	if (destlen < 0)
		return nullptr;

	wchar_t* ucs2 = (wchar_t*)mir_alloc((destlen + 1) * sizeof(wchar_t));
	if (ucs2 == nullptr)
		return nullptr;

	if (Utf8toUcs2(str, len, ucs2, destlen) >= 0) {
		ucs2[destlen] = 0;
		return ucs2;
	}

	mir_free(ucs2);

	return nullptr;
}

/////////////////////////////////////////////////////////////////////////////////////////
// mir_utf8encode - converts MBCS string to the UTF8-encoded format

#ifdef _MSC_VER
MIR_CORE_DLL(char*) mir_utf8encodecp(const char* src, int codepage)
{
	int len;
	bool needs_free = false;
	char* result = nullptr;
	wchar_t* tempBuf;

	if (src == nullptr)
		return nullptr;

	len = (int)strlen(src);

	__try {
		tempBuf = (wchar_t*)alloca((len + 1) * sizeof(wchar_t));
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		tempBuf = (wchar_t*)mir_alloc((len + 1) * sizeof(wchar_t));
		if (tempBuf == nullptr) return nullptr;
		needs_free = true;
	}

	len = MultiByteToWideChar(codepage, 0, src, -1, tempBuf, len + 1);

	int destlen = mir_utf8len(tempBuf, len);
	if (destlen >= 0) {
		result = (char*)mir_alloc(destlen + 1);
		if (result) {
			Ucs2toUtf8(tempBuf, len, result, destlen);
			result[destlen] = 0;
		}
	}

	if (needs_free)
		mir_free(tempBuf);

	return result;
}

MIR_CORE_DLL(char*) mir_utf8encode(const char* src)
{
	return mir_utf8encodecp(src, Langpack_GetDefaultCodePage());
}
#endif

/////////////////////////////////////////////////////////////////////////////////////////
// mir_utf8encode - converts UCS2 string to the UTF8-encoded format

MIR_CORE_DLL(char*) mir_utf8encodeW(const wchar_t* src)
{
	if (src == nullptr)
		return nullptr;

	int len = (int)wcslen(src);

	int destlen = mir_utf8len(src, len);
	if (destlen < 0) return nullptr;

	char* result = (char*)mir_alloc(destlen + 1);
	if (result == nullptr)
		return nullptr;

	Ucs2toUtf8(src, len, result, destlen);
	result[destlen] = 0;

	return result;
}

/////////////////////////////////////////////////////////////////////////////////////////
// Utf8CheckString - checks if a string is a valid utf8-encoded string

MIR_CORE_DLL(BOOL) Utf8CheckString(const char *str)
{
	int expect_bytes = 0, utf_found = 0;

	if (!str) return 0;

	while (*str) {
		if ((*str & 0x80) == 0) {
			/* Looks like an ASCII character */
			if (expect_bytes)
				/* byte of UTF-8 character expected */
				return 0;
		}
		else {
			/* Looks like byte of an UTF-8 character */
			if (expect_bytes) {
				/* expect_bytes already set: first byte of UTF-8 char already seen */
				if ((*str & 0xC0) != 0x80) {
					/* again first byte ?!?! */
					return 0;
				}
			}
			else {
				/* First byte of the UTF-8 character */
				/* count initial one bits and set expect_bytes to 1 less */
				char ch = *str;
				while (ch & 0x80) {
					expect_bytes++;
					ch = (ch & 0x7f) << 1;
				}
			}
			/* OK, next byte of UTF-8 character */
			/* Decrement number of expected bytes */
			if (--expect_bytes == 0)
				utf_found = 1;
		}
		str++;
	}

	return (utf_found && expect_bytes == 0);
}
