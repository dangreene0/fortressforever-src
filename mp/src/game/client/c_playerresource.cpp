//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Entity that propagates general data needed by clients for every player.
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "c_playerresource.h"
#include "c_ff_team.h"
#include "gamestringpool.h"

#include "ff_shareddefs.h"

#ifdef HL2MP
#include "hl2mp_gamerules.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

const float PLAYER_RESOURCE_THINK_INTERVAL = 0.2f;

IMPLEMENT_CLIENTCLASS_DT_NOBASE(C_PlayerResource, DT_PlayerResource, CPlayerResource)
	RecvPropArray3( RECVINFO_ARRAY(m_iPing), RecvPropInt( RECVINFO(m_iPing[0]))),
	RecvPropArray3( RECVINFO_ARRAY(m_iScore), RecvPropInt( RECVINFO(m_iScore[0]))),
	RecvPropArray3( RECVINFO_ARRAY(m_iFortPoints), RecvPropInt( RECVINFO(m_iFortPoints[0]))),
	RecvPropArray3( RECVINFO_ARRAY(m_iDeaths), RecvPropInt( RECVINFO(m_iDeaths[0]))),
	RecvPropArray3( RECVINFO_ARRAY(m_bConnected), RecvPropInt( RECVINFO(m_bConnected[0]))),
	RecvPropArray3( RECVINFO_ARRAY(m_iTeam), RecvPropInt( RECVINFO(m_iTeam[0]))),
	RecvPropArray3( RECVINFO_ARRAY(m_bAlive), RecvPropInt( RECVINFO(m_bAlive[0]))),
	RecvPropArray3( RECVINFO_ARRAY(m_iHealth), RecvPropInt( RECVINFO(m_iHealth[0]))),
	RecvPropArray3( RECVINFO_ARRAY(m_iArmor), RecvPropInt(RECVINFO(m_iArmor[0]))),
	RecvPropArray3( RECVINFO_ARRAY(m_iClass), RecvPropInt(RECVINFO(m_iClass[0]))), // |-- Mirv: Current class
	RecvPropArray3( RECVINFO_ARRAY(m_iChannel), RecvPropInt(RECVINFO(m_iChannel[0]))), // |-- Mirv: Channel information
	RecvPropArray3( RECVINFO_ARRAY(m_iAssists), RecvPropInt(RECVINFO(m_iAssists[0]))),
	RecvPropBool( RECVINFO(m_bIsIntermission)),
END_RECV_TABLE()

BEGIN_PREDICTION_DATA( C_PlayerResource )

	DEFINE_PRED_ARRAY( m_szName, FIELD_STRING, MAX_PLAYERS+1, FTYPEDESC_PRIVATE ),
	DEFINE_PRED_ARRAY( m_iPing, FIELD_INTEGER, MAX_PLAYERS+1, FTYPEDESC_PRIVATE ),
	DEFINE_PRED_ARRAY( m_iScore, FIELD_INTEGER, MAX_PLAYERS+1, FTYPEDESC_PRIVATE ),
	DEFINE_PRED_ARRAY( m_iFortPoints, FIELD_INTEGER, MAX_PLAYERS+1, FTYPEDESC_PRIVATE ),
	DEFINE_PRED_ARRAY( m_iDeaths, FIELD_INTEGER, MAX_PLAYERS+1, FTYPEDESC_PRIVATE ),
	DEFINE_PRED_ARRAY( m_bConnected, FIELD_BOOLEAN, MAX_PLAYERS+1, FTYPEDESC_PRIVATE ),
	DEFINE_PRED_ARRAY( m_iTeam, FIELD_INTEGER, MAX_PLAYERS+1, FTYPEDESC_PRIVATE ),
	DEFINE_PRED_ARRAY( m_bAlive, FIELD_BOOLEAN, MAX_PLAYERS+1, FTYPEDESC_PRIVATE ),
	DEFINE_PRED_ARRAY( m_iHealth, FIELD_INTEGER, MAX_PLAYERS+1, FTYPEDESC_PRIVATE ),
	DEFINE_PRED_ARRAY( m_iArmor, FIELD_INTEGER, MAX_PLAYERS + 1, FTYPEDESC_PRIVATE),
	DEFINE_PRED_ARRAY( m_iClass, FIELD_INTEGER, MAX_PLAYERS + 1, FTYPEDESC_PRIVATE),
	DEFINE_PRED_ARRAY( m_iChannel, FIELD_INTEGER, MAX_PLAYERS + 1, FTYPEDESC_PRIVATE),
	DEFINE_PRED_ARRAY( m_iAssists, FIELD_INTEGER, MAX_PLAYERS + 1, FTYPEDESC_PRIVATE),

END_PREDICTION_DATA()	

C_PlayerResource *g_PR;

IGameResources * GameResources( void ) { return g_PR; }

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_PlayerResource::C_PlayerResource()
{
	memset( m_szName, 0, sizeof(m_szName) );
	memset( m_iPing, 0, sizeof( m_iPing ) );
//	memset( m_iPacketloss, 0, sizeof( m_iPacketloss ) );
	memset( m_iScore, 0, sizeof( m_iScore ) );
	memset( m_iFortPoints, 0, sizeof(m_iFortPoints) );
	memset( m_iDeaths, 0, sizeof( m_iDeaths ) );
	memset( m_bConnected, 0, sizeof( m_bConnected ) );
	memset( m_iTeam, 0, sizeof( m_iTeam ) );
	memset( m_bAlive, 0, sizeof( m_bAlive ) );
	memset( m_iHealth, 0, sizeof( m_iHealth ) );
	memset( m_iArmor, 0, sizeof(m_iArmor) );
	memset( m_iClass, 0, sizeof(m_iClass) );	// |-- Mirv: Current class

	memset( m_iChannel, 0, sizeof(m_iChannel) ); // |-- Mirv: Channel information

	memset( m_iAssists, 0, sizeof(m_iAssists) );

	m_szUnconnectedName = 0;
	
	for ( int i=0; i<MAX_TEAMS; i++ )
	{
		m_Colors[i] = COLOR_GREY;
	}

	m_Colors[TEAM_SPECTATOR] = TEAM_COLOR_SPECTATOR;
	m_Colors[TEAM_BLUE]		 = TEAM_COLOR_BLUE;
	m_Colors[TEAM_RED]		 = TEAM_COLOR_RED;
	m_Colors[TEAM_YELLOW]	 = TEAM_COLOR_YELLOW;
	m_Colors[TEAM_GREEN]	 = TEAM_COLOR_GREEN;

	g_PR = this;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_PlayerResource::~C_PlayerResource()
{
	g_PR = NULL;
}

void C_PlayerResource::OnDataChanged(DataUpdateType_t updateType)
{
	BaseClass::OnDataChanged( updateType );
	if ( updateType == DATA_UPDATE_CREATED )
	{
		SetNextClientThink( gpGlobals->curtime + PLAYER_RESOURCE_THINK_INTERVAL );
	}
}

void C_PlayerResource::UpdatePlayerName( int slot )
{
	if ( slot < 1 || slot > MAX_PLAYERS )
	{
		Error( "UpdatePlayerName with bogus slot %d\n", slot );
		return;
	}
	if (!m_szUnconnectedName )
		m_szUnconnectedName = AllocPooledString( PLAYER_UNCONNECTED_NAME );
	
	player_info_t sPlayerInfo;
	if ( IsConnected( slot ) && engine->GetPlayerInfo( slot, &sPlayerInfo ) )
	{
		m_szName[slot] = AllocPooledString( sPlayerInfo.name );
	}
	else 
	{
		m_szName[slot] = m_szUnconnectedName;
	}
}

void C_PlayerResource::ClientThink()
{
	BaseClass::ClientThink();

	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		UpdatePlayerName( i );
	}

	SetNextClientThink( gpGlobals->curtime + PLAYER_RESOURCE_THINK_INTERVAL );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *C_PlayerResource::GetPlayerName( int iIndex )
{
	if ( iIndex < 1 || iIndex > MAX_PLAYERS )
	{
		Assert( false );
		return PLAYER_ERROR_NAME;
	}
	
	if ( !IsConnected( iIndex ) )
		return PLAYER_UNCONNECTED_NAME;

	// X360TBD: Network - figure out why the name isn't set
	if ( !m_szName[ iIndex ] || !Q_stricmp( m_szName[ iIndex ], PLAYER_UNCONNECTED_NAME ) )
	{
		// If you get a full "reset" uncompressed update from server, then you can have NULLNAME show up in the scoreboard
		UpdatePlayerName( iIndex );
	}

	// This gets updated in ClientThink, so it could be up to 1 second out of date, oh well.
	return m_szName[iIndex];
}

bool C_PlayerResource::IsAlive(int iIndex )
{
	return m_bAlive[iIndex];
}

int C_PlayerResource::GetTeam(int iIndex )
{
	if ( iIndex < 1 || iIndex > MAX_PLAYERS )
	{
		Assert( false );
		return 0;
	}
	else
	{
		return m_iTeam[iIndex];
	}
}

const char * C_PlayerResource::GetTeamName(int index)
{
	C_Team *team = GetGlobalTeam( index );

	if ( !team )
		return "Unknown";

	return team->Get_Name();
}

int C_PlayerResource::GetTeamScore(int index)
{
	C_Team *team = GetGlobalTeam( index );

	if ( !team )
		return 0;

	return team->Get_Score();
}

int C_PlayerResource::GetTeamFortPoints(int index)
{
	C_Team* team = GetGlobalTeam(index);

	if (!team)
		return 0;

	return team->Get_FortPoints();
}

float C_PlayerResource::GetTeamScoreTime(int index)
{
	C_Team* pTeam = GetGlobalTeam(index);

	if (!pTeam)
		return 0.0f;

	return pTeam->Get_ScoreTime();
}

int C_PlayerResource::GetTeamDeaths(int index)
{
	C_Team* team = GetGlobalTeam(index);

	if (!team)
		return 0;

	return team->Get_Deaths();
}

// --> Mirv: So menus can show correct limits
int C_PlayerResource::GetTeamClassLimits(int index, int classindex)
{
	C_FFTeam* team = (C_FFTeam*)GetGlobalTeam(index);

	if (!team)
		return 0;

	return team->Get_Classes(classindex);
}

int C_PlayerResource::GetTeamLimits(int index)
{
	C_FFTeam* team = (C_FFTeam*)GetGlobalTeam(index);

	if (!team)
		return -1;

	return team->Get_Teams();
}
// <-- Mirv: So menus can show correct limits

int C_PlayerResource::GetFrags(int index )
{
	//return 666;
	// BEG: Added by Mulchman
	if (!IsConnected(index))
		return 0;

	return m_iScore[index];
	// END: Added by Mulchman
}

bool C_PlayerResource::IsLocalPlayer(int index)
{
	C_BasePlayer *pPlayer =	C_BasePlayer::GetLocalPlayer();

	if ( !pPlayer )
		return false;

	return ( index == pPlayer->entindex() );
}


bool C_PlayerResource::IsHLTV(int index)
{
	if ( !IsConnected( index ) )
		return false;

	player_info_t sPlayerInfo;
	
	if ( engine->GetPlayerInfo( index, &sPlayerInfo ) )
	{
		return sPlayerInfo.ishltv;
	}
	
	return false;
}

bool C_PlayerResource::IsReplay(int index)
{
#if defined( REPLAY_ENABLED )
	if ( !IsConnected( index ) )
		return false;

	player_info_t sPlayerInfo;

	if ( engine->GetPlayerInfo( index, &sPlayerInfo ) )
	{
		return sPlayerInfo.isreplay;
	}
#endif

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool C_PlayerResource::IsFakePlayer( int iIndex )
{
	if ( !IsConnected( iIndex ) )
		return false;

	// Yuck, make sure it's up to date
	player_info_t sPlayerInfo;
	if ( engine->GetPlayerInfo( iIndex, &sPlayerInfo ) )
	{
		return sPlayerInfo.fakeplayer;
	}
	
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	C_PlayerResource::GetPing( int iIndex )
{
	if ( !IsConnected( iIndex ) )
		return 0;

	return m_iPing[iIndex];
}

//-----------------------------------------------------------------------------
// Purpose: 
/*-----------------------------------------------------------------------------
int	C_PlayerResource::GetPacketloss( int iIndex )
{
	if ( !IsConnected( iIndex ) )
		return 0;

	return m_iPacketloss[iIndex];
}*/

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	C_PlayerResource::GetPlayerScore( int iIndex )
{
	if ( !IsConnected( iIndex ) )
		return 0;

	return m_iScore[iIndex];
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	C_PlayerResource::GetFortPoints(int iIndex)
{
	if (!IsConnected(iIndex))
		return 0;

	return m_iFortPoints[iIndex];
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	C_PlayerResource::GetDeaths( int iIndex )
{
	if ( !IsConnected( iIndex ) )
		return 0;

	return m_iDeaths[iIndex];
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	C_PlayerResource::GetHealth( int iIndex )
{
	if ( !IsConnected( iIndex ) )
		return 0;

	return m_iHealth[iIndex];
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	C_PlayerResource::GetArmor(int iIndex)
{
	if (!IsConnected(iIndex))
		return 0;

	return m_iArmor[iIndex];
}

// --> Mirv: Get the player's class
int	C_PlayerResource::GetClass(int iIndex)
{
	if (!IsConnected(iIndex))
		return 0;

	return m_iClass[iIndex];
}
// <-- Mirv: Get the player's class

const Color &C_PlayerResource::GetTeamColor(int index )
{
	if ( index < 1 || index >= MAX_TEAMS )
	{
		Assert( false );
		static Color blah;
		return blah;
	}
	else
	{
		return m_Colors[index];
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool C_PlayerResource::IsConnected( int iIndex )
{
	if ( iIndex < 1 || iIndex > MAX_PLAYERS )
		return false;
	else
		return m_bConnected[iIndex];
}

// --> Mirv: Channel info
//-----------------------------------------------------------------------------
// Purpose: Return the voice channel that this player is using
//-----------------------------------------------------------------------------
int C_PlayerResource::GetChannel(int iIndex)
{
	if (iIndex < 1 || iIndex > MAX_PLAYERS)
	{
		Assert(0);
		return 0;
	}
	else
		return m_iChannel[iIndex];
}
// <-- Mirv: Channel info

#ifdef CLIENT_DLL
bool Client_IsIntermission()
{
	C_PlayerResource* pr = dynamic_cast <C_PlayerResource*> (GameResources());
	if (!pr)
		return false;
	return pr->m_bIsIntermission;
}
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	C_PlayerResource::GetAssists(int iIndex)
{
	if (!IsConnected(iIndex))
		return 0;

	return m_iAssists[iIndex];
}