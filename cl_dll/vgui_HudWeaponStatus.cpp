/***
*
*	Copyright (c) 1999, Valve LLC. All rights reserved.
*
*	This product contains software technology licensed from Id
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
//
// vgui_HudWeaponStatus.cpp
//
// CWeaponStatusPanel: the history of weapons, ammo and items picked up, a
// column of icons along the left edge that each go after HISTORY_TIME
// seconds, and the tank's gas can, full or empty, which the tank HUD shows
//

#include "VGUI_Font.h"

#include "hud.h"
#include "cl_util.h"
#include "camera.h"
#include "kbutton.h"
#include "cvardef.h"
#include "usercmd.h"
#include "const.h"
#include "camera.h"
#include "in_defs.h"
#include "parsemsg.h"

#include "vgui_int.h"
#include "vgui_TeamFortressViewport.h"
#include "vgui_ServerBrowser.h"
#include "vgui_HudWeaponStatus.h"

// ammo types 1 to 6 and items 7 and 8 are their index - 1, weapons go
// through HistoryIcon()
static char *sHistoryIcons[MAX_HISTORY_ICONS] =
{
	"ammo_buckshot",
	"ammo_chem",
	"ammo_battery",
	"ammo_minigun",
	"ammo_rocket",
	"ammo_gauss",
	"medkit",
	"item_armor",
	"history_fists",
	"history_gauss",
	"history_shotgun",
	"history_minigun",
	"history_beamgun",
	"history_dml",
	"history_dmlGrenade",
	"history_chemgun",
	"history_aicore",
	"history_minicoolers",
	"history_sniper",
};

CWeaponStatusPanel::CWeaponStatusPanel( int iTrans, int iRemoveMe, int x, int y, int wide, int tall ) : CMenuPanel( iTrans, iRemoveMe, x, y, wide, tall )
{
	if ( ScreenWidth > 640 )
	{
		m_iMaxItems = 10;
		m_iTall = 300;
	}
	if ( ScreenWidth < 640 )
	{
		m_iMaxItems = 5;
		m_iTall = 150;
	}
	if ( ScreenWidth == 640 )
	{
		m_iMaxItems = 8;
		m_iTall = 240;
	}

	for ( int i = 0; i < MAX_HISTORY_ICONS; i++ )
		m_pIcons[i] = LoadTGA( sHistoryIcons[i] );

	// from the bottom up
	for ( int i = 0; i < m_iMaxItems; i++ )
	{
		m_Items[i].pLabel = new CImageLabel( m_pIcons[0], 0, m_iTall - 35 * ( i + 1 ), 64, 32 );
		m_Items[i].pLabel->setParent( this );
		m_Items[i].pLabel->setVisible( false );
		m_Items[i].flHideTime = 0;
	}

	m_pGasCan = new CImageLabel( "gascan", 55, m_iTall - 65, 64, 64 );
	m_pGasCan->setParent( this );
	m_pGasCan->setVisible( false );

	m_pEmptyGasCan = new CImageLabel( "emptygascan", 55, m_iTall - 65, 64, 64 );
	m_pEmptyGasCan->setParent( this );
	m_pEmptyGasCan->setVisible( false );

	m_iNextItem = 0;
	m_iNumShown = 0;
	m_flHideTime = 0;
	m_iGasCanShown = false;
	m_iWeaponsShown = 0;
}

// The icon of a weapon ID
int HistoryIcon( int iId )
{
	int iIcon = 0;

	switch ( iId )
	{
	case 1:		iIcon = 8;	break;	// WEAPON_FISTS
	case 30:	iIcon = 9;	break;	// WEAPON_GAUSSPISTOL
	case 17:	iIcon = 10;	break;	// WEAPON_SHOTGUN
	case 28:	iIcon = 11;	break;	// WEAPON_MINIGUN
	case 14:	iIcon = 12;	break;	// WEAPON_BEAMGUN
	case 16:	iIcon = 13;	break;	// WEAPON_DML
	case 13:	iIcon = 14;	break;	// WEAPON_DMLGRENADE
	case 19:	iIcon = 15;	break;	// WEAPON_SPCHEMICALGUN
	case 7:		iIcon = 16;	break;	// WEAPON_AICORE
	case 23:	iIcon = 17;	break;	// WEAPON_CUST_COOLERS
	case 18:	iIcon = 18;	break;	// WEAPON_CUST_SNIPER
	}

	return iIcon;
}

// The tank HUD shows the gas can while the player drives
void CWeaponStatusPanel::ShowGasCan( void )
{
	setVisible( true );
	m_iGasCanShown = true;
	setSize( 120, m_iTall );
	m_pGasCan->setVisible( true );
}

void CWeaponStatusPanel::ShowEmptyGasCan( void )
{
	setVisible( true );
	m_iGasCanShown = true;
	setSize( 120, m_iTall );
	m_pEmptyGasCan->setVisible( true );
}

void CWeaponStatusPanel::HideGasCans( void )
{
	m_pGasCan->setVisible( false );
	m_pEmptyGasCan->setVisible( false );
	m_iGasCanShown = false;
	setSize( 40, m_iTall );
	setVisible( false );
}

// A pickup: iIndex is the ammo type or item (from 1), or the weapon ID if
// iWeapon is set
void CWeaponStatusPanel::AddToHistory( int iIndex, int iWeapon )
{
	int iIcon = iIndex - 1;

	if ( iWeapon )
	{
		iIcon = HistoryIcon( iIndex );
		m_iWeaponsShown++;
	}

	// weapon icons are wider: the panel widens
	if ( !m_iGasCanShown )
	{
		if ( m_iWeaponsShown )
			setSize( 80, m_iTall );
		else
			setSize( 40, m_iTall );
	}

	if ( iIcon < 0 || iIcon >= MAX_HISTORY_ICONS )
	{
		gEngfuncs.Con_NPrintf( 0, "Invalid History Index %d in CWeaponStatusPanel::AddToHistory", iIcon );
		return;
	}

	setVisible( true );

	m_Items[m_iNextItem].pLabel->m_pTGA = m_pIcons[iIcon];
	m_Items[m_iNextItem].pLabel->setImage( m_Items[m_iNextItem].pLabel->m_pTGA );
	m_Items[m_iNextItem].pLabel->setVisible( true );
	m_Items[m_iNextItem].flHideTime = gEngfuncs.GetClientTime() + HISTORY_TIME;
	m_Items[m_iNextItem].iWeapon = iWeapon;

	m_iNumShown++;
	if ( m_iNumShown > m_iMaxItems )
		m_iNumShown = m_iMaxItems;

	m_iNextItem++;
	if ( m_iNextItem + 1 > m_iMaxItems )
		m_iNextItem = 0;

	m_flHideTime = 0;
}

// Take down the icons whose time is up, and the panel a moment after the
// last one unless the gas can is up
void CWeaponStatusPanel::paintBackground( void )
{
	float flTime = gEngfuncs.GetClientTime();

	for ( int i = 0; i < m_iMaxItems; i++ )
	{
		if ( m_Items[i].flHideTime == 0 || m_Items[i].flHideTime >= flTime )
			continue;

		m_Items[i].pLabel->setVisible( false );
		m_Items[i].flHideTime = 0;

		if ( --m_iNumShown < 0 )
			m_iNumShown = 0;

		if ( m_iNumShown == 0 )
			m_flHideTime = flTime + 0.1;
		else
			m_flHideTime = 0;

		if ( m_Items[i].iWeapon )
		{
			if ( --m_iWeaponsShown < 0 )
				m_iWeaponsShown = 0;
		}
	}

	if ( m_iNumShown == 0 )
	{
		m_iNextItem = 0;

		if ( m_flHideTime != 0 && !m_iGasCanShown && m_flHideTime < flTime )
		{
			setVisible( false );
			m_flHideTime = 0;
			return;
		}
	}

	CTransparentPanel::paintBackground();
}

// Called each time a new level is started
void CWeaponStatusPanel::Initialize( void )
{
	setVisible( false );

	m_iNextItem = 0;
	m_iNumShown = 0;
	m_flHideTime = 0;
	m_iWeaponsShown = 0;

	HideGasCans();

	for ( int i = 0; i < m_iMaxItems; i++ )
		m_Items[i].pLabel->setVisible( false );
}

void CWeaponStatusPanel::Reset( void )
{
	CMenuPanel::Reset();
}

void CWeaponStatusPanel::Open( void )
{
	Update();
	CMenuPanel::Open();
}

void CWeaponStatusPanel::Update( void )
{
}
