/*
UserinfoEx plugin for Miranda IM

Copyright:
© 2006-2010 DeathAxe, Yasnovidyashii, Merlin, K. Romanov, Kreol

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#ifndef _UINFOEX_MENUITEMS_H_INCLUDED_
#define _UINFOEX_MENUITEMS_H_INCLUDED_

extern HGENMENU hMenuItemRefresh;
extern HGENMENU *hMenuItemAccount;


void RebuildMenu();

void RebuildMain();
void RebuildContact();
void RebuildGroup();
void RebuildSubGroup();

int OnBuildGroupMenu(WPARAM wParam, LPARAM lParam);
int OnBuildStatusMenu(WPARAM wParam, LPARAM lParam);

#endif /* _UINFOEX_MENUITEMS_H_INCLUDED_ */
