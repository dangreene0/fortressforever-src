//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#include "cbase.h"
#include "ff_hud_chat.h"
#include "hud_macros.h"
#include "text_message.h"
#include "vguicenterprint.h"
#include "vgui/ILocalize.h"
#include "c_team.h"
#include "c_playerresource.h"
#include "c_ff_player.h"
#include "ff_gamerules.h"
#include "ihudlcd.h"
#include "voice_status.h"

#include <igameresources.h>	// |-- Mirv: Access to play details

bool g_fBlockedStatus[256] = { false };		// |-- Mirv: Hold whether these dudes are blocked

extern ConVar hud_saytext_time;
extern ConVar cl_showtextmsg;
extern ConVar cl_chatfilters;
extern ConVar cl_chatfilter_version;
extern ConVar cl_mute_all_comms;

CVoiceStatus* g_VoiceStatus = NULL;

ConVar cl_chat_colorize("cl_chat_colorize", "1", FCVAR_ARCHIVE, "Enable/disable text messages colorization.");
ConVar cl_chat_color_default("cl_chat_color_default", "255 170 0", FCVAR_ARCHIVE, "Set the default chat text color(before colorization).");

extern ConVar sv_specchat;

// Yar!
static CHudChat* g_pHudChat = NULL;

// --> Mirv: Colours!
Color g_ColorConsole(153, 255, 153, 255);
Color g_ColorOrange(255, 170, 0, 255);

Color GetDefaultChatColor()
{
	int r, g, b;
	if (sscanf(cl_chat_color_default.GetString(), "%i %i %i", &r, &g, &b) != 3)
		return g_ColorOrange;

	r = clamp(r, 0, 255);
	g = clamp(g, 0, 255);
	b = clamp(b, 0, 255);
	return Color(r, g, b, 255);
}

Color GetClientColor(int clientIndex)
{
	if (clientIndex == 0) // console msg
	{
		return g_ColorConsole;
	}
	else
	{
		IGameResources* gr = GameResources();

		if (!gr)
			return GetDefaultChatColor();

		return gr->GetTeamColor(gr->GetTeam(clientIndex));
	}
}
// <-- Mirv: Colours!

// Forward declare
class CHudChatLine;

// Dump text
void DumpBufToChatline(CHudChatLine* pChatLine, char* szText, int& iPos)
{
	wchar_t wszTemp[128];
	g_pVGuiLocalize->ConvertANSIToUnicode(szText, wszTemp, sizeof(wszTemp));
	pChatLine->InsertString(wszTemp);
	iPos = 0;
}

DECLARE_HUDELEMENT(CHudChat);

DECLARE_HUD_MESSAGE(CHudChat, SayText);
DECLARE_HUD_MESSAGE(CHudChat, SayText2);
DECLARE_HUD_MESSAGE(CHudChat, TextMsg);


//=====================
//CHudChatLine
//=====================

void CHudChatLine::ApplySchemeSettings(vgui::IScheme* pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	m_hFont = pScheme->GetFont("ChatFont");
	SetBorder(NULL);
	SetBgColor(Color(0, 0, 0, 0));
	SetFgColor(Color(0, 0, 0, 0));

	SetFont(m_hFont);
}

//=====================
//CHudChatInputLine
//=====================

void CHudChatInputLine::ApplySchemeSettings(vgui::IScheme* pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	vgui::HFont hFont = pScheme->GetFont("ChatFont");

	m_pPrompt->SetFont(hFont);
	m_pInput->SetFont(hFont);

	m_pInput->SetFgColor(pScheme->GetColor("Chat.TypingText", pScheme->GetColor("Panel.FgColor", Color(255, 255, 255, 255))));
}

//=====================
//CHudChat
//=====================

CHudChat::CHudChat(const char* pElementName) : BaseClass(pElementName)
{

}

CHudChat::~CHudChat(void)
{
	g_pHudChat = NULL;
}

void CHudChat::CreateChatInputLine(void)
{
	m_pChatInput = new CHudChatInputLine(this, "ChatInputLine");
	m_pChatInput->SetVisible(false);

	// Only the chat input line is a popup now.
	// This means that we can see the rest of the text behind the scoreboard.
	// When the chat input line goes up the scoreboard will be removed so the 
	// disparity is not obvious.
	m_pChatInput->MakePopup();
}

void CHudChat::CreateChatLines(void)
{
	m_ChatLine = new CHudChatLine(this, "ChatLine1");
	m_ChatLine->SetVisible(false);
}

void CHudChat::ApplySchemeSettings(vgui::IScheme* pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	SetBgColor(Color(0, 0, 0, 0));
	SetFgColor(Color(0, 0, 0, 0));
}


void CHudChat::Init(void)
{
	BaseClass::Init();

	HOOK_HUD_MESSAGE(CHudChat, SayText);
	HOOK_HUD_MESSAGE(CHudChat, SayText2);
	HOOK_HUD_MESSAGE(CHudChat, TextMsg);

	g_pHudChat = this;
}

//-----------------------------------------------------------------------------
// Purpose: Overrides base reset to not cancel chat at round restart
//-----------------------------------------------------------------------------
void CHudChat::Reset(void)
{
}

int CHudChat::GetChatInputOffset(void)
{
	/*if (m_pChatInput->IsVisible())
	{
		return m_iFontHeight;
	}
	else
		return 0;*/

	// For now lets not shift text around when we're writing.
	// It is a bit jumpy and doesn't look so good.
	return m_iFontHeight;
}

Color CHudChat::GetClientColor(int clientIndex)
{
	if (clientIndex == 0) // console msg
	{
		return g_ColorYellow;
	}
	else if (g_PR)
	{
		switch (g_PR->GetTeam(clientIndex))
		{
		case TEAM_BLUE: return g_ColorBlue;
		case TEAM_RED: return g_ColorRed;
		case TEAM_YELLOW: return g_ColorYellow;
		case TEAM_GREEN: return g_ColorGreen;
		default: return g_ColorGrey;
		}
	}

	return g_ColorGrey;
}

//-----------------------------------------------------------------------------
// Purpose: Does a ClientPrint message but from the client (didn't want to
//			overwrite the client version which does... nothing, literally)
//-----------------------------------------------------------------------------
void ClientPrintMsg(C_BasePlayer* player, int msg_dest, const char* msg_name, const char* param1, const char* param2, const char* param3, const char* param4)
{
	if (g_pHudChat)
	{
		char szString[2048];

		wchar_t szBuf[5][128];
		wchar_t outputBuf[256];

		for (int i = 0; i < 5; ++i)
		{
			switch (i)
			{
			case 0: Q_snprintf(szString, sizeof(szString), "%s", msg_name); break;
			case 1: Q_snprintf(szString, sizeof(szString), "%s", param1); break;
			case 2: Q_snprintf(szString, sizeof(szString), "%s", param2); break;
			case 3: Q_snprintf(szString, sizeof(szString), "%s", param3); break;
			case 4: Q_snprintf(szString, sizeof(szString), "%s", param4); break;
			}

			char* tmpStr = hudtextmessage->LookupString(szString, &msg_dest);
			const wchar_t* pBuf = g_pVGuiLocalize->Find(tmpStr);
			if (pBuf)
			{
				// Copy pBuf into szBuf[i].
				int nMaxChars = sizeof(szBuf[i]) / sizeof(wchar_t);
				wcsncpy(szBuf[i], pBuf, nMaxChars);
				szBuf[i][nMaxChars - 1] = 0;
			}
			else
			{
				if (i)
				{
					StripEndNewlineFromString(tmpStr);  // these strings are meant for subsitution into the main strings, so cull the automatic end newlines
				}
				g_pVGuiLocalize->ConvertANSIToUnicode(tmpStr, szBuf[i], sizeof(szBuf[i]));
			}
		}

		if (!cl_showtextmsg.GetInt())
			return;

		//int len;
		switch (msg_dest)
		{
		case HUD_PRINTCENTER:
			g_pVGuiLocalize->ConstructString(outputBuf, sizeof(outputBuf), szBuf[0], 4, szBuf[1], szBuf[2], szBuf[3], szBuf[4]);
			internalCenterPrint->Print(ConvertCRtoNL(outputBuf));
			break;

			/*
		case HUD_PRINTNOTIFY:
			szString[0] = 1;  // mark this message to go into the notify buffer
			g_pVGuiLocalize->ConstructString( outputBuf, sizeof(outputBuf), szBuf[0], 4, szBuf[1], szBuf[2], szBuf[3], szBuf[4] );
			g_pVGuiLocalize->ConvertUnicodeToANSI( outputBuf, szString+1, sizeof(szString)-1 );
			len = strlen( szString );
			if ( len && szString[len-1] != '\n' && szString[len-1] != '\r' )
			{
				Q_strncat( szString, "\n", sizeof(szString), 1 );
			}
			Msg( "%s", ConvertCRtoNL( szString ) );
			break;

		case HUD_PRINTTALK:
			g_pVGuiLocalize->ConstructString( outputBuf, sizeof(outputBuf), szBuf[0], 4, szBuf[1], szBuf[2], szBuf[3], szBuf[4] );
			g_pVGuiLocalize->ConvertUnicodeToANSI( outputBuf, szString, sizeof(szString) );
			len = strlen( szString );
			if ( len && szString[len-1] != '\n' && szString[len-1] != '\r' )
			{
				Q_strncat( szString, "\n", sizeof(szString), 1 );
			}
			g_pHudChat->Printf( "%s", ConvertCRtoNL( szString ) );
			break;

		case HUD_PRINTCONSOLE:
			g_pVGuiLocalize->ConstructString( outputBuf, sizeof(outputBuf), szBuf[0], 4, szBuf[1], szBuf[2], szBuf[3], szBuf[4] );
			g_pVGuiLocalize->ConvertUnicodeToANSI( outputBuf, szString, sizeof(szString) );
			len = strlen( szString );
			if ( len && szString[len-1] != '\n' && szString[len-1] != '\r' )
			{
				Q_strncat( szString, "\n", sizeof(szString), 1 );
			}
			Msg( "%s", ConvertCRtoNL( szString ) );
			break;
			*/
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pszName - 
//			iSize - 
//			*pbuf - 
//-----------------------------------------------------------------------------
void CHudChat::MsgFunc_SayText(bf_read& msg)
{
	char szString[256];

	int client = msg.ReadByte();

	// --> Mirv: Actually bud, you're blocked
	if (g_fBlockedStatus[client])
		return;
	// <-- Mirv: Actually bud, you're blocked

	msg.ReadString(szString, sizeof(szString));
	bool bWantsToChat = msg.ReadByte();

	IGameResources* gr = GameResources();

	if (gr && CBasePlayer::GetLocalPlayer())
	{
		bool bIsSpectator = gr->GetTeam(client) < TEAM_BLUE;
		bool bLocalPlayerSpectator = CBasePlayer::GetLocalPlayer()->GetTeamNumber() < TEAM_BLUE;

		// Don't block rcon messages
		if (client != 0 && !bLocalPlayerSpectator && bIsSpectator && !sv_specchat.GetBool())
			return;
	}

	if (bWantsToChat)
	{
		// print raw chat text
		ChatPrintf(client, "%s", szString);
	}
	else
	{
		// try to lookup translated string
		Printf(NULL, "%s", hudtextmessage->LookupString(szString));
		Msg("%s", szString);
	}
}

wchar_t* ReadLocalizedRadioCommandString(bf_read& msg, wchar_t* pOut, int outSize, bool bStripNewline)
{
	char szString[2048];
	msg.ReadString(szString, sizeof(szString));

	const wchar_t* pBuf = g_pVGuiLocalize->Find(szString);
	if (pBuf)
	{
		wcsncpy(pOut, pBuf, outSize / sizeof(wchar_t));
		pOut[outSize / sizeof(wchar_t) - 1] = 0;
	}
	else
	{
		g_pVGuiLocalize->ConvertANSIToUnicode(szString, pOut, outSize);
	}

	if (bStripNewline)
		StripEndNewlineFromString(pOut);

	return pOut;
}

// Message handler for text messages
// displays a string, looking them up from the titles.txt file, which can be localised
// parameters:
//   byte:   message direction  ( HUD_PRINTCONSOLE, HUD_PRINTNOTIFY, HUD_PRINTCENTER, HUD_PRINTTALK )
//   string: message
// optional parameters:
//   string: message parameter 1
//   string: message parameter 2
//   string: message parameter 3
//   string: message parameter 4
// any string that starts with the character '#' is a message name, and is used to look up the real message in titles.txt
// the next (optional) one to four strings are parameters for that string (which can also be message names if they begin with '#')
void CHudChat::MsgFunc_TextMsg(bf_read& msg)
{
	char szString[2048];
	int msg_dest = msg.ReadByte();

	wchar_t szBuf[5][128];
	wchar_t outputBuf[256];

	for (int i = 0; i < 5; ++i)
	{
		msg.ReadString(szString, sizeof(szString));
		char* tmpStr = hudtextmessage->LookupString(szString, &msg_dest);
		const wchar_t* pBuf = g_pVGuiLocalize->Find(tmpStr);
		if (pBuf)
		{
			// Copy pBuf into szBuf[i].
			int nMaxChars = sizeof(szBuf[i]) / sizeof(wchar_t);
			wcsncpy(szBuf[i], pBuf, nMaxChars);
			szBuf[i][nMaxChars - 1] = 0;
		}
		else
		{
			if (i)
			{
				StripEndNewlineFromString(tmpStr);  // these strings are meant for subsitution into the main strings, so cull the automatic end newlines
			}
			g_pVGuiLocalize->ConvertANSIToUnicode(tmpStr, szBuf[i], sizeof(szBuf[i]));
		}
	}

	if (!cl_showtextmsg.GetInt())
		return;

	int len;
	switch (msg_dest)
	{
	case HUD_PRINTCENTER:
		g_pVGuiLocalize->ConstructString(outputBuf, sizeof(outputBuf), szBuf[0], 4, szBuf[1], szBuf[2], szBuf[3], szBuf[4]);
		internalCenterPrint->Print(ConvertCRtoNL(outputBuf));
		break;

	case HUD_PRINTNOTIFY:
		szString[0] = 1;  // mark this message to go into the notify buffer
		g_pVGuiLocalize->ConstructString(outputBuf, sizeof(outputBuf), szBuf[0], 4, szBuf[1], szBuf[2], szBuf[3], szBuf[4]);
		g_pVGuiLocalize->ConvertUnicodeToANSI(outputBuf, szString + 1, sizeof(szString) - 1);
		len = strlen(szString);
		if (len && szString[len - 1] != '\n' && szString[len - 1] != '\r')
		{
			Q_strncat(szString, "\n", sizeof(szString), 1);
		}
		Msg("%s", ConvertCRtoNL(szString));
		break;

	case HUD_PRINTTALK:
		g_pVGuiLocalize->ConstructString(outputBuf, sizeof(outputBuf), szBuf[0], 4, szBuf[1], szBuf[2], szBuf[3], szBuf[4]);
		g_pVGuiLocalize->ConvertUnicodeToANSI(outputBuf, szString, sizeof(szString));
		len = strlen(szString);
		if (len && szString[len - 1] != '\n' && szString[len - 1] != '\r')
		{
			Q_strncat(szString, "\n", sizeof(szString), 1);
		}
		Printf(NULL, "%s", ConvertCRtoNL(szString));
		break;

	case HUD_PRINTCONSOLE:
		g_pVGuiLocalize->ConstructString(outputBuf, sizeof(outputBuf), szBuf[0], 4, szBuf[1], szBuf[2], szBuf[3], szBuf[4]);
		g_pVGuiLocalize->ConvertUnicodeToANSI(outputBuf, szString, sizeof(szString));
		len = strlen(szString);
		if (len && szString[len - 1] != '\n' && szString[len - 1] != '\r')
		{
			Q_strncat(szString, "\n", sizeof(szString), 1);
		}
		Msg("%s", ConvertCRtoNL(szString));
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *fmt - 
//			... - 
//-----------------------------------------------------------------------------
void CHudChat::ChatPrintf(int iPlayerIndex, const char* fmt, ...)
{
	va_list marker;
	char msg[4096];

	va_start(marker, fmt);
	Q_vsnprintf(msg, sizeof(msg), fmt, marker);
	va_end(marker);

	// Strip any trailing '\n'
	if (strlen(msg) > 0 && msg[strlen(msg) - 1] == '\n')
		msg[strlen(msg) - 1] = 0;

	// Strip leading \n characters ( or notify/color signifiers ) for empty string check
	char* pmsg = msg;
	while (*pmsg && (*pmsg == '\n' || (*pmsg > 0 && *pmsg < COLOR_MAX)))
		pmsg++;

	if (!*pmsg)
		return;

	// Now strip just newlines, since we want the color info for printing
	pmsg = msg;
	while (*pmsg && (*pmsg == '\n'))
		pmsg++;

	if (!*pmsg)
		return;

	CBaseHudChatLine* line = (CBaseHudChatLine*)FindUnusedChatLine();
	if (!line)
		line = (CBaseHudChatLine*)FindUnusedChatLine();

	if (!line)
		return;

	/*if (iFilter != CHAT_FILTER_NONE)
	{
		if (!(iFilter & GetFilterFlags()))
			return;
	}*/

	// If a player is muted for voice, also mute them for text because jerks gonna jerk.
	if (cl_mute_all_comms.GetBool() && iPlayerIndex != 0)
	{
		if (GetClientVoiceMgr() && GetClientVoiceMgr()->IsPlayerBlocked(iPlayerIndex))
			return;
	}

	if (*pmsg < 32)
		hudlcd->AddChatLine(pmsg + 1);
	else
		hudlcd->AddChatLine(pmsg);

	line->SetText("");

	int iNameStart = 0;
	int iNameLength = 0;

	player_info_t sPlayerInfo;
	if (iPlayerIndex == 0)
	{
		Q_memset(&sPlayerInfo, 0, sizeof(player_info_t));
		Q_strncpy(sPlayerInfo.name, "Console", sizeof(sPlayerInfo.name));
	}
	else
		engine->GetPlayerInfo(iPlayerIndex, &sPlayerInfo);

	int bufSize = (strlen(pmsg) + 1) * sizeof(wchar_t);
	wchar_t* wbuf = static_cast<wchar_t*>(_malloca(bufSize));
	if (wbuf)
	{
		Color clrNameColor = GetClientColor(iPlayerIndex);

		line->SetExpireTime();

		g_pVGuiLocalize->ConvertANSIToUnicode(pmsg, wbuf, bufSize);

		// find the player's name in the unicode string, in case there is no color markup
		const char* pName = sPlayerInfo.name;

		if (pName)
		{
			wchar_t wideName[MAX_PLAYER_NAME_LENGTH];
			g_pVGuiLocalize->ConvertANSIToUnicode(pName, wideName, sizeof(wideName));

			const wchar_t* nameInString = wcsstr(wbuf, wideName);

			if (nameInString)
			{
				iNameStart = (nameInString - wbuf);
				iNameLength = wcslen(wideName);
			}
		}

		line->SetVisible(false);
		line->SetNameStart(iNameStart);
		line->SetNameLength(iNameLength);
		line->SetNameColor(clrNameColor);

		line->InsertAndColorizeText(wbuf, iPlayerIndex);
	}
}