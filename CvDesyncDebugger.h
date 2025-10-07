#pragma once

#ifndef CV_DESYNC_DEBUGGER_H
#define CV_DESYNC_DEBUGGER_H

#include "CvGameCoreDLLPCH.h"

// Enhanced desync debugging utilities
class CvDesyncDebugger
{
public:
    enum SyncCheckType
    {
        SYNC_CITIES,
        SYNC_UNITS,
        SYNC_PLOTS,
        SYNC_PLAYERS,
        SYNC_RNG
    };

    // Enable enhanced debugging features
    static void enableStackTraceDebugging();
    
    // Check if currently in a resync operation (uses binary function)
    static bool isCurrentlyResyncing();
    
    // Perform targeted sync check for specific entity types
    static void performTargetedSyncCheck(SyncCheckType type, int entityId = -1);
    
    // Enhanced logging with context information
    static void logDesyncWithContext(const std::string& variableName, 
                                   const std::string& currentValue,
                                   const std::string& otherValue,
                                   const std::string& callStack = "");
    
    // Intelligent resync with better error handling
    static void triggerIntelligentResync(const std::string& reason);
    
    // Periodic sync validation
    static void performPeriodicSyncCheck();

private:
    static int s_lastSyncTurn;
    static const int SYNC_CHECK_INTERVAL = 5; // Every 5 turns
};

// Enhanced FAutoVariable with better debugging
template<typename ValueType>
class FAutoVariableDebug : public FAutoVariable<ValueType>
{
public:
    FAutoVariableDebug(const std::string& name) : FAutoVariable<ValueType>(name) {}
    
    virtual bool compare(FDataStream& otherValue) const override
    {
        ValueType other;
        otherValue >> other;
        const bool result = other == this->currentValue();

        if (!result) {
            // Enhanced logging with more context
            std::ostringstream detailedLog;
            detailedLog << "[DESYNC] " << this->name() 
                       << " | Expected: " << FSerialization::toString(this->currentValue())
                       << " | Received: " << FSerialization::toString(other)
                       << " | Turn: " << GC.getGame().getGameTurn()
                       << " | Player: " << GC.getGame().getActivePlayer();
            
            // Add stack trace if debugging is enabled
            if (gGlobals.getOutOfSyncDebuggingEnabled()) {
                detailedLog << " | Stack trace available";
            }
            
            CvDesyncDebugger::logDesyncWithContext(
                this->name(),
                FSerialization::toString(this->currentValue()),
                FSerialization::toString(other),
                detailedLog.str()
            );
            
            gGlobals.getGame().setDesynced(true);
            
            // Optionally trigger automatic resync for critical variables
            if (this->name().find("Critical") != std::string::npos) {
                CvDesyncDebugger::triggerIntelligentResync("Critical variable desync: " + this->name());
            }
        }

        return result;
    }
};

#endif // CV_DESYNC_DEBUGGER_H