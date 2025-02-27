/////////////////////////////////////////////////////////////////////////////////////////
// Miranda NG: the free IM client for Microsoft* Windows*
//
// Copyright (C) 2012-25 Miranda NG team,
// Copyright (c) 2000-09 Miranda ICQ/IM project,
// all portions of this codebase are copyrighted to the people
// listed in contributors.txt.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// you should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// part of tabSRMM messaging plugin for Miranda.
//
// (C) 2005-2010 by silvercircle _at_ gmail _dot_ com and contributors
//
// The class CSkin implements the skinning engine and loads skins from
// their skin definition files (.tsk).
//
// CImageItem implements a single rectangular skin item with an image
// and its rendering.

#ifndef __THEMES_H
#define __THEMES_H

class CSideBarButton;

struct TSButtonCtrl : public MButtonCtrl
{
	HICON   hIconPrivate, overlay;
	bool    bToolbarButton;			// is a toolbar button (important for aero background rendering)
	bool    bTitleButton;
	bool    bDimmed;

	TContainerData *pContainer;
	CSideBarButton *sitem;
};

void CustomizeButton(HWND hwndButton);

#define BUTTONSETASDIMMED        (BUTTONSETASFLATBTN + 11)
#define BUTTONSETCONTAINER       (BUTTONSETASFLATBTN + 12)
#define BUTTONSETASTITLE         (BUTTONSETASFLATBTN + 13)
#define BUTTONSETASNORMAL        (BUTTONSETASFLATBTN + 14)
#define BUTTONGETSTATEID         (BUTTONSETASFLATBTN + 15)
#define BUTTONSETASTOOLBARBUTTON (BUTTONSETASFLATBTN + 21)
#define BUTTONSETASSIDEBARBUTTON (BUTTONSETASFLATBTN + 22)
#define BUTTONSETOVERLAYICON	   (BUTTONSETASFLATBTN + 23)

// FLAGS
#define CORNER_NONE 0
#define CORNER_ACTIVE 1
#define CORNER_TL 2
#define CORNER_TR 4
#define CORNER_BR 8
#define CORNER_BL 16
#define CORNER_ALL (CORNER_TL | CORNER_TR | CORNER_BR | CORNER_BL | CORNER_ACTIVE)

#define GRADIENT_NONE 0
#define GRADIENT_ACTIVE 1
#define GRADIENT_LR 2
#define GRADIENT_RL 4
#define GRADIENT_TB 8
#define GRADIENT_BT 16

#define IMAGE_PERPIXEL_ALPHA 1
#define IMAGE_FLAG_DIVIDED 2
#define IMAGE_FILLSOLID 4
#define IMAGE_GLYPH 8

#define IMAGE_STRETCH_V 1
#define IMAGE_STRETCH_H 2
#define IMAGE_STRETCH_B 4

#define BUTTON_ISINTERNAL 1
#define BUTTON_ISTOGGLE 2
#define BUTTON_ISSERVICE 4
#define BUTTON_ISPROTOSERVICE 8
#define BUTTON_PASSHCONTACTW 16
#define BUTTON_PASSHCONTACTL 32
#define BUTTON_ISDBACTION    64
#define BUTTON_ISCONTACTDBACTION 128
#define BUTTON_DBACTIONONCONTACT 256
#define BUTTON_ISSIDEBAR 512
#define BUTTON_NORMALGLYPHISICON 1024
#define BUTTON_PRESSEDGLYPHISICON 2048
#define BUTTON_HOVERGLYPHISICON 4096
#define BUTTON_HASLABEL 8192

#define CLCDEFAULT_GRADIENT 0
#define CLCDEFAULT_CORNER 0

#define CLCDEFAULT_COLOR 0xd0d0d0
#define CLCDEFAULT_COLOR2 0xd0d0d0

#define CLCDEFAULT_TEXTCOLOR 0x000000

#define CLCDEFAULT_COLOR2_TRANSPARENT 1

#define CLCDEFAULT_ALPHA 100
#define CLCDEFAULT_MRGN_LEFT 0
#define CLCDEFAULT_MRGN_TOP 0
#define CLCDEFAULT_MRGN_RIGHT 0
#define CLCDEFAULT_MRGN_BOTTOM 0
#define CLCDEFAULT_IGNORE 1

struct AeroEffect
{
	wchar_t  tszName[40];
	uint32_t m_baseColor;
	uint32_t m_gradientColor;
	uint8_t  m_baseAlpha;
	uint8_t  m_finalAlpha;
	uint8_t  m_cornerType;
	uint8_t  m_gradientType;
	uint32_t m_cornerRadius;
	uint32_t m_glowSize;
	COLORREF m_clrBack, m_clrToolbar, m_clrToolbar2;

	void (TSAPI *pfnEffectRenderer)(const HDC hdc, const RECT *rc, int iEffectArea);
};
/**
 * CImageItem implementes image-based skin items. These items are loaded
 * from a skin file definition (.tsk file) and are then linked to one or
 * more skin items.
 */
class CImageItem
{
public:
	CImageItem()
	{
		memset(this, 0, sizeof(CImageItem));
	}
	CImageItem(const CImageItem &From)
	{
		*this = From;
		m_nextItem = nullptr;
	}
	CImageItem(const wchar_t *szName)
	{
		memset(this, 0, sizeof(CImageItem));
		mir_snwprintf(m_szName, szName);
		m_szName[39] = 0;
	}

	CImageItem(uint8_t bottom, uint8_t left, uint8_t top, uint8_t right, HDC hdc, HBITMAP hbm, uint32_t dwFlags,
		HBRUSH brush, uint8_t alpha, int inner_height, int inner_width, int height, int width)
	{
		m_bBottom = bottom;
		m_bLeft = left;
		m_bTop = top;
		m_bRight = right;
		m_hdc = hdc;
		m_hbm = hbm;
		m_dwFlags = dwFlags;
		m_fillBrush = brush;
		m_inner_height = inner_height;
		m_inner_width = inner_width;
		m_height = height;
		m_width = width;

		m_bf.SourceConstantAlpha = alpha;
		m_bf.AlphaFormat = 0;
		m_bf.BlendOp = AC_SRC_OVER;
		m_bf.BlendFlags = 0;
	}
	~CImageItem()
	{
		Free();
	}

	void Clear()
	{
		m_hdc = nullptr; m_hbm = nullptr; m_hbmOld = nullptr;
		m_fillBrush = nullptr;
	}

	void setBitmap(const HBITMAP hbm)
	{
		m_hbm = hbm;
	}

	void setAlphaFormat(const uint8_t bFormat, const uint8_t bConstantAlpha)
	{
		m_bf.AlphaFormat = bFormat;
		m_bf.SourceConstantAlpha = bConstantAlpha;
	}

	void setMetrics(const LONG width, const LONG height)
	{
		m_height = height;
		m_width = width;

		m_inner_height = m_height - m_bBottom - m_bTop;
		m_inner_width = m_width - m_bLeft - m_bRight;
		if (!(m_dwFlags & IMAGE_FLAG_DIVIDED))
			m_bStretch = IMAGE_STRETCH_B;
	}

	const BLENDFUNCTION &getBF() const { return m_bf; }

	void              Free();
	CImageItem*       getNextItem() const { return(m_nextItem); }
	void              setNextItem(CImageItem *item) { m_nextItem = item; }
	HBITMAP           getHbm() const { return(m_hbm); }
	uint32_t          getFlags() const { return(m_dwFlags); }
	HDC               getDC() const { return(m_hdc); }
	const wchar_t*    getName() const { return (m_szName); }
	wchar_t*          Read(const wchar_t *szFilename);
	void              Create(const wchar_t *szImageFile);
	void __fastcall   Render(const HDC hdc, const RECT *rc, bool fIgnoreGlyph) const;
	static void TSAPI	PreMultiply(HBITMAP hBitmap, int mode);
	static void TSAPI	SetBitmap32Alpha(HBITMAP hBitmap, uint8_t bAlpha = 255);
	static void TSAPI	Colorize(HBITMAP hBitmap, uint8_t dr, uint8_t dg, uint8_t db, uint8_t alpha = 0);

public:
	bool m_fValid; // verified item, indicates that all parameters are valid

private:
	wchar_t  m_szName[40];                         // everything wants a name, an image item doesn't need one though
	HBITMAP  m_hbm;                                // the bitmap handle
	uint8_t  m_bLeft, m_bRight, m_bTop, m_bBottom; // sizing margins for the outer 8 image parts
	uint8_t  m_alpha;                              // constant alpha for the entire image, applied via m_bf. sums with perpixel alpha
	uint32_t m_dwFlags;                            // flags
	HDC      m_hdc;                                // *can* hold a pre-created hdc to speed up rendering
	HBITMAP  m_hbmOld;                             // old bitmap, needs to be selected into m_hdc before destroying it
	int      m_inner_height, m_inner_width;        // dimensions of the inner image part
	int      m_width, m_height;                    // width and height of the image, in pixels
	uint8_t  m_bStretch;                           // stretch mode (unused in tabSRMM
	HBRUSH   m_fillBrush;                          // brush to fill the inner part (faster) dwFlags & IMAGE_FILLSOLID must be set
	LONG     m_glyphMetrics[4];                    // these coordinates point into the glyph image (if IMAGE_GLYPH is set)

	CImageItem* m_nextItem;                        // next item in a set of image items (usually the skin set)
	BLENDFUNCTION m_bf;                            // for AlphaBlend()
};


struct CSkinItem
{
	wchar_t  szName[40];
	char     szDBname[40];
	int      statusID;

	uint8_t  GRADIENT;
	uint8_t  CORNER;

	uint32_t COLOR;
	uint32_t COLOR2;

	uint8_t  COLOR2_TRANSPARENT;

	uint32_t TEXTCOLOR;

	int      ALPHA;
	int      MARGIN_LEFT;
	int      MARGIN_TOP;
	int      MARGIN_RIGHT;
	int      MARGIN_BOTTOM;
	uint8_t  IGNORED;
	uint32_t BORDERSTYLE;
	CImageItem *imageItem;
};

/**
 * Implements the skinning engine. There is only one instance of this class and
 * it always holds the currently loaded skin (if any).
 */

class CSkin
{
public:
	enum
	{
		AERO_EFFECT_NONE = 0,
		AERO_EFFECT_MILK = 1,
		AERO_EFFECT_CARBON = 2,
		AERO_EFFECT_SOLID = 3,
		AERO_EFFECT_WHITE = 4,
		AERO_EFFECT_CUSTOM = 5,
		AERO_EFFECT_CUSTOM2 = 6,
		AERO_EFFECT_LAST = 7
	};
	enum
	{
		AERO_EFFECT_AREA_MENUBAR = 0,
		AERO_EFFECT_AREA_STATUSBAR = 1,
		AERO_EFFECT_AREA_INFOPANEL = 2,
		AERO_EFFECT_AREA_TAB_ACTIVE = 3,
		AERO_EFFECT_AREA_TAB_HOVER = 4,
		AERO_EFFECT_AREA_TAB_NORMAL = 5,
		AERO_EFFECT_AREA_SIDEBAR_LEFT = 6,
		AERO_EFFECT_AREA_SIDEBAR_RIGHT = 7,
		AERO_EFFECT_AREA_TAB_TOP = 0x1000,
		AERO_EFFECT_AREA_TAB_BOTTOM = 0x2000
	};

	enum
	{
		DEFAULT_GLOW_SIZE = 10
	};

	/*
	 * avatar border types (skinned mode only)
	 */
	enum
	{
		AVBORDER_NONE = 0,
		AVBORDER_NORMAL = 1,
		AVBORDER_ROUNDED = 2
	};

	CSkin()
	{
		memset(this, 0, sizeof(CSkin));
		m_default_bf.SourceConstantAlpha = 255;
		m_default_bf.AlphaFormat = AC_SRC_ALPHA;
		m_default_bf.BlendOp = AC_SRC_OVER;
	}

	~CSkin()
	{
		Unload();
	}

	void  Init(bool fStartup = false);
	void  Load(void);
	void  Unload();
	void  UnloadAeroTabs();
	void  setFileName();
	void  ReadItem(const int id, const wchar_t *section);
	void  LoadItems();
	void  LoadIcon(const wchar_t *szSection, const wchar_t *name, HICON &hIcon);
	void  ReadImageItem(const wchar_t *szItemName);
	void  ReadButtonItem(const wchar_t *itemName) const;
	bool  haveGlyphItem() const { return(m_fHaveGlyph); }
	int   getNrIcons() const { return(m_nrSkinIcons); }
	uint32_t getDwmColor() const { return(m_dwmColor); }

	const TIconDescW* getIconDesc(const int id) const { return(&m_skinIcons[id]); }
	/**
	 * get the glyph image item (a single PNG image, containing a number of textures
	 * for the skin.
	 *
	 * @return CImageItem&: reference to the glyph item. Cannot be
	 *  	   modified.
	 *
	 */
	const CImageItem*		getGlyphItem() const
	{
		return(m_fHaveGlyph ? &m_glyphItem : nullptr);
	}
	bool					warnToClose() const;
	COLORREF				getColorKey() const { return(m_ContainerColorKey); }

	void					setupAeroSkins();
	void 					extractSkinsAndLogo(bool fForceOverwrite = false) const;
	void					setupTabCloseBitmap(bool fDeleteOnly = false);

	/*
	 * static member functions
	 */
	static void     TSAPI SkinDrawBGFromDC(HWND hwndClient, HWND hwnd, RECT *rcClient, HDC hdcTarget);
	static void     TSAPI SkinDrawBG(HWND hwndClient, HWND hwnd, TContainerData *pContainer, RECT *rcClient, HDC hdcTarget);
	static void     TSAPI DrawDimmedIcon(HDC hdc, LONG left, LONG top, LONG dx, LONG dy, HICON hIcon, uint8_t alpha);
	static uint32_t TSAPI HexStringToLong(const wchar_t *szSource);
	static UINT     TSAPI DrawRichEditFrame(HWND hwnd, const CMsgDialog *mwdat, UINT skinID, UINT msg, WPARAM wParam, LPARAM lParam, WNDPROC OldWndProc);
	static UINT     TSAPI NcCalcRichEditFrame(HWND hwnd, const CMsgDialog *mwdat, UINT skinID, UINT msg, WPARAM wParam, LPARAM lParam, WNDPROC OldWndProc);
	static HBITMAP  TSAPI CreateAeroCompatibleBitmap(const RECT &rc, HDC dc);
	static int      TSAPI RenderText(HDC hdc, HANDLE hTheme, const wchar_t *szText, RECT *rc, uint32_t dtFlags, const int iGlowSize = DEFAULT_GLOW_SIZE, COLORREF clr = 0, bool fForceAero = false);
	static void     TSAPI MapClientToParent(HWND hwndClient, HWND hwndParent, RECT &rc);
	static void     TSAPI ApplyAeroEffect(const HDC hdc, const RECT* rc, int iEffectArea);
	static void     TSAPI setAeroEffect(const LRESULT effect);
	static void     TSAPI initAeroEffect();
	static HANDLE   TSAPI InitiateBufferedPaint(const HDC hdcSrc, RECT& rc, HDC& hdcOut);
	static void     TSAPI FinalizeBufferedPaint(HANDLE hbp, RECT *rc);
	static bool     TSAPI DrawItem(const HDC hdc, const RECT *rc, const CSkinItem *item);
	static void     TSAPI FillBack(const HDC hdc, RECT* rc);
	static bool     TSAPI IsThemed(void);

public:
	static bool     m_DisableScrollbars, m_bClipBorder;
	static char     m_SkinnedFrame_left, m_SkinnedFrame_right, m_SkinnedFrame_bottom, m_SkinnedFrame_caption;
	static char     m_realSkinnedFrame_left, m_realSkinnedFrame_right, m_realSkinnedFrame_bottom, m_realSkinnedFrame_caption;
	static HPEN     m_SkinLightShadowPen, m_SkinDarkShadowPen;
	static int      m_titleBarLeftOff, m_titleButtonTopOff, m_captionOffset, m_captionPadding, m_titleBarRightOff, m_sidebarTopOffset, m_sidebarBottomOffset, m_bRoundedCorner;
	static SIZE     m_titleBarButtonSize;
	static int      m_bAvatarBorderType;
	static COLORREF m_avatarBorderClr, m_tmp_tb_low, m_tmp_tb_high;
	static COLORREF m_sideBarContainerBG;
	static COLORREF m_ContainerColorKey, m_DefaultFontColor;
	static HBRUSH   m_ContainerColorKeyBrush, m_MenuBGBrush;
	static bool     m_skinEnabled;
	static bool     m_frameSkins;
	static HICON    m_closeIcon, m_minIcon, m_maxIcon;
	static BLENDFUNCTION m_default_bf;										// general purpose bf, dynamically modified when needed

	/*
	 * cached bitmap for tab close button
	 */

	static HBITMAP	m_tabCloseBitmap, m_tabCloseOldBitmap;
	static HDC		m_tabCloseHDC;

	/*
	 * controls the aero effect. Set by initAeroEffect()
	 */

	static UINT			 m_aeroEffect; // effect id, initAeroEffect() is using it to set the parameters below.
	static AeroEffect	 m_aeroEffects[AERO_EFFECT_LAST];
	static AeroEffect	 m_currentAeroEffect;
	static AeroEffect* m_pCurrentAeroEffect;
	static uint32_t    m_glowSize;
	static HBRUSH      m_BrushBack, m_BrushFill;

	static COLORREF    m_dwmColorRGB;

	static CImageItem* m_switchBarItem, *m_tabTop, *m_tabBottom, *m_tabGlowTop, *m_tabGlowBottom;
	static bool        m_fAeroSkinsValid;

private:
	wchar_t            m_tszFileName[MAX_PATH];				// full path and filename of the currently loaded skin
	CSkinItem*         m_SkinItems;
	CImageItem*        m_ImageItems;							// the list of image item objects
	CImageItem         m_glyphItem;

	bool               m_fLoadOnStartup;						// load the skin on plugin initialization.
	bool               m_fHaveGlyph;
	void               SkinCalcFrameWidth();
	TIconDescW*        m_skinIcons;
	int                m_nrSkinIcons;
	uint32_t           m_dwmColor;

	static void TSAPI AeroEffectCallback_Milk(const HDC hdc, const RECT *rc, int iEffectArea);
	static void TSAPI AeroEffectCallback_Carbon(const HDC hdc, const RECT *rc, int iEffectArea);
	static void TSAPI AeroEffectCallback_Solid(const HDC hdc, const RECT *rc, int iEffectArea);
};

extern CSkin *Skin;

/*
* data structs
*/

struct ButtonItem
{
	wchar_t     szName[40];
	HWND        hWnd;
	LONG        xOff, yOff;
	LONG        width, height;
	CImageItem *imgNormal, *imgPressed, *imgHover;
	LONG_PTR    normalGlyphMetrics[4];
	LONG_PTR    hoverGlyphMetrics[4];
	LONG_PTR    pressedGlyphMetrics[4];
	uint32_t    dwFlags, dwStockFlags;
	uint32_t    uId;
	wchar_t     szTip[256];
	char        szService[256];
	char        szModule[256], szSetting[256];
	uint8_t     bValuePush[256], bValueRelease[256];
	uint32_t    type;
	void        (*pfnAction)(ButtonItem *item, HWND hwndDlg, CMsgDialog *dat, HWND hwndItem);
	void        (*pfnCallback)(ButtonItem *item, HWND hwndDlg, CMsgDialog *dat, HWND hwndItem);
	wchar_t     tszLabel[40];
	ButtonItem *nextItem;
	HANDLE      hContact;
	CMsgDialog *dat;
};

struct ButtonSet
{
	ButtonItem *items;
	LONG        left, top, right, bottom;               // client area offsets, calculated from button layout
};

#endif /* __THEMES_H */
