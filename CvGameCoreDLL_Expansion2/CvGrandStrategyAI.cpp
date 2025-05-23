/*	-------------------------------------------------------------------------------------------------------
	© 1991-2012 Take-Two Interactive Software and its subsidiaries.  Developed by Firaxis Games.  
	Sid Meier's Civilization V, Civ, Civilization, 2K Games, Firaxis Games, Take-Two Interactive Software 
	and their respective logos are all trademarks of Take-Two interactive Software, Inc.  
	All other marks and trademarks are the property of their respective owners.  
	All rights reserved. 
	------------------------------------------------------------------------------------------------------- */
#include "CvGameCoreDLLPCH.h"
#include "CvGrandStrategyAI.h"
#include "CvEconomicAI.h"
#include "CvCitySpecializationAI.h"
#include "CvDiplomacyAI.h"
#include "CvMinorCivAI.h"
#include "ICvDLLUserInterface.h"
#include "CvSpanSerialization.h"

// must be included after all other headers
#include "LintFree.h"

//------------------------------------------------------------------------------
CvAIGrandStrategyXMLEntry::CvAIGrandStrategyXMLEntry(void):
	m_piFlavorValue(NULL),
	m_piSpecializationBoost(NULL),
	m_piFlavorModValue(NULL)
{
}
//------------------------------------------------------------------------------
CvAIGrandStrategyXMLEntry::~CvAIGrandStrategyXMLEntry(void)
{
	SAFE_DELETE_ARRAY(m_piFlavorValue);
	SAFE_DELETE_ARRAY(m_piSpecializationBoost);
	SAFE_DELETE_ARRAY(m_piFlavorModValue);
}
//------------------------------------------------------------------------------
bool CvAIGrandStrategyXMLEntry::CacheResults(Database::Results& kResults, CvDatabaseUtility& kUtility)
{
	if(!CvBaseInfo::CacheResults(kResults, kUtility))
		return false;

	//Arrays
	const char* szType = GetType();
	kUtility.SetFlavors(m_piFlavorValue, "AIGrandStrategy_Flavors", "AIGrandStrategyType", szType);
	kUtility.SetYields(m_piSpecializationBoost, "AIGrandStrategy_Yields", "AIGrandStrategyType", szType);
	kUtility.SetFlavors(m_piFlavorModValue, "AIGrandStrategy_FlavorMods", "AIGrandStrategyType", szType);

	return true;
}

/// What Flavors will be added by adopting this Grand Strategy?
int CvAIGrandStrategyXMLEntry::GetFlavorValue(int i) const
{
	ASSERT_DEBUG(i < GC.getNumFlavorTypes(), "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_piFlavorValue ? m_piFlavorValue[i] : -1;
}

/// What Flavors will be added by adopting this Grand Strategy?
int CvAIGrandStrategyXMLEntry::GetSpecializationBoost(YieldTypes eYield) const
{
	ASSERT_DEBUG(eYield < NUM_YIELD_TYPES, "Index out of bounds");
	ASSERT_DEBUG(eYield > -1, "Index out of bounds");
	return m_piSpecializationBoost ? m_piSpecializationBoost[(int)eYield] : 0;
}

/// What Flavors will be added by adopting this Grand Strategy?
int CvAIGrandStrategyXMLEntry::GetFlavorModValue(int i) const
{
	ASSERT_DEBUG(i < GC.getNumFlavorTypes(), "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_piFlavorModValue ? m_piFlavorModValue[i] : 0;
}



//=====================================
// CvAIGrandStrategyXMLEntries
//=====================================
/// Constructor
CvAIGrandStrategyXMLEntries::CvAIGrandStrategyXMLEntries(void)
{

}

/// Destructor
CvAIGrandStrategyXMLEntries::~CvAIGrandStrategyXMLEntries(void)
{
	DeleteArray();
}

/// Returns vector of AIStrategy entries
std::vector<CvAIGrandStrategyXMLEntry*>& CvAIGrandStrategyXMLEntries::GetAIGrandStrategyEntries()
{
	return m_paAIGrandStrategyEntries;
}

/// Number of defined AIStrategies
int CvAIGrandStrategyXMLEntries::GetNumAIGrandStrategies()
{
	return m_paAIGrandStrategyEntries.size();
}

/// Clear AIStrategy entries
void CvAIGrandStrategyXMLEntries::DeleteArray()
{
	for(std::vector<CvAIGrandStrategyXMLEntry*>::iterator it = m_paAIGrandStrategyEntries.begin(); it != m_paAIGrandStrategyEntries.end(); ++it)
	{
		SAFE_DELETE(*it);
	}

	m_paAIGrandStrategyEntries.clear();
}

/// Get a specific entry
CvAIGrandStrategyXMLEntry* CvAIGrandStrategyXMLEntries::GetEntry(int index)
{
	return (index!=NO_AIGRANDSTRATEGY) ? m_paAIGrandStrategyEntries[index] : NULL;
}



//=====================================
// CvGrandStrategyAI
//=====================================
/// Constructor
CvGrandStrategyAI::CvGrandStrategyAI():
	m_paiGrandStrategyPriority(NULL)
{
}

/// Destructor
CvGrandStrategyAI::~CvGrandStrategyAI(void)
{
}

/// Initialize
void CvGrandStrategyAI::Init(CvAIGrandStrategyXMLEntries* pAIGrandStrategies, CvPlayer* pPlayer)
{
	// Store off the pointer to the AIStrategies active for this game
	m_pAIGrandStrategies = pAIGrandStrategies;

	m_pPlayer = pPlayer;

	// Initialize AIGrandStrategy status array
	ASSERT_DEBUG(m_paiGrandStrategyPriority==NULL, "about to leak memory, CvGrandStrategyAI::m_paiGrandStrategyPriority");
	m_paiGrandStrategyPriority = FNEW(int[m_pAIGrandStrategies->GetNumAIGrandStrategies()], c_eCiv5GameplayDLL, 0);

	Reset();
}

/// Deallocate memory created in initialize
void CvGrandStrategyAI::Uninit()
{
	SAFE_DELETE_ARRAY(m_paiGrandStrategyPriority);
}

/// Reset AIStrategy status array to all false
void CvGrandStrategyAI::Reset()
{
	int iI = 0;

	m_iNumTurnsSinceActiveSet = 0;

	m_eActiveGrandStrategy = NO_AIGRANDSTRATEGY;

	for(iI = 0; iI < m_pAIGrandStrategies->GetNumAIGrandStrategies(); iI++)
	{
		m_paiGrandStrategyPriority[iI] = -1;
	}

	for(iI = 0; iI < MAX_MAJOR_CIVS; iI++)
	{
		m_eGuessOtherPlayerActiveGrandStrategy[iI] = NO_AIGRANDSTRATEGY;
		m_eGuessOtherPlayerActiveGrandStrategyConfidence[iI] = GUESS_CONFIDENCE_UNSURE;
	}

	//defaults
	m_iFlavorGold = 7;
	m_iFlavorScience = 7;
	m_iFlavorCulture = 7;
	m_iFlavorProduction = 7;
	m_iFlavorFaith = 7;
	m_iFlavorHappiness = 7;
	m_iFlavorGrowth = 7;
	m_iFlavorDiplomacy = 7;
}

///
template<typename GrandStrategyAI, typename Visitor>
void CvGrandStrategyAI::Serialize(GrandStrategyAI& grandStrategyAI, Visitor& visitor)
{
	visitor(grandStrategyAI.m_iNumTurnsSinceActiveSet);
	visitor(grandStrategyAI.m_eActiveGrandStrategy);

	ASSERT_DEBUG(grandStrategyAI.m_pAIGrandStrategies != NULL && grandStrategyAI.m_pAIGrandStrategies->GetNumAIGrandStrategies() > 0, "Number of AIGrandStrategies to serialize is expected to greater than 0");
	visitor(MakeConstSpan(grandStrategyAI.m_paiGrandStrategyPriority, grandStrategyAI.m_pAIGrandStrategies->GetNumAIGrandStrategies()));

	visitor(grandStrategyAI.m_eGuessOtherPlayerActiveGrandStrategy);
	visitor(grandStrategyAI.m_eGuessOtherPlayerActiveGrandStrategyConfidence);
}

/// Serialization read
void CvGrandStrategyAI::Read(FDataStream& kStream)
{
	CvStreamLoadVisitor serialVisitor(kStream);
	CvGrandStrategyAI::Serialize(*this, serialVisitor);

}

/// Serialization write
void CvGrandStrategyAI::Write(FDataStream& kStream) const
{
	CvStreamSaveVisitor serialVisitor(kStream);
	CvGrandStrategyAI::Serialize(*this, serialVisitor);
}

FDataStream& operator>>(FDataStream& stream, CvGrandStrategyAI& grandStrategyAI)
{
	grandStrategyAI.Read(stream);
	return stream;
}
FDataStream& operator<<(FDataStream& stream, const CvGrandStrategyAI& grandStrategyAI)
{
	grandStrategyAI.Write(stream);
	return stream;
}

/// Returns the Player object the Strategies are associated with
CvPlayer* CvGrandStrategyAI::GetPlayer()
{
	return m_pPlayer;
}

/// Returns AIGrandStrategies object stored in this class
CvAIGrandStrategyXMLEntries* CvGrandStrategyAI::GetAIGrandStrategies()
{
	return m_pAIGrandStrategies;
}

/// Runs every turn to determine what the player's Active Grand Strategy is and to change Priority Levels as necessary
void CvGrandStrategyAI::DoTurn()
{
	DoGuessOtherPlayersActiveGrandStrategy();

	int iGrandStrategiesLoop = 0;
	AIGrandStrategyTypes eGrandStrategy;
	CvAIGrandStrategyXMLEntry* pGrandStrategy = NULL;
	CvString strGrandStrategyName;

	//Only run this on turns we need it.
	if ((GetActiveGrandStrategy() != NO_AIGRANDSTRATEGY && GetNumTurnsSinceActiveSet() == 0) || (GetActiveGrandStrategy() == NO_AIGRANDSTRATEGY))
	{
		// Loop through all GrandStrategies to set their Priorities
		for (iGrandStrategiesLoop = 0; iGrandStrategiesLoop < GetAIGrandStrategies()->GetNumAIGrandStrategies(); iGrandStrategiesLoop++)
		{
			eGrandStrategy = (AIGrandStrategyTypes)iGrandStrategiesLoop;
			pGrandStrategy = GetAIGrandStrategies()->GetEntry(iGrandStrategiesLoop);
			strGrandStrategyName = (CvString)pGrandStrategy->GetType();

			// Base Priority looks at Personality Flavors (0 - 10) and multiplies * the Flavors attached to a Grand Strategy (0-10),
			// so expect a number between 0 and 100 back from this
			int iPriority = GetBaseGrandStrategyPriority(eGrandStrategy);

			int iTempPriority = 0;
			if (strGrandStrategyName == "AIGRANDSTRATEGY_CONQUEST")
			{
				iTempPriority += GetConquestPriority();
			}
			else if (strGrandStrategyName == "AIGRANDSTRATEGY_CULTURE")
			{
				iTempPriority += GetCulturePriority();
			}
			else if (strGrandStrategyName == "AIGRANDSTRATEGY_UNITED_NATIONS")
			{
				iTempPriority += GetUnitedNationsPriority();
			}
			else if (strGrandStrategyName == "AIGRANDSTRATEGY_SPACESHIP")
			{
				iTempPriority += GetSpaceshipPriority();
			}

			//reduce the potency of these until the mid game.
			int MaxTurn = GC.getGame().getEstimateEndTurn();
			if (MaxTurn > 0)
			{
				iTempPriority *= (GC.getGame().getGameTurn() * 2);
				iTempPriority /= MaxTurn;
			}

			iPriority += iTempPriority;

			// Give a boost to the current strategy so that small fluctuation doesn't cause a big change
			if (GetActiveGrandStrategy() == eGrandStrategy && GetActiveGrandStrategy() != NO_AIGRANDSTRATEGY)
			{
				iPriority += /*50*/ GD_INT_GET(AI_GRAND_STRATEGY_CURRENT_STRATEGY_WEIGHT);
			}

			SetGrandStrategyPriority(eGrandStrategy, max(1, iPriority));
		}
		// Now look at what we think the other players in the game are up to - we might have an opportunity to capitalize somewhere
		vector<int> viNumGrandStrategiesAdopted;
		int iNumPlayers = 0;

		// Init vector
		for (iGrandStrategiesLoop = 0; iGrandStrategiesLoop < GetAIGrandStrategies()->GetNumAIGrandStrategies(); iGrandStrategiesLoop++)
		{
			iNumPlayers = 0;

			// Tally up how many players we think are pusuing each Grand Strategy
			for (int iMajorLoop = 0; iMajorLoop < MAX_MAJOR_CIVS; iMajorLoop++)
			{
				PlayerTypes ePlayer = (PlayerTypes)iMajorLoop;
				if (ePlayer == NO_PLAYER || ePlayer == GetPlayer()->GetID())
					continue;

				if (GET_PLAYER(ePlayer).isAlive())
				{
					if (GET_TEAM(GetPlayer()->getTeam()).isHasMet(GET_PLAYER(ePlayer).getTeam()))
					{
						if (GetGuessOtherPlayerActiveGrandStrategy(ePlayer) == (AIGrandStrategyTypes)iGrandStrategiesLoop)
						{
							if (OtherPlayerDoingBetterThanUs(ePlayer, (AIGrandStrategyTypes)iGrandStrategiesLoop))
							{
								iNumPlayers++;
							}
						}
					}
				}
			}

			viNumGrandStrategiesAdopted.push_back(iNumPlayers);
		}

		vector<int> viGrandStrategyChangeForLogging;

		// Now modify our preferences based on how many people are going for stuff
		for (iGrandStrategiesLoop = 0; iGrandStrategiesLoop < GetAIGrandStrategies()->GetNumAIGrandStrategies(); iGrandStrategiesLoop++)
		{
			eGrandStrategy = (AIGrandStrategyTypes)iGrandStrategiesLoop;

			//Get 1/3.
			int iFraction = (GetGrandStrategyPriority(eGrandStrategy) * /*33*/ GD_INT_GET(AI_GRAND_STRATEGY_OTHER_PLAYERS_GS_MULTIPLIER)) / 100;
			int iNumPursuing = viNumGrandStrategiesAdopted[eGrandStrategy];
			int iChange = 0;
			if (iNumPursuing > 0)
			{
				//Modified to only reduce based on known and if we're doing better/worse than them.
				//If 3+ are doing better than us at this GS, we need to move on.
				iChange = min(iFraction * iNumPursuing, GetGrandStrategyPriority(eGrandStrategy));
			}

			ChangeGrandStrategyPriority(eGrandStrategy, -iChange);
			viGrandStrategyChangeForLogging.push_back(-iChange);
		}
		// Now see which Grand Strategy should be active, based on who has the highest Priority right now
		// Grand Strategy must be run for at least 10 turns
		int iBestPriority = -1;
		int iPriority = 0;

		AIGrandStrategyTypes eBestGrandStrategy = NO_AIGRANDSTRATEGY;

		for (iGrandStrategiesLoop = 0; iGrandStrategiesLoop < GetAIGrandStrategies()->GetNumAIGrandStrategies(); iGrandStrategiesLoop++)
		{
			eGrandStrategy = (AIGrandStrategyTypes)iGrandStrategiesLoop;

			iPriority = GetGrandStrategyPriority(eGrandStrategy);

			if (iPriority > iBestPriority)
			{
				iBestPriority = iPriority;
				eBestGrandStrategy = eGrandStrategy;
			}
		}

		if (eBestGrandStrategy != GetActiveGrandStrategy())
		{
			SetActiveGrandStrategy(eBestGrandStrategy);
			m_pPlayer->GetCitySpecializationAI()->SetSpecializationsDirty(SPECIALIZATION_UPDATE_NEW_GRAND_STRATEGY);
		}
		ChangeNumTurnsSinceActiveSet(1);

		LogGrandStrategies(viGrandStrategyChangeForLogging);
	}
	else
	{
		ChangeNumTurnsSinceActiveSet(1);
		if (GetNumTurnsSinceActiveSet() >= /*10*/ GD_INT_GET(AI_GRAND_STRATEGY_NUM_TURNS_STRATEGY_MUST_BE_ACTIVE))
		{
			//Reset to zero.
			SetNumTurnsSinceActiveSet(0);
		}
	}

	if (!m_pPlayer->isHuman())
	{
		m_iFlavorGold = GetPersonalityAndGrandStrategy((FlavorTypes)GC.getInfoTypeForString("FLAVOR_GOLD"));
		m_iFlavorScience = GetPersonalityAndGrandStrategy((FlavorTypes)GC.getInfoTypeForString("FLAVOR_SCIENCE"));
		m_iFlavorCulture = GetPersonalityAndGrandStrategy((FlavorTypes)GC.getInfoTypeForString("FLAVOR_CULTURE"));
		m_iFlavorProduction = GetPersonalityAndGrandStrategy((FlavorTypes)GC.getInfoTypeForString("FLAVOR_PRODUCTION"));
		m_iFlavorFaith = GetPersonalityAndGrandStrategy((FlavorTypes)GC.getInfoTypeForString("FLAVOR_RELIGION"));
		m_iFlavorHappiness = GetPersonalityAndGrandStrategy((FlavorTypes)GC.getInfoTypeForString("FLAVOR_HAPPINESS"));
		m_iFlavorGrowth = GetPersonalityAndGrandStrategy((FlavorTypes)GC.getInfoTypeForString("FLAVOR_GROWTH"));
		m_iFlavorDiplomacy = GetPersonalityAndGrandStrategy((FlavorTypes)GC.getInfoTypeForString("FLAVOR_DIPLOMACY"));
	}

}

/// Returns Priority for Conquest Grand Strategy
int CvGrandStrategyAI::GetConquestPriority()
{
	int iPriority = 0;

	// If Conquest Victory isn't even available then don't bother with anything
	VictoryTypes eVictory = (VictoryTypes)GC.getInfoTypeForString("VICTORY_DOMINATION", true);
	if (eVictory == NO_VICTORY || !GC.getGame().isVictoryValid(eVictory))
	{
		if (!GC.getGame().areNoVictoriesValid())
		{
			return -100;
		}
	}
	
	// Game options have disabled war as an option to win, so pick something else
	if (!GC.getGame().CanPlayerAttemptDominationVictory(GetPlayer()->GetID(), NO_PLAYER, GC.getGame().areNoVictoriesValid()))
		return -100;

	// If we're close to winning, let's go to the finish line!
	if (GetPlayer()->GetDiplomacyAI()->IsCloseToWorldConquest())
	{
		iPriority += 9000;
	}

	int iGeneralWarlikeness = GetPlayer()->GetDiplomacyAI()->GetMajorCivApproachBias(CIV_APPROACH_WAR);
	int iGeneralHostility = GetPlayer()->GetDiplomacyAI()->GetMajorCivApproachBias(CIV_APPROACH_HOSTILE);
	int iGeneralDeceptiveness = GetPlayer()->GetDiplomacyAI()->GetMajorCivApproachBias(CIV_APPROACH_DECEPTIVE);
	int iGeneralFriendliness = GetPlayer()->GetDiplomacyAI()->GetMajorCivApproachBias(CIV_APPROACH_FRIENDLY);

	int iGeneralApproachModifier = max(max(iGeneralDeceptiveness, iGeneralHostility), iGeneralWarlikeness) - iGeneralFriendliness;
	// Boldness gives the base weight for Conquest (no flavors added earlier)
	int iEra = m_pPlayer->GetCurrentEra();
	if(iEra <= 0)
	{
		iEra = 1;
	}
	iPriority += (GetPlayer()->GetDiplomacyAI()->GetBoldness() + iGeneralApproachModifier + GetPlayer()->GetDiplomacyAI()->GetMeanness()) * (10 - iEra); // make a little less likely as time goes on

	CvTeam& pTeam = GET_TEAM(GetPlayer()->getTeam());

	// How many turns must have passed before we test for having met nobody?
	if (GC.getGame().getElapsedGameTurns() >= /*20*/ GD_INT_GET(AI_GS_CONQUEST_NOBODY_MET_FIRST_TURN))
	{
		// If we haven't met any Major Civs yet, then we probably shouldn't be planning on conquering the world
		bool bHasMetMajor = false;

		for (int iTeamLoop = 0; iTeamLoop < MAX_CIV_TEAMS; iTeamLoop++)
		{
			if (pTeam.GetID() != iTeamLoop && !GET_TEAM((TeamTypes)iTeamLoop).isMinorCiv())
			{
				if (pTeam.isHasMet((TeamTypes)iTeamLoop))
				{
					bHasMetMajor = true;
					break;
				}
			}
		}
		if (!bHasMetMajor)
		{
			iPriority += /*-100*/ GD_INT_GET(AI_GRAND_STRATEGY_CONQUEST_NOBODY_MET_WEIGHT);
		}
	}

	int iNumConquests = GetPlayer()->GetDiplomacyAI()->GetPlayerNumMajorsConquered(GetPlayer()->GetID());
	if (iNumConquests == 1)
	{
		iPriority += 50;
	}
	else if (iNumConquests > 1)
	{
		iPriority += (iNumConquests * 125);
	}

	// How many turns must have passed before we test for us having a weak military?
	if (GC.getGame().getElapsedGameTurns() >= /*60*/ GD_INT_GET(AI_GS_CONQUEST_MILITARY_STRENGTH_FIRST_TURN))
	{
		// Compare our military strength to the rest of the world
		int iWorldMilitaryStrength = GC.getGame().GetWorldMilitaryStrengthAverage(GetPlayer()->GetID(), true, true);

		//Reduce world average if we're rocking multiple capitals.
		if (iNumConquests > 0)
		{
			iWorldMilitaryStrength *= 100;
			iWorldMilitaryStrength /= (100 + (iNumConquests * 10));
		}

		if (iWorldMilitaryStrength > 0)
		{
			int iMilitaryRatio = (GetPlayer()->GetMilitaryMight() - iWorldMilitaryStrength) * /*100*/ GD_INT_GET(AI_GRAND_STRATEGY_CONQUEST_POWER_RATIO_MULTIPLIER) / iWorldMilitaryStrength;

			// Make the likelihood of BECOMING a warmonger lower than dropping the bad behavior
			if (iMilitaryRatio > 0)
				iMilitaryRatio /= 2;

			iPriority += iMilitaryRatio;	// This will add between -100 and 100 depending on this player's MilitaryStrength relative the world average. The number will typically be near 0 though, as it's fairly hard to get away from the world average
		}
	}

	// If we're at war, then boost the weight a bit
	if (pTeam.getAtWarCount(/*bIgnoreMinors*/ false) > 0)
	{
		iPriority += /*10*/ GD_INT_GET(AI_GRAND_STRATEGY_CONQUEST_AT_WAR_WEIGHT);
	}

	// If our neighbors are cramping our style, consider less... scrupulous means of obtaining more land
	int iNumPlayersMet = 1;	// Include 1 for me!
	int iTotalLandMe = 0;
	int iTotalLandPlayersMet = 0;

	bool bDesperate = GetPlayer()->GetPlayerPolicies()->GetLateGamePolicyTree() != NO_POLICY_BRANCH_TYPE && !GetPlayer()->GetDiplomacyAI()->IsCloseToWorldConquest() && !GetPlayer()->GetDiplomacyAI()->IsCloseToCultureVictory() && !GetPlayer()->GetDiplomacyAI()->IsCloseToDiploVictory() && !GetPlayer()->GetDiplomacyAI()->IsCloseToSpaceshipVictory();
	int iTotalNumDangerPlayers = 0;

	// Count the number of Majors we know
	for (int iMajorLoop = 0; iMajorLoop < MAX_MAJOR_CIVS; iMajorLoop++)
	{
		PlayerTypes ePlayer = (PlayerTypes) iMajorLoop;

		if (GET_PLAYER(ePlayer).isAlive() && iMajorLoop != GetPlayer()->GetID())
		{
			if (pTeam.isHasMet(GET_PLAYER(ePlayer).getTeam()))
			{
				iNumPlayersMet++;
				if (GetPlayer()->GetPlayerPolicies()->GetLateGamePolicyTree() != NO_POLICY_BRANCH_TYPE)
				{
					if (GET_PLAYER(ePlayer).GetDiplomacyAI()->IsCloseToCultureVictory() || GET_PLAYER(ePlayer).GetDiplomacyAI()->IsCloseToDiploVictory() || GET_PLAYER(ePlayer).GetDiplomacyAI()->IsCloseToSpaceshipVictory())
					{
						//Close to nothing, and someone else is? Eek!
						if (bDesperate)
							iTotalNumDangerPlayers += 25;
						//someone else is close, but we are too? Let's keep an eye on domination as an option.
						else
							iTotalNumDangerPlayers += 5;
					}
					//They have an ideology, and we don't? Uh-oh.
					else if (bDesperate)
						iTotalNumDangerPlayers += 5;
				}
			}
		}
	}

	if (iNumPlayersMet > 0)
	{
		// Check every plot for ownership
		for (int iPlotLoop = 0; iPlotLoop < GC.getMap().numPlots(); iPlotLoop++)
		{
			if (GC.getMap().plotByIndexUnchecked(iPlotLoop)->isOwned())
			{
				PlayerTypes ePlayer = GC.getMap().plotByIndexUnchecked(iPlotLoop)->getOwner();

				if (ePlayer == GetPlayer()->GetID())
				{
					iTotalLandPlayersMet++;
					iTotalLandMe++;
				}
				else if (!GET_PLAYER(ePlayer).isMinorCiv() && pTeam.isHasMet(GET_PLAYER(ePlayer).getTeam()))
				{
					iTotalLandPlayersMet++;
				}
			}
		}

		iTotalLandPlayersMet /= iNumPlayersMet;

		if (iTotalLandMe > 0)
		{
			if (iTotalLandPlayersMet / iTotalLandMe > 1)
			{
				iPriority += GetPlayer()->IsCramped() ? /*100*/ GD_INT_GET(AI_GRAND_STRATEGY_CONQUEST_CRAMPED_WEIGHT) * 5 : /*20*/ GD_INT_GET(AI_GRAND_STRATEGY_CONQUEST_CRAMPED_WEIGHT);
			}
		}
		if (iTotalNumDangerPlayers > 0)
		{
			iPriority += iTotalNumDangerPlayers * iEra;
		}
	}
	// if we do not have nukes and we know someone else who does...
	if (GetPlayer()->getNumNukeUnits() == 0)
	{
		for (int iMajorLoop = 0; iMajorLoop < MAX_MAJOR_CIVS; iMajorLoop++)
		{
			PlayerTypes ePlayer = (PlayerTypes)iMajorLoop;

			if (GET_PLAYER(ePlayer).isAlive() && iMajorLoop != GetPlayer()->GetID())
			{
				if (pTeam.isHasMet(GET_PLAYER(ePlayer).getTeam()))
				{
					if (GET_PLAYER(ePlayer).getNumNukeUnits() > 0)
					{
						iPriority -= 50;
						break;
					}
				}
			}
		}
	}

	int iPriorityBonus = 0;
	//Add priority value based on flavors of policies we've acquired.
	for (int iPolicyLoop = 0; iPolicyLoop < GC.getNumPolicyInfos(); iPolicyLoop++)
	{
		const PolicyTypes ePolicy = static_cast<PolicyTypes>(iPolicyLoop);
		CvPolicyEntry* pkPolicyInfo = GC.getPolicyInfo(ePolicy);
		if (pkPolicyInfo)
		{
			if (GetPlayer()->GetPlayerPolicies()->HasPolicy(ePolicy) && !GetPlayer()->GetPlayerPolicies()->IsPolicyBlocked(ePolicy))
			{
				for (int iFlavorLoop = 0; iFlavorLoop < GC.getNumFlavorTypes(); iFlavorLoop++)
				{
					if (GC.getFlavorTypes((FlavorTypes)iFlavorLoop) == "FLAVOR_OFFENSE")
					{
						iPriorityBonus += pkPolicyInfo->GetFlavorValue(iFlavorLoop);
					}
					if (GC.getFlavorTypes((FlavorTypes)iFlavorLoop) == "FLAVOR_MOBILE")
					{
						iPriorityBonus += pkPolicyInfo->GetFlavorValue(iFlavorLoop);
					}
					else if (GC.getFlavorTypes((FlavorTypes)iFlavorLoop) == "FLAVOR_MILITARY_TRAINING")
					{
						iPriorityBonus += pkPolicyInfo->GetFlavorValue(iFlavorLoop);
					}
					else if (GC.getFlavorTypes((FlavorTypes)iFlavorLoop) == "FLAVOR_NAVAL")
					{
						iPriorityBonus += pkPolicyInfo->GetFlavorValue(iFlavorLoop);
					}
					else if (GC.getFlavorTypes((FlavorTypes)iFlavorLoop) == "FLAVOR_NAVAL_RECON")
					{
						iPriorityBonus += pkPolicyInfo->GetFlavorValue(iFlavorLoop);
					}
					else if (GC.getFlavorTypes((FlavorTypes)iFlavorLoop) == "FLAVOR_RANGED")
					{
						iPriorityBonus += pkPolicyInfo->GetFlavorValue(iFlavorLoop);
					}
					else if (GC.getFlavorTypes((FlavorTypes)iFlavorLoop) == "FLAVOR_AIR")
					{
						iPriorityBonus += pkPolicyInfo->GetFlavorValue(iFlavorLoop);
					}

					//AI_UNIT_PRODUCTION
					else if (MOD_AI_UNIT_PRODUCTION)
					{
						if (GC.getFlavorTypes((FlavorTypes)iFlavorLoop) == "FLAVOR_ARCHER")
						{
							iPriorityBonus += pkPolicyInfo->GetFlavorValue(iFlavorLoop);
						}
						else if (GC.getFlavorTypes((FlavorTypes)iFlavorLoop) == "FLAVOR_SIEGE")
						{
							iPriorityBonus += pkPolicyInfo->GetFlavorValue(iFlavorLoop);
						}
						else if (GC.getFlavorTypes((FlavorTypes)iFlavorLoop) == "FLAVOR_SKIRMISHER")
						{
							iPriorityBonus += pkPolicyInfo->GetFlavorValue(iFlavorLoop);
						}
						else if (GC.getFlavorTypes((FlavorTypes)iFlavorLoop) == "FLAVOR_NAVAL_MELEE")
						{
							iPriorityBonus += pkPolicyInfo->GetFlavorValue(iFlavorLoop);
						}
						else if (GC.getFlavorTypes((FlavorTypes)iFlavorLoop) == "FLAVOR_NAVAL_RANGED")
						{
							iPriorityBonus += pkPolicyInfo->GetFlavorValue(iFlavorLoop);
						}
						else if (GC.getFlavorTypes((FlavorTypes)iFlavorLoop) == "FLAVOR_SUBMARINE")
						{
							iPriorityBonus += pkPolicyInfo->GetFlavorValue(iFlavorLoop);
						}
						else if (GC.getFlavorTypes((FlavorTypes)iFlavorLoop) == "FLAVOR_FIGHTER")
						{
							iPriorityBonus += pkPolicyInfo->GetFlavorValue(iFlavorLoop);
						}
						else if (GC.getFlavorTypes((FlavorTypes)iFlavorLoop) == "FLAVOR_BOMBER")
						{
							iPriorityBonus += pkPolicyInfo->GetFlavorValue(iFlavorLoop);
						}
					}
				}
			}
		}
	}
	//Look for Buildings and grab flavors.
	int iLoop = 0;
	for (CvCity* pLoopCity = m_pPlayer->firstCity(&iLoop); pLoopCity != NULL; pLoopCity = m_pPlayer->nextCity(&iLoop))
	{
		if (pLoopCity != NULL)
		{
			//Add in our base production value.
			iPriorityBonus += (pLoopCity->getProduction() / 10);

			int iNumBuildingInfos = GC.getNumBuildingInfos();
			for (int iI = 0; iI < iNumBuildingInfos; iI++)
			{
				const BuildingTypes eBuildingLoop = static_cast<BuildingTypes>(iI);
				if (eBuildingLoop == NO_BUILDING)
					continue;
				if (pLoopCity->GetCityBuildings()->GetNumBuilding(eBuildingLoop) > 0)
				{
					CvBuildingEntry* pkLoopBuilding = GC.getBuildingInfo(eBuildingLoop);
					if (pkLoopBuilding)
					{
						for (int iFlavorLoop = 0; iFlavorLoop < GC.getNumFlavorTypes(); iFlavorLoop++)
						{
							if (GC.getFlavorTypes((FlavorTypes)iFlavorLoop) == "FLAVOR_OFFENSE")
							{
								iPriorityBonus += pkLoopBuilding->GetFlavorValue(iFlavorLoop) * 2;
							}
							else if (GC.getFlavorTypes((FlavorTypes)iFlavorLoop) == "FLAVOR_MILITARY_TRAINING")
							{
								iPriorityBonus += pkLoopBuilding->GetFlavorValue(iFlavorLoop);
							}
							else if (GC.getFlavorTypes((FlavorTypes)iFlavorLoop) == "FLAVOR_NAVAL")
							{
								iPriorityBonus += pkLoopBuilding->GetFlavorValue(iFlavorLoop);
							}
						}
					}
				}
			}
		}
	}
	/////////
	//RELIGION CHECKS
	////////////
	ReligionTypes eReligion = m_pPlayer->GetReligions()->GetStateReligion();
	if (eReligion != NO_RELIGION)
	{
		const CvReligion* pReligion = GC.getGame().GetGameReligions()->GetReligion(eReligion, m_pPlayer->GetID());
		if (pReligion)
		{
			CvCity* pHolyCity = pReligion->GetHolyCity();
			CvBeliefXMLEntries* pkBeliefs = GC.GetGameBeliefs();
			const int iNumBeliefs = pkBeliefs->GetNumBeliefs();
			for (int iI = 0; iI < iNumBeliefs; iI++)
			{
				const BeliefTypes eBelief(static_cast<BeliefTypes>(iI));
				CvBeliefEntry* pEntry = pkBeliefs->GetEntry(eBelief);
				if (pEntry && pReligion->m_Beliefs.HasBelief(eBelief) && pReligion->m_Beliefs.IsBeliefValid(eBelief, eReligion, m_pPlayer->GetID(), pHolyCity))
				{
					if (pEntry->GetCombatModifierEnemyCities() > 0)
					{
						iPriorityBonus += pEntry->GetCombatModifierEnemyCities();
					}
					if (pEntry->GetCombatVersusOtherReligionTheirLands() > 0)
					{
						iPriorityBonus += pEntry->GetCombatVersusOtherReligionTheirLands();
					}
					if (pEntry->GetFaithFromKills() > 0)
					{
						iPriorityBonus += pEntry->GetFaithFromKills();
					}
					for (int iI = 0; iI < NUM_YIELD_TYPES; iI++)
					{
						const YieldTypes eYield = static_cast<YieldTypes>(iI);
						if (eYield != NO_YIELD)
						{
							if (pEntry->GetYieldFromConquest(eYield) > 0)
							{
								iPriorityBonus += pEntry->GetYieldFromConquest(eYield);
							}
							if (pEntry->GetYieldFromKills(eYield) > 0)
							{
								iPriorityBonus += pEntry->GetYieldFromKills(eYield);
							}
						}
					}
				}
			}
		}
	}
	iPriorityBonus /= 8;

	if (m_pPlayer->GetPlayerTraits()->IsWarmonger())
	{
		iPriorityBonus += 100;
	}
	if (m_pPlayer->GetPlayerTraits()->IsSmaller())
	{
		iPriorityBonus -= 100;
	}
	if (m_pPlayer->GetPlayerTraits()->IsTourism())
	{
		iPriorityBonus += 25;
	}
	if (m_pPlayer->GetPlayerTraits()->IsDiplomat())
	{
		iPriorityBonus += -25;
	}
	if (m_pPlayer->GetPlayerTraits()->IsNerd())
	{
		iPriorityBonus += 25;
	}
	if (m_pPlayer->GetPlayerTraits()->IsReligious())
	{
		iPriorityBonus += 25;
	}
	if (m_pPlayer->GetPlayerTraits()->IsExpansionist())
	{
		iPriorityBonus += 50;
	}

	iPriority += iPriorityBonus;

	return iPriority;
}

/// Returns Priority for Culture Grand Strategy
int CvGrandStrategyAI::GetCulturePriority()
{
	int iPriority = 0;

	// If Culture Victory isn't even available then don't bother with anything
	VictoryTypes eVictory = (VictoryTypes) GC.getInfoTypeForString("VICTORY_CULTURAL", true);
	if(eVictory == NO_VICTORY || !GC.getGame().isVictoryValid(eVictory))
	{
		return -100;
	}

	// If we're close to winning, let's go to the finish line!
	if (GetPlayer()->GetDiplomacyAI()->IsCloseToCultureVictory())
	{
		iPriority += 8000;
	}

	// Before tourism kicks in, add weight based on flavor
	int iFlavorCulture =  m_pPlayer->GetFlavorManager()->GetPersonalityIndividualFlavor((FlavorTypes)GC.getInfoTypeForString("FLAVOR_CULTURE"));

	int iEra = m_pPlayer->GetCurrentEra();
	if(iEra <= 0)
	{
		iEra = 1;
	}
	iPriority += ((iEra * iFlavorCulture * 150) / 100);

	// Loop through Players to see how we are doing on Tourism and Culture
	PlayerTypes eLoopPlayer;
	int iOurCulture = m_pPlayer->GetTotalJONSCulturePerTurnTimes100();
	int iOurTourism = m_pPlayer->GetCulture()->GetTourism();
	int iNumCivsBehindCulture = 0;
	int iNumCivsAheadCulture = 0;
	int iNumCivsBehindTourism = 0;
	int iNumCivsAheadTourism = 0;
	int iNumCivsAlive = 0;

	for(int iPlayerLoop = 0; iPlayerLoop < MAX_CIV_PLAYERS; iPlayerLoop++)
	{
		eLoopPlayer = (PlayerTypes) iPlayerLoop;
		CvPlayer &kPlayer = GET_PLAYER(eLoopPlayer);

		if (kPlayer.isAlive() && !kPlayer.isMinorCiv() && !kPlayer.isBarbarian() && iPlayerLoop != m_pPlayer->GetID())
		{
			if (iOurCulture > kPlayer.GetTotalJONSCulturePerTurnTimes100())
			{
				iNumCivsAheadCulture++;
			}
			else
			{
				iNumCivsBehindCulture++;
			}
			if (iOurTourism > kPlayer.GetCulture()->GetTourism())
			{
				iNumCivsAheadTourism++;
			}
			else
			{
				iNumCivsBehindTourism++;
			}
			iNumCivsAlive++;
		}
	}

	if (iNumCivsAlive > 0 && iNumCivsAheadCulture > iNumCivsBehindCulture)
	{
		iPriority += /*50*/ GD_INT_GET(AI_GS_CULTURE_AHEAD_WEIGHT) * (iNumCivsAheadCulture - iNumCivsBehindCulture) / iNumCivsAlive;
	}
	if (iNumCivsAlive > 0 && iNumCivsAheadTourism > iNumCivsBehindTourism)
	{
		iPriority += /*100*/ GD_INT_GET(AI_GS_CULTURE_TOURISM_AHEAD_WEIGHT) * (iNumCivsAheadTourism - iNumCivsBehindTourism) / iNumCivsAlive;
	}
	if (iNumCivsAlive > 0 && iNumCivsAheadTourism < iNumCivsBehindTourism)
	{
		iPriority -= /*100*/ GD_INT_GET(AI_GS_CULTURE_TOURISM_AHEAD_WEIGHT) * (iNumCivsBehindTourism - iNumCivsAheadTourism) / iNumCivsAlive;
	}

	// for every civ we are Influential over increase this
	int iNumInfluential = m_pPlayer->GetCulture()->GetNumCivsInfluentialOn();
	iPriority += iNumInfluential * /*50*/ GD_INT_GET(AI_GS_CULTURE_INFLUENTIAL_CIV_MOD);

	int iPriorityBonus = 0;

	//Add in our base culture value.
	iPriorityBonus += (m_pPlayer->getJONSCultureTimes100() / 3000);
	iPriorityBonus += (m_pPlayer->GetCulture()->GetTourism() / 130);

	iPriorityBonus += (m_pPlayer->GetNumEffectiveCities() * -10);

	//Add priority value based on flavors of policies we've acquired.
	for(int iPolicyLoop = 0; iPolicyLoop < GC.getNumPolicyInfos(); iPolicyLoop++)
	{
		const PolicyTypes ePolicy = static_cast<PolicyTypes>(iPolicyLoop);
		CvPolicyEntry* pkPolicyInfo = GC.getPolicyInfo(ePolicy);
		if(pkPolicyInfo)
		{
			if(GetPlayer()->GetPlayerPolicies()->HasPolicy(ePolicy) && !GetPlayer()->GetPlayerPolicies()->IsPolicyBlocked(ePolicy))
			{
				for(int iFlavorLoop = 0; iFlavorLoop < GC.getNumFlavorTypes(); iFlavorLoop++)
				{
					if(GC.getFlavorTypes((FlavorTypes) iFlavorLoop) == "FLAVOR_GREAT_PEOPLE")
					{
						iPriorityBonus += pkPolicyInfo->GetFlavorValue(iFlavorLoop);
					}
					else if(GC.getFlavorTypes((FlavorTypes) iFlavorLoop) == "FLAVOR_CULTURE")
					{
						iPriorityBonus += pkPolicyInfo->GetFlavorValue(iFlavorLoop);
					}
					else if(GC.getFlavorTypes((FlavorTypes) iFlavorLoop) == "FLAVOR_WONDER")
					{
						iPriorityBonus += pkPolicyInfo->GetFlavorValue(iFlavorLoop);
					}
					else if (GC.getFlavorTypes((FlavorTypes)iFlavorLoop) == "FLAVOR_HAPPINESS")
					{
						iPriorityBonus += pkPolicyInfo->GetFlavorValue(iFlavorLoop);
					}
				}
			}
		}
	}
	//Look for Buildings and grab flavors.
	int iLoop = 0;
	for (CvCity* pLoopCity = m_pPlayer->firstCity(&iLoop); pLoopCity != NULL; pLoopCity = m_pPlayer->nextCity(&iLoop)) 
	{
		if(pLoopCity != NULL)
		{
			int iNumBuildingInfos = GC.getNumBuildingInfos();
			for(int iI = 0; iI < iNumBuildingInfos; iI++)
			{
				const BuildingTypes eBuildingLoop = static_cast<BuildingTypes>(iI);
				if(eBuildingLoop == NO_BUILDING)
					continue;
				if(pLoopCity->GetCityBuildings()->GetNumBuilding(eBuildingLoop) > 0)
				{
					CvBuildingEntry* pkLoopBuilding = GC.getBuildingInfo(eBuildingLoop);
					if(pkLoopBuilding)
					{
						for(int iFlavorLoop = 0; iFlavorLoop < GC.getNumFlavorTypes(); iFlavorLoop++)
						{
							if(GC.getFlavorTypes((FlavorTypes) iFlavorLoop) == "FLAVOR_CULTURE")
							{
								iPriorityBonus += pkLoopBuilding->GetFlavorValue(iFlavorLoop);
							}
							else if(GC.getFlavorTypes((FlavorTypes) iFlavorLoop) == "FLAVOR_GREAT_PEOPLE")
							{
								iPriorityBonus += pkLoopBuilding->GetFlavorValue(iFlavorLoop);
							}
							else if(GC.getFlavorTypes((FlavorTypes) iFlavorLoop) == "FLAVOR_WONDER")
							{
								iPriorityBonus += pkLoopBuilding->GetFlavorValue(iFlavorLoop);
							}
							else if (GC.getFlavorTypes((FlavorTypes)iFlavorLoop) == "FLAVOR_HAPPINESS")
							{
								iPriorityBonus += pkLoopBuilding->GetFlavorValue(iFlavorLoop);
							}
						}
					}
				}
			}
		}
	}
	/////////
	//RELIGION CHECKS
	////////////
	ReligionTypes eReligion = m_pPlayer->GetReligions()->GetStateReligion();
	if (eReligion != NO_RELIGION)
	{
		const CvReligion* pReligion = GC.getGame().GetGameReligions()->GetReligion(eReligion, m_pPlayer->GetID());
		if (pReligion)
		{
			CvCity* pHolyCity = pReligion->GetHolyCity();
			CvBeliefXMLEntries* pkBeliefs = GC.GetGameBeliefs();
			const int iNumBeliefs = pkBeliefs->GetNumBeliefs();
			for(int iI = 0; iI < iNumBeliefs; iI++)
			{
				const BeliefTypes eBelief(static_cast<BeliefTypes>(iI));
				CvBeliefEntry* pEntry = pkBeliefs->GetEntry(eBelief);
				if (pEntry && pReligion->m_Beliefs.HasBelief(eBelief) && pReligion->m_Beliefs.IsBeliefValid(eBelief, eReligion, m_pPlayer->GetID(), pHolyCity))
				{
					if(pEntry->FaithPurchaseAllGreatPeople())
					{
						iPriorityBonus += 100;
					}
					int iNumBuildingInfos = GC.getNumBuildingInfos();
					for(int iI = 0; iI < iNumBuildingInfos; iI++)
					{
						const BuildingTypes eBuildingLoop = static_cast<BuildingTypes>(iI);
						if(eBuildingLoop == NO_BUILDING)
							continue;

						CvBuildingEntry* pkLoopBuilding = GC.getBuildingInfo(eBuildingLoop);
						if(pkLoopBuilding)
						{
							if(pEntry->GetBuildingClassYieldChange(pkLoopBuilding->GetBuildingClassType(), YIELD_TOURISM))
							{
								iPriorityBonus += 100;
								break;
							}
						}
					}
					if(pEntry->GetFaithBuildingTourism() > 0)
					{
						iPriorityBonus += 100;
					}
					for(int iI = 0; iI < NUM_YIELD_TYPES; iI++)
					{
						const YieldTypes eYield = static_cast<YieldTypes>(iI);
						if(eYield != NO_YIELD)
						{
							if(pEntry->GetYieldFromPolicyUnlock(eYield) > 0)
							{
								iPriorityBonus += pEntry->GetYieldFromPolicyUnlock(eYield);
							}
						}
					}
				}
			}
		}
	}
	iPriorityBonus /= 8;

	if (m_pPlayer->GetPlayerTraits()->IsWarmonger())
	{
		iPriorityBonus -= 50;
	}
	if (m_pPlayer->GetPlayerTraits()->IsSmaller())
	{
		iPriorityBonus += 50;
	}
	if (m_pPlayer->GetPlayerTraits()->IsTourism())
	{
		iPriorityBonus += 100;
	}
	if (m_pPlayer->GetPlayerTraits()->IsDiplomat())
	{
		iPriorityBonus += 25;
	}
	if (m_pPlayer->GetPlayerTraits()->IsNerd())
	{
		iPriorityBonus += 25;
	}
	if (m_pPlayer->GetPlayerTraits()->IsReligious())
	{
		iPriorityBonus += 25;
	}
	if (m_pPlayer->GetPlayerTraits()->IsExpansionist())
	{
		iPriorityBonus -= 50;
	}

	iPriority += iPriorityBonus;

	return iPriority;
}

/// Returns Priority for United Nations Grand Strategy
int CvGrandStrategyAI::GetUnitedNationsPriority()
{
	int iPriority = 0;
	PlayerTypes ePlayer = m_pPlayer->GetID();

	// If UN Victory isn't even available then don't bother with anything
	VictoryTypes eVictory = (VictoryTypes) GC.getInfoTypeForString("VICTORY_DIPLOMATIC", true);
	if(eVictory == NO_VICTORY || !GC.getGame().isVictoryValid(eVictory))
	{
		return -100;
	}

	// If we're close to winning, let's go to the finish line!
	if (GetPlayer()->GetDiplomacyAI()->IsCloseToDiploVictory())
	{
		iPriority += 7000;
	}

	int iPriorityBonus = 0;
	//Add in our base gold value.
	iPriorityBonus += (m_pPlayer->GetTreasury()->CalculateBaseNetGold() / 25);

	if (MOD_BALANCE_VP)
	{
		ResourceTypes ePaper = (ResourceTypes)GC.getInfoTypeForString("RESOURCE_PAPER", true);
		if (ePaper != NO_RESOURCE && m_pPlayer->getResourceFromCSAlliances(ePaper) > 0)
		{
			iPriorityBonus += m_pPlayer->getResourceFromCSAlliances(ePaper) / 5;
		}
	}

	//Add priority value based on flavors of policies we've acquired.
	for(int iPolicyLoop = 0; iPolicyLoop < GC.getNumPolicyInfos(); iPolicyLoop++)
	{
		const PolicyTypes ePolicy = static_cast<PolicyTypes>(iPolicyLoop);
		CvPolicyEntry* pkPolicyInfo = GC.getPolicyInfo(ePolicy);
		if(pkPolicyInfo)
		{
			if(GetPlayer()->GetPlayerPolicies()->HasPolicy(ePolicy) && !GetPlayer()->GetPlayerPolicies()->IsPolicyBlocked(ePolicy))
			{
				for(int iFlavorLoop = 0; iFlavorLoop < GC.getNumFlavorTypes(); iFlavorLoop++)
				{
					if(GC.getFlavorTypes((FlavorTypes) iFlavorLoop) == "FLAVOR_GOLD")
					{
						iPriorityBonus += pkPolicyInfo->GetFlavorValue(iFlavorLoop);
					}
					else if(GC.getFlavorTypes((FlavorTypes) iFlavorLoop) == "FLAVOR_DIPLOMACY")
					{
						iPriorityBonus += pkPolicyInfo->GetFlavorValue(iFlavorLoop) * 3;
					}
					else if (GC.getFlavorTypes((FlavorTypes)iFlavorLoop) == "FLAVOR_PRODUCTION")
					{
						iPriorityBonus += pkPolicyInfo->GetFlavorValue(iFlavorLoop);
					}
					else if (GC.getFlavorTypes((FlavorTypes)iFlavorLoop) == "FLAVOR_RELIGION")
					{
						iPriorityBonus += pkPolicyInfo->GetFlavorValue(iFlavorLoop);
					}
				}
			}
		}
	}
	//Look for Buildings and grab flavors.
	int iLoop = 0;
	for (CvCity* pLoopCity = m_pPlayer->firstCity(&iLoop); pLoopCity != NULL; pLoopCity = m_pPlayer->nextCity(&iLoop)) 
	{
		if(pLoopCity != NULL)
		{
			int iNumBuildingInfos = GC.getNumBuildingInfos();
			for(int iI = 0; iI < iNumBuildingInfos; iI++)
			{
				const BuildingTypes eBuildingLoop = static_cast<BuildingTypes>(iI);
				if(eBuildingLoop == NO_BUILDING)
					continue;
				if(pLoopCity->GetCityBuildings()->GetNumBuilding(eBuildingLoop) > 0)
				{
					CvBuildingEntry* pkLoopBuilding = GC.getBuildingInfo(eBuildingLoop);
					if(pkLoopBuilding)
					{
						for(int iFlavorLoop = 0; iFlavorLoop < GC.getNumFlavorTypes(); iFlavorLoop++)
						{
							if(GC.getFlavorTypes((FlavorTypes) iFlavorLoop) == "FLAVOR_GOLD")
							{
								iPriorityBonus += pkLoopBuilding->GetFlavorValue(iFlavorLoop);
							}
							else if(GC.getFlavorTypes((FlavorTypes) iFlavorLoop) == "FLAVOR_DIPLOMACY")
							{
								iPriorityBonus += pkLoopBuilding->GetFlavorValue(iFlavorLoop) * 2;
							}
							else if (GC.getFlavorTypes((FlavorTypes)iFlavorLoop) == "FLAVOR_PRODUCTION")
							{
								iPriorityBonus += pkLoopBuilding->GetFlavorValue(iFlavorLoop);
							}
							else if (GC.getFlavorTypes((FlavorTypes)iFlavorLoop) == "FLAVOR_RELIGION")
							{
								iPriorityBonus += pkLoopBuilding->GetFlavorValue(iFlavorLoop);
							}
						}
					}
				}
			}
		}
	}
	/////////
	//RELIGION CHECKS
	////////////
	ReligionTypes eReligion = m_pPlayer->GetReligions()->GetStateReligion();
	if (eReligion != NO_RELIGION)
	{
		const CvReligion* pReligion = GC.getGame().GetGameReligions()->GetReligion(eReligion, m_pPlayer->GetID());
		if (pReligion)
		{
			CvCity* pHolyCity = pReligion->GetHolyCity();
			CvBeliefXMLEntries* pkBeliefs = GC.GetGameBeliefs();
			const int iNumBeliefs = pkBeliefs->GetNumBeliefs();
			for (int iI = 0; iI < iNumBeliefs; iI++)
			{
				const BeliefTypes eBelief(static_cast<BeliefTypes>(iI));
				CvBeliefEntry* pEntry = pkBeliefs->GetEntry(eBelief);
				if (pEntry && pReligion->m_Beliefs.HasBelief(eBelief) && pReligion->m_Beliefs.IsBeliefValid(eBelief, eReligion, m_pPlayer->GetID(), pHolyCity))
				{
					if(pEntry->GetExtraVotes())
					{
						iPriorityBonus += pEntry->GetExtraVotes();
					}
					if(pEntry->GetCityStateInfluenceModifier() > 0)
					{
						iPriorityBonus += pEntry->GetCityStateInfluenceModifier();
					}
					if(pEntry->GetCityStateFriendshipModifier() > 0)
					{
						iPriorityBonus += pEntry->GetCityStateFriendshipModifier();
					}
					if(pEntry->GetCityStateMinimumInfluence() > 0)
					{
						iPriorityBonus += pEntry->GetCityStateMinimumInfluence();
					}
					if(pEntry->GetMissionaryInfluenceCS() > 0)
					{
						iPriorityBonus += pEntry->GetMissionaryInfluenceCS();
					}
					for(int iI = 0; iI < NUM_YIELD_TYPES; iI++)
					{
						const YieldTypes eYield = static_cast<YieldTypes>(iI);
						if(eYield != NO_YIELD)
						{
							if(pEntry->GetYieldFromHost(eYield) > 0)
							{
								iPriorityBonus += pEntry->GetYieldFromHost(eYield);
							}
							if(pEntry->GetYieldFromProposal(eYield) > 0)
							{
								iPriorityBonus += pEntry->GetYieldFromProposal(eYield);
							}
						}
					}
				}
			}
		}
	}
	iPriorityBonus /= 10;

	if (m_pPlayer->GetPlayerTraits()->IsWarmonger())
	{
		iPriorityBonus -= 100;
	}
	if (m_pPlayer->GetPlayerTraits()->IsSmaller())
	{
		iPriorityBonus += 25;
	}
	if (m_pPlayer->GetPlayerTraits()->IsTourism())
	{
		iPriorityBonus += 25;
	}
	if (m_pPlayer->GetPlayerTraits()->IsDiplomat())
	{
		iPriorityBonus += 100;
	}
	if (m_pPlayer->GetPlayerTraits()->IsNerd())
	{
		iPriorityBonus += 25;
	}
	if (m_pPlayer->GetPlayerTraits()->IsReligious())
	{
		iPriorityBonus += 50;
	}
	if (m_pPlayer->GetPlayerTraits()->IsExpansionist())
	{
		iPriorityBonus += 50;
	}

	iPriority += iPriorityBonus;

	int iVotesNeededToWin = GC.getGame().GetVotesNeededForDiploVictory();

	int iVotesControlled = 0;
	int iVotesControlledDelta = 0;
	int iUnalliedCityStates = 0;
	if (GC.getGame().GetGameLeagues()->GetNumActiveLeagues() == 0)
	{
		// Before leagues kick in, add weight based on flavor
		int iFlavorDiplo =  m_pPlayer->GetFlavorManager()->GetPersonalityIndividualFlavor((FlavorTypes)GC.getInfoTypeForString("FLAVOR_DIPLOMACY"));
		iPriority += (10 - m_pPlayer->GetCurrentEra()) * iFlavorDiplo * 125 / 100;
	}
	else
	{
		CvLeague* pLeague = GC.getGame().GetGameLeagues()->GetActiveLeague();
		ASSERT_DEBUG(pLeague != NULL);
		if (pLeague != NULL)
		{
			// Votes we control
			iVotesControlled += pLeague->CalculateStartingVotesForMember(ePlayer);

			// Votes other players control
			int iHighestOtherPlayerVotes = 0;
			for (int iPlayerLoop = 0; iPlayerLoop < MAX_CIV_PLAYERS; iPlayerLoop++)
			{
				PlayerTypes eLoopPlayer = (PlayerTypes) iPlayerLoop;

				if(eLoopPlayer != ePlayer && GET_PLAYER(eLoopPlayer).isAlive())
				{
					if (GET_PLAYER(eLoopPlayer).isMinorCiv())
					{
						if (GET_PLAYER(eLoopPlayer).GetMinorCivAI()->GetAlly() == NO_PLAYER)
						{
							iUnalliedCityStates++;
						}
					}
					else
					{
						int iOtherPlayerVotes = pLeague->CalculateStartingVotesForMember(eLoopPlayer);
						if (iOtherPlayerVotes > iHighestOtherPlayerVotes)
						{
							iHighestOtherPlayerVotes = iOtherPlayerVotes;
						}
					}
				}
			}

			// How we compare
			iVotesControlledDelta = iVotesControlled - iHighestOtherPlayerVotes;
		}
	}

	// Are we close to winning?
	if (iVotesControlled >= iVotesNeededToWin)
	{
		iPriority *= 15;
	}
	else if (iVotesControlled >= ((iVotesNeededToWin * 3) / 4))
	{
		iPriority *= 10;
	}
	else if (iVotesControlled >= ((iVotesNeededToWin * 2) / 4))
	{
		iPriority *= 5;
	}
	// We have the most votes
	if (iVotesControlledDelta > 0)
	{
		iPriority += MAX(100, iVotesControlledDelta * 20);
	}
	// We are equal or behind in votes
	else
	{
		// Could we make up the difference with currently unallied city-states?
		int iPotentialCityStateVotes = iUnalliedCityStates * 2;
		int iPotentialVotesDelta = iPotentialCityStateVotes + iVotesControlledDelta;
		if (iPotentialVotesDelta > 0)
		{
			iPriority += MAX(20, iPotentialVotesDelta * 5);
		}
		else if (iPotentialVotesDelta < 0)
		{
			iPriority += MIN(-100, iPotentialVotesDelta * -50);
		}
	}

	// factor in some traits that could be useful (or harmful)
	iPriority += (m_pPlayer->GetPlayerTraits()->GetCityStateFriendshipModifier() * 2);
	iPriority += (m_pPlayer->GetPlayerTraits()->GetCityStateBonusModifier() * 2);
	iPriority -= (m_pPlayer->GetPlayerTraits()->GetCityStateCombatModifier() * 2);
	iPriority -= (m_pPlayer->GetCityStateCombatModifier());
	iPriority += (m_pPlayer->GetPlayerTraits()->GetAllianceCSDefense() / 2);
	iPriority += (m_pPlayer->GetPlayerTraits()->GetAllianceCSStrength() / 2);
	for(int iI = 0; iI < NUM_YIELD_TYPES; iI++)
	{
		const YieldTypes eYield = static_cast<YieldTypes>(iI);
		if(eYield != NO_YIELD)
		{
			iPriority += m_pPlayer->GetPlayerTraits()->GetYieldFromCSAlly(eYield);
			iPriority += m_pPlayer->GetPlayerTraits()->GetYieldFromCSFriend(eYield);
		}
	}

	return iPriority;
}

/// Returns Priority for Spaceship Grand Strategy
int CvGrandStrategyAI::GetSpaceshipPriority()
{
	int iPriority = 0;

	// If SS Victory isn't even available then don't bother with anything
	VictoryTypes eVictory = (VictoryTypes) GC.getInfoTypeForString("VICTORY_SPACE_RACE", true);
	if(eVictory == NO_VICTORY || !GC.getGame().isVictoryValid(eVictory))
	{
		return -100;
	}

	// If we're close to winning, let's go to the finish line!
	if (GetPlayer()->GetDiplomacyAI()->IsCloseToSpaceshipVictory())
	{
		iPriority += 10000;
	}

	int iFlavorScience =  m_pPlayer->GetFlavorManager()->GetPersonalityIndividualFlavor((FlavorTypes)GC.getInfoTypeForString("FLAVOR_SCIENCE"));

	// the later the game the greater the chance
	iPriority += ((m_pPlayer->GetCurrentEra() * m_pPlayer->GetCurrentEra()) * max(1, iFlavorScience) * 300) / 100;

	int iNumCivsAheadScience = 0;
	int iNumCivsBehindScience = 0;
	int iNumCivsAlive = 0;

	for (int iPlayerLoop = 0; iPlayerLoop < MAX_CIV_PLAYERS; iPlayerLoop++)
	{
		PlayerTypes eLoopPlayer = (PlayerTypes) iPlayerLoop;
		CvPlayer &kPlayer = GET_PLAYER(eLoopPlayer);

		if (kPlayer.isAlive() && !kPlayer.isMinorCiv() && !kPlayer.isBarbarian() && iPlayerLoop != m_pPlayer->GetID())
		{
			if (GET_TEAM(GetPlayer()->getTeam()).GetTeamTechs()->GetNumTechsKnown() > GET_TEAM(kPlayer.getTeam()).GetTeamTechs()->GetNumTechsKnown())
			{
				iNumCivsAheadScience++;
			}
			else
			{
				iNumCivsBehindScience++;
			}
			iNumCivsAlive++;
		}
	}
	if (iNumCivsAlive > 0 && iNumCivsAheadScience > iNumCivsBehindScience)
	{
		iPriority += /*50*/ GD_INT_GET(AI_GS_CULTURE_AHEAD_WEIGHT) * (iNumCivsAheadScience - iNumCivsBehindScience) / iNumCivsAlive;
	}

	//Are we in first overall? Bump it!
	if (iNumCivsAheadScience == iNumCivsAlive)
	{
		iPriority += /*50*/ GD_INT_GET(AI_GS_CULTURE_AHEAD_WEIGHT) * iNumCivsAlive;
	}
	// if I already built the Apollo Program I am very likely to follow through
	ProjectTypes eApolloProgram = (ProjectTypes) GC.getInfoTypeForString("PROJECT_APOLLO_PROGRAM", true);
	if(eApolloProgram != NO_PROJECT)
	{
		if(GET_TEAM(m_pPlayer->getTeam()).getProjectCount(eApolloProgram) > 0)
		{
			iPriority += /*150*/ GD_INT_GET(AI_GS_SS_HAS_APOLLO_PROGRAM);
		}
	}

	int iPriorityBonus = 0;
	//Add in our base science value.
	iPriorityBonus += (m_pPlayer->GetScience() / 25);

	//Add priority value based on flavors of policies we've acquired.
	for(int iPolicyLoop = 0; iPolicyLoop < GC.getNumPolicyInfos(); iPolicyLoop++)
	{
		const PolicyTypes ePolicy = static_cast<PolicyTypes>(iPolicyLoop);
		CvPolicyEntry* pkPolicyInfo = GC.getPolicyInfo(ePolicy);
		if(pkPolicyInfo)
		{
			if(GetPlayer()->GetPlayerPolicies()->HasPolicy(ePolicy) && !GetPlayer()->GetPlayerPolicies()->IsPolicyBlocked(ePolicy))
			{
				for(int iFlavorLoop = 0; iFlavorLoop < GC.getNumFlavorTypes(); iFlavorLoop++)
				{
					if(GC.getFlavorTypes((FlavorTypes) iFlavorLoop) == "FLAVOR_SPACESHIP")
					{
						iPriorityBonus += pkPolicyInfo->GetFlavorValue(iFlavorLoop);
					}
					else if(GC.getFlavorTypes((FlavorTypes) iFlavorLoop) == "FLAVOR_SCIENCE")
					{
						iPriorityBonus += pkPolicyInfo->GetFlavorValue(iFlavorLoop);
					}
					else if(GC.getFlavorTypes((FlavorTypes) iFlavorLoop) == "FLAVOR_GROWTH")
					{
						iPriorityBonus += pkPolicyInfo->GetFlavorValue(iFlavorLoop);
					}
					else if (GC.getFlavorTypes((FlavorTypes)iFlavorLoop) == "FLAVOR_TILE_IMPROVEMENT")
					{
						iPriorityBonus += pkPolicyInfo->GetFlavorValue(iFlavorLoop);
					}
				}
			}
		}
	}
	//Look for Buildings and grab flavors.
	int iLoop = 0;
	for (CvCity* pLoopCity = m_pPlayer->firstCity(&iLoop); pLoopCity != NULL; pLoopCity = m_pPlayer->nextCity(&iLoop)) 
	{
		if(pLoopCity != NULL)
		{
			int iNumBuildingInfos = GC.getNumBuildingInfos();
			for(int iI = 0; iI < iNumBuildingInfos; iI++)
			{
				const BuildingTypes eBuildingLoop = static_cast<BuildingTypes>(iI);
				if(eBuildingLoop == NO_BUILDING)
					continue;
				if(pLoopCity->GetCityBuildings()->GetNumBuilding(eBuildingLoop) > 0)
				{
					CvBuildingEntry* pkLoopBuilding = GC.getBuildingInfo(eBuildingLoop);
					if(pkLoopBuilding)
					{
						for(int iFlavorLoop = 0; iFlavorLoop < GC.getNumFlavorTypes(); iFlavorLoop++)
						{
							if(GC.getFlavorTypes((FlavorTypes) iFlavorLoop) == "FLAVOR_SPACESHIP")
							{
								iPriorityBonus += pkLoopBuilding->GetFlavorValue(iFlavorLoop);
							}
							else if(GC.getFlavorTypes((FlavorTypes) iFlavorLoop) == "FLAVOR_SCIENCE")
							{
								iPriorityBonus += pkLoopBuilding->GetFlavorValue(iFlavorLoop);
							}
							else if(GC.getFlavorTypes((FlavorTypes) iFlavorLoop) == "FLAVOR_GROWTH")
							{
								iPriorityBonus += pkLoopBuilding->GetFlavorValue(iFlavorLoop);
							}
							else if (GC.getFlavorTypes((FlavorTypes)iFlavorLoop) == "FLAVOR_TILE_IMPROVEMENT")
							{
								iPriorityBonus += pkLoopBuilding->GetFlavorValue(iFlavorLoop);
							}
						}
					}
				}
			}
		}
	}
	/////////
	//RELIGION CHECKS
	////////////
	ReligionTypes eReligion = m_pPlayer->GetReligions()->GetStateReligion();
	if (eReligion != NO_RELIGION)
	{
		const CvReligion* pReligion = GC.getGame().GetGameReligions()->GetReligion(eReligion, m_pPlayer->GetID());
		if (pReligion)
		{
			CvCity* pHolyCity = pReligion->GetHolyCity();
			CvBeliefXMLEntries* pkBeliefs = GC.GetGameBeliefs();
			const int iNumBeliefs = pkBeliefs->GetNumBeliefs();
			for(int iI = 0; iI < iNumBeliefs; iI++)
			{
				const BeliefTypes eBelief(static_cast<BeliefTypes>(iI));
				CvBeliefEntry* pEntry = pkBeliefs->GetEntry(eBelief);
				if (pEntry && pReligion->m_Beliefs.HasBelief(eBelief) && pReligion->m_Beliefs.IsBeliefValid(eBelief, eReligion, m_pPlayer->GetID(), pHolyCity))
				{
					if(pEntry->GetSciencePerOtherReligionFollower() > 0)
					{
						iPriorityBonus += pEntry->GetSciencePerOtherReligionFollower() * 10;
					}
					
					for(int iI = 0; iI < NUM_YIELD_TYPES; iI++)
					{
						const YieldTypes eYield = static_cast<YieldTypes>(iI);
						if(eYield != NO_YIELD)
						{
							if(pEntry->GetYieldFromEraUnlock(eYield) > 0)
							{
								iPriorityBonus += pEntry->GetYieldFromEraUnlock(eYield);
							}
							if(pEntry->GetYieldPerScience(eYield) > 0)
							{
								iPriorityBonus += pEntry->GetYieldPerScience(eYield);
							}
						}
					}
				}
			}
		}
	}
	iPriorityBonus /= 10;

	if (m_pPlayer->GetPlayerTraits()->IsWarmonger())
	{
		iPriorityBonus -= 25;
	}
	if (m_pPlayer->GetPlayerTraits()->IsSmaller())
	{
		iPriorityBonus += 50;
	}
	if (m_pPlayer->GetPlayerTraits()->IsTourism())
	{
		iPriorityBonus += 25;
	}
	if (m_pPlayer->GetPlayerTraits()->IsDiplomat())
	{
		iPriorityBonus += 25;
	}
	if (m_pPlayer->GetPlayerTraits()->IsNerd())
	{
		iPriorityBonus += 100;
	}
	if (m_pPlayer->GetPlayerTraits()->IsReligious())
	{
		iPriorityBonus += 50;
	}
	if (m_pPlayer->GetPlayerTraits()->IsExpansionist())
	{
		iPriorityBonus -= 50;
	}

	iPriority += iPriorityBonus;

	return iPriority;
}

/// Get the base Priority for a Grand Strategy; these are elements common to ALL Grand Strategies
int CvGrandStrategyAI::GetBaseGrandStrategyPriority(AIGrandStrategyTypes eGrandStrategy)
{
	CvAIGrandStrategyXMLEntry* pGrandStrategy = GetAIGrandStrategies()->GetEntry(eGrandStrategy);

	int iPriority = 0;

	// Personality effect on Priority
	for(int iFlavorLoop = 0; iFlavorLoop < GC.getNumFlavorTypes(); iFlavorLoop++)
	{
		if(pGrandStrategy->GetFlavorValue(iFlavorLoop) != 0)
		{
			iPriority += (pGrandStrategy->GetFlavorValue(iFlavorLoop) * GetPlayer()->GetFlavorManager()->GetPersonalityIndividualFlavor((FlavorTypes) iFlavorLoop));
		}
	}

	return iPriority;
}

/// Get the base Priority for a Grand Strategy; these are elements common to ALL Grand Strategies
int CvGrandStrategyAI::GetPersonalityAndGrandStrategy(FlavorTypes eFlavorType, bool bBoostGSMainFlavor)
{
	if (m_eActiveGrandStrategy != NO_AIGRANDSTRATEGY)
	{
		CvAIGrandStrategyXMLEntry* pGrandStrategy = GetAIGrandStrategies()->GetEntry(m_eActiveGrandStrategy);
		int iModdedFlavor = pGrandStrategy->GetFlavorModValue(eFlavorType) + m_pPlayer->GetFlavorManager()->GetPersonalityIndividualFlavor(eFlavorType);
		iModdedFlavor = max(0,iModdedFlavor);

		if (bBoostGSMainFlavor && (pGrandStrategy->GetFlavorValue(eFlavorType) > 0))
		{
			iModdedFlavor = min(10, ((pGrandStrategy->GetFlavorValue(eFlavorType) + iModdedFlavor + 1) / 2));
		}

		return iModdedFlavor;
	}
	return m_pPlayer->GetFlavorManager()->GetPersonalityIndividualFlavor(eFlavorType);
}

/// Returns the Active Grand Strategy for this Player: how am I trying to win right now?
AIGrandStrategyTypes CvGrandStrategyAI::GetActiveGrandStrategy() const
{
	return m_eActiveGrandStrategy;
}

/// Sets the Active Grand Strategy for this Player: how am I trying to win right now?
void CvGrandStrategyAI::SetActiveGrandStrategy(AIGrandStrategyTypes eGrandStrategy)
{
	if(eGrandStrategy != NO_AIGRANDSTRATEGY)
	{
		m_eActiveGrandStrategy = eGrandStrategy;

		SetNumTurnsSinceActiveSet(0);
	}
}

/// The number of turns since the Active Strategy was last set
int CvGrandStrategyAI::GetNumTurnsSinceActiveSet() const
{
	return m_iNumTurnsSinceActiveSet;
}

/// Set the number of turns since the Active Strategy was last set
void CvGrandStrategyAI::SetNumTurnsSinceActiveSet(int iValue)
{
	m_iNumTurnsSinceActiveSet = iValue;
	ASSERT_DEBUG(m_iNumTurnsSinceActiveSet >= 0);
}

/// Change the number of turns since the Active Strategy was last set
void CvGrandStrategyAI::ChangeNumTurnsSinceActiveSet(int iChange)
{
	if(iChange != 0)
	{
		m_iNumTurnsSinceActiveSet += iChange;
	}

	ASSERT_DEBUG(m_iNumTurnsSinceActiveSet >= 0);
}

/// Returns the Priority Level the player has for a particular Grand Strategy
int CvGrandStrategyAI::GetGrandStrategyPriority(AIGrandStrategyTypes eGrandStrategy) const
{
	ASSERT_DEBUG(eGrandStrategy != NO_AIGRANDSTRATEGY);
	return m_paiGrandStrategyPriority[eGrandStrategy];
}

/// Sets the Priority Level the player has for a particular Grand Strategy
void CvGrandStrategyAI::SetGrandStrategyPriority(AIGrandStrategyTypes eGrandStrategy, int iValue)
{
	ASSERT_DEBUG(eGrandStrategy != NO_AIGRANDSTRATEGY);
	m_paiGrandStrategyPriority[eGrandStrategy] = iValue;
}

/// Changes the Priority Level the player has for a particular Grand Strategy
void CvGrandStrategyAI::ChangeGrandStrategyPriority(AIGrandStrategyTypes eGrandStrategy, int iChange)
{
	ASSERT_DEBUG(eGrandStrategy != NO_AIGRANDSTRATEGY);

	if(iChange != 0)
	{
		m_paiGrandStrategyPriority[eGrandStrategy] += iChange;
	}
}



// **********
// Stuff relating to guessing what other Players are up to
// **********



/// Runs every turn to try and figure out what other known Players' Grand Strategies are
void CvGrandStrategyAI::DoGuessOtherPlayersActiveGrandStrategy()
{
	CvWeightedVector<int> vGrandStrategyPriorities;
	vector<int>  vGrandStrategyPrioritiesForLogging;

	GuessConfidenceTypes eGuessConfidence = GUESS_CONFIDENCE_UNSURE;

	int iGrandStrategiesLoop = 0;
	AIGrandStrategyTypes eGrandStrategy = NO_AIGRANDSTRATEGY;
	CvAIGrandStrategyXMLEntry* pGrandStrategy = 0;
	CvString strGrandStrategyName;

	CvTeam& pTeam = GET_TEAM(GetPlayer()->getTeam());

	int iMajorLoop = 0;
	PlayerTypes eMajor = NO_PLAYER;

	int iPriority = 0;

	// Establish world Military strength average
	int iWorldMilitaryAverage = GC.getGame().GetWorldMilitaryStrengthAverage(GetPlayer()->GetID(), true, true);

	// Establish world culture and tourism averages
	int iNumPlayersAlive = 0;
	int iWorldCultureAverage = 0;
	int iWorldTourismAverage = 0;
	for(iMajorLoop = 0; iMajorLoop < MAX_MAJOR_CIVS; iMajorLoop++)
	{
		eMajor = (PlayerTypes) iMajorLoop;

		if(GET_PLAYER(eMajor).isAlive())
		{
			iWorldCultureAverage += (int)(GET_PLAYER(eMajor).GetJONSCultureEverGeneratedTimes100() / 100);
			iWorldTourismAverage += GET_PLAYER(eMajor).GetCulture()->GetTourism() / 100;
			iNumPlayersAlive++;
		}
	}
	iWorldCultureAverage /= iNumPlayersAlive;
	iWorldTourismAverage /= iNumPlayersAlive;

	// Establish world Tech progress average
	iNumPlayersAlive = 0;
	int iWorldNumTechsAverage = 0;
	TeamTypes eTeam;
	for(int iTeamLoop = 0; iTeamLoop < MAX_MAJOR_CIVS; iTeamLoop++)	// Looping over all MAJOR teams
	{
		eTeam = (TeamTypes) iTeamLoop;

		if(GET_TEAM(eTeam).isAlive())
		{
			iWorldNumTechsAverage += GET_TEAM(eTeam).GetTeamTechs()->GetNumTechsKnown();
			iNumPlayersAlive++;
		}
	}
	iWorldNumTechsAverage /= iNumPlayersAlive;

	// Look at every Major we've met
	for(iMajorLoop = 0; iMajorLoop < MAX_MAJOR_CIVS; iMajorLoop++)
	{
		eMajor = (PlayerTypes) iMajorLoop;

		if(GET_PLAYER(eMajor).isAlive() && iMajorLoop != GetPlayer()->GetID())
		{
			if(pTeam.isHasMet(GET_PLAYER(eMajor).getTeam()))
			{
				for(iGrandStrategiesLoop = 0; iGrandStrategiesLoop < GetAIGrandStrategies()->GetNumAIGrandStrategies(); iGrandStrategiesLoop++)
				{
					eGrandStrategy = (AIGrandStrategyTypes) iGrandStrategiesLoop;
					pGrandStrategy = GetAIGrandStrategies()->GetEntry(iGrandStrategiesLoop);
					strGrandStrategyName = (CvString) pGrandStrategy->GetType();

					if(strGrandStrategyName == "AIGRANDSTRATEGY_CONQUEST")
					{
						iPriority = GetGuessOtherPlayerConquestPriority(eMajor, iWorldMilitaryAverage);
					}
					else if(strGrandStrategyName == "AIGRANDSTRATEGY_CULTURE")
					{
						iPriority = GetGuessOtherPlayerCulturePriority(eMajor, iWorldCultureAverage, iWorldTourismAverage);
					}
					else if(strGrandStrategyName == "AIGRANDSTRATEGY_UNITED_NATIONS")
					{
						iPriority = GetGuessOtherPlayerUnitedNationsPriority(eMajor);
					}
					else if(strGrandStrategyName == "AIGRANDSTRATEGY_SPACESHIP")
					{
						iPriority = GetGuessOtherPlayerSpaceshipPriority(eMajor, iWorldNumTechsAverage);
					}

					vGrandStrategyPriorities.push_back(iGrandStrategiesLoop, iPriority);
					vGrandStrategyPrioritiesForLogging.push_back(iPriority);
				}

				if(vGrandStrategyPriorities.size() > 0)
				{
					// Add "No Grand Strategy" in case we just don't have enough info to go on
					iPriority = /*40*/ GD_INT_GET(AI_GRAND_STRATEGY_GUESS_NO_CLUE_WEIGHT);

					vGrandStrategyPriorities.push_back(NO_AIGRANDSTRATEGY, iPriority);
					vGrandStrategyPrioritiesForLogging.push_back(iPriority);

					vGrandStrategyPriorities.StableSortItems();

					eGrandStrategy = (AIGrandStrategyTypes) vGrandStrategyPriorities.GetElement(0);
					iPriority = vGrandStrategyPriorities.GetWeight(0);
					eGuessConfidence = GUESS_CONFIDENCE_UNSURE;

					// How confident are we in our Guess?
					if(eGrandStrategy != NO_AIGRANDSTRATEGY)
					{
						if(iPriority >= /*150*/ GD_INT_GET(AI_GRAND_STRATEGY_GUESS_POSITIVE_THRESHOLD))
						{
							eGuessConfidence = GUESS_CONFIDENCE_POSITIVE;
						}
						else if(iPriority >= /*80*/ GD_INT_GET(AI_GRAND_STRATEGY_GUESS_LIKELY_THRESHOLD))
						{
							eGuessConfidence = GUESS_CONFIDENCE_LIKELY;
						}
						else
						{
							eGuessConfidence = GUESS_CONFIDENCE_UNSURE;
						}
					}

					SetGuessOtherPlayerActiveGrandStrategy(eMajor, eGrandStrategy, eGuessConfidence);

					LogGuessOtherPlayerGrandStrategy(vGrandStrategyPrioritiesForLogging, eMajor);
				}

				vGrandStrategyPriorities.clear();
				vGrandStrategyPrioritiesForLogging.clear();
			}
		}
	}
}

/// What does this AI BELIEVE another player's Active Grand Strategy to be?
AIGrandStrategyTypes CvGrandStrategyAI::GetGuessOtherPlayerActiveGrandStrategy(PlayerTypes ePlayer) const
{
	ASSERT_DEBUG(ePlayer < MAX_MAJOR_CIVS);
	return (AIGrandStrategyTypes) m_eGuessOtherPlayerActiveGrandStrategy[ePlayer];
}

/// How confident is the AI in its guess of what another player's Active Grand Strategy is?
GuessConfidenceTypes CvGrandStrategyAI::GetGuessOtherPlayerActiveGrandStrategyConfidence(PlayerTypes ePlayer) const
{
	ASSERT_DEBUG(ePlayer < MAX_MAJOR_CIVS);
	return (GuessConfidenceTypes) m_eGuessOtherPlayerActiveGrandStrategyConfidence[ePlayer];
}

/// Sets what this AI BELIEVES another player's Active Grand Strategy to be
void CvGrandStrategyAI::SetGuessOtherPlayerActiveGrandStrategy(PlayerTypes ePlayer, AIGrandStrategyTypes eGrandStrategy, GuessConfidenceTypes eGuessConfidence)
{
	ASSERT_DEBUG(ePlayer < MAX_MAJOR_CIVS);
	m_eGuessOtherPlayerActiveGrandStrategy[ePlayer] = eGrandStrategy;
	m_eGuessOtherPlayerActiveGrandStrategyConfidence[ePlayer] = eGuessConfidence;
}

bool CvGrandStrategyAI::OtherPlayerDoingBetterThanUs(PlayerTypes ePlayer, AIGrandStrategyTypes eGrandStrategy)
{
	CvAIGrandStrategyXMLEntry* pGrandStrategy = 0;
	CvString strGrandStrategyName;

	if(ePlayer == NO_PLAYER || eGrandStrategy == NO_AIGRANDSTRATEGY)
		return false;

	pGrandStrategy = GetAIGrandStrategies()->GetEntry((int)eGrandStrategy);
	if(pGrandStrategy)
	{
		strGrandStrategyName = (CvString) pGrandStrategy->GetType();
		if(strGrandStrategyName == "AIGRANDSTRATEGY_CONQUEST")
		{
			int iNumCapitalsUs = GetPlayer()->GetNumCapitalCities();
			int iNumCapitalsThem = GET_PLAYER(ePlayer).GetNumCapitalCities();
			int iMilitaryPowerUs = GetPlayer()->GetMilitaryMight();
			int iMilitaryPowerThem = GET_PLAYER(ePlayer).GetMilitaryMight();
			iMilitaryPowerUs *= max(1, iNumCapitalsUs);
			iMilitaryPowerThem *= max(1, iNumCapitalsThem);
			if(iMilitaryPowerThem > iMilitaryPowerUs)
			{
				return true;
			}
		}
		else if(strGrandStrategyName == "AIGRANDSTRATEGY_CULTURE")
		{
			int iOurInfluence = GetPlayer()->GetCulture()->GetNumCivsInfluentialOn();
			int iOurTourism = GetPlayer()->GetCulture()->GetTourism() / 100;
			int iOurCulture = GetPlayer()->GetTotalJONSCulturePerTurnTimes100() / 100;
			int iOurTotal = (iOurCulture + iOurTourism) * (max(1, iOurInfluence));
			
			int iTheirInfluence = GET_PLAYER(ePlayer).GetCulture()->GetNumCivsInfluentialOn();
			int iTheirTourism = GET_PLAYER(ePlayer).GetCulture()->GetTourism() / 100;
			int iTheirCulture = GET_PLAYER(ePlayer).GetTotalJONSCulturePerTurnTimes100() / 100;
			int iTheirTotal = (iTheirCulture + iTheirTourism) * (max(1, iTheirInfluence));

			if(iTheirTotal > iOurTotal)
			{
				return true;
			}
		}
		else if(strGrandStrategyName == "AIGRANDSTRATEGY_UNITED_NATIONS")
		{
			int iTheirVotes = 0;
			int iMyVotes = 0;
			CvLeague* pLeague = GC.getGame().GetGameLeagues()->GetActiveLeague();
			if(pLeague != NULL)
			{
				iTheirVotes = pLeague->CalculateStartingVotesForMember(ePlayer);
				iMyVotes = pLeague->CalculateStartingVotesForMember(m_pPlayer->GetID());
				if((iTheirVotes * 2) > (iMyVotes * 3))
				{
					return true;
				}
			}	
		}
		else if(strGrandStrategyName == "AIGRANDSTRATEGY_SPACESHIP")
		{
			ProjectTypes eApolloProgram = (ProjectTypes) GC.getInfoTypeForString("PROJECT_APOLLO_PROGRAM", true);
			if(eApolloProgram != NO_PROJECT)
			{
				//They have Apollo, and we don't.
				if(GET_TEAM(GET_PLAYER(ePlayer).getTeam()).getProjectCount(eApolloProgram) > 0)
				{
					if(GET_TEAM(m_pPlayer->getTeam()).getProjectCount(eApolloProgram) <= 0)
					{
						return true;
					}
				}
				else
				{
					int iTheirTechNum = GET_TEAM(GET_PLAYER(ePlayer).getTeam()).GetTeamTechs()->GetNumTechsKnown();
					int iOurTechNum = GET_TEAM(GetPlayer()->getTeam()).GetTeamTechs()->GetNumTechsKnown();
					int iOurScience = GetPlayer()->GetScience();
					int iTheirScience = GET_PLAYER(ePlayer).GetScience();
					int iOurTotal = (iOurTechNum * iOurScience);
					int iTheirTotal = (iTheirTechNum * iTheirScience);

					if(iTheirTotal > iOurTotal)
					{
						return true;
					}
				}
			}
		}
	}
	return false;
}

/// Guess as to how much another Player is prioritizing Conquest as his means of winning the game
int CvGrandStrategyAI::GetGuessOtherPlayerConquestPriority(PlayerTypes ePlayer, int iWorldMilitaryAverage)
{
	// If they can't attempt Domination Victory because of game options, then don't bother with any of this.
	if (!GC.getGame().CanPlayerAttemptDominationVictory(ePlayer, NO_PLAYER, GC.getGame().areNoVictoriesValid()))
		return 0;

	int iConquestPriority = 0;

	// Compare their Military to the world average; Possible range is 100 to -100 (but will typically be around -20 to 20)
	if(iWorldMilitaryAverage > 0)
	{
		iConquestPriority += (GET_PLAYER(ePlayer).GetMilitaryMight() - iWorldMilitaryAverage) * /*100*/ GD_INT_GET(AI_GRAND_STRATEGY_CONQUEST_POWER_RATIO_MULTIPLIER) / iWorldMilitaryAverage;
	}

	// Minors attacked
	iConquestPriority += GetPlayer()->GetDiplomacyAI()->GetOtherPlayerNumMinorsAttacked(ePlayer) * /*5*/ GD_INT_GET(AI_GRAND_STRATEGY_CONQUEST_WEIGHT_PER_MINOR_ATTACKED);

	// Minors Conquered
	iConquestPriority += GetPlayer()->GetDiplomacyAI()->GetPlayerNumMinorsConquered(ePlayer) * /*10*/ GD_INT_GET(AI_GRAND_STRATEGY_CONQUEST_WEIGHT_PER_MINOR_CONQUERED);

	// Majors attacked
	iConquestPriority += GetPlayer()->GetDiplomacyAI()->GetOtherPlayerNumMajorsAttacked(ePlayer) * /*10*/ GD_INT_GET(AI_GRAND_STRATEGY_CONQUEST_WEIGHT_PER_MAJOR_ATTACKED);

	// Majors Conquered
	iConquestPriority += GetPlayer()->GetDiplomacyAI()->GetPlayerNumMajorsConquered(ePlayer) * /*15*/ GD_INT_GET(AI_GRAND_STRATEGY_CONQUEST_WEIGHT_PER_MAJOR_CONQUERED);

	//Autocracy is usually a sure bet for conquest strategy.
	//More than half of all Capitals?
	if(GET_PLAYER(ePlayer).GetNumCapitalCities() >= 1 && (GET_PLAYER(ePlayer).GetNumCapitalCities() >= (GC.getGame().GetNumMajorCivsEver() / 2)))
	{
		iConquestPriority *= GET_PLAYER(ePlayer).GetNumCapitalCities() * 20;
	}
	if(GET_PLAYER(ePlayer).GetDiplomacyAI()->GetWarmongerThreat(ePlayer) >= THREAT_MAJOR)
	{
		iConquestPriority *= GET_PLAYER(ePlayer).GetDiplomacyAI()->GetWarmongerThreat(ePlayer) * 5;
	}
	PolicyBranchTypes eCurrentBranchType = GET_PLAYER(ePlayer).GetPlayerPolicies()->GetLateGamePolicyTree();

	if (eCurrentBranchType == (PolicyBranchTypes)GD_INT_GET(POLICY_BRANCH_AUTOCRACY) || eCurrentBranchType == (PolicyBranchTypes)GD_INT_GET(POLICY_BRANCH_ORDER))
	{
		iConquestPriority *= 3;
		iConquestPriority /= 2;
	}

	return max(iConquestPriority, 0);
}

/// Guess as to how much another Player is prioritizing Culture as his means of winning the game
int CvGrandStrategyAI::GetGuessOtherPlayerCulturePriority(PlayerTypes ePlayer, int iWorldCultureAverage, int iWorldTourismAverage)
{
	VictoryTypes eVictory = (VictoryTypes) GC.getInfoTypeForString("VICTORY_CULTURAL", true);

	// If Culture Victory isn't even available then don't bother with anything
	if (eVictory == NO_VICTORY)
		return 0;

	int iCulturePriority = 0;
	int iRatio = 0;

	// Compare their Culture to the world average; Possible range is 75 to -75
	if(iWorldCultureAverage > 0)
	{
		iRatio = ((int)(GET_PLAYER(ePlayer).GetJONSCultureEverGeneratedTimes100() / 100) - iWorldCultureAverage) * /*60*/ GD_INT_GET(AI_GS_CULTURE_RATIO_MULTIPLIER) / iWorldCultureAverage;
		if (iRatio > GD_INT_GET(AI_GS_CULTURE_RATIO_MULTIPLIER))
		{
			iCulturePriority += GD_INT_GET(AI_GS_CULTURE_RATIO_MULTIPLIER);
		}
		else if (iRatio < -GD_INT_GET(AI_GS_CULTURE_RATIO_MULTIPLIER))
		{
			iCulturePriority += -GD_INT_GET(AI_GS_CULTURE_RATIO_MULTIPLIER);
		}
		iCulturePriority += iRatio;
	}

	// Compare their Tourism to the world average; Possible range is 75 to -75
	if(iWorldTourismAverage > 0)
	{
		iRatio = (GET_PLAYER(ePlayer).GetCulture()->GetTourism() / 100 - iWorldTourismAverage) * /*80*/ GD_INT_GET(AI_GS_TOURISM_RATIO_MULTIPLIER) / iWorldTourismAverage;
		if (iRatio > GD_INT_GET(AI_GS_TOURISM_RATIO_MULTIPLIER))
		{
			iCulturePriority += GD_INT_GET(AI_GS_TOURISM_RATIO_MULTIPLIER);
		}
		else if (iRatio < -GD_INT_GET(AI_GS_TOURISM_RATIO_MULTIPLIER))
		{
			iCulturePriority += -GD_INT_GET(AI_GS_TOURISM_RATIO_MULTIPLIER);
		}
		iCulturePriority += iRatio;	
	}

	//Influential on a lot of civs?
	if(GET_PLAYER(ePlayer).GetCulture()->GetNumCivsInfluentialOn() >= (GC.getGame().GetNumMajorCivsEver() / 2))
	{
		iCulturePriority *= (GET_PLAYER(ePlayer).GetCulture()->GetNumCivsInfluentialOn() * 5);
	}

	//Freedom is usually a sure bet for culture strategy.
	PolicyBranchTypes eCurrentBranchType = GET_PLAYER(ePlayer).GetPlayerPolicies()->GetLateGamePolicyTree();

	if (eCurrentBranchType == (PolicyBranchTypes)GD_INT_GET(POLICY_BRANCH_FREEDOM) || eCurrentBranchType == (PolicyBranchTypes)GD_INT_GET(POLICY_BRANCH_AUTOCRACY))
	{
		iCulturePriority *= 3;
		iCulturePriority /= 2;
	}

	return max(iCulturePriority, 0);
}

/// Guess as to how much another Player is prioritizing the UN as his means of winning the game
int CvGrandStrategyAI::GetGuessOtherPlayerUnitedNationsPriority(PlayerTypes ePlayer)
{
	VictoryTypes eVictory = (VictoryTypes) GC.getInfoTypeForString("VICTORY_DIPLOMATIC", true);

	// If UN Victory isn't even available then don't bother with anything
	if (eVictory == NO_VICTORY)
		return 0;

	int iTheirCityStateAllies = 0;
	int iTheirCityStateFriends = 0;
	int iCityStatesAlive = 0;
	for(int iPlayerLoop = 0; iPlayerLoop < MAX_CIV_PLAYERS; iPlayerLoop++)
	{
		PlayerTypes eLoopPlayer = (PlayerTypes) iPlayerLoop;

		if (eLoopPlayer != ePlayer && GET_PLAYER(eLoopPlayer).isAlive() && GET_PLAYER(eLoopPlayer).isMinorCiv())
		{
			iCityStatesAlive++;
			if (GET_PLAYER(eLoopPlayer).GetMinorCivAI()->IsAllies(ePlayer))
			{
				iTheirCityStateAllies++;
			}
			else if (GET_PLAYER(eLoopPlayer).GetMinorCivAI()->IsFriends(ePlayer))
			{
				iTheirCityStateFriends++;
			}
		}
	}
	iCityStatesAlive = MAX(iCityStatesAlive, 1);

	int iPriority = (iTheirCityStateAllies + (iTheirCityStateFriends / 3)) * /*300*/ GD_INT_GET(AI_GS_UN_SECURED_VOTE_MOD) / iCityStatesAlive;

	//Freedom is usually a sure bet for diplomatic strategy.
	PolicyBranchTypes eCurrentBranchType = GET_PLAYER(ePlayer).GetPlayerPolicies()->GetLateGamePolicyTree();

	if (eCurrentBranchType == (PolicyBranchTypes)GD_INT_GET(POLICY_BRANCH_FREEDOM) || eCurrentBranchType == (PolicyBranchTypes)GD_INT_GET(POLICY_BRANCH_AUTOCRACY))
	{
		iPriority *= 3;
		iPriority /= 2;
	}

	//Lots of votes? That's not good.
	if(GC.getGame().GetGameLeagues()->GetActiveLeague() != NULL)
	{
		if(GC.getGame().GetGameLeagues()->GetActiveLeague()->CalculateStartingVotesForMember(ePlayer) > GC.getGame().GetVotesNeededForDiploVictory())
		{
			iPriority *= (GC.getGame().GetGameLeagues()->GetActiveLeague()->CalculateStartingVotesForMember(ePlayer) / 4);
		}
	}

	return max(iPriority, 0);
}

/// Guess as to how much another Player is prioritizing the SS as his means of winning the game
int CvGrandStrategyAI::GetGuessOtherPlayerSpaceshipPriority(PlayerTypes ePlayer, int iWorldNumTechsAverage)
{
	VictoryTypes eVictory = (VictoryTypes) GC.getInfoTypeForString("VICTORY_SPACE_RACE", true);

	// If SS Victory isn't even available then don't bother with anything
	if (eVictory == NO_VICTORY)
		return 0;

	TeamTypes eTeam = GET_PLAYER(ePlayer).getTeam();

	// If the player has the Apollo Program we're pretty sure he's going for the SS
	ProjectTypes eApolloProgram = (ProjectTypes) GC.getInfoTypeForString("PROJECT_APOLLO_PROGRAM", true);
	if(eApolloProgram != NO_PROJECT)
	{
		if(GET_TEAM(eTeam).getProjectCount(eApolloProgram) > 0)
		{
			return /*150*/ GD_INT_GET(AI_GS_SS_HAS_APOLLO_PROGRAM);
		}
	}

	int iNumTechs = GET_TEAM(eTeam).GetTeamTechs()->GetNumTechsKnown();
	int iSSPriority = (iNumTechs - iWorldNumTechsAverage) * /*600*/ GD_INT_GET(AI_GS_SS_TECH_PROGRESS_MOD) / max(iWorldNumTechsAverage, 1);

	PolicyBranchTypes eCurrentBranchType = GET_PLAYER(ePlayer).GetPlayerPolicies()->GetLateGamePolicyTree();
	if (eCurrentBranchType == (PolicyBranchTypes)GD_INT_GET(POLICY_BRANCH_ORDER) || eCurrentBranchType == (PolicyBranchTypes)GD_INT_GET(POLICY_BRANCH_FREEDOM))
	{
		iSSPriority *= 3;
		iSSPriority /= 2;
	}

	return max(iSSPriority, 0);
}


// PRIVATE METHODS

/// Log GrandStrategy state: what are the Priorities and who is Active?
void CvGrandStrategyAI::LogGrandStrategies(const vector<int>& vModifiedGrandStrategyPriorities)
{
	if(GC.getLogging() && GC.getAILogging())
	{
		CvString strOutBuf;
		CvString strBaseString;
		CvString strTemp;
		CvString playerName;
		CvString strDesc;
		CvString strLogName;

		// Find the name of this civ and city
		playerName = GetPlayer()->getCivilizationShortDescription();

		// Open the log file
		if(GC.getPlayerAndCityAILogSplit())
		{
			strLogName = "GrandStrategyAI_Log_" + playerName + ".csv";
		}
		else
		{
			strLogName = "GrandStrategyAI_Log.csv";
		}

		FILogFile* pLog = NULL;
		pLog = LOGFILEMGR.GetLog(strLogName, FILogFile::kDontTimeStamp);

		AIGrandStrategyTypes eGrandStrategy;

		// Loop through Grand Strategies
		for(int iGrandStrategyLoop = 0; iGrandStrategyLoop < GC.getNumAIGrandStrategyInfos(); iGrandStrategyLoop++)
		{
			// Get the leading info for this line
			strBaseString.Format("%03d, ", GC.getGame().getElapsedGameTurns());
			strBaseString += playerName + ", ";

			eGrandStrategy = (AIGrandStrategyTypes) iGrandStrategyLoop;

			// GrandStrategy Info
			CvAIGrandStrategyXMLEntry* pEntry = GC.getAIGrandStrategyInfo(eGrandStrategy);
			const char* szAIGrandStrategyType = (pEntry != NULL)? pEntry->GetType() : "Unknown Type";

			if(GetActiveGrandStrategy() == eGrandStrategy)
			{
				strTemp.Format("*** %s, %d, %d", szAIGrandStrategyType, GetGrandStrategyPriority(eGrandStrategy), vModifiedGrandStrategyPriorities[eGrandStrategy]);
			}
			else
			{
				strTemp.Format("%s, %d, %d", szAIGrandStrategyType, GetGrandStrategyPriority(eGrandStrategy), vModifiedGrandStrategyPriorities[eGrandStrategy]);
			}
			strOutBuf = strBaseString + strTemp;
			pLog->Msg(strOutBuf);
		}
	}
}

/// Log our guess as to other Players' Active Grand Strategy
void CvGrandStrategyAI::LogGuessOtherPlayerGrandStrategy(const vector<int>& vGrandStrategyPriorities, PlayerTypes ePlayer)
{
	if(GC.getLogging() && GC.getAILogging())
	{
		CvString strOutBuf;
		CvString strBaseString;
		CvString strTemp;
		CvString playerName;
		CvString otherPlayerName;
		CvString strDesc;
		CvString strLogName;

		// Find the name of this civ and city
		playerName = GetPlayer()->getCivilizationShortDescription();

		// Open the log file
		if(GC.getPlayerAndCityAILogSplit())
		{
			strLogName = "GrandStrategyAI_Guess_Log_" + playerName + ".csv";
		}
		else
		{
			strLogName = "GrandStrategyAI_Guess_Log.csv";
		}

		FILogFile* pLog = NULL;
		pLog = LOGFILEMGR.GetLog(strLogName, FILogFile::kDontTimeStamp);

		AIGrandStrategyTypes eGrandStrategy;
		int iPriority = 0;

		// Loop through Grand Strategies
		for(int iGrandStrategyLoop = 0; iGrandStrategyLoop < GC.getNumAIGrandStrategyInfos(); iGrandStrategyLoop++)
		{
			// Get the leading info for this line
			strBaseString.Format("%03d, ", GC.getGame().getElapsedGameTurns());
			strBaseString += playerName + ", ";

			eGrandStrategy = (AIGrandStrategyTypes) iGrandStrategyLoop;
			iPriority = vGrandStrategyPriorities[iGrandStrategyLoop];

			CvAIGrandStrategyXMLEntry* pEntry = GC.getAIGrandStrategyInfo(eGrandStrategy);
			const char* szGrandStrategyType = (pEntry != NULL)? pEntry->GetType() : "Unknown Strategy";

			// GrandStrategy Info
			if(GetActiveGrandStrategy() == eGrandStrategy)
			{
				strTemp.Format("*** %s, %d", szGrandStrategyType, iPriority);
			}
			else
			{
				strTemp.Format("%s, %d", szGrandStrategyType, iPriority);
			}
			otherPlayerName = GET_PLAYER(ePlayer).getCivilizationShortDescription();
			strOutBuf = strBaseString + otherPlayerName + ", " + strTemp;

			if(GetGuessOtherPlayerActiveGrandStrategy(ePlayer) == eGrandStrategy)
			{
				// Confidence in our guess
				switch(GetGuessOtherPlayerActiveGrandStrategyConfidence(ePlayer))
				{
				case GUESS_CONFIDENCE_POSITIVE:
					strTemp.Format("Positive");
					break;
				case GUESS_CONFIDENCE_LIKELY:
					strTemp.Format("Likely");
					break;
				case GUESS_CONFIDENCE_UNSURE:
					strTemp.Format("Unsure");
					break;
				}

				strOutBuf += ", " + strTemp;
			}

			pLog->Msg(strOutBuf);
		}

		// One more entry for NO GRAND STRATEGY
		// Get the leading info for this line
		strBaseString.Format("%03d, ", GC.getGame().getElapsedGameTurns());
		strBaseString += playerName + ", ";

		iPriority = vGrandStrategyPriorities[GC.getNumAIGrandStrategyInfos()];

		// GrandStrategy Info
		strTemp.Format("NO_GRAND_STRATEGY, %d", iPriority);
		otherPlayerName = GET_PLAYER(ePlayer).getCivilizationShortDescription();
		strOutBuf = strBaseString + otherPlayerName + ", " + strTemp;
		pLog->Msg(strOutBuf);
	}
}