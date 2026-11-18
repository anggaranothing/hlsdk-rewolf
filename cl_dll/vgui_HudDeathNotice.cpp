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
// vgui_HudDeathNotice.cpp
//
// the death notices as a VGUI panel: killer, weapon icon and victim, in
// lines down the top right corner of the screen. Each line goes after
// DEATH_NOTICE_TIME seconds and the ones below move up.
//

#include "VGUI_Font.h"
#include <VGUI_TextImage.h>

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
#include "vgui_HudDeathNotice.h"

// indexed by DeathNoticeIcon()
static char *sDeathIcons[MAX_DEATH_ICONS] =
{
	"death_fists",
	"death_gauss",
	"death_shotgun",
	"death_minigun",
	"death_beamgun",
	"death_dml",
	"death_dmlGrenade",
	"death_chemgun",
	"death_aicore",
	"death_skull",
};

#define DEATH_ICON_SKULL		9

CDeathNoticePanel::CDeathNoticePanel( int iTrans, int iRemoveMe, int x, int y, int wide, int tall ) : CMenuPanel( iTrans, iRemoveMe, x, y, wide, tall )
{
	int r, g, b, a;
	int iKillerWide, iKillerTall, iVictimWide, iVictimTall;

	m_iMaxLines = MAX_DEATH_NOTICE_LINES;

	CSchemeManager *pSchemes = gViewPort->GetSchemeManager();

	for ( int i = 0; i < MAX_DEATH_ICONS; i++ )
		m_pIcons[i] = LoadTGA( sDeathIcons[i] );

	SchemeHandle_t hTextScheme = pSchemes->getSchemeHandle( "Death Notice Text" );

	for ( int i = 0; i < m_iMaxLines; i++ )
	{
		m_Notices[i].pIcon = new CImageLabel( m_pIcons[0], 0, i * 36, 64, 32 );
		m_Notices[i].pIcon->setParent( this );
		m_Notices[i].pIcon->setVisible( false );
		m_Notices[i].flHideTime = 0;

		m_Notices[i].pKiller = new TextPanel( "longest_player_name", 0, i * 36, 128, 64 );
		m_Notices[i].pKiller->setParent( this );
		m_Notices[i].pKiller->setFont( pSchemes->getFont( hTextScheme ) );
		pSchemes->getFgColor( hTextScheme, r, g, b, a );
		m_Notices[i].pKiller->setFgColor( r, g, b, a );
		pSchemes->getBgColor( hTextScheme, r, g, b, a );
		m_Notices[i].pKiller->setBgColor( r, g, b, a );
		m_Notices[i].pKiller->getTextImage()->getTextSizeWrapped( iKillerWide, iKillerTall );
		m_Notices[i].pKiller->setSize( iKillerWide, iKillerTall );
		m_Notices[i].pKiller->setVisible( false );

		m_Notices[i].pVictim = new TextPanel( "longest_player_name", iKillerWide + 128, i * 36, 128, 64 );
		m_Notices[i].pVictim->setParent( this );
		m_Notices[i].pVictim->setFont( pSchemes->getFont( hTextScheme ) );
		pSchemes->getFgColor( hTextScheme, r, g, b, a );
		m_Notices[i].pVictim->setFgColor( r, g, b, a );
		pSchemes->getBgColor( hTextScheme, r, g, b, a );
		m_Notices[i].pVictim->setBgColor( r, g, b, a );
		m_Notices[i].pVictim->getTextImage()->getTextSizeWrapped( iVictimWide, iVictimTall );
		m_Notices[i].pVictim->setSize( iVictimWide, iVictimTall );
		m_Notices[i].pVictim->setVisible( false );

		m_Notices[i].iLine = 0;
	}

	m_iNext = 0;
	m_iNumShown = 0;
	m_iLine = 0;
}

// The icon for what killed: the "d_" names of the death messages
int DeathNoticeIcon( const char *pszKilledWith )
{
	if ( !strcmp( "d_gausspistol", pszKilledWith ) )
		return 1;
	if ( !strcmp( "d_gauss_bolt", pszKilledWith ) )
		return 1;
	if ( !strcmp( "d_gauss_charged", pszKilledWith ) )
		return 1;
	if ( !strcmp( "d_fists", pszKilledWith ) )
		return 0;
	if ( !strcmp( "d_shotgun", pszKilledWith ) )
		return 2;
	if ( !strcmp( "d_minigun", pszKilledWith ) )
		return 3;
	if ( !strcmp( "d_beamgun", pszKilledWith ) )
		return 4;
	if ( !strcmp( "d_ball_lightning", pszKilledWith ) )
		return 4;
	if ( !strcmp( "d_lightning_bug", pszKilledWith ) )
		return 4;
	if ( !strcmp( "d_tripmine", pszKilledWith ) )
		return 6;
	if ( !strcmp( "d_dmlRocket", pszKilledWith ) )
		return 5;
	if ( !strcmp( "d_dml_cluster", pszKilledWith ) )
		return 6;
	if ( !strcmp( "d_gauss_bolt", pszKilledWith ) )		// sic: tested twice
		return 1;
	if ( !strcmp( "d_grenade", pszKilledWith ) )
		return 6;
	if ( !strcmp( "d_chem_grenade", pszKilledWith ) )
		return 7;
	if ( !strcmp( "d_DmlHandGrenade", pszKilledWith ) )
		return 6;

	return DEATH_ICON_SKULL;
}

// Put a notice on its line, or hide it once it has gone off the top
inline void PlaceNotice( deathnotice_t *pNotice )
{
	if ( pNotice->iLine < 0 )
	{
		pNotice->iLine = 0;
		pNotice->pIcon->setVisible( false );
		pNotice->pKiller->setVisible( false );
		pNotice->pVictim->setVisible( false );
	}
	else
	{
		int x, y;
		int iY = pNotice->iLine * 36;

		pNotice->pKiller->getPos( x, y );
		pNotice->pKiller->setPos( x, iY );
		pNotice->pIcon->getPos( x, y );
		pNotice->pIcon->setPos( x, iY );
		pNotice->pVictim->getPos( x, y );
		pNotice->pVictim->setPos( x, iY );
	}
}

// A death: killer, icon and victim on the next line; when the lines are
// full, all move up one first
void CDeathNoticePanel::AddNotice( char *pszKilledWith, char *pszKiller, char *pszVictim )
{
	int iIcon = DeathNoticeIcon( pszKilledWith );
	int iKillerWide, iKillerTall, iVictimWide, iVictimTall;
	int x, y;

	// suicides show no killer
	if ( !strcmp( pszKiller, pszVictim ) )
		pszKiller = "";

	setVisible( true );

	if ( m_iLine >= m_iMaxLines )
	{
		for ( int i = 0; i < m_iMaxLines; i++ )
		{
			m_Notices[i].iLine--;
			PlaceNotice( &m_Notices[i] );
		}

		m_iLine = m_iMaxLines - 1;
	}

	m_Notices[m_iNext].pKiller->setText( pszKiller );
	m_Notices[m_iNext].pKiller->getTextImage()->getTextSizeWrapped( iKillerWide, iKillerTall );
	m_Notices[m_iNext].pKiller->setSize( iKillerWide, iKillerTall );
	m_Notices[m_iNext].pKiller->setVisible( true );

	m_Notices[m_iNext].pVictim->setText( pszVictim );
	m_Notices[m_iNext].pVictim->getTextImage()->getTextSizeWrapped( iVictimWide, iVictimTall );
	m_Notices[m_iNext].pVictim->setSize( iVictimWide, iVictimTall );
	m_Notices[m_iNext].pVictim->setVisible( true );

	m_Notices[m_iNext].pIcon->m_pTGA = m_pIcons[iIcon];
	m_Notices[m_iNext].pIcon->setImage( m_Notices[m_iNext].pIcon->m_pTGA );
	m_Notices[m_iNext].pIcon->setVisible( true );

	m_Notices[m_iNext].flHideTime = gEngfuncs.GetClientTime() + DEATH_NOTICE_TIME;

	// right-aligned: killer, icon, victim
	int iIconWide = ( iIcon == DEATH_ICON_SKULL ) ? 32 : 115;

	m_Notices[m_iNext].pKiller->getPos( x, y );
	m_Notices[m_iNext].pKiller->setPos( ScreenWidth - iIconWide - iVictimWide - iKillerWide, y );
	m_Notices[m_iNext].pVictim->getPos( x, y );
	m_Notices[m_iNext].pVictim->setPos( ScreenWidth - iVictimWide, y );
	m_Notices[m_iNext].pIcon->getPos( x, y );
	m_Notices[m_iNext].pIcon->setPos( ScreenWidth - iIconWide - iVictimWide, y );

	m_Notices[m_iNext].iLine = m_iLine;

	PlaceNotice( &m_Notices[m_iNext] );

	m_iLine++;
	m_iNumShown++;
	if ( m_iNumShown > m_iMaxLines )
		m_iNumShown = m_iMaxLines;

	m_iNext++;
	if ( m_iNext >= m_iMaxLines )
		m_iNext = 0;
}

// Take down the lines whose time is up, and move the others up
void CDeathNoticePanel::paintBackground( void )
{
	float flTime = gEngfuncs.GetClientTime();

	for ( int i = 0; i < m_iMaxLines; i++ )
	{
		if ( m_Notices[i].flHideTime == 0 || m_Notices[i].flHideTime >= flTime )
			continue;

		m_Notices[i].pIcon->setVisible( false );
		m_Notices[i].pKiller->setVisible( false );
		m_Notices[i].pVictim->setVisible( false );
		m_Notices[i].flHideTime = 0;

		if ( --m_iNumShown < 0 )
			m_iNumShown = 0;

		for ( int j = 0; j < m_iMaxLines; j++ )
		{
			m_Notices[j].iLine--;
			PlaceNotice( &m_Notices[j] );
		}

		if ( --m_iLine < 0 )
			m_iLine = 0;
	}

	if ( m_iNumShown == 0 )
	{
		m_iNext = 0;
		m_iLine = 0;
	}

	CTransparentPanel::paintBackground();
}

void CDeathNoticePanel::Reset( void )
{
	CMenuPanel::Reset();
}

void CDeathNoticePanel::Open( void )
{
	Update();
	CMenuPanel::Open();
}

// Called each time a new level is started
void CDeathNoticePanel::Initialize( void )
{
	setVisible( false );

	m_iNext = 0;
	m_iNumShown = 0;
	m_iLine = 0;

	for ( int i = 0; i < m_iMaxLines; i++ )
	{
		m_Notices[i].pIcon->setVisible( false );
		m_Notices[i].pVictim->setVisible( false );
		m_Notices[i].pKiller->setVisible( false );
		m_Notices[i].iLine = 0;
	}
}

void CDeathNoticePanel::Update( void )
{
}
