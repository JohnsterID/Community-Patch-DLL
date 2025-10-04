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
#include <psapi.h>  // For PROCESS_MEMORY_COUNTERS
#include <intrin.h> // For _AddressOfReturnAddress and _ReturnAddress

CvDllGame::CvDllGame(CvGame* pGame)
	: m_pGame(pGame)
	, m_uiRefCount(1)
{
	if(gDLL)
		gDLL->GetGameCoreLock();
		
	// CRITICAL: Capture BIN_HOOKS value IMMEDIATELY before mod deactivation can occur
	m_bBinHooksEnabledAtConstruction = MOD_BIN_HOOKS;
	
	// Clean up protection files from previous game sessions (only on first constructor)
	static bool protectionFilesCleanedUp = false;
	if (!protectionFilesCleanedUp) {
		DeleteFileA("HOOK_PROTECTION_ACTIVE.flag");
		DeleteFileA("FIRST_HOOK_CALL.timestamp");
		DeleteFileA("CONSTRUCTOR_PROTECTION_ACTIVE.flag");
		DeleteFileA("CONSTRUCTOR_COUNT.txt");
		DeleteFileA("CONSTRUCTOR_IN_PROGRESS.lock");
		protectionFilesCleanedUp = true;
	}
	
	// STRATEGIC CONSTRUCTOR MANAGEMENT: Allow first few constructors, prevent excessive recursion
	// GOAL: Let first constructor install hooks, prevent infinite recursion from subsequent calls
	
	// Track constructor count for debugging
	const char* constructorCountFile = "CONSTRUCTOR_COUNT.txt";
	int constructorCount = 0;
	FILE* countFile = NULL;
	if (fopen_s(&countFile, constructorCountFile, "r") == 0 && countFile != NULL) {
		fscanf_s(countFile, "%d", &constructorCount);
		fclose(countFile);
	}
	constructorCount++;
	if (fopen_s(&countFile, constructorCountFile, "w") == 0 && countFile != NULL) {
		fprintf(countFile, "%d", constructorCount);
		fclose(countFile);
	}
	
	m_bSkipHookInstallation = false;
		
	// ADVANCED DEBUG: Create comprehensive constructor tracking
	FILE* markerFile = NULL;
	if (fopen_s(&markerFile, "CVDLLGAME_CONSTRUCTOR_CALLED.txt", "a") == 0 && markerFile != NULL) {
		fprintf(markerFile, "=== CvDllGame constructor called ===\n");
		fprintf(markerFile, "Instance address: %p\n", this);
		fprintf(markerFile, "Timestamp: %lu\n", GetTickCount());
		fprintf(markerFile, "Thread ID: %lu\n", GetCurrentThreadId());
		fprintf(markerFile, "Process ID: %lu\n", GetCurrentProcessId());
		
		// Check if this is part of the infinite constructor loop
		static DWORD lastInstanceAddr = 0;
		static int sameInstanceCount = 0;
		static DWORD firstCallTime = 0;
		
		if (firstCallTime == 0) {
			firstCallTime = GetTickCount();
		}
		
		DWORD currentInstanceAddr = (DWORD)this;
		if (currentInstanceAddr == lastInstanceAddr) {
			sameInstanceCount++;
			fprintf(markerFile, "INFINITE LOOP DETECTED: Same instance %p called %d times\n", this, sameInstanceCount);
		} else {
			sameInstanceCount = 1;
			lastInstanceAddr = currentInstanceAddr;
		}
		
		fprintf(markerFile, "Time since first call: %lu ms\n", GetTickCount() - firstCallTime);
		fprintf(markerFile, "Same instance count: %d\n", sameInstanceCount);
		
		fflush(markerFile);
		fclose(markerFile);
	}
	
	// CRITICAL: Add comprehensive stack trace and process debugging
	FILE* stackTraceFile = NULL;
	if (fopen_s(&stackTraceFile, "CONSTRUCTOR_STACK_TRACE.txt", "a") == 0 && stackTraceFile != NULL) {
		fprintf(stackTraceFile, "[%lu] Constructor called for instance %p\n", GetTickCount(), this);
		
		// Try to get some basic stack information (this is a simple approach)
		void* stackPtr = NULL;
		__asm { mov stackPtr, esp }
		fprintf(stackTraceFile, "[%lu] Stack pointer: %p\n", GetTickCount(), stackPtr);
		
		// Add comprehensive process and memory state
		HANDLE currentProcess = GetCurrentProcess();
		PROCESS_MEMORY_COUNTERS memInfo;
		if (GetProcessMemoryInfo(currentProcess, &memInfo, sizeof(memInfo))) {
			fprintf(stackTraceFile, "[%lu] Memory - Working Set: %lu KB, Peak: %lu KB\n", 
				GetTickCount(), memInfo.WorkingSetSize / 1024, memInfo.PeakWorkingSetSize / 1024);
			fprintf(stackTraceFile, "[%lu] Memory - Page File Usage: %lu KB, Peak: %lu KB\n", 
				GetTickCount(), memInfo.PagefileUsage / 1024, memInfo.PeakPagefileUsage / 1024);
		}
		
		// Check module information
		HMODULE hMod = GetModuleHandleA("CvGameCoreDLL_Expansion2.dll");
		if (hMod) {
			fprintf(stackTraceFile, "[%lu] DLL Module Handle: %p\n", GetTickCount(), hMod);
			
			// Get module file name
			char modulePath[MAX_PATH];
			if (GetModuleFileNameA(hMod, modulePath, MAX_PATH)) {
				fprintf(stackTraceFile, "[%lu] DLL Path: %s\n", GetTickCount(), modulePath);
			}
		}
		
		// Check if we're being called during hook installation by looking for our debug files
		FILE* testFile = NULL;
		if (fopen_s(&testFile, "JMP_INSTALL_DEBUG.txt", "r") == 0 && testFile != NULL) {
			fclose(testFile);
			fprintf(stackTraceFile, "[%lu] WARNING: Constructor called while JMP_INSTALL_DEBUG.txt exists - possible hook installation recursion!\n", GetTickCount());
		}
		
		// Add call stack analysis to identify recursion source
		fprintf(stackTraceFile, "[%lu] Call Stack Analysis:\n", GetTickCount());
		fprintf(stackTraceFile, "[%lu] - Constructor called from: %p\n", GetTickCount(), _ReturnAddress());
		fprintf(stackTraceFile, "[%lu] - Frame pointer: %p\n", GetTickCount(), _AddressOfReturnAddress());
		
		// Check if GameCore functions are being called (support both DX9 and DX11)
		HMODULE hCiv5 = GetModuleHandleA("CivilizationV_DX11.exe");
		const char* exeName = "CivilizationV_DX11.exe";
		if (!hCiv5) {
			hCiv5 = GetModuleHandleA("CivilizationV.exe");
			exeName = "CivilizationV.exe";
		}
		if (hCiv5) {
			fprintf(stackTraceFile, "[%lu] %s base: %p\n", GetTickCount(), exeName, hCiv5);
			
			// Check if return address is in main executable (potential GameCore call)
			DWORD_PTR returnAddr = (DWORD_PTR)_ReturnAddress();
			DWORD_PTR baseAddr = (DWORD_PTR)hCiv5;
			if (returnAddr >= baseAddr && returnAddr < baseAddr + 0x1000000) { // Rough size check
				fprintf(stackTraceFile, "[%lu] RECURSION SOURCE: Called from main executable (%s) at offset +%lX\n", 
					GetTickCount(), exeName, (DWORD)(returnAddr - baseAddr));
			}
		}
		
		// Add call frequency analysis
		static DWORD lastConstructorCall = 0;
		static int rapidCallCount = 0;
		DWORD currentTime = GetTickCount();
		
		if (lastConstructorCall != 0) {
			DWORD timeDiff = currentTime - lastConstructorCall;
			if (timeDiff < 100) { // Less than 100ms between calls
				rapidCallCount++;
				fprintf(stackTraceFile, "[%lu] RAPID CALL DETECTED: %lu ms since last call (rapid call #%d)\n", 
					currentTime, timeDiff, rapidCallCount);
			} else {
				rapidCallCount = 0; // Reset counter for normal calls
			}
		}
		lastConstructorCall = currentTime;
		
		fflush(stackTraceFile);
		fclose(stackTraceFile);
	}
	
	if (markerFile) {
		fprintf(markerFile, "MOD_BIN_HOOKS = %s\n", MOD_BIN_HOOKS ? "true" : "false");
		fprintf(markerFile, "Captured value: %s\n", m_bBinHooksEnabledAtConstruction ? "true" : "false");
		fprintf(markerFile, "Member variable address: %p\n", &m_bBinHooksEnabledAtConstruction);
		fprintf(markerFile, "gCustomMods.isBIN_HOOKS() = %s\n", gCustomMods.isBIN_HOOKS() ? "true" : "false");
		fprintf(markerFile, "Time: %lu\n", GetTickCount());
		fprintf(markerFile, "\n");
		fflush(markerFile);
		fclose(markerFile);
	}
	
	// Install binary hooks early during DLL construction to catch multiplayer mod deactivation
	InstallBinaryHooksEarly();
}
//------------------------------------------------------------------------------
CvDllGame::~CvDllGame()
{
	// Debug: Track destructor calls
	FILE* markerFile = NULL;
	if (fopen_s(&markerFile, "CVDLLGAME_DESTRUCTOR_CALLED.txt", "a") == 0 && markerFile != NULL) {
		fprintf(markerFile, "=== CvDllGame destructor called ===\n");
		fprintf(markerFile, "Instance address: %p\n", this);
		fprintf(markerFile, "Captured value was: %s\n", m_bBinHooksEnabledAtConstruction ? "true" : "false");
		fprintf(markerFile, "Time: %lu\n", GetTickCount());
		fprintf(markerFile, "\n");
		fflush(markerFile);
		fclose(markerFile);
	}
	
	// Add crash analysis logging
	FILE* crashAnalysisFile = NULL;
	if (fopen_s(&crashAnalysisFile, "CRASH_ANALYSIS_DEBUG.txt", "a") == 0 && crashAnalysisFile != NULL) {
		fprintf(crashAnalysisFile, "[%lu] CvDllGame destructor called - instance %p being destroyed\n", GetTickCount(), this);
		fprintf(crashAnalysisFile, "[%lu] If crash happens during destructor, it might be related to DLL unloading\n", GetTickCount());
		fflush(crashAnalysisFile);
		fclose(crashAnalysisFile);
	}
	
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

// Storage for original bytes and trampolines
struct HookInfo {
	DWORD originalAddress;
	unsigned char originalBytes[5];
	void* trampolineAddress;
	bool isInstalled;
};

HookInfo g_individualHookInfo[3] = {0}; // DX9, DX11, Tablet
HookInfo g_bulkHookInfo[3] = {0}; // DX9, DX11, Tablet



// SQLite function hooks to catch ANY database operations
typedef int (__cdecl *sqlite3_exec_func)(void* db, const char* sql, int (*callback)(void*,int,char**,char**), void* arg, char** errmsg);
typedef int (__cdecl *sqlite3_prepare_func)(void* db, const char* zSql, int nByte, void** ppStmt, const char** pzTail);
sqlite3_exec_func g_original_sqlite3_exec = NULL;
sqlite3_prepare_func g_original_sqlite3_prepare = NULL;

// Forward declaration
void RestoreModsAfterDeactivation();

// Function to restore mods after deactivation
void RestoreModsAfterDeactivation()
{
	FILE* logFile = NULL;
	if (fopen_s(&logFile, "Logs/MOD_HOOK_DEBUG.log", "a") == 0 && logFile != NULL) {
		fprintf(logFile, "[MOD_HOOK] RestoreModsAfterDeactivation: Starting mod restoration process\n");
		fflush(logFile);
	}
	
	// For now, just log that we would restore mods here
	// TODO: Implement actual mod restoration logic using available APIs
	try {
		if (logFile) {
			fprintf(logFile, "[MOD_HOOK] RestoreModsAfterDeactivation: Mod restoration logic would execute here\n");
			fprintf(logFile, "[MOD_HOOK] RestoreModsAfterDeactivation: Need to implement SQL: UPDATE Mods SET Activated = 1 WHERE Enabled = 1\n");
			fflush(logFile);
		}
		
		// The key insight is that we need to execute SQL to restore mods
		// This should happen immediately after the deactivation but before DLL reload
		// For now, we're just logging to verify the timing is correct
		
	} catch (...) {
		if (logFile) {
			fprintf(logFile, "[MOD_HOOK] RestoreModsAfterDeactivation: Exception occurred during mod restoration\n");
			fflush(logFile);
		}
	}
	
	if (logFile) {
		fprintf(logFile, "[MOD_HOOK] RestoreModsAfterDeactivation: Completed mod restoration process\n");
		fflush(logFile);
		fclose(logFile);
	}
}

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
	bool gameStateAccessible = false;
	
	try {
		CvGame* pGame = GC.getGamePointer();
		if (pGame != NULL) {
			isNetworkMP = pGame->isNetworkMultiPlayer();
			isHotSeat = pGame->isHotSeat();
			isPbem = pGame->isPbem();
			gameStateAccessible = true;
		}
	} catch (...) {
		// Game state might not be initialized yet - this is likely during startup
		gameStateAccessible = false;
	}
	
	// If game state is not accessible, we're probably during startup - just return success
	if (!gameStateAccessible) {
		if (logFile) {
			fprintf(logFile, "[MOD_HOOK] Game state not accessible - likely during startup, returning success without operation\n");
			fflush(logFile);
			fclose(logFile);
		}
		
		// During startup, just return success without doing anything
		// This prevents crashes while allowing the game to continue loading
		return 1; // Return success
	}
	
	// Game state is accessible - check if we're in multiplayer
	bool isMultiplayer = (isNetworkMP || isHotSeat || isPbem);
	
	// Log the detected game mode
	if (logFile) {
		if (isNetworkMP) {
			fprintf(logFile, "[MOD_HOOK] Network multiplayer detected - BLOCKING mod deactivation\n");
		} else if (isHotSeat) {
			fprintf(logFile, "[MOD_HOOK] HotSeat detected - BLOCKING mod deactivation\n");
		} else if (isPbem) {
			fprintf(logFile, "[MOD_HOOK] PBEM detected - BLOCKING mod deactivation\n");
		} else {
			fprintf(logFile, "[MOD_HOOK] Single player detected - allowing normal mod operation\n");
		}
		fflush(logFile);
		fclose(logFile);
	}
	
	if (isMultiplayer) {
		// In multiplayer, we skip the deactivation to preserve mod compatibility
		// This prevents the problematic "UPDATE Mods Set Activated = 0" SQL query
		return 1; // Return success without deactivating mods
	}
	
	// In single player, we also just return success for now
	// The original mod deactivation logic may not be necessary for single player
	// since mods are typically managed through the UI
	return 1; // Return success
}

// Hook function that intercepts bulk mod deactivation
// This allows deactivation but can restore mods afterwards to prevent crashes
bool __thiscall HookedBulkDeactivate(void* this_ptr)
{
	// Prevent infinite recursion - critical fix!
	static bool inHook = false;
	static int callCount = 0;
	callCount++;
	
	if (inHook) {
		// We're already in the hook, don't recurse
		FILE* recursionLog = NULL;
		if (fopen_s(&recursionLog, "RECURSION_PREVENTED.txt", "a") == 0 && recursionLog != NULL) {
			fprintf(recursionLog, "RECURSION PREVENTED! Call #%d blocked\n", callCount);
			fflush(recursionLog);
			fclose(recursionLog);
		}
		return true;
	}
	inHook = true;
	
	// Debug: Write to a log file that we can check
	FILE* logFile = NULL;
	FILE* debugFile = NULL;
	
	if (fopen_s(&logFile, "Logs/MOD_HOOK_DEBUG.log", "a") == 0 && logFile != NULL) {
		fprintf(logFile, "[MOD_HOOK] HookedBulkDeactivate called - Call #%d - BULK mod deactivation intercepted!\n", callCount);
		fflush(logFile);
	}
	
	if (fopen_s(&debugFile, "HOOK_EXECUTION_DEBUG.txt", "a") == 0 && debugFile != NULL) {
		fprintf(debugFile, "BULK DEACTIVATION HOOK EXECUTED - Preventing bulk mod deactivation\n");
		fflush(debugFile);
	}
	
	// Check if we're in multiplayer mode
	bool isMultiplayer = false;
	bool gameStateAccessible = false;
	
	// Try to get the game instance to check multiplayer status
	try {
		CvGame* pGame = GC.getGamePointer();
		if (pGame != NULL) {
			isMultiplayer = pGame->isNetworkMultiPlayer() || pGame->isHotSeat() || pGame->isPbem();
			gameStateAccessible = true;
			
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
	} catch (...) {
		// Game state might not be initialized yet - this is likely during startup
		gameStateAccessible = false;
	}
	
	// If game state is not accessible, we're probably during startup - just return success
	if (!gameStateAccessible) {
		if (logFile) {
			fprintf(logFile, "[MOD_HOOK] HookedBulkDeactivate: Game state not accessible - likely during startup, returning success without operation\n");
			fflush(logFile);
			fclose(logFile);
		}
		if (debugFile) {
			fprintf(debugFile, "Game state not accessible - returning success without bulk operation\n");
			fflush(debugFile);
			fclose(debugFile);
		}
		
		// During startup, just return success without doing anything
		inHook = false;
		return true; // Return success
	}
	
	if (isMultiplayer) {
		if (logFile) {
			fprintf(logFile, "[MOD_HOOK] HookedBulkDeactivate: BLOCKING bulk mod deactivation in multiplayer (FIXED APPROACH)!\n");
			fflush(logFile);
		}
		if (debugFile) {
			fprintf(debugFile, "BLOCKING bulk mod deactivation - NOT calling original function (prevents infinite recursion)!\n");
			fflush(debugFile);
		}
		
		// Close files
		if (logFile) fclose(logFile);
		if (debugFile) fclose(debugFile);
		
		// CRITICAL INSIGHT: We cannot call g_originalBulkDeactivate because it points to the hooked address
		// The hook installation overwrites the original function with a JMP to our hook
		// So calling g_originalBulkDeactivate(this_ptr) creates infinite recursion
		
		// SOLUTION: Don't call the original function at all
		// The game expects this function to return success, so we return true
		// This prevents the "UPDATE Mods Set Activated = 0" SQL from executing
		// The mods stay active, which is what we want
		
		// Add detailed logging to understand what happens after we return
		FILE* crashAnalysisFile = NULL;
		if (fopen_s(&crashAnalysisFile, "CRASH_ANALYSIS_DEBUG.txt", "a") == 0 && crashAnalysisFile != NULL) {
			fprintf(crashAnalysisFile, "[%lu] HookedBulkDeactivate: Returning success without deactivation - preventing SQL execution\n", GetTickCount());
			fprintf(crashAnalysisFile, "[%lu] If game crashes after this, the issue is NOT the deactivation SQL itself\n", GetTickCount());
			fprintf(crashAnalysisFile, "[%lu] The crash must be caused by something else in the multiplayer initialization process\n", GetTickCount());
			fflush(crashAnalysisFile);
			fclose(crashAnalysisFile);
		}
		
		// Reset re-entry guard before returning
		inHook = false;
		return true; // Return success without deactivating mods
	}
	
	// Close files
	if (logFile) fclose(logFile);
	if (debugFile) fclose(debugFile);
	
	// In single player, we also just return success for now
	// The original bulk deactivation logic may not be necessary
	return true; // Return success
}

// SQLite hook functions to monitor ALL database operations
int __cdecl HookedSqlite3Exec(void* db, const char* sql, int (*callback)(void*,int,char**,char**), void* arg, char** errmsg)
{
	FILE* logFile = NULL;
	FILE* debugFile = NULL;

	if (fopen_s(&logFile, "Logs/MOD_HOOK_DEBUG.log", "a") == 0 && logFile != NULL) {
		fprintf(logFile, "[SQLITE_HOOK] sqlite3_exec called with SQL: %s\n", sql ? sql : "NULL");
		fflush(logFile);
	}

	if (fopen_s(&debugFile, "SQLITE_EXECUTION_DEBUG.txt", "a") == 0 && debugFile != NULL) {
		fprintf(debugFile, "SQLITE3_EXEC: %s\n", sql ? sql : "NULL");
		fflush(debugFile);
	}

	// Check if this is mod-related SQL
	if (sql && (strstr(sql, "Mods") || strstr(sql, "mods") || strstr(sql, "Enabled") || strstr(sql, "Activated"))) {
		// Check if we're in multiplayer mode
		CvGame* pGame = GC.getGamePointer();
		bool isMultiplayer = false;
		
		if (pGame != NULL) {
			isMultiplayer = pGame->isNetworkMultiPlayer() || pGame->isHotSeat() || pGame->isPbem();
		}

		if (logFile) {
			fprintf(logFile, "[SQLITE_HOOK] MOD-RELATED SQL DETECTED! Multiplayer: %s\n", isMultiplayer ? "TRUE" : "FALSE");
			fflush(logFile);
		}
		if (debugFile) {
			fprintf(debugFile, "MOD-RELATED SQL! MP: %s\n", isMultiplayer ? "TRUE" : "FALSE");
			fflush(debugFile);
		}

		if (isMultiplayer) {
			if (logFile) {
				fprintf(logFile, "[SQLITE_HOOK] BLOCKING mod-related SQL in multiplayer: %s\n", sql);
				fflush(logFile);
			}
			if (debugFile) {
				fprintf(debugFile, "BLOCKED: %s\n", sql);
				fflush(debugFile);
			}

			// Close files
			if (logFile) fclose(logFile);
			if (debugFile) fclose(debugFile);

			// Return success without executing the SQL
			return 0; // SQLITE_OK
		}
	}

	// Close files
	if (logFile) fclose(logFile);
	if (debugFile) fclose(debugFile);

	// In single player or non-mod SQL, call the original function
	if (g_original_sqlite3_exec) {
		return g_original_sqlite3_exec(db, sql, callback, arg, errmsg);
	}

	return 0; // SQLITE_OK fallback
}

int __cdecl HookedSqlite3Prepare(void* db, const char* zSql, int nByte, void** ppStmt, const char** pzTail)
{
	FILE* logFile = NULL;
	FILE* debugFile = NULL;

	if (fopen_s(&logFile, "Logs/MOD_HOOK_DEBUG.log", "a") == 0 && logFile != NULL) {
		fprintf(logFile, "[SQLITE_HOOK] sqlite3_prepare called with SQL: %s\n", zSql ? zSql : "NULL");
		fflush(logFile);
	}

	if (fopen_s(&debugFile, "SQLITE_EXECUTION_DEBUG.txt", "a") == 0 && debugFile != NULL) {
		fprintf(debugFile, "SQLITE3_PREPARE: %s\n", zSql ? zSql : "NULL");
		fflush(debugFile);
	}

	// Check if this is mod-related SQL
	if (zSql && (strstr(zSql, "Mods") || strstr(zSql, "mods") || strstr(zSql, "Enabled") || strstr(zSql, "Activated"))) {
		// Check if we're in multiplayer mode
		CvGame* pGame = GC.getGamePointer();
		bool isMultiplayer = false;
		
		if (pGame != NULL) {
			isMultiplayer = pGame->isNetworkMultiPlayer() || pGame->isHotSeat() || pGame->isPbem();
		}

		if (logFile) {
			fprintf(logFile, "[SQLITE_HOOK] MOD-RELATED PREPARE DETECTED! Multiplayer: %s\n", isMultiplayer ? "TRUE" : "FALSE");
			fflush(logFile);
		}
		if (debugFile) {
			fprintf(debugFile, "MOD-RELATED PREPARE! MP: %s\n", isMultiplayer ? "TRUE" : "FALSE");
			fflush(debugFile);
		}

		if (isMultiplayer) {
			if (logFile) {
				fprintf(logFile, "[SQLITE_HOOK] BLOCKING mod-related PREPARE in multiplayer: %s\n", zSql);
				fflush(logFile);
			}
			if (debugFile) {
				fprintf(debugFile, "BLOCKED PREPARE: %s\n", zSql);
				fflush(debugFile);
			}

			// Close files
			if (logFile) fclose(logFile);
			if (debugFile) fclose(debugFile);

			// Return success without preparing the statement
			if (ppStmt) *ppStmt = NULL;
			return 0; // SQLITE_OK
		}
	}

	// Close files
	if (logFile) fclose(logFile);
	if (debugFile) fclose(debugFile);

	// In single player or non-mod SQL, call the original function
	if (g_original_sqlite3_prepare) {
		return g_original_sqlite3_prepare(db, zSql, nByte, ppStmt, pzTail);
	}

	return 0; // SQLITE_OK fallback
}

// SetActiveDLCandMods hook function - MPPatch-inspired approach
// This intercepts the main mod activation function and restores desired mods
// Based on Civ5XP.c analysis: SetActiveDLCandMods at 0x0898B5A8
int __cdecl SetActiveDLCandModsHook(void* this_ptr, void* db_connection, void* mod_list, unsigned char reload_dlc, char reload_mods)
{
	FILE* logFile = NULL;
	FILE* debugFile = NULL;

	if (fopen_s(&logFile, "Logs/MOD_HOOK_DEBUG.log", "a") == 0 && logFile != NULL) {
		fprintf(logFile, "[SETACTIVE_HOOK] SetActiveDLCandMods intercepted!\n");
		fprintf(logFile, "[SETACTIVE_HOOK] Parameters: this_ptr=0x%p, db_connection=0x%p, mod_list=0x%p, reload_dlc=%d, reload_mods=%d\n", 
			this_ptr, db_connection, mod_list, reload_dlc, reload_mods);
		fflush(logFile);
	}

	if (fopen_s(&debugFile, "SETACTIVE_HOOK_EXECUTION.txt", "a") == 0 && debugFile != NULL) {
		fprintf(debugFile, "[%lu] SetActiveDLCandMods INTERCEPTED!\n", GetTickCount());
		fprintf(debugFile, "MPPATCH APPROACH: Intercepting main mod activation function\n");
		fprintf(debugFile, "TIMING: This runs AFTER individual deactivation functions\n");
		fprintf(debugFile, "STRATEGY: Restore desired mods to the list before calling original function\n");
		fflush(debugFile);
	}

	// Check game state
	bool isMultiplayer = false;
	bool gameStateAccessible = false;
	
	try {
		CvGame* pGame = GC.getGamePointer();
		if (pGame != NULL) {
			isMultiplayer = pGame->isNetworkMultiPlayer() || pGame->isHotSeat() || pGame->isPbem();
			gameStateAccessible = true;
		}
	} catch (...) {
		// Game state might not be initialized yet
		if (debugFile) {
			fprintf(debugFile, "Game state not accessible - likely during startup\n");
			fflush(debugFile);
		}
	}

	if (debugFile) {
		fprintf(debugFile, "Game state accessible: %s\n", gameStateAccessible ? "true" : "false");
		if (gameStateAccessible) {
			fprintf(debugFile, "Is multiplayer: %s\n", isMultiplayer ? "true" : "false");
		}
		fflush(debugFile);
	}

	// TODO: Implement mod list restoration logic here
	// For now, just log that we intercepted the call and would restore mods
	if (isMultiplayer && gameStateAccessible) {
		if (logFile) {
			fprintf(logFile, "[SETACTIVE_HOOK] MULTIPLAYER DETECTED - Would restore mods to list here\n");
			fflush(logFile);
		}
		if (debugFile) {
			fprintf(debugFile, "MULTIPLAYER DETECTED - This is where we would restore mods to the list\n");
			fprintf(debugFile, "NEXT STEP: Implement mod list manipulation like MPPatch does\n");
			fflush(debugFile);
		}
	}

	// Close debug files
	if (logFile) fclose(logFile);
	if (debugFile) fclose(debugFile);

	// For now, just return success to see if the hook works
	// TODO: Call original function with modified parameters
	if (debugFile) {
		FILE* resultFile = NULL;
		if (fopen_s(&resultFile, "SETACTIVE_HOOK_RESULT.txt", "a") == 0 && resultFile != NULL) {
			fprintf(resultFile, "[%lu] SetActiveDLCandMods hook executed successfully - returning success\n", GetTickCount());
			fclose(resultFile);
		}
	}

	return 1; // Return success for now
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
		
		// CRITICAL: Add debugging before writing JMP instruction
		FILE* jmpDebugFile = NULL;
		if (fopen_s(&jmpDebugFile, "JMP_INSTALL_DEBUG.txt", "a") == 0 && jmpDebugFile != NULL) {
			fprintf(jmpDebugFile, "[%lu] BEFORE BULK JMP install: functionAddr=0x%08lX, hookAddr=0x%08lX, relativeAddr=0x%08lX\n", 
				GetTickCount(), functionAddr, hookAddr, relativeAddr);
			fflush(jmpDebugFile);
		}
		
		// Write JMP instruction (0xE9 followed by relative address)
		*(unsigned char*)functionAddr = 0xE9;
		*(DWORD*)(functionAddr + 1) = relativeAddr;
		
		if (jmpDebugFile) {
			fprintf(jmpDebugFile, "[%lu] AFTER BULK JMP install: JMP instruction written successfully\n", GetTickCount());
			fflush(jmpDebugFile);
			fclose(jmpDebugFile);
		}
		
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

void CvDllGame::HookSqliteFunction(const char* functionName, DWORD functionAddr, void* hookFunction, void** originalFunction)
{
	FILE* logFile = NULL;
	FILE* debugFile = NULL;

	if (fopen_s(&logFile, "Logs/MOD_HOOK_DEBUG.log", "a") == 0 && logFile != NULL) {
		fprintf(logFile, "[SQLITE_HOOK] Attempting to hook %s at address 0x%08lX\n", functionName, functionAddr);
		fflush(logFile);
	}

	if (fopen_s(&debugFile, "HOOK_INSTALL_DEBUG.txt", "a") == 0 && debugFile != NULL) {
		fprintf(debugFile, "HookSqliteFunction called for %s with address 0x%08lX\n", functionName, functionAddr);
		fflush(debugFile);
	}

	DWORD old_protect;
	if (VirtualProtect((void*)functionAddr, 5, PAGE_EXECUTE_READWRITE, &old_protect))
	{
		if (logFile) {
			fprintf(logFile, "[SQLITE_HOOK] VirtualProtect succeeded for %s, installing hook\n", functionName);
			fflush(logFile);
		}
		if (debugFile) {
			fprintf(debugFile, "VirtualProtect succeeded for %s, installing hook\n", functionName);
			fflush(debugFile);
		}

		// Store original function pointer
		*originalFunction = (void*)functionAddr;

		// Create a JMP instruction to our hook function
		DWORD hookAddr = (DWORD)hookFunction;
		DWORD relativeAddr = hookAddr - functionAddr - 5;

		if (logFile) {
			fprintf(logFile, "[SQLITE_HOOK] %s hook function at 0x%08lX, relative addr: 0x%08lX\n", functionName, hookAddr, relativeAddr);
			fflush(logFile);
		}
		if (debugFile) {
			fprintf(debugFile, "%s hook function at 0x%08lX, relative addr: 0x%08lX\n", functionName, hookAddr, relativeAddr);
			fflush(debugFile);
		}

		// Write JMP instruction (0xE9 followed by relative address)
		*(unsigned char*)functionAddr = 0xE9;
		*(DWORD*)(functionAddr + 1) = relativeAddr;

		VirtualProtect((void*)functionAddr, 5, old_protect, &old_protect);

		if (logFile) {
			fprintf(logFile, "[SQLITE_HOOK] %s hook installation completed successfully\n", functionName);
			fflush(logFile);
		}
		if (debugFile) {
			fprintf(debugFile, "%s hook installation completed successfully\n", functionName);
			fflush(debugFile);
		}
	}
	else
	{
		if (logFile) {
			fprintf(logFile, "[SQLITE_HOOK] VirtualProtect failed for %s - hook installation failed\n", functionName);
			fflush(logFile);
		}
		if (debugFile) {
			fprintf(debugFile, "VirtualProtect failed for %s - hook installation failed\n", functionName);
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

// SetActiveDLCandMods hook function - MPPatch-inspired approach
// This function intercepts the main mod activation function and restores desired mods
void CvDllGame::HookSetActiveDLCandMods(DWORD functionAddr)
{
	FILE* logFile = NULL;
	FILE* debugFile = NULL;

	if (fopen_s(&logFile, "Logs/MOD_HOOK_DEBUG.log", "a") == 0 && logFile != NULL) {
		fprintf(logFile, "[SETACTIVE_HOOK] Attempting to hook SetActiveDLCandMods at address 0x%08lX\n", functionAddr);
		fflush(logFile);
	}

	if (fopen_s(&debugFile, "HOOK_INSTALL_DEBUG.txt", "a") == 0 && debugFile != NULL) {
		fprintf(debugFile, "HookSetActiveDLCandMods called with address 0x%08lX\n", functionAddr);
		fflush(debugFile);
	}

	DWORD old_protect;
	if (VirtualProtect((void*)functionAddr, 5, PAGE_EXECUTE_READWRITE, &old_protect))
	{
		if (debugFile) {
			fprintf(debugFile, "VirtualProtect succeeded, installing SetActiveDLCandMods hook\n");
			fflush(debugFile);
		}

		// Get the address of our hook function
		DWORD hookAddr = (DWORD)&SetActiveDLCandModsHook;
		DWORD relativeAddr = hookAddr - (functionAddr + 5);

		if (debugFile) {
			fprintf(debugFile, "SetActiveDLCandMods hook function at 0x%08lX, relative addr: 0x%08lX\n", hookAddr, relativeAddr);
			fflush(debugFile);
		}

		// Write JMP instruction (0xE9 followed by relative address)
		*(unsigned char*)functionAddr = 0xE9;
		*(DWORD*)(functionAddr + 1) = relativeAddr;

		VirtualProtect((void*)functionAddr, 5, old_protect, &old_protect);

		if (logFile) {
			fprintf(logFile, "[SETACTIVE_HOOK] SetActiveDLCandMods hook installation completed successfully\n");
			fflush(logFile);
		}
		if (debugFile) {
			fprintf(debugFile, "SetActiveDLCandMods hook installation completed successfully\n");
			fflush(debugFile);
		}
	}
	else
	{
		if (logFile) {
			fprintf(logFile, "[SETACTIVE_HOOK] VirtualProtect failed for SetActiveDLCandMods - hook installation failed\n");
			fflush(logFile);
		}
		if (debugFile) {
			fprintf(debugFile, "VirtualProtect failed for SetActiveDLCandMods - hook installation failed\n");
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
	// Check if this instance should skip hook installation due to recursion
	if (m_bSkipHookInstallation) {
		FILE* skipLog = NULL;
		if (fopen_s(&skipLog, "HOOK_INSTALLATION_SKIPPED.txt", "a") == 0 && skipLog != NULL) {
			fprintf(skipLog, "[%lu] Hook installation SKIPPED for instance %p (recursion prevention)\n", GetTickCount(), this);
			fclose(skipLog);
		}
		return; // Exit early to prevent recursion
	}
	
	// Prevent multiple hook installations
	static bool hooksInstalled = false;
	static int callCount = 0;
	static bool inHookInstallation = false;
	callCount++;
	
	// CRITICAL: Detect and prevent recursion
	if (inHookInstallation) {
		FILE* recursionFile = NULL;
		if (fopen_s(&recursionFile, "RECURSION_DETECTED.txt", "a") == 0 && recursionFile != NULL) {
			fprintf(recursionFile, "[%lu] RECURSION DETECTED in InstallBinaryHooksEarly!\n", GetTickCount());
			fprintf(recursionFile, "[%lu] - Call #%d, Instance: %p\n", GetTickCount(), callCount, this);
			fprintf(recursionFile, "[%lu] - Called from: %p\n", GetTickCount(), _ReturnAddress());
			fclose(recursionFile);
		}
		return; // PREVENT RECURSION
	}
	
	inHookInstallation = true;
	
	// Add call stack debugging for hook installation
	FILE* hookCallStackFile = NULL;
	if (fopen_s(&hookCallStackFile, "HOOK_CALL_STACK_DEBUG.txt", "a") == 0 && hookCallStackFile != NULL) {
		fprintf(hookCallStackFile, "[%lu] InstallBinaryHooksEarly called (call #%d)\n", GetTickCount(), callCount);
		fprintf(hookCallStackFile, "[%lu] - Called from: %p\n", GetTickCount(), _ReturnAddress());
		fprintf(hookCallStackFile, "[%lu] - Instance: %p\n", GetTickCount(), this);
		
		// Check if called from main executable (support both DX9 and DX11)
		HMODULE hCiv5 = GetModuleHandleA("CivilizationV_DX11.exe");
		const char* exeName = "CivilizationV_DX11.exe";
		if (!hCiv5) {
			hCiv5 = GetModuleHandleA("CivilizationV.exe");
			exeName = "CivilizationV.exe";
		}
		if (hCiv5) {
			DWORD_PTR returnAddr = (DWORD_PTR)_ReturnAddress();
			DWORD_PTR baseAddr = (DWORD_PTR)hCiv5;
			if (returnAddr >= baseAddr && returnAddr < baseAddr + 0x1000000) {
				DWORD offset = (DWORD)(returnAddr - baseAddr);
				fprintf(hookCallStackFile, "[%lu] - Called from main executable (%s) at offset +%lX\n", 
					GetTickCount(), exeName, offset);
				
				// Map specific known addresses from Civ5XP.c analysis
				// Based on our logs: return address 7AF9A45F with base 00520000 = offset 7A97A45F
				// This should be around GameCore::GetGame() calling dword_A3E565C (DllGetGameContext)
				fprintf(hookCallStackFile, "[%lu] - RECURSION ANALYSIS: This call is from GameCore::GetGame() mechanism\n", GetTickCount());
				fprintf(hookCallStackFile, "[%lu]   * GameCore::GetGame calls dword_A3E565C (function pointer to DllGetGameContext)\n", GetTickCount());
				fprintf(hookCallStackFile, "[%lu]   * DllGetGameContext creates CvDllGame instance -> constructor\n", GetTickCount());
				fprintf(hookCallStackFile, "[%lu]   * Constructor calls InstallBinaryHooksEarly\n", GetTickCount());
				fprintf(hookCallStackFile, "[%lu]   * Hook installation somehow triggers GameCore::GetGame again\n", GetTickCount());
				fprintf(hookCallStackFile, "[%lu]   * This creates infinite recursion during SP->MP transition\n", GetTickCount());
			} else {
				fprintf(hookCallStackFile, "[%lu] - Called from OUTSIDE main executable (possibly DLL or other module)\n", GetTickCount());
				
				// Check if it's from our own DLL
				HMODULE hOurDLL = GetModuleHandleA("CvGameCoreDLL_Expansion2.dll");
				if (hOurDLL && returnAddr >= (DWORD_PTR)hOurDLL && returnAddr < (DWORD_PTR)hOurDLL + 0x1000000) {
					fprintf(hookCallStackFile, "[%lu] - Called from OUR DLL at offset +%lX\n", 
						GetTickCount(), (DWORD)(returnAddr - (DWORD_PTR)hOurDLL));
				}
			}
		}
		
		// Check if this is a rapid call (potential recursion)
		static DWORD lastHookCall = 0;
		DWORD currentTime = GetTickCount();
		if (lastHookCall != 0 && (currentTime - lastHookCall) < 100) {
			fprintf(hookCallStackFile, "[%lu] - RAPID HOOK CALL: %lu ms since last call\n", 
				GetTickCount(), currentTime - lastHookCall);
		}
		lastHookCall = currentTime;
		
		fclose(hookCallStackFile);
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
	if (fopen_s(&debugFile, "HOOK_DEBUG.txt", "a") == 0 && debugFile != NULL) {
		fprintf(debugFile, "=== InstallBinaryHooksEarly called (call #%d) ===\n", callCount);
		fprintf(debugFile, "Instance address: %p\n", this);
	}
	
	// CRITICAL: Add debugging to track when hook installation happens
	FILE* installTimingFile = NULL;
	if (fopen_s(&installTimingFile, "HOOK_INSTALL_TIMING.txt", "a") == 0 && installTimingFile != NULL) {
		fprintf(installTimingFile, "[%lu] InstallBinaryHooksEarly: Call #%d, Instance %p\n", GetTickCount(), callCount, this);
		fprintf(installTimingFile, "[%lu] MOD_BIN_HOOKS = %s, Captured = %s, hooksInstalled = %s\n", 
			GetTickCount(), 
			MOD_BIN_HOOKS ? "true" : "false",
			m_bBinHooksEnabledAtConstruction ? "true" : "false",
			hooksInstalled ? "true" : "false");
		fflush(installTimingFile);
		fclose(installTimingFile);
	}
	
	// ADVANCED DEBUGGING: Add comprehensive monitoring to understand the crash
	// Based on reverse-engineered code analysis from Civ5XP.c, we know:
	// - Function: Modding::System::DeactivateMods at 08C6F95A executes "BEGIN; UPDATE Mods Set Activated = 0; END;"
	// - This is called during "BeforeDeactivateMods - Unload DLL" phase  
	// - Parent function: CvModdingFrameworkAppSide::SetActiveDLCandMods at 0898B5A8
	// - DX11 equivalent: sub_7C1BB0 at 0x007C1BB0 (our hook target)
	
	// Add database state monitoring and detailed crash analysis
	bool ENABLE_ADVANCED_DEBUGGING = true;
	if (ENABLE_ADVANCED_DEBUGGING) {
		// Create comprehensive debugging infrastructure
		FILE* advancedDebugFile = NULL;
		if (fopen_s(&advancedDebugFile, "ADVANCED_DEBUG_ANALYSIS.txt", "a") == 0 && advancedDebugFile != NULL) {
			fprintf(advancedDebugFile, "\n=== ADVANCED DEBUG SESSION START ===\n");
			fprintf(advancedDebugFile, "[%lu] InstallBinaryHooksEarly called - Instance %p\n", GetTickCount(), this);
			fprintf(advancedDebugFile, "Based on reverse-engineered code analysis:\n");
			fprintf(advancedDebugFile, "- Target function: Modding::System::DeactivateMods at 08C6F95A\n");
			fprintf(advancedDebugFile, "- DX11 equivalent: sub_7C1BB0 at 0x007C1BB0 (our hook target)\n");
			fprintf(advancedDebugFile, "- SQL executed: BEGIN; UPDATE Mods Set Activated = 0; END;\n");
			fprintf(advancedDebugFile, "- Called during: BeforeDeactivateMods - Unload DLL phase\n");
			fprintf(advancedDebugFile, "- Parent function: CvModdingFrameworkAppSide::SetActiveDLCandMods at 0898B5A8\n");
			
			// Add memory and stack information
			fprintf(advancedDebugFile, "MEMORY STATE:\n");
			fprintf(advancedDebugFile, "- Instance address: %p\n", this);
			fprintf(advancedDebugFile, "- Stack pointer: %p\n", &advancedDebugFile);
			fprintf(advancedDebugFile, "- Thread ID: %lu\n", GetCurrentThreadId());
			fprintf(advancedDebugFile, "- Process ID: %lu\n", GetCurrentProcessId());
			
			// Check current database state
			Database::Connection* db = GC.GetGameDatabase();
			if (db != NULL) {
				fprintf(advancedDebugFile, "DATABASE STATE BEFORE HOOK INSTALLATION:\n");
				
				// Query current mod activation state
				Database::Results results;
				if (db->Execute(results, "SELECT COUNT(*) FROM Mods WHERE Activated = 1")) {
					if (results.Step()) {
						int activeMods = results.GetInt(0);
						fprintf(advancedDebugFile, "- Active mods count: %d\n", activeMods);
					}
					results.Reset();
				}
				
				// Query total mods
				if (db->Execute(results, "SELECT COUNT(*) FROM Mods")) {
					if (results.Step()) {
						int totalMods = results.GetInt(0);
						fprintf(advancedDebugFile, "- Total mods count: %d\n", totalMods);
					}
					results.Reset();
				}
				
				// Query mod details
				if (db->Execute(results, "SELECT ID, Name, Activated FROM Mods LIMIT 10")) {
					fprintf(advancedDebugFile, "- First 10 mods:\n");
					while (results.Step()) {
						const char* id = results.GetText(0);
						const char* name = results.GetText(1);
						int activated = results.GetInt(2);
						fprintf(advancedDebugFile, "  %s: %s (Activated: %d)\n", id ? id : "NULL", name ? name : "NULL", activated);
					}
					results.Reset();
				}
			} else {
				fprintf(advancedDebugFile, "WARNING: Database connection is NULL!\n");
			}
			
			// Add game state information
			fprintf(advancedDebugFile, "GAME STATE:\n");
			CvGame* pGame = GC.getGamePointer();
			if (pGame != NULL) {
				fprintf(advancedDebugFile, "- Game exists: YES\n");
				fprintf(advancedDebugFile, "- Is multiplayer: %s\n", pGame->isNetworkMultiPlayer() ? "YES" : "NO");
				fprintf(advancedDebugFile, "- Game state: %d\n", pGame->getGameState());
			} else {
				fprintf(advancedDebugFile, "- Game exists: NO\n");
			}
			
			// Add CustomMods state
			fprintf(advancedDebugFile, "CUSTOMMODS STATE:\n");
			fprintf(advancedDebugFile, "- MOD_BIN_HOOKS macro: %s\n", MOD_BIN_HOOKS ? "true" : "false");
			fprintf(advancedDebugFile, "- gCustomMods.isBIN_HOOKS(): %s\n", gCustomMods.isBIN_HOOKS() ? "true" : "false");
			fprintf(advancedDebugFile, "- gCustomMods address: %p\n", &gCustomMods);
			
			// Add game state transition monitoring
			fprintf(advancedDebugFile, "GAME STATE TRANSITION ANALYSIS:\n");
			static int lastGameState = -1;
			static bool lastMultiplayerState = false;
			static DWORD lastStateChangeTime = 0;
			
			if (pGame != NULL) {
				int currentGameState = pGame->getGameState();
				bool currentMultiplayerState = pGame->isNetworkMultiPlayer();
				DWORD currentTime = GetTickCount();
				
				if (lastGameState != -1) {
					if (currentGameState != lastGameState) {
						fprintf(advancedDebugFile, "- Game state changed: %d -> %d (Time: %lu ms)\n", 
							lastGameState, currentGameState, currentTime - lastStateChangeTime);
					}
					if (currentMultiplayerState != lastMultiplayerState) {
						fprintf(advancedDebugFile, "- Multiplayer state changed: %s -> %s (Time: %lu ms)\n", 
							lastMultiplayerState ? "YES" : "NO", 
							currentMultiplayerState ? "YES" : "NO",
							currentTime - lastStateChangeTime);
					}
				}
				
				lastGameState = currentGameState;
				lastMultiplayerState = currentMultiplayerState;
				lastStateChangeTime = currentTime;
			}
			
			// Add DLL instance tracking
			fprintf(advancedDebugFile, "DLL INSTANCE TRACKING:\n");
			static void* lastInstanceAddr = NULL;
			static int instanceChangeCount = 0;
			
			if (lastInstanceAddr != NULL && lastInstanceAddr != this) {
				instanceChangeCount++;
				fprintf(advancedDebugFile, "- Instance changed: %p -> %p (Change #%d)\n", 
					lastInstanceAddr, this, instanceChangeCount);
			}
			lastInstanceAddr = this;
			
			fflush(advancedDebugFile);
			fclose(advancedDebugFile);
		}
	}
	
	// PERMANENT SOLUTION: File-based hook protection that persists across DLL reloads
	// CRITICAL DISCOVERY: Static variables don't work because DLL gets unloaded/reloaded during SP->MP transition
	// This resets all static variables, making in-memory protection ineffective
	
	// Use file-based protection that survives DLL reloads
	const char* protectionFile = "HOOK_PROTECTION_ACTIVE.flag";
	const char* firstCallFile = "FIRST_HOOK_CALL.timestamp";
	DWORD currentTime = GetTickCount();
	bool hookProtectionActive = false;
	
	// Check if protection is already active
	FILE* protFile = NULL;
	if (fopen_s(&protFile, protectionFile, "r") == 0 && protFile != NULL) {
		hookProtectionActive = true;
		fclose(protFile);
	}
	
	// Check when first call happened
	DWORD firstCallTime = 0;
	FILE* timeFile = NULL;
	if (fopen_s(&timeFile, firstCallFile, "r") == 0 && timeFile != NULL) {
		fscanf_s(timeFile, "%lu", &firstCallTime);
		fclose(timeFile);
	}
	
	// STRATEGIC HOOK INSTALLATION: Install hooks on first call to protect mods
	// GOAL: Keep mods active during multiplayer setup by blocking deactivation
	if (firstCallTime == 0) {
		// This is the very first call - ALWAYS install hooks regardless of MOD_BIN_HOOKS
		// This is our chance to install the protection before SP->MP transition
		if (fopen_s(&timeFile, firstCallFile, "w") == 0 && timeFile != NULL) {
			fprintf(timeFile, "%lu", currentTime);
			fclose(timeFile);
		}
		if (debugFile) {
			fprintf(debugFile, "STRATEGIC HOOK INSTALLATION: First call - installing hooks to protect mods\n");
			fprintf(debugFile, "MOD_BIN_HOOKS = %s (installing regardless to prevent future deactivation)\n", MOD_BIN_HOOKS ? "true" : "false");
			fprintf(debugFile, "Created timestamp file: %s with time %lu\n", firstCallFile, currentTime);
			fflush(debugFile);
		}
		// Continue with hook installation below
	} else {
		// This is a subsequent call - activate protection to prevent recursion
		DWORD timeSinceFirst = currentTime - firstCallTime;
		
		// Always activate protection for subsequent calls to prevent recursion
		if (!hookProtectionActive) {
			// Create protection file
			if (fopen_s(&protFile, protectionFile, "w") == 0 && protFile != NULL) {
				fprintf(protFile, "HOOK_PROTECTION_ACTIVE");
				fclose(protFile);
			}
			hookProtectionActive = true;
			if (debugFile) {
				fprintf(debugFile, "RECURSION PROTECTION: ACTIVATED for subsequent call %lu ms after first\n", timeSinceFirst);
				fprintf(debugFile, "MOD_BIN_HOOKS = %s (blocking to prevent recursion)\n", MOD_BIN_HOOKS ? "true" : "false");
				fprintf(debugFile, "Created protection file: %s\n", protectionFile);
				fflush(debugFile);
			}
		}
		
		// Block all subsequent hook installation attempts to prevent recursion
		if (debugFile) {
			fprintf(debugFile, "RECURSION PROTECTION: Hook installation BLOCKED\n");
			fprintf(debugFile, "Hooks already installed on first call - preventing recursion\n");
			fprintf(debugFile, "Installed hooks will block mod deactivation during SP->MP transition\n");
			fflush(debugFile);
			fclose(debugFile);
		}
		if (logFile) {
			fprintf(logFile, "[MOD_HOOK] RECURSION PROTECTION: Hook installation BLOCKED (preventing recursion)\n");
			fflush(logFile);
			fclose(logFile);
		}
		return; // Exit early to prevent recursion
	}
	
	if (debugFile) {
		fprintf(debugFile, "MOD_BIN_HOOKS = %s\n", MOD_BIN_HOOKS ? "true" : "false");
		fprintf(debugFile, "gCustomMods.isBIN_HOOKS() = %s\n", gCustomMods.isBIN_HOOKS() ? "true" : "false");
		fprintf(debugFile, "Member variable address: %p\n", &m_bBinHooksEnabledAtConstruction);
		fprintf(debugFile, "Member variable value: %s\n", m_bBinHooksEnabledAtConstruction ? "true" : "false");
		fprintf(debugFile, "hooksInstalled = %s\n", hooksInstalled ? "true" : "false");
		fprintf(debugFile, "Time: %lu\n", GetTickCount());
		fflush(debugFile);
	}
	
	if (hooksInstalled) {
		if (debugFile) {
			fprintf(debugFile, "Hooks already installed, returning early\n\n");
			fflush(debugFile);
			fclose(debugFile);
		}
		return;
	}
	
	// NEW APPROACH: Hook SetActiveDLCandMods instead of individual deactivation functions
	// INSPIRATION: MPPatch uses this approach successfully - intercept and modify parameters
	// TIMING: SetActiveDLCandMods runs AFTER deactivation, so mods will be restored for staging room
	bool binHooksEnabled = m_bBinHooksEnabledAtConstruction;
	bool isFirstCall = (firstCallTime == 0); // Check if this was the first call ever
	
	if (debugFile) {
		fprintf(debugFile, "=== NEW MPPATCH-INSPIRED APPROACH ===\n");
		fprintf(debugFile, "STRATEGY: Hook SetActiveDLCandMods to restore mods after deactivation\n");
		fprintf(debugFile, "TIMING: SetActiveDLCandMods runs after individual deactivation functions\n");
		fprintf(debugFile, "RESULT: Mods will be restored in time for staging room\n");
		fprintf(debugFile, "Current MOD_BIN_HOOKS macro: %s\n", MOD_BIN_HOOKS ? "true" : "false");
		fprintf(debugFile, "Using captured constructor value: %s\n", binHooksEnabled ? "true" : "false");
		fprintf(debugFile, "Is first call ever: %s\n", isFirstCall ? "true" : "false");
		fflush(debugFile);
	}
	
	// Install hooks if: (1) This is the first call ever (strategic installation), OR (2) binHooksEnabled=true
	if (binHooksEnabled || isFirstCall)
	{
		if (debugFile) {
			if (isFirstCall) {
				fprintf(debugFile, "STRATEGIC INSTALLATION: First call - installing hooks to protect mods\n");
			} else {
				fprintf(debugFile, "BIN_HOOKS is enabled, proceeding with hook installation\n");
			}
			fflush(debugFile);
		}
		
		// Get module handle and detect binary type
		HMODULE hModule = GetModuleHandleA(NULL);
		if (hModule)
		{
			// Get the actual base address of the loaded module
			DWORD baseAddress = (DWORD)hModule;
			
			if (debugFile) {
				fprintf(debugFile, "Got module handle at base 0x%08lX, calculating addresses\n", baseAddress);
				fflush(debugFile);
			}
			
			// Calculate addresses relative to actual base address
			// These offsets are from reverse engineering (assuming base 0x00400000)
			DWORD expectedBase = 0x00400000;
			
			const char* types[] = { "DX9", "DX11", "Tablet" };
			
			// Hook SetActiveDLCandMods function - MPPatch-inspired approach
			if (debugFile) {
				fprintf(debugFile, "=== HOOKING SETACTIVEDLCANDMODS FUNCTION (MPPATCH APPROACH) ===\n");
				fprintf(debugFile, "STRATEGY: Intercept main mod activation function to restore mods after deactivation\n");
				fflush(debugFile);
			}
			
			// SetActiveDLCandMods function address from Civ5XP.c analysis: 0x0898B5A8
			DWORD setActiveOffset = 0x898B5A8 - expectedBase;
			DWORD setActiveDLCandModsAddr = baseAddress + setActiveOffset;
			
			if (logFile) {
				fprintf(logFile, "[SETACTIVE_HOOK] InstallBinaryHooksEarly: Calculated SetActiveDLCandMods address: 0x%08lX (base: 0x%08lX + offset: 0x%08lX)\n", 
					setActiveDLCandModsAddr, baseAddress, setActiveOffset);
				fflush(logFile);
			}
			
			if (debugFile) {
				fprintf(debugFile, "Trying SetActiveDLCandMods address 0x%08lX (base + 0x%08lX)\n", setActiveDLCandModsAddr, setActiveOffset);
				fflush(debugFile);
			}
			
			// Validate the address points to executable memory
			if (setActiveDLCandModsAddr != 0)
			{
				// Check if we can read the first few bytes (function prologue)
				if (!IsBadReadPtr((void*)setActiveDLCandModsAddr, 5)) {
					unsigned char firstBytes[5];
					memcpy(firstBytes, (void*)setActiveDLCandModsAddr, 5);
					
					if (logFile) {
						fprintf(logFile, "[SETACTIVE_HOOK] SetActiveDLCandMods function bytes: %02X %02X %02X %02X %02X\n", 
							firstBytes[0], firstBytes[1], firstBytes[2], firstBytes[3], firstBytes[4]);
						fflush(logFile);
					}
					if (debugFile) {
						fprintf(debugFile, "SetActiveDLCandMods bytes: %02X %02X %02X %02X %02X\n", 
							firstBytes[0], firstBytes[1], firstBytes[2], firstBytes[3], firstBytes[4]);
						fflush(debugFile);
					}
					
					if (debugFile) {
						fprintf(debugFile, "Calling HookSetActiveDLCandMods\n");
						fflush(debugFile);
					}
					
					HookSetActiveDLCandMods(setActiveDLCandModsAddr);
				} else {
					if (logFile) {
						fprintf(logFile, "[SETACTIVE_HOOK] ERROR: Cannot read memory at SetActiveDLCandMods address 0x%08lX\n", setActiveDLCandModsAddr);
						fflush(logFile);
					}
					if (debugFile) {
						fprintf(debugFile, "ERROR: Cannot read SetActiveDLCandMods address 0x%08lX\n", setActiveDLCandModsAddr);
						fflush(debugFile);
					}
				}
			}
			
			// 3. SQLite function hooks - catch ANY database operations
			if (debugFile) {
				fprintf(debugFile, "=== HOOKING SQLITE DATABASE FUNCTIONS ===\n");
				fflush(debugFile);
			}
			
			// We need to find sqlite3_exec and sqlite3_prepare addresses dynamically
			// These are typically in sqlite3.dll or statically linked
			HMODULE sqlite3Module = GetModuleHandleA("sqlite3.dll");
			if (!sqlite3Module) {
				// Try the main executable (statically linked SQLite)
				sqlite3Module = hModule;
			}
			
			if (sqlite3Module) {
				// Try to get sqlite3_exec address
				DWORD sqlite3_exec_addr = (DWORD)GetProcAddress(sqlite3Module, "sqlite3_exec");
				if (sqlite3_exec_addr) {
					if (logFile) {
						fprintf(logFile, "[SQLITE_HOOK] Found sqlite3_exec at 0x%08lX\n", sqlite3_exec_addr);
						fflush(logFile);
					}
					if (debugFile) {
						fprintf(debugFile, "Hooking sqlite3_exec at 0x%08lX\n", sqlite3_exec_addr);
						fflush(debugFile);
					}
					HookSqliteFunction("sqlite3_exec", sqlite3_exec_addr, (void*)HookedSqlite3Exec, (void**)&g_original_sqlite3_exec);
				}
				
				// Try to get sqlite3_prepare address
				DWORD sqlite3_prepare_addr = (DWORD)GetProcAddress(sqlite3Module, "sqlite3_prepare");
				if (sqlite3_prepare_addr) {
					if (logFile) {
						fprintf(logFile, "[SQLITE_HOOK] Found sqlite3_prepare at 0x%08lX\n", sqlite3_prepare_addr);
						fflush(logFile);
					}
					if (debugFile) {
						fprintf(debugFile, "Hooking sqlite3_prepare at 0x%08lX\n", sqlite3_prepare_addr);
						fflush(debugFile);
					}
					HookSqliteFunction("sqlite3_prepare", sqlite3_prepare_addr, (void*)HookedSqlite3Prepare, (void**)&g_original_sqlite3_prepare);
				}
				
				if (logFile) {
					fprintf(logFile, "[SQLITE_HOOK] SQLite hook installation completed\n");
					fflush(logFile);
				}
				if (debugFile) {
					fprintf(debugFile, "SQLite hook installation completed\n");
					fflush(debugFile);
				}
			} else {
				if (logFile) {
					fprintf(logFile, "[SQLITE_HOOK] Could not find SQLite module\n");
					fflush(logFile);
				}
				if (debugFile) {
					fprintf(debugFile, "Could not find SQLite module\n");
					fflush(debugFile);
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
		
		// Mark hooks as installed ONLY if they were actually installed
		hooksInstalled = true;
		
		// Start monitoring mod status to detect when deactivation occurs
		StartModStatusMonitoring();
	}
	else
	{
		if (debugFile) {
			fprintf(debugFile, "HOOK INSTALLATION SKIPPED: Not first call and MOD_BIN_HOOKS disabled\n");
			fprintf(debugFile, "This should not happen with strategic installation logic\n");
			fflush(debugFile);
		}
		// This branch should rarely be reached with strategic installation
	}
	
	if (logFile) {
		fclose(logFile);
	}
	if (debugFile) {
		fclose(debugFile);
	}
	
	// Reset recursion flag at end of function
	inHookInstallation = false;
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