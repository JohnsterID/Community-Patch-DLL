#include "CvGameCoreDLLPCH.h"
#include "CvDesyncDebugger.h"
#include "CvGame.h"
#include "CvGlobals.h"

// Static member initialization
int CvDesyncDebugger::s_lastSyncTurn = 0;

void CvDesyncDebugger::enableStackTraceDebugging()
{
    // Enable the out-of-sync debugging feature
    gGlobals.setOutOfSyncDebuggingEnabled(true);
    
    std::string logMsg = "Enhanced desync debugging enabled - stack traces will be collected";
    gGlobals.getDLLIFace()->netMessageDebugLog(logMsg);
}

bool CvDesyncDebugger::isCurrentlyResyncing()
{
    // This would interface with the binary NetForceResync::IsResyncing() function
    // For now, we can check if the game is marked as desynced
    return GC.getGame().isDesynced();
}

void CvDesyncDebugger::performTargetedSyncCheck(SyncCheckType type, int entityId)
{
    if (!GC.getGame().isNetworkMultiPlayer()) {
        return;
    }
    
    std::ostringstream logMsg;
    logMsg << "Performing targeted sync check - Type: ";
    
    switch(type) {
        case SYNC_CITIES:
            logMsg << "Cities";
            FSerialization::SyncCities();
            break;
        case SYNC_UNITS:
            logMsg << "Units";
            FSerialization::SyncUnits();
            break;
        case SYNC_PLOTS:
            logMsg << "Plots";
            FSerialization::SyncPlots();
            break;
        case SYNC_PLAYERS:
            logMsg << "Players";
            FSerialization::SyncPlayer();
            break;
        case SYNC_RNG:
            logMsg << "RNG";
            // Would interface with NetRandomNumberGeneratorSyncCheck
            break;
    }
    
    if (entityId >= 0) {
        logMsg << " | Entity ID: " << entityId;
    }
    
    gGlobals.getDLLIFace()->netMessageDebugLog(logMsg.str());
}

void CvDesyncDebugger::logDesyncWithContext(const std::string& variableName, 
                                          const std::string& currentValue,
                                          const std::string& otherValue,
                                          const std::string& callStack)
{
    std::ostringstream logMsg;
    logMsg << "[ENHANCED_DESYNC] Variable: " << variableName 
           << " | Current: " << currentValue 
           << " | Other: " << otherValue
           << " | Turn: " << GC.getGame().getGameTurn()
           << " | Player: " << GC.getGame().getActivePlayer();
    
    if (!callStack.empty()) {
        logMsg << " | Context: " << callStack;
    }
    
    gGlobals.getDLLIFace()->netMessageDebugLog(logMsg.str());
}

void CvDesyncDebugger::triggerIntelligentResync(const std::string& reason)
{
    CvGame& game = GC.getGame();
    
    if (!game.IsExeWantForceResyncAvailable()) {
        gGlobals.getDLLIFace()->netMessageDebugLog("Cannot trigger resync: not available");
        return;
    }
    
    // Log the reason for resync
    std::ostringstream logMsg;
    logMsg << "Triggering intelligent resync - Reason: " << reason 
           << " | Turn: " << game.getGameTurn()
           << " | Active Player: " << game.getActivePlayer();
    gGlobals.getDLLIFace()->netMessageDebugLog(logMsg.str());
    
    // Clear all deltas before resync to ensure clean state
    FSerialization::ClearPlayerDeltas();
    FSerialization::ClearCityDeltas();
    FSerialization::ClearUnitDeltas();
    FSerialization::ClearPlotDeltas();
    
    // Trigger the resync
    game.SetExeWantForceResyncValue(1);
}

void CvDesyncDebugger::performPeriodicSyncCheck()
{
    if (!GC.getGame().isNetworkMultiPlayer()) {
        return;
    }
    
    int currentTurn = GC.getGame().getGameTurn();
    if (currentTurn - s_lastSyncTurn >= SYNC_CHECK_INTERVAL) {
        // Perform lightweight sync checks
        std::string logMsg = "Performing periodic sync validation - Turn: " + std::to_string(currentTurn);
        gGlobals.getDLLIFace()->netMessageDebugLog(logMsg);
        
        FSerialization::SyncPlayer();
        FSerialization::SyncCities();
        
        s_lastSyncTurn = currentTurn;
    }
}