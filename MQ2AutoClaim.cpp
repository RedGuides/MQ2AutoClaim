/* 
MQ2AutoClaim  - Claim your free station cash

   Original Author : Dewey2461
	 Updated by Chatwiththisname.
   FILES: MQ2AutoClaim.INI - Used to store the "next" reward date. 

   Original Author notes: This was originally a macro, it was converted to a plugin so it will automatically run at startup. 
		WARNING: Makes heavy use of ParseMacroData to Evaluate MQ2 macro code, you are free to refactor it. 
			If you make changes please push the changes back to the author. 

	Update 12/16/2019 - Chatwiththisname - 
		No longer requires you open the DB Store window. 
		No longer parses macro data.
		Now uses login name instead of character name.
		Optional INI Entry added - AutoClosePopup, defaults to 0. If anything other than 0 it's true. If it's not present it's false.
			if true will automatically close the popup you get after claiming DBCash
*/

#include "../MQ2Plugin.h"

PreSetup("MQ2AutoClaim");
int GetSubscriptionLevel();
void LoadINI();

enum Subscription {
	SUB_BRONZE,
	SUB_SILVER,
	SUB_GOLD
};

int PluginState = 0;
bool bClaimed = false;//Did I hit the claim button?
bool bdebugging = false;//toggle for dev debugging.
bool bDiscardPopup = false;//Should I automatically discard the popup offer after claiming.
bool bINILoaded = false;//if the INI has been loaded or not.

PLUGIN_API VOID InitializePlugin(VOID)
{
	PluginState = 1;
}

PLUGIN_API VOID ShutdownPlugin(VOID)
{

}

PLUGIN_API VOID SetGameState(DWORD GameState)
{
	if (gGameState == GAMESTATE_CHARSELECT)	PluginState = 1;
}


// 01234567    0123456789
// mm/dd/yy or mm/dd/yyyy
void ParseDate(char* s, int& m, int& d, int& y)
{
	m = d = y = 0;
	int c = 1;
	while (*s)
	{
		if (*s >= '0' && *s <= '9') {
			switch (c) {
			case 1: m = m * 10 + (*s - '0'); break;
			case 2: d = d * 10 + (*s - '0'); break;
			case 3: y = y * 10 + (*s - '0'); break;
			}
		}
		if (*s == '/') c++;
		s++;
	}
	if (y < 100) y += 2000;
}

int CompareDates(char* s1, char* s2)
{
	int m1, d1, y1;
	int m2, d2, y2;

	ParseDate(s1, m1, d1, y1);
	ParseDate(s2, m2, d2, y2);

	if (y1 != y2) return y1 - y2;
	if (m1 != m2) return m1 - m2;
	return d1 - d2;
}



// Doing all the heavy lifting in OnPulse via a State Machine "PluginState"
PLUGIN_API VOID OnPulse(VOID)
{
	if (!PluginState || gGameState != GAMESTATE_INGAME || !GetCharInfo() || !GetCharInfo2() || !GetCharInfo()->pSpawn) 
		return;
	if (!bINILoaded) {
		LoadINI();
	}

	if (bdebugging) WriteChatf("PluginState: %i", PluginState);

	static unsigned long long Tick = 0;
	static unsigned long long AbortTick = 0;
	static char szDesc[MAX_STRING];
	static char szName[MAX_STRING];//This is the account name.
	static char szCash[64];
	static char szDate[12];
	CSidlScreenWnd* MarketWnd = (CSidlScreenWnd*)FindMQ2Window("MarketPlaceWnd");
	CXWnd* Funds;
	if (MarketWnd)
		Funds = MarketWnd->GetChildItem("MKPW_AvailableFundsUpper");
	CStmlWnd* Desc;
	if (MarketWnd)
		Desc = (CStmlWnd*)MarketWnd->GetChildItem("MKPW_ClaimDescription");
	CSidlScreenWnd* PopupWnd = (CSidlScreenWnd*)FindMQ2Window("PurchaseGroupWnd");

	Tick = GetTickCount642();

	if (Tick > AbortTick && PluginState != 1)
	{
		WriteChatf("[MQ2AutoClaim] Aborting... 120s should be plenty of time so something went wrong");
		PluginState = 0;
		return;
	}

	switch (PluginState) {
	case 1:
	{
		PluginState = 0;
		WriteChatf("\ag[MQ2AutoClaim]\aw Automatically claims your free station cash - Credit \ayDewey2461\aw");

		if (GetSubscriptionLevel() != SUB_GOLD) {
			WriteChatf("\ag[MQ2AutoClaim]\aw Account is not gold. No free station cash.");
			return;
		}

		sprintf_s(szName, 64, GetLoginName());
		GetPrivateProfileString("NextCheck", szName, "01/01/2000", szDate, 12, INIFileName);

		time_t now = time(0);
		struct tm localTime;
		localtime_s(&localTime, &now);

		//Get Current Month
		char month[4] = { 0 };
		_itoa_s(localTime.tm_mon + 1, month, 4, 10);
		//Get Current Day
		char day[4] = { 0 };
		_itoa_s(localTime.tm_mday, day, 4, 10);
		//Get current Year.
		char year[5] = { 0 };
		_itoa_s(localTime.tm_year + 1900, year, 5, 10);
		//put them in a single string
		char date[14] = { 0 };
		sprintf_s(date, "%s\\%s\\%s", month, day, year);

		if (CompareDates(date, szDate) < 0) {
			WriteChatf("\ag[MQ2AutoClaim]\aw Next check scheduled for \ay%s\aw", szDate);
			return;
		}

		// We are GOLD and NextCheck looks like we might have some SC ready. 
		AbortTick = Tick + 120000;
		PluginState = 2;
		break;
	}
	case 2: // Wait for market place window to open and populate	
		if (MarketWnd && Funds) {
				GetCXStr(Funds->CGetWindowText(), szCash, 64);
				if (bdebugging) WriteChatf("Current Funds: %s", szCash);
		}
		if (!szCash[0]) return;

		if (MarketWnd && Desc) {	
			GetCXStr(Desc->STMLText, szDesc, MAX_STRING);
			if (bdebugging) WriteChatf("Desc: %s", szDesc);
		}
		if (!szDesc[0]) return;

		if (strncmp(szDesc, "Reward expires:", 15) == 0) {
			EzCommand("/notify MKPW_ClaimWindow MKPW_ClaimClickHereBtn leftmouseup");
			if (bdebugging) WriteChatf("Hitting Claim!");
			bClaimed = true;
			PluginState = 3;
			return;
		}
		WriteChatf("\ag[MQ2AutoClaim]\aw Sorry, No free SC yet.");
		PluginState = 4;
		break;
	case 3:	// Wait for funds to update 
	{
		char sztemp[64] = { 0 };
		if (MarketWnd && Funds) {
			GetCXStr(Funds->CGetWindowText(), sztemp, 64);
			if (bdebugging) WriteChatf("Comparing Funds. Current: %s, Previous: %s", sztemp, szCash);
		}
		if (_stricmp(sztemp, szCash) == 0) 
			return;
		WriteChatf("\at[\agMQ2AutoClaim\at]\aw: \agClaimed your +500 free SC! You have \ay %s \aw SC.", sztemp);
		PluginState = 4;
		break;
	}
	case 4:
		if (MarketWnd && Desc) {
			GetCXStr(Desc->STMLText, szDesc, MAX_STRING);
		}
		if (strncmp(szDesc, "Next reward:", 12) == 0) {
			szDesc[38] = 0;//cut off the string
			WritePrivateProfileString("NextCheck", szName, &szDesc[29], INIFileName);
		}
		else { // Try again tomorrow 
			time_t now = time(0);
			struct tm localTime;
			localtime_s(&localTime, &now);

			//Get Current Month
			char month[4] = { 0 };
			_itoa_s(localTime.tm_mon + 1, month, 4, 10);
			//Get Current Day
			char day[4] = { 0 };
			_itoa_s(localTime.tm_mday+1, day, 4, 10);
			//Get current Year.
			char year[5] = { 0 };
			_itoa_s(localTime.tm_year + 1900, year, 5, 10);
			//put them in a single string
			char date[14] = { 0 };
			sprintf_s(date, "%s\\%s\\%s", month, day, year);

			WritePrivateProfileString("NextCheck", szName, date, INIFileName);
		}
		PluginState = 5;
		break;
	case 5:
		if (PopupWnd) {
			if (PopupWnd->IsVisible()) {
				if (bDiscardPopup) PopupWnd->SetVisible(false);
				bClaimed = false;
			}
		}
		if (!bClaimed) PluginState = 0;
		break;
	}

}

int GetSubscriptionLevel() {
	if (EQADDR_SUBSCRIPTIONTYPE && *EQADDR_SUBSCRIPTIONTYPE) {
		if (DWORD dwsubtype = *(DWORD*)EQADDR_SUBSCRIPTIONTYPE) {
			BYTE subtype = *(BYTE*)dwsubtype;
			return subtype;
		}
	}
	return false;
}

void LoadINI() {
	bINILoaded = true;
	int temp = GetPrivateProfileInt("Settings", "AutoClosePopup", -1, INIFileName);
	if (temp != -1) {
		bDiscardPopup = (temp != 0 ? 1 : 0);
		if (bdebugging) WriteChatf("INI Entry was already present ->temp: %i. Setting: %s",temp, (bDiscardPopup ? "\agOn" : "\arOff"));
	}
	else {
		WritePrivateProfileString("Settings", "AutoClosePopup", "0", INIFileName);
		if (bdebugging) WriteChatf("INI entry was \arnot\ax present ->temp: %i. Setting: %s", temp, (bDiscardPopup ? "\agOn" : "\arOff"));
	}
}