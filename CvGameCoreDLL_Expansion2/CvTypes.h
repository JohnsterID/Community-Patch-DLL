/*	-------------------------------------------------------------------------------------------------------
	© 1991-2012 Take-Two Interactive Software and its subsidiaries.  Developed by Firaxis Games.  
	Sid Meier's Civilization V, Civ, Civilization, 2K Games, Firaxis Games, Take-Two Interactive Software 
	and their respective logos are all trademarks of Take-Two interactive Software, Inc.  
	All other marks and trademarks are the property of their respective owners.  
	All rights reserved. 
	------------------------------------------------------------------------------------------------------- */
#pragma once

//This utility caches commonly used type ID's from the database.
namespace CvTypes
{
void AcquireTypes(Database::Connection& db);

#if defined(MOD_BALANCE_CORE_MILITARY_LOGGING)
const std::string& GetMissionName(MissionTypes eMission);
extern std::tr1::unordered_map<MissionTypes, std::string> MissionNameLookup;
#endif

//MissionTypes
MissionTypes getMISSION_MOVE_TO();
MissionTypes getMISSION_ROUTE_TO();
MissionTypes getMISSION_MOVE_TO_UNIT();
MissionTypes getMISSION_SWAP_UNITS();
MissionTypes getMISSION_SKIP();
MissionTypes getMISSION_SLEEP();
MissionTypes getMISSION_ALERT();
MissionTypes getMISSION_FORTIFY();
MissionTypes getMISSION_GARRISON();
MissionTypes getMISSION_SET_UP_FOR_RANGED_ATTACK(); //deprecated
MissionTypes getMISSION_EMBARK(); //deprecated 
MissionTypes getMISSION_DISEMBARK(); //deprecated
MissionTypes getMISSION_AIRPATROL();
MissionTypes getMISSION_HEAL();
MissionTypes getMISSION_AIRLIFT();
MissionTypes getMISSION_NUKE();
MissionTypes getMISSION_PARADROP();
MissionTypes getMISSION_AIR_SWEEP();
MissionTypes getMISSION_REBASE();
MissionTypes getMISSION_RANGE_ATTACK();
MissionTypes getMISSION_PILLAGE();
MissionTypes getMISSION_FOUND();
MissionTypes getMISSION_JOIN();
MissionTypes getMISSION_CONSTRUCT();
MissionTypes getMISSION_DISCOVER();
MissionTypes getMISSION_HURRY();
MissionTypes getMISSION_TRADE();
MissionTypes getMISSION_BUY_CITY_STATE();
MissionTypes getMISSION_REPAIR_FLEET();
MissionTypes getMISSION_SPACESHIP();
MissionTypes getMISSION_CULTURE_BOMB();
MissionTypes getMISSION_FOUND_RELIGION();
MissionTypes getMISSION_GOLDEN_AGE();
MissionTypes getMISSION_BUILD();
MissionTypes getMISSION_LEAD();
MissionTypes getMISSION_DIE_ANIMATION();
MissionTypes getMISSION_BEGIN_COMBAT();
MissionTypes getMISSION_END_COMBAT();
MissionTypes getMISSION_AIRSTRIKE();	//doesn't seem to be used
MissionTypes getMISSION_SURRENDER();
MissionTypes getMISSION_CAPTURED();
MissionTypes getMISSION_IDLE();
MissionTypes getMISSION_DIE();
MissionTypes getMISSION_DAMAGE();
MissionTypes getMISSION_MULTI_SELECT();
MissionTypes getMISSION_MULTI_DESELECT();
MissionTypes getMISSION_WAIT_FOR();
MissionTypes getMISSION_SPREAD_RELIGION();
MissionTypes getMISSION_ENHANCE_RELIGION();
MissionTypes getMISSION_REMOVE_HERESY();
MissionTypes getMISSION_ESTABLISH_TRADE_ROUTE();
MissionTypes getMISSION_PLUNDER_TRADE_ROUTE();
MissionTypes getMISSION_GREAT_WORK();
MissionTypes getMISSION_CHANGE_TRADE_UNIT_HOME_CITY();
MissionTypes getMISSION_SELL_EXOTIC_GOODS();
MissionTypes getMISSION_GIVE_POLICIES();
MissionTypes getMISSION_ONE_SHOT_TOURISM();
MissionTypes getMISSION_CHANGE_ADMIRAL_PORT();
#if defined(MOD_BALANCE_CORE)
MissionTypes getMISSION_FREE_LUXURY();
#endif
unsigned int getNUM_MISSION_TYPES();

GreatWorkArtifactClass getARTIFACT_ANCIENT_RUIN();
GreatWorkArtifactClass getARTIFACT_BARBARIAN_CAMP();
GreatWorkArtifactClass getARTIFACT_BATTLE_RANGED();
GreatWorkArtifactClass getARTIFACT_BATTLE_MELEE(); 
GreatWorkArtifactClass getARTIFACT_RAZED_CITY();
GreatWorkArtifactClass getARTIFACT_WRITING();
#if defined(MOD_BALANCE_CORE)
GreatWorkArtifactClass getARTIFACT_SARCOPHAGUS();
#endif

GreatWorkSlotType getGREAT_WORK_SLOT_ART_ARTIFACT();
GreatWorkSlotType getGREAT_WORK_SLOT_LITERATURE();
GreatWorkSlotType getGREAT_WORK_SLOT_MUSIC();
GreatWorkSlotType getGREAT_WORK_SLOT_RELIC();
GreatWorkSlotType getGREAT_WORK_SLOT_FILM();

}