/*	-------------------------------------------------------------------------------------------------------
	© 1991-2012 Take-Two Interactive Software and its subsidiaries.  Developed by Firaxis Games.  
	Sid Meier's Civilization V, Civ, Civilization, 2K Games, Firaxis Games, Take-Two Interactive Software 
	and their respective logos are all trademarks of Take-Two interactive Software, Inc.  
	All other marks and trademarks are the property of their respective owners.  
	All rights reserved. 
	------------------------------------------------------------------------------------------------------- */

#include "CvGameCoreDLLPCH.h"
#include "CvDllGame.h"
#include "CvDllContext.h"
#include "CvDllGameDeals.h"
#include "CvDllPlayer.h"
#include "CvDllPlot.h"
#include "CvDllRandom.h"
#include "CvDllUnit.h"
#include "CvDllCity.h"

#include "CvGameTextMgr.h"

CvDllGame::CvDllGame(CvGame* pGame)
	: m_pGame(pGame)
	, m_uiRefCount(1)
{
	if(gDLL)
		gDLL->GetGameCoreLock();
		
	// Debug: Create marker file to track when DLL constructor is called
	FILE* markerFile = NULL;
	if (fopen_s(&markerFile, "CVDLLGAME_CONSTRUCTOR_CALLED.txt", "w") == 0 && markerFile != NULL) {
		fprintf(markerFile, "CvDllGame constructor was called\n");
		fprintf(markerFile, "MOD_BIN_HOOKS = %s\n", MOD_BIN_HOOKS ? "true" : "false");
		fclose(markerFile);
	}
	
	// Try to install binary hooks early, during DLL construction
	InstallBinaryHooksEarly();
}
//------------------------------------------------------------------------------
CvDllGame::~CvDllGame()
{
	if(gDLL)
		gDLL->ReleaseGameCoreLock();
}
//------------------------------------------------------------------------------
void* CvDllGame::QueryInterface(GUID guidInterface)
{
	if(guidInterface == ICvUnknown::GetInterfaceId() ||
		guidInterface == ICvGame1::GetInterfaceId()	 ||
		guidInterface == ICvGame2::GetInterfaceId())
	{
		IncrementReference();
		return this;
	}

	return NULL;
}
//------------------------------------------------------------------------------
unsigned int CvDllGame::IncrementReference()
{
	++m_uiRefCount;
	return m_uiRefCount;
}
//------------------------------------------------------------------------------
unsigned int CvDllGame::DecrementReference()
{
	if(m_uiRefCount == 1)
	{
		delete this;
		return 0;
	}
	else
	{
		--m_uiRefCount;
		return m_uiRefCount;
	}
}
//------------------------------------------------------------------------------
unsigned int CvDllGame::GetReferenceCount() const
{
	return m_uiRefCount;
}
//------------------------------------------------------------------------------
void CvDllGame::Destroy()
{
	DecrementReference();
}
//------------------------------------------------------------------------------
void CvDllGame::operator delete(void* p)
{
	CvDllGameContext::Free(p);
}
//------------------------------------------------------------------------------
void* CvDllGame::operator new(size_t bytes)
{
	return CvDllGameContext::Allocate(bytes);
}
//------------------------------------------------------------------------------
ICvPlayer1* CvDllGame::GetPlayer(const PlayerTypes ePlayer)
{
	return new CvDllPlayer(&CvPlayerAI::getPlayer(ePlayer));
}
//------------------------------------------------------------------------------
CvGame* CvDllGame::GetInstance()
{
	return m_pGame;
}
//------------------------------------------------------------------------------
PlayerTypes CvDllGame::GetActivePlayer()
{
	return m_pGame->getActivePlayer();
}
//------------------------------------------------------------------------------
TeamTypes CvDllGame::GetActiveTeam()
{
	return m_pGame->getActiveTeam();
}
//------------------------------------------------------------------------------
int CvDllGame::GetGameTurn() const
{
	return m_pGame->getGameTurn();
}
//------------------------------------------------------------------------------
void CvDllGame::ChangeNumGameTurnActive(int iChange, const char* why)
{
	std::string strWhy;
	if(why != NULL && strlen(why) > 0)
		strWhy = why;

	m_pGame->changeNumGameTurnActive(iChange, strWhy);
}
//------------------------------------------------------------------------------
int CvDllGame::CountHumanPlayersAlive() const
{
	return m_pGame->countHumanPlayersAlive();
}
//------------------------------------------------------------------------------
int CvDllGame::CountNumHumanGameTurnActive()
{
	return m_pGame->countNumHumanGameTurnActive();
}
//------------------------------------------------------------------------------
bool CvDllGame::CyclePlotUnits(ICvPlot1* pPlot, bool bForward, bool bAuto, int iCount)
{
	CvPlot* pkPlot = (NULL != pPlot)? static_cast<CvDllPlot*>(pPlot)->GetInstance() : NULL;
	return m_pGame->cyclePlotUnits(pkPlot, bForward, bAuto, iCount);
}
//------------------------------------------------------------------------------
void CvDllGame::CycleUnits(bool bClear, bool bForward, bool bWorkers)
{
	m_pGame->cycleUnits(bClear, bForward, bWorkers);
}
//------------------------------------------------------------------------------
void CvDllGame::DoGameStarted()
{
	m_pGame->DoGameStarted();

	if (MOD_BIN_HOOKS)
		InitExeStuff();
}
//------------------------------------------------------------------------------
void CvDllGame::EndTurnTimerReset()
{
	m_pGame->endTurnTimerReset();
}
//------------------------------------------------------------------------------
void CvDllGame::EndTurnTimerSemaphoreDecrement()
{
	m_pGame->endTurnTimerSemaphoreDecrement();
}
//------------------------------------------------------------------------------
void CvDllGame::EndTurnTimerSemaphoreIncrement()
{
	m_pGame->endTurnTimerSemaphoreIncrement();
}
//------------------------------------------------------------------------------
int CvDllGame::GetAction(int iKeyStroke, bool bAlt, bool bShift, bool bCtrl)
{
	return m_pGame->GetAction(iKeyStroke, bAlt, bShift, bCtrl);
}
//------------------------------------------------------------------------------
int CvDllGame::IsAction(int iKeyStroke, bool bAlt, bool bShift, bool bCtrl)
{
	return m_pGame->IsAction(iKeyStroke, bAlt, bShift, bCtrl);
}
//------------------------------------------------------------------------------
ImprovementTypes CvDllGame::GetBarbarianCampImprovementType()
{
	return m_pGame->GetBarbarianCampImprovementType();
}
//------------------------------------------------------------------------------
int CvDllGame::GetElapsedGameTurns() const
{
	return m_pGame->getElapsedGameTurns();
}
//------------------------------------------------------------------------------
bool CvDllGame::GetFOW()
{
	return m_pGame->getFOW();
}
//------------------------------------------------------------------------------
ICvGameDeals1* CvDllGame::GetGameDeals()
{
	return new CvDllGameDeals(&m_pGame->GetGameDeals());
}
//------------------------------------------------------------------------------
GameSpeedTypes CvDllGame::GetGameSpeedType() const
{
	return m_pGame->getGameSpeedType();
}
//------------------------------------------------------------------------------
GameStateTypes CvDllGame::GetGameState()
{
	return m_pGame->getGameState();
}
//------------------------------------------------------------------------------
HandicapTypes CvDllGame::GetHandicapType() const
{
	return m_pGame->getHandicapType();
}
//------------------------------------------------------------------------------
ICvRandom1* CvDllGame::GetRandomNumberGenerator()
{
	return new CvDllRandom(&m_pGame->getJonRand());
}
//------------------------------------------------------------------------------
int CvDllGame::GetJonRandNum(int iNum, const char* pszLog)
{
	return m_pGame->getJonRandNum(iNum, pszLog);
}
//------------------------------------------------------------------------------
int CvDllGame::GetMaxTurns() const
{
	return m_pGame->getMaxTurns();
}
//------------------------------------------------------------------------------
int CvDllGame::GetNumGameTurnActive()
{
	return m_pGame->getNumGameTurnActive();
}
//------------------------------------------------------------------------------
PlayerTypes CvDllGame::GetPausePlayer()
{
	return m_pGame->getPausePlayer();
}
//------------------------------------------------------------------------------
bool CvDllGame::GetPbemTurnSent() const
{
	return m_pGame->getPbemTurnSent();
}
//------------------------------------------------------------------------------
ICvUnit1* CvDllGame::GetPlotUnit(ICvPlot1* pPlot, int iIndex)
{
	CvPlot* pkPlot = (NULL != pPlot)? static_cast<CvDllPlot*>(pPlot)->GetInstance() : NULL;
	CvUnit* pkUnit = m_pGame->getPlotUnit(pkPlot, iIndex);

	return (NULL != pkUnit)? new CvDllUnit(pkUnit) : NULL;
}
//------------------------------------------------------------------------------
unsigned int CvDllGame::GetVariableCitySizeFromPopulation(unsigned int nPopulation)
{
	return m_pGame->GetVariableCitySizeFromPopulation(nPopulation);
}
//------------------------------------------------------------------------------
VictoryTypes CvDllGame::GetVictory() const
{
	return m_pGame->getVictory();
}
//------------------------------------------------------------------------------
int CvDllGame::GetVotesNeededForDiploVictory() const
{
	return m_pGame->GetVotesNeededForDiploVictory();
}
//------------------------------------------------------------------------------
TeamTypes CvDllGame::GetWinner() const
{
	return m_pGame->getWinner();
}
//------------------------------------------------------------------------------
int CvDllGame::GetWinningTurn() const
{
	return m_pGame->GetWinningTurn();
}
//------------------------------------------------------------------------------
void CvDllGame::HandleAction(int iAction)
{
	m_pGame->handleAction(iAction);
}
//------------------------------------------------------------------------------
bool CvDllGame::HasTurnTimerExpired()
{
	ASSERT(0, "Obsolete");
	return false;
}
//------------------------------------------------------------------------------
bool CvDllGame::HasTurnTimerExpired(PlayerTypes playerID)
{
	return m_pGame->hasTurnTimerExpired(playerID);
}
//------------------------------------------------------------------------------
void CvDllGame::TurnTimerSync(float fCurTurnTime, float fTurnStartTime)
{
	m_pGame->TurnTimerSync(fCurTurnTime, fTurnStartTime);
}
//------------------------------------------------------------------------------
void CvDllGame::GetTurnTimerData(float& fCurTurnTime, float& fTurnStartTime)
{
	m_pGame->GetTurnTimerData(fCurTurnTime, fTurnStartTime);
}
//------------------------------------------------------------------------------
void CvDllGame::Init(HandicapTypes eHandicap)
{
	m_pGame->init(eHandicap);
}
//------------------------------------------------------------------------------
bool CvDllGame::Init2()
{
	return m_pGame->init2();
}
//------------------------------------------------------------------------------
void CvDllGame::InitScoreCalculation()
{
	m_pGame->initScoreCalculation();
}
//------------------------------------------------------------------------------
void CvDllGame::InitTacticalAnalysisMap(int iNumPlots)
{
	//handled in Tactical AI now ...
}
//------------------------------------------------------------------------------
bool CvDllGame::IsCityScreenBlocked()
{
	return m_pGame->IsCityScreenBlocked();
}
//------------------------------------------------------------------------------
bool CvDllGame::CanOpenCityScreen(PlayerTypes eOpener, ICvCity1* pkCity)
{
	CvCity* pCity = GC.UnwrapCityPointer(pkCity);
	return m_pGame->CanOpenCityScreen(eOpener, pCity);
}
//------------------------------------------------------------------------------
bool CvDllGame::IsDebugMode() const
{
	return m_pGame->isDebugMode();
}
//------------------------------------------------------------------------------
bool CvDllGame::IsFinalInitialized() const
{
	return m_pGame->isFinalInitialized();
}
//------------------------------------------------------------------------------
bool CvDllGame::IsGameMultiPlayer() const
{
	return m_pGame->isGameMultiPlayer();
}
//------------------------------------------------------------------------------
bool CvDllGame::IsHotSeat() const
{
	return m_pGame->isHotSeat();
}
//------------------------------------------------------------------------------
bool CvDllGame::IsMPOption(MultiplayerOptionTypes eIndex) const
{
	return m_pGame->isMPOption(eIndex);
}
//------------------------------------------------------------------------------
bool CvDllGame::IsNetworkMultiPlayer() const
{
	return m_pGame->isNetworkMultiPlayer();
}
//------------------------------------------------------------------------------
bool CvDllGame::IsOption(GameOptionTypes eIndex) const
{
	return m_pGame->isOption(eIndex);
}
//------------------------------------------------------------------------------
bool CvDllGame::IsPaused()
{
	return m_pGame->isPaused();
}
//------------------------------------------------------------------------------
bool CvDllGame::IsPbem() const
{
	return m_pGame->isPbem();
}
//------------------------------------------------------------------------------
bool CvDllGame::IsTeamGame() const
{
	return m_pGame->isTeamGame();
}
//------------------------------------------------------------------------------
void CvDllGame::LogGameState(bool bLogHeaders)
{
	m_pGame->LogGameState(bLogHeaders);
}
//------------------------------------------------------------------------------
void CvDllGame::ResetTurnTimer()
{
	m_pGame->resetTurnTimer();
}
//------------------------------------------------------------------------------
void CvDllGame::SelectAll(ICvPlot1* pPlot)
{
	CvPlot* pkPlot = (NULL != pPlot)? static_cast<CvDllPlot*>(pPlot)->GetInstance() : NULL;
	m_pGame->selectAll(pkPlot);
}
//------------------------------------------------------------------------------
void CvDllGame::SelectGroup(ICvUnit1* pUnit, bool bShift, bool bCtrl, bool bAlt)
{
	CvUnit* pkUnit = (NULL != pUnit)? static_cast<CvDllUnit*>(pUnit)->GetInstance() : NULL;
	m_pGame->selectGroup(pkUnit, bShift, bCtrl, bAlt);
}
//------------------------------------------------------------------------------
bool CvDllGame::SelectionListIgnoreBuildingDefense()
{
	return m_pGame->selectionListIgnoreBuildingDefense();
}
//------------------------------------------------------------------------------
void CvDllGame::SelectionListMove(ICvPlot1* pPlot, bool bShift)
{
	CvPlot* pkPlot = (NULL != pPlot)? static_cast<CvDllPlot*>(pPlot)->GetInstance() : NULL;
	m_pGame->selectionListMove(pkPlot, bShift);
}
//------------------------------------------------------------------------------
void CvDllGame::SelectSettler()
{
	m_pGame->SelectSettler();
}
//------------------------------------------------------------------------------
void CvDllGame::SelectUnit(ICvUnit1* pUnit, bool bClear, bool bToggle, bool bSound)
{
	CvUnit* pkUnit = (NULL != pUnit)? static_cast<CvDllUnit*>(pUnit)->GetInstance() : NULL;
	m_pGame->selectUnit(pkUnit, bClear, bToggle, bSound);
}
//------------------------------------------------------------------------------
void CvDllGame::SendPlayerOptions(bool bForce)
{
	m_pGame->sendPlayerOptions(bForce);
}
//------------------------------------------------------------------------------
void CvDllGame::SetDebugMode(bool bDebugMode)
{
	m_pGame->setDebugMode(bDebugMode);
}
//------------------------------------------------------------------------------
void CvDllGame::SetFinalInitialized(bool bNewValue)
{
	m_pGame->setFinalInitialized(bNewValue);
}
//------------------------------------------------------------------------------
void CvDllGame::SetInitialTime(unsigned int uiNewValue)
{
	m_pGame->setInitialTime(uiNewValue);
}
//------------------------------------------------------------------------------
void CvDllGame::SetMPOption(MultiplayerOptionTypes eIndex, bool bEnabled)
{
	m_pGame->setMPOption(eIndex, bEnabled);
}
//------------------------------------------------------------------------------
void CvDllGame::SetPausePlayer(PlayerTypes eNewValue)
{
	m_pGame->setPausePlayer(eNewValue);
}
//------------------------------------------------------------------------------
void CvDllGame::SetTimeStr(_Inout_z_cap_c_(256) char* szString, int iGameTurn, bool bSave)
{
	if(szString)
	{
		CvString strString;
		CvGameTextMgr::setDateStr(strString,
		                          iGameTurn,
		                          bSave,
		                          m_pGame->getCalendar(),
		                          m_pGame->getStartYear(),
		                          m_pGame->getGameSpeedType());

		strcpy_s(szString, 256, strString.c_str());
	}
}
//------------------------------------------------------------------------------
bool CvDllGame::TunerEverConnected() const
{
	return m_pGame->TunerEverConnected();
}
//------------------------------------------------------------------------------
void CvDllGame::Uninit()
{
	// There exists a bug that allows the game core to be shutdown early if mods are loaded.
	// Executing `SetGameViewRenderType(GameViewTypes.GAMEVIEW_NONE)` in Lua will trigger this.
	// 
	// Because the first step of the shutdown process deinitializes the game instance, this
	// should trap before the program has a chance to enter a broken state which hopefully
	// protects users from any exploits that could be leveraged while the state is broken.
	if (CvPreGame::gameStarted())
		BUILTIN_TRAP();

	m_pGame->uninit();
}
//------------------------------------------------------------------------------
void CvDllGame::UnitIsMoving()
{
	m_pGame->unitIsMoving();
}
//------------------------------------------------------------------------------
void CvDllGame::Update()
{
	m_pGame->update();
}
//------------------------------------------------------------------------------
void CvDllGame::UpdateSelectionList()
{
	m_pGame->updateSelectionList();
}
//------------------------------------------------------------------------------
void CvDllGame::UpdateTestEndTurn()
{
	m_pGame->updateTestEndTurn();
}
//------------------------------------------------------------------------------
void CvDllGame::Read(FDataStream& kStream)
{
	m_pGame->Read(kStream);
}
//------------------------------------------------------------------------------
void CvDllGame::Write(FDataStream& kStream) const
{
	m_pGame->Write(kStream);
#ifdef EA_EVENT_GAME_SAVE // Paz - set m_bSavedOnce = true AFTER the first save
	m_pGame->SetGameEventsSaveGame(true);
#endif
}
//------------------------------------------------------------------------------
void CvDllGame::ReadSupportingClassData(FDataStream& kStream)
{
	m_pGame->ReadSupportingClassData(kStream);
}
//------------------------------------------------------------------------------
void CvDllGame::WriteSupportingClassData(FDataStream& kStream) const
{
	m_pGame->WriteSupportingClassData(kStream);
}
//------------------------------------------------------------------------------
void CvDllGame::WriteReplay(FDataStream& kStream) const
{
	m_pGame->writeReplay(kStream);
}
//------------------------------------------------------------------------------
bool CvDllGame::CanMoveUnitTo(ICvUnit1* pUnit, ICvPlot1* pPlot) const
{
	if(pUnit == NULL || pPlot == NULL)
	{
		return false;
	}

	CvUnit* pkUnit = GC.UnwrapUnitPointer(pUnit);
	CvPlot* pkPlot = GC.UnwrapPlotPointer(pPlot);

	SPathFinderUserData data(pkUnit,CvUnit::MOVEFLAG_IGNORE_DANGER | CvUnit::MOVEFLAG_IGNORE_STACKING_SELF,1);

	// can the unit actually walk there
	return GC.GetPathFinder().DoesPathExist(pkUnit->getX(), pkUnit->getY(), pkPlot->getX(), pkPlot->getY(), data);
}

//------------------------------------------------------------------------------
void CvDllGame::NetMessageStaticsReset()
{
	m_pGame->NetMessageStaticsReset();
}
//------------------------------------------------------------------------------
bool CvDllGame::GetGreatWorkAudio(int GreatWorkIndex, char* strSound, int length)
{
	if (GreatWorkIndex == -1)
	{
		return false;
	}

	const GreatWorkType eType = m_pGame->GetGameCulture()->GetGreatWorkType(GreatWorkIndex);
	if(eType != NO_GREAT_WORK)
	{
		const CvString audio = CultureHelpers::GetGreatWorkAudio(eType);

		if(audio.GetLength() <= length)
		{
			if(strcpy_s(strSound, length, audio.c_str()) == 0)
			{
				return true;
			}
		}
	}

	return false;
}
//------------------------------------------------------------------------------
void CvDllGame::SetLastTurnAICivsProcessed()
{
	m_pGame->SetLastTurnAICivsProcessed();
}
//------------------------------------------------------------------------------
bool endsWith(const char* str, const char* ending)
{
	size_t str_len = strlen(str);
	size_t ending_len = strlen(ending);
	return str_len >= ending_len && !strcmp(str + str_len - ending_len, ending);
}

#ifdef WIN32
// Original function pointer for individual mod deactivation (Lua: DisableMod)
typedef int (__cdecl *DeactivateModsFunc)();
DeactivateModsFunc g_originalDeactivateMods = NULL;

// Original function pointer for bulk mod deactivation (ExecuteMultiple: "UPDATE Mods Set Activated = 0")
typedef bool (__thiscall *BulkDeactivateFunc)(void* this_ptr);
BulkDeactivateFunc g_originalBulkDeactivate = NULL;

// Hook function that intercepts individual mod disabling
// This prevents "UPDATE Mods Set Enabled = 0 WHERE ModID = ?" during multiplayer setup
int __cdecl HookedDeactivateMods()
{
	// Debug: Write to a log file that we can check
	FILE* logFile = NULL;
	FILE* debugFile = NULL;
	
	if (fopen_s(&logFile, "Logs/MOD_HOOK_DEBUG.log", "a") == 0 && logFile != NULL) {
		fprintf(logFile, "[MOD_HOOK] Individual mod disable function intercepted!\n");
		fflush(logFile);
	}
	
	// Also create a simple debug file to make it obvious the hook was called
	if (fopen_s(&debugFile, "HOOK_EXECUTION_DEBUG.txt", "a") == 0 && debugFile != NULL) {
		fprintf(debugFile, "Individual mod disable function intercepted!\n");
		fflush(debugFile);
		fclose(debugFile);
	}
	
	// Check game state
	bool isNetworkMP = false;
	bool isHotSeat = false;
	bool isPbem = false;
	
	try {
		isNetworkMP = GC.getGame().isNetworkMultiPlayer();
		isHotSeat = GC.getGame().isHotSeat();
		isPbem = GC.getGame().isPbem();
	} catch (...) {
		// Game state might not be initialized yet
		if (logFile) {
			fprintf(logFile, "[MOD_HOOK] Game state not accessible, assuming multiplayer\n");
			fflush(logFile);
		}
	}
	
	// Log the detected game mode
	if (logFile) {
		if (isNetworkMP) {
			fprintf(logFile, "[MOD_HOOK] Network multiplayer detected - preserving mods\n");
		} else if (isHotSeat) {
			fprintf(logFile, "[MOD_HOOK] HotSeat detected - preserving mods\n");
		} else if (isPbem) {
			fprintf(logFile, "[MOD_HOOK] PBEM detected - preserving mods\n");
		} else {
			fprintf(logFile, "[MOD_HOOK] Single player or unknown mode detected\n");
		}
		
		// For now, always preserve mods to test if the hook is working
		fprintf(logFile, "[MOD_HOOK] Preserving mods (bypassing deactivation)\n");
		fflush(logFile);
		fclose(logFile);
	}
	
	return 1; // Return success without deactivating mods
	
	// Original logic (commented out for testing):
	/*
	if (isNetworkMP || isHotSeat || isPbem)
	{
		// In multiplayer, we skip the deactivation to preserve mod compatibility
		// This prevents the problematic "UPDATE Mods Set Activated = 0" SQL query
		return 1; // Return success without deactivating mods
	}
	
	// In single player, call the original function
	if (g_originalDeactivateMods)
		return g_originalDeactivateMods();
	
	return 1; // Fallback success
	*/
}

// Hook function that intercepts bulk mod deactivation
// This prevents "BEGIN; UPDATE Mods Set Activated = 0; END;" during multiplayer setup
bool __fastcall HookedBulkDeactivate(void* this_ptr, void* edx)
{
	// Debug: Write to a log file that we can check
	FILE* logFile = NULL;
	FILE* debugFile = NULL;
	
	if (fopen_s(&logFile, "Logs/MOD_HOOK_DEBUG.log", "a") == 0 && logFile != NULL) {
		fprintf(logFile, "[MOD_HOOK] HookedBulkDeactivate called - BULK mod deactivation intercepted!\n");
		fflush(logFile);
	}
	
	if (fopen_s(&debugFile, "HOOK_EXECUTION_DEBUG.txt", "a") == 0 && debugFile != NULL) {
		fprintf(debugFile, "BULK DEACTIVATION HOOK EXECUTED - Preventing bulk mod deactivation\n");
		fflush(debugFile);
	}
	
	// Check if we're in multiplayer mode
	bool isMultiplayer = false;
	
	// Try to get the game instance to check multiplayer status
	CvGame* pGame = GC.getGamePointer();
	if (pGame != NULL) {
		isMultiplayer = pGame->isNetworkMultiPlayer() || pGame->isHotSeat() || pGame->isPbem();
		
		if (logFile) {
			fprintf(logFile, "[MOD_HOOK] HookedBulkDeactivate: Multiplayer status = %s\n", 
				isMultiplayer ? "TRUE" : "FALSE");
			fflush(logFile);
		}
		if (debugFile) {
			fprintf(debugFile, "Multiplayer status = %s\n", isMultiplayer ? "TRUE" : "FALSE");
			fflush(debugFile);
		}
	}
	
	if (isMultiplayer) {
		if (logFile) {
			fprintf(logFile, "[MOD_HOOK] HookedBulkDeactivate: BLOCKING bulk mod deactivation in multiplayer!\n");
			fflush(logFile);
		}
		if (debugFile) {
			fprintf(debugFile, "BLOCKING bulk mod deactivation in multiplayer!\n");
			fflush(debugFile);
		}
		
		// Close files
		if (logFile) fclose(logFile);
		if (debugFile) fclose(debugFile);
		
		// Return success without deactivating mods
		return true;
	}
	
	// Close files
	if (logFile) fclose(logFile);
	if (debugFile) fclose(debugFile);
	
	// In single player, call the original function
	if (g_originalBulkDeactivate) {
		// Use __thiscall convention - pass this_ptr as ECX register
		return g_originalBulkDeactivate(this_ptr);
	}
	
	return true; // Fallback success
}

void CvDllGame::HookDeactivateModsFunction(DWORD functionAddr)
{
	// Debug: Log hook installation attempt to file
	FILE* logFile = NULL;
	FILE* debugFile = NULL;
	
	if (fopen_s(&logFile, "Logs/MOD_HOOK_DEBUG.log", "a") == 0 && logFile != NULL) {
		fprintf(logFile, "[MOD_HOOK] Attempting to hook DeactivateMods at address 0x%08lX\n", functionAddr);
		fflush(logFile);
	}
	
	if (fopen_s(&debugFile, "HOOK_INSTALL_DEBUG.txt", "a") == 0 && debugFile != NULL) {
		fprintf(debugFile, "HookDeactivateModsFunction called with address 0x%08lX\n", functionAddr);
		fflush(debugFile);
	}
	
	DWORD old_protect;
	if (VirtualProtect((void*)functionAddr, 5, PAGE_EXECUTE_READWRITE, &old_protect))
	{
		if (logFile) {
			fprintf(logFile, "[MOD_HOOK] VirtualProtect succeeded, installing hook\n");
			fflush(logFile);
		}
		if (debugFile) {
			fprintf(debugFile, "VirtualProtect succeeded, installing hook\n");
			fflush(debugFile);
		}
		
		// Store original function pointer (first 5 bytes will be overwritten)
		g_originalDeactivateMods = (DeactivateModsFunc)functionAddr;
		
		// Create a JMP instruction to our hook function
		DWORD hookAddr = (DWORD)&HookedDeactivateMods;
		DWORD relativeAddr = hookAddr - functionAddr - 5;
		
		if (logFile) {
			fprintf(logFile, "[MOD_HOOK] Hook function at 0x%08lX, relative addr: 0x%08lX\n", hookAddr, relativeAddr);
			fflush(logFile);
		}
		if (debugFile) {
			fprintf(debugFile, "Hook function at 0x%08lX, relative addr: 0x%08lX\n", hookAddr, relativeAddr);
			fflush(debugFile);
		}
		
		// Write JMP instruction (0xE9 followed by relative address)
		*(unsigned char*)functionAddr = 0xE9;
		*(DWORD*)(functionAddr + 1) = relativeAddr;
		
		VirtualProtect((void*)functionAddr, 5, old_protect, &old_protect);
		
		if (logFile) {
			fprintf(logFile, "[MOD_HOOK] Hook installation completed successfully\n");
			fflush(logFile);
		}
		if (debugFile) {
			fprintf(debugFile, "Hook installation completed successfully\n");
			fflush(debugFile);
		}
	}
	else
	{
		if (logFile) {
			fprintf(logFile, "[MOD_HOOK] VirtualProtect failed - hook installation failed\n");
			fflush(logFile);
		}
		if (debugFile) {
			fprintf(debugFile, "VirtualProtect failed - hook installation failed\n");
			fflush(debugFile);
		}
	}
	
	if (logFile) {
		fclose(logFile);
	}
	if (debugFile) {
		fclose(debugFile);
	}
}

void CvDllGame::HookBulkDeactivateFunction(DWORD functionAddr)
{
	// Debug: Log hook installation attempt to file
	FILE* logFile = NULL;
	FILE* debugFile = NULL;
	
	if (fopen_s(&logFile, "Logs/MOD_HOOK_DEBUG.log", "a") == 0 && logFile != NULL) {
		fprintf(logFile, "[MOD_HOOK] Attempting to hook BulkDeactivate at address 0x%08lX\n", functionAddr);
		fflush(logFile);
	}
	
	if (fopen_s(&debugFile, "HOOK_INSTALL_DEBUG.txt", "a") == 0 && debugFile != NULL) {
		fprintf(debugFile, "HookBulkDeactivateFunction called with address 0x%08lX\n", functionAddr);
		fflush(debugFile);
	}
	
	DWORD old_protect;
	if (VirtualProtect((void*)functionAddr, 5, PAGE_EXECUTE_READWRITE, &old_protect))
	{
		if (logFile) {
			fprintf(logFile, "[MOD_HOOK] VirtualProtect succeeded, installing bulk hook\n");
			fflush(logFile);
		}
		if (debugFile) {
			fprintf(debugFile, "VirtualProtect succeeded, installing bulk hook\n");
			fflush(debugFile);
		}
		
		// Store original function pointer (first 5 bytes will be overwritten)
		g_originalBulkDeactivate = (BulkDeactivateFunc)functionAddr;
		
		// Create a JMP instruction to our hook function
		DWORD hookAddr = (DWORD)&HookedBulkDeactivate;
		DWORD relativeAddr = hookAddr - functionAddr - 5;
		
		if (logFile) {
			fprintf(logFile, "[MOD_HOOK] Bulk hook function at 0x%08lX, relative addr: 0x%08lX\n", hookAddr, relativeAddr);
			fflush(logFile);
		}
		if (debugFile) {
			fprintf(debugFile, "Bulk hook function at 0x%08lX, relative addr: 0x%08lX\n", hookAddr, relativeAddr);
			fflush(debugFile);
		}
		
		// Write JMP instruction (0xE9 followed by relative address)
		*(unsigned char*)functionAddr = 0xE9;
		*(DWORD*)(functionAddr + 1) = relativeAddr;
		
		VirtualProtect((void*)functionAddr, 5, old_protect, &old_protect);
		
		if (logFile) {
			fprintf(logFile, "[MOD_HOOK] Bulk hook installation completed successfully\n");
			fflush(logFile);
		}
		if (debugFile) {
			fprintf(debugFile, "Bulk hook installation completed successfully\n");
			fflush(debugFile);
		}
	}
	else
	{
		if (logFile) {
			fprintf(logFile, "[MOD_HOOK] VirtualProtect failed - bulk hook installation failed\n");
			fflush(logFile);
		}
		if (debugFile) {
			fprintf(debugFile, "VirtualProtect failed - bulk hook installation failed\n");
			fflush(debugFile);
		}
	}
	
	if (logFile) {
		fclose(logFile);
	}
	if (debugFile) {
		fclose(debugFile);
	}
}
#endif

void CvDllGame::StartModStatusMonitoring()
{
	// Create a background thread to monitor mod status changes
	// This will help us detect when mods get deactivated even if our hooks aren't called
	
	FILE* monitorFile = NULL;
	if (fopen_s(&monitorFile, "MOD_MONITOR_STARTED.txt", "w") == 0 && monitorFile != NULL) {
		fprintf(monitorFile, "Mod status monitoring started\n");
		fclose(monitorFile);
	}
	
	// For now, just log that monitoring started
	// In a full implementation, we'd create a thread that periodically checks the database
	// and logs when the mod count changes
}

void CvDllGame::InstallBinaryHooksEarly()
{
#ifdef WIN32
	// Prevent multiple hook installations
	static bool hooksInstalled = false;
	if (hooksInstalled) {
		return;
	}
	
	// Install binary hooks early during DLL construction to catch multiplayer mod deactivation
	FILE* logFile = NULL;
	FILE* debugFile = NULL;
	
	// Try to create log in Logs directory
	if (fopen_s(&logFile, "Logs/MOD_HOOK_DEBUG.log", "a") == 0 && logFile != NULL) {
		fprintf(logFile, "[MOD_HOOK] InstallBinaryHooksEarly: MOD_BIN_HOOKS = %s\n", 
			MOD_BIN_HOOKS ? "true" : "false");
		fflush(logFile);
	}
	
	// Also create a simple debug file in current directory as fallback
	if (fopen_s(&debugFile, "HOOK_DEBUG.txt", "w") == 0 && debugFile != NULL) {
		fprintf(debugFile, "InstallBinaryHooksEarly called\n");
		fprintf(debugFile, "MOD_BIN_HOOKS = %s\n", MOD_BIN_HOOKS ? "true" : "false");
		fflush(debugFile);
	}
	
	// Force early check of BIN_HOOKS directly from database since CustomMods might not be initialized yet
	bool binHooksEnabled = false;
	try {
		// Try to get BIN_HOOKS value directly from database
		Database::Connection* db = GC.GetGameDatabase();
		if (db) {
			Database::Results kResults;
			if (db->SelectWhere(kResults, "CustomModOptions", "Name='BIN_HOOKS'")) {
				if (kResults.Step()) {
					binHooksEnabled = (kResults.GetInt("Value") == 1);
					if (debugFile) {
						fprintf(debugFile, "Direct database check: BIN_HOOKS = %s\n", binHooksEnabled ? "true" : "false");
						fflush(debugFile);
					}
				}
			}
		}
	} catch (...) {
		// Fallback to MOD_BIN_HOOKS macro if database access fails
		binHooksEnabled = MOD_BIN_HOOKS;
		if (debugFile) {
			fprintf(debugFile, "Database access failed, using MOD_BIN_HOOKS macro: %s\n", binHooksEnabled ? "true" : "false");
			fflush(debugFile);
		}
	}
	
	if (binHooksEnabled)
	{
		if (debugFile) {
			fprintf(debugFile, "BIN_HOOKS is enabled, proceeding with hook installation\n");
			fflush(debugFile);
		}
		
		// Get module handle and detect binary type
		HMODULE hModule = GetModuleHandleA(NULL);
		if (hModule)
		{
			if (debugFile) {
				fprintf(debugFile, "Got module handle, trying hook addresses\n");
				fflush(debugFile);
			}
			
			// Hook BOTH individual AND bulk mod deactivation functions to cover all scenarios
			
			// 1. Individual mod disable functions (Lua API: DisableMod) - "UPDATE Mods Set Enabled = 0 WHERE ModID = ?"
			DWORD individualAddresses[] = {
				0x007B9510,  // DX9 - sub_7B9510 (Lua: DisableMod)
				0x007C1ED0,  // DX11 - sub_7C1ED0 (Lua: DisableMod)
				0x007C2F80   // Tablet - sub_7C2F80 (Lua: DisableMod)
			};
			
			// 2. Bulk mod deactivation functions - "BEGIN; UPDATE Mods Set Activated = 0; END;"
			DWORD bulkAddresses[] = {
				0x007B91F0,  // DX9 - sub_7B91F0 (bulk deactivation)
				0x007C1BB0,  // DX11 - sub_7C1BB0 (bulk deactivation)
				0x007C2C60   // Tablet - sub_7C2C60 (bulk deactivation)
			};
			
			const char* types[] = { "DX9", "DX11", "Tablet" };
			
			// Hook individual mod disable functions
			if (debugFile) {
				fprintf(debugFile, "=== HOOKING INDIVIDUAL MOD DISABLE FUNCTIONS ===\n");
				fflush(debugFile);
			}
			
			for (int i = 0; i < 3; i++)
			{
				DWORD deactivateModsAddr = individualAddresses[i];
				
				if (logFile) {
					fprintf(logFile, "[MOD_HOOK] InstallBinaryHooksEarly: Hooking %s individual mod disable function at 0x%08lX\n", 
						types[i], deactivateModsAddr);
					fflush(logFile);
				}
				
				if (debugFile) {
					fprintf(debugFile, "Trying %s individual address 0x%08lX\n", types[i], deactivateModsAddr);
					fflush(debugFile);
				}
				
				if (deactivateModsAddr != 0)
				{
					if (debugFile) {
						fprintf(debugFile, "Calling HookDeactivateModsFunction for %s individual\n", types[i]);
						fflush(debugFile);
					}
					
					HookDeactivateModsFunction(deactivateModsAddr);
				}
			}
			
			// Hook bulk mod deactivation functions
			if (debugFile) {
				fprintf(debugFile, "=== HOOKING BULK MOD DEACTIVATION FUNCTIONS ===\n");
				fflush(debugFile);
			}
			
			for (int i = 0; i < 3; i++)
			{
				DWORD bulkDeactivateAddr = bulkAddresses[i];
				
				if (logFile) {
					fprintf(logFile, "[MOD_HOOK] InstallBinaryHooksEarly: Hooking %s bulk mod deactivation function at 0x%08lX\n", 
						types[i], bulkDeactivateAddr);
					fflush(logFile);
				}
				
				if (debugFile) {
					fprintf(debugFile, "Trying %s bulk address 0x%08lX\n", types[i], bulkDeactivateAddr);
					fflush(debugFile);
				}
				
				if (bulkDeactivateAddr != 0)
				{
					if (debugFile) {
						fprintf(debugFile, "Calling HookBulkDeactivateFunction for %s bulk\n", types[i]);
						fflush(debugFile);
					}
					
					HookBulkDeactivateFunction(bulkDeactivateAddr);
				}
			}
		}
		else
		{
			if (logFile) {
				fprintf(logFile, "[MOD_HOOK] InstallBinaryHooksEarly: Failed to get module handle\n");
				fflush(logFile);
			}
			if (debugFile) {
				fprintf(debugFile, "Failed to get module handle\n");
				fflush(debugFile);
			}
		}
	}
	else
	{
		if (debugFile) {
			fprintf(debugFile, "MOD_BIN_HOOKS is disabled - no hook installation\n");
			fflush(debugFile);
		}
	}
	
	if (logFile) {
		fclose(logFile);
	}
	if (debugFile) {
		fclose(debugFile);
	}
	
	// Mark hooks as installed to prevent repeated installations
	hooksInstalled = true;
	
	// Start monitoring mod status to detect when deactivation occurs
	StartModStatusMonitoring();
#endif
}

void CvDllGame::InitExeStuff()
{
	// Runtime interoperability layer for multiplayer synchronization features
	
	// Simple debug - create a marker file to confirm InitExeStuff is called
	FILE* markerFile = NULL;
	if (fopen_s(&markerFile, "INITEXESTUFF_CALLED.txt", "w") == 0 && markerFile != NULL) {
		fprintf(markerFile, "InitExeStuff was called\n");
		fclose(markerFile);
	}
	// 
	// This function establishes communication with the host application to enable
	// enhanced multiplayer functionality through standard Windows API calls.
	// Implementation varies by binary variant to ensure compatibility across
	// different game configurations and platforms.
	//
	// The system provides access to internal game state variables that are
	// necessary for advanced multiplayer coordination features, particularly
	// for host-controlled synchronization operations.
	//
	// todo: support additional binary variants (dx9, tablet)
	// todo: enhanced error handling and logging
	// todo: optimize memory usage patterns
	CvBinType binType;

	char moduleName[1024];
	if (!GetModuleFileName(NULL, moduleName, sizeof(moduleName)))
	{
		// todo: log error (GetLastError)
		binType = BIN_UNKNOWN;
	}
	else if (endsWith(moduleName, "CivilizationV.exe"))
		binType = BIN_DX9;
	else if (endsWith(moduleName, "CivilizationV_DX11.exe"))
		binType = BIN_DX11;
	else if (endsWith(moduleName, "CivilizationV_Tablet.exe"))
		binType = BIN_TABLET;
	else
	{
		// todo: log moduleName
		binType = BIN_UNKNOWN;
	}

	m_pGame->SetExeBinType(binType);

#ifdef WIN32
	if (binType == BIN_DX11 || binType == BIN_DX9 || binType == BIN_TABLET)
	{
		DWORD baseAddr = (DWORD) GetModuleHandleA(NULL);
		DWORD headersOffset = 0x400000;
		DWORD totalOffset = baseAddr - headersOffset;

		DWORD wantForceResyncAddr = 0;
		if (binType == BIN_DX11)
		{
			wantForceResyncAddr = 0x02dd2f68;
		}
		else if (binType == BIN_DX9)
		{
			wantForceResyncAddr = 0x02dc2d78;
		}
		else if (binType == BIN_TABLET)
		{
			wantForceResyncAddr = 0x02dd4f60;
		}

		if (wantForceResyncAddr != 0)
		{
			int* s_wantForceResync = reinterpret_cast<int*>(wantForceResyncAddr + totalOffset);
			m_pGame->SetExeWantForceResyncPointer(s_wantForceResync);
		}

		// Hook Modding::System::DeactivateMods to preserve multiplayer-compatible mods
		FILE* logFile = NULL;
		if (fopen_s(&logFile, "Logs/MOD_HOOK_DEBUG.log", "a") == 0 && logFile != NULL) {
			fprintf(logFile, "[MOD_HOOK] InitExeStuff: MOD_BIN_HOOKS = %s, binType = %d\n", 
				MOD_BIN_HOOKS ? "true" : "false", binType);
			fflush(logFile);
		}
		
		if (MOD_BIN_HOOKS)
		{
			DWORD deactivateModsAddr = 0;
			if (binType == BIN_DX11)
				deactivateModsAddr = 0x007C1BB0;  // sub_7C1BB0 - calls "UPDATE Mods Set Activated = 0"
			else if (binType == BIN_DX9)
				deactivateModsAddr = 0x007B91F0;  // sub_7B91F0 - calls "UPDATE Mods Set Activated = 0"
			else if (binType == BIN_TABLET)
				deactivateModsAddr = 0x007C2C60;  // sub_7C2C60 - calls "UPDATE Mods Set Activated = 0"

			if (logFile) {
				fprintf(logFile, "[MOD_HOOK] InitExeStuff: deactivateModsAddr = 0x%08lX, totalOffset = 0x%08lX\n", 
					deactivateModsAddr, totalOffset);
				fflush(logFile);
			}

			if (deactivateModsAddr != 0)
			{
				if (logFile) {
					fprintf(logFile, "[MOD_HOOK] InitExeStuff: Calling HookDeactivateModsFunction\n");
					fflush(logFile);
				}
				HookDeactivateModsFunction(deactivateModsAddr + totalOffset);
			}
			else
			{
				if (logFile) {
					fprintf(logFile, "[MOD_HOOK] InitExeStuff: No address found for binType %d\n", binType);
					fflush(logFile);
				}
			}
		}
		
		if (logFile) {
			fclose(logFile);
		}
	}
#endif

	/*{
	    // the very basic example of how to fill something with NOPs
		DWORD old_protect;
		DWORD hookLocation = 0x51e031;
		DWORD hookResultAddress = hookLocation + totalOffset;
		if (VirtualProtect((void*)(hookResultAddress), 16, PAGE_EXECUTE_READWRITE, &old_protect))
		{
			*(unsigned char*)(hookResultAddress) = 0x90;
			*(unsigned char*)(hookResultAddress + 1) = 0x90;
			*(unsigned char*)(hookResultAddress + 2) = 0x90;
			*(unsigned char*)(hookResultAddress + 3) = 0x90;
			*(unsigned char*)(hookResultAddress + 4) = 0x90;
			*(unsigned char*)(hookResultAddress + 5) = 0x90;
			*(unsigned char*)(hookResultAddress + 6) = 0x90;
			*(unsigned char*)(hookResultAddress + 7) = 0x90;
			*(unsigned char*)(hookResultAddress + 8) = 0x90;
			*(unsigned char*)(hookResultAddress + 9) = 0x90;
			*(unsigned char*)(hookResultAddress + 10) = 0x90;
			*(unsigned char*)(hookResultAddress + 11) = 0x90;
			*(unsigned char*)(hookResultAddress + 12) = 0x90;
			*(unsigned char*)(hookResultAddress + 13) = 0x90;
			*(unsigned char*)(hookResultAddress + 14) = 0x90;
			*(unsigned char*)(hookResultAddress + 15) = 0x90;
			VirtualProtect((void*)(hookResultAddress), 16, old_protect, &old_protect);
		}
	}*/
}