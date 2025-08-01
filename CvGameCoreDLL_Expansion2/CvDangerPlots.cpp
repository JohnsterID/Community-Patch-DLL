/*	-------------------------------------------------------------------------------------------------------
	© 1991-2012 Take-Two Interactive Software and its subsidiaries.  Developed by Firaxis Games.  
	Sid Meier's Civilization V, Civ, Civilization, 2K Games, Firaxis Games, Take-Two Interactive Software 
	and their respective logos are all trademarks of Take-Two interactive Software, Inc.  
	All other marks and trademarks are the property of their respective owners.  
	All rights reserved. 
	------------------------------------------------------------------------------------------------------- */
#include "CvGameCoreDLLPCH.h"
#include "CvDangerPlots.h"
#include "CvGameCoreUtils.h"
#include "CvAStar.h"
#include "CvEnumSerialization.h"
#include "CvDiplomacyAI.h"
#include "CvMilitaryAI.h"
#include "CvMinorCivAI.h"

// must be included after all other headers
#include "LintFree.h"
#ifdef _MSC_VER
#pragma warning ( disable : 4505 ) // unreferenced local function has been removed.. needed by REMARK below
#endif//_MSC_VER

//this adds up quickly if there multiple invisible tiles around ...
#define FOG_DEFAULT_DANGER (1)

/// Constructor
CvDangerPlots::CvDangerPlots(void)
	: m_ePlayer(NO_PLAYER)
	, m_bDirty(false)
	, m_iTurnBuilt(0)
	, m_DangerPlots()
{
}

/// Destructor
CvDangerPlots::~CvDangerPlots(void)
{
	Uninit();
}

/// Initialize
void CvDangerPlots::Init(PlayerTypes ePlayer)
{
	Uninit();
	m_ePlayer = ePlayer;
	//cannot allocate anything yet because the map might not exist yet
}

/// Uninitialize
void CvDangerPlots::Uninit()
{
	m_ePlayer = NO_PLAYER;
	m_bDirty = false;
	m_iTurnBuilt = 0;
	m_DangerPlots.clear();
	m_knownUnits.clear();
	m_vanishedUnits.clear();
}

bool CvDangerPlots::UpdateDangerSingleUnit(const CvUnit* pLoopUnit, bool bIgnoreVisibility, const PlotIndexContainer& plotsToIgnoreForZOC)
{
	if(ShouldIgnoreUnit(pLoopUnit, bIgnoreVisibility))
		return false;

	//the IGNORE_DANGER flag is extremely important here, otherwise we can get into endless loops
	//(when the pathfinder does a lazy danger update)
	int iFlags = CvUnit::MOVEFLAG_IGNORE_STACKING_SELF | CvUnit::MOVEFLAG_IGNORE_STACKING_NEUTRAL | CvUnit::MOVEFLAG_IGNORE_ENEMIES | CvUnit::MOVEFLAG_IGNORE_DANGER;
	if (MOD_CORE_TWO_PASS_DANGER)
		iFlags |= CvUnit::MOVEFLAG_SELECTIVE_ZOC;
	else
		iFlags |= CvUnit::MOVEFLAG_IGNORE_ZOC;

	ReachablePlots reachablePlots = TacticalAIHelpers::GetAllPlotsInReachThisTurn(pLoopUnit,pLoopUnit->plot(),iFlags,0,pLoopUnit->maxMoves(),plotsToIgnoreForZOC);

	if (pLoopUnit->IsCanAttackRanged())
	{
		//for ranged every tile we can enter with movement left is a base for attack
		std::set<int> attackableTiles = TacticalAIHelpers::GetPlotsUnderRangedAttackFrom(pLoopUnit,reachablePlots,false,false);

		for (std::set<int>::iterator attackTile=attackableTiles.begin(); attackTile!=attackableTiles.end(); ++attackTile)
		{
			CvPlot* pAttackTile = GC.getMap().plotByIndexUnchecked(*attackTile);
			AssignUnitDangerValue(pLoopUnit, pAttackTile);
		}

		//ranged units can capture as well
		for (ReachablePlots::iterator moveTile = reachablePlots.begin(); moveTile != reachablePlots.end(); ++moveTile)
		{
			CvPlot* pMoveTile = GC.getMap().plotByIndexUnchecked(moveTile->iPlotIndex);
			if (pLoopUnit->isNativeDomain(pMoveTile))
				m_DangerPlots[moveTile->iPlotIndex].m_apCaptureUnits.push_back(make_pair(pLoopUnit->getOwner(), pLoopUnit->GetID()));
		}
	}
	else
	{
		//for melee every tile we can move into can be attacked
		for (ReachablePlots::iterator moveTile=reachablePlots.begin(); moveTile!=reachablePlots.end(); ++moveTile)
		{
			CvPlot* pMoveTile = GC.getMap().plotByIndexUnchecked(moveTile->iPlotIndex);
			if (pLoopUnit->isNativeDomain(pMoveTile))
			{
				AssignUnitDangerValue(pLoopUnit, pMoveTile);
				if (!pMoveTile->isEnemyCity(*pLoopUnit)) //only melee units can move into enemy cities
					m_DangerPlots[moveTile->iPlotIndex].m_apCaptureUnits.push_back(make_pair(pLoopUnit->getOwner(), pLoopUnit->GetID()));
			}
		}
	}

	return true;
}

/// Updates the danger plots values to reflect threats across the map
void CvDangerPlots::UpdateDanger()
{
	//we call this function in three situations
	// * save game loaded (need to reconstruct m_DangerPlots). do not change m_knownUnits / m_vanishedUnits --> called from ui thread!
	// * new turn (enemy units moved). update both both m_knownUnits / m_vanishedUnits
	// * dirty flag (war state change, new enemies). update m_knownUnits, keep m_vanishedUnits

	bool bReload = (m_iTurnBuilt == -1);
	bool bTurnChange = !bReload && (m_iTurnBuilt != GC.getGame().getGameTurn());
	bool bWarChange = !bReload && !bTurnChange;

	//do not update from the UI thread, might lead to desyncs!
	if (!bReload && !gDLL->IsGameCoreThread())
		return;

	//note: we do not do a dirty check here, that is done in GetDanger()

	//allocate on demand
	if (m_DangerPlots.empty())
	{
		int iGridSize = GC.getMap().numPlots();
		m_DangerPlots = vector<CvDangerPlotContents>(iGridSize);
		for (int i = 0; i < iGridSize; i++)
		{
			CvPlot* pPlot = GC.getMap().plotByIndexUnchecked(i);
			m_DangerPlots[i].m_pPlot = pPlot;
		}
	}

	//two pass danger is dangerous ... it might happen that a covering unit moves away, leaving other exposed
	if (MOD_CORE_TWO_PASS_DANGER)
	{
		CvPlayer& thisPlayer = GET_PLAYER(m_ePlayer);
		PlotIndexContainer plotsWithOwnedUnitsLikelyToBeKilled;

		//first pass
		UpdateDangerInternal(plotsWithOwnedUnitsLikelyToBeKilled, bTurnChange, bWarChange);

		//find out which units might die and therefore won't have a ZOC
		int iLoop;
		for (CvUnit* pLoopUnit = thisPlayer.firstUnit(&iLoop); pLoopUnit != NULL; pLoopUnit = thisPlayer.nextUnit(&iLoop))
		{
			if (pLoopUnit->IsCombatUnit() && pLoopUnit->GetDanger() > pLoopUnit->GetCurrHitPoints())
				plotsWithOwnedUnitsLikelyToBeKilled.push_back(pLoopUnit->plot()->GetPlotIndex());
		}

		//second pass
		if (!plotsWithOwnedUnitsLikelyToBeKilled.empty())
			UpdateDangerInternal(plotsWithOwnedUnitsLikelyToBeKilled, false, true); //turn change already processed
	}
	else
	{
		PlotIndexContainer dummy;
		UpdateDangerInternal(dummy, bTurnChange, bWarChange);
	}
}

//fog of war is dangerous, but we don't know whether we will take the damage or not ...
void CvDangerPlots::AddFogDanger(CvPlot* pOrigin, TeamTypes eEnemyTeam, int iRange, bool bCheckOwnership)
{
	if (m_DangerPlots.empty()) //nothing to do
		return;

	iRange = range(iRange, 1, 5);
	CvPlayer& thisPlayer = GET_PLAYER(m_ePlayer);
	TeamTypes thisTeam = thisPlayer.getTeam();

	//if there are invisible plots next to this unit/city, other enemies might be hiding there
	for (int i = 0; i < RING_PLOTS[iRange]; i++)
	{
		CvPlot* pPotentialHiddedUnitPlot = iterateRingPlots(pOrigin, i);
		if (pPotentialHiddedUnitPlot && 
			!pPotentialHiddedUnitPlot->isVisible(thisTeam) && 
			!pPotentialHiddedUnitPlot->isImpassable(eEnemyTeam))
		{
			if (bCheckOwnership && pPotentialHiddedUnitPlot->getTeam() != eEnemyTeam)
				continue;

			//for simplicity assume each hidden unit can hit a target within range 2
			for (int j = 0; j < RING_PLOTS[2]; j++)
			{
				CvPlot* pAttackPlot = iterateRingPlots(pPotentialHiddedUnitPlot, j);
				if (pAttackPlot && pAttackPlot->getDomain() == pOrigin->getDomain())
					//todo: split between low-danger fog and high-danger fog depending on distance to closest enemy city 
					m_DangerPlots[pAttackPlot->GetPlotIndex()].m_iFogCount++;
			}
		}
	}
}

void CvDangerPlots::UpdateDangerInternal(const PlotIndexContainer& plotsToIgnoreForZOC, bool bTurnChange, bool bWarChange)
{
	// danger plots have not been initialized yet, so no need to update
	if (m_DangerPlots.empty())
		return;

	// wipe out values
	int iGridSize = GC.getMap().numPlots();
	for (int i = 0; i < iGridSize; i++)
		m_DangerPlots[i].reset();

	//it's fair to call this function with both turnChange and warChange false! this would be the reload case
	UnitSet knownUnitsPrevUpdate = m_knownUnits;
	if (bTurnChange)
		m_vanishedUnits.clear();
	if (bWarChange || bTurnChange)
		m_knownUnits.clear();

	// important. do this first to avoid recursion
	m_bDirty = false;
	m_iTurnBuilt = GC.getGame().getGameTurn();

	CvPlayer& thisPlayer = GET_PLAYER(m_ePlayer);
	TeamTypes thisTeam = thisPlayer.getTeam();
	// for each opposing civ
	for (int iPlayer = 0; iPlayer < MAX_PLAYERS; iPlayer++)
	{
		PlayerTypes ePlayer = (PlayerTypes)iPlayer;
		CvPlayer& loopPlayer = GET_PLAYER(ePlayer);
		TeamTypes eLoopTeam = loopPlayer.getTeam();

		if (!loopPlayer.isAlive())
			continue;

		if (eLoopTeam == thisTeam)
			continue;

		if (ShouldIgnorePlayer(ePlayer))
			continue;

		//for each unit
		int iLoop = 0;
		for (CvUnit* pLoopUnit = loopPlayer.firstUnit(&iLoop); pLoopUnit != NULL; pLoopUnit = loopPlayer.nextUnit(&iLoop))
		{
			if (UpdateDangerSingleUnit(pLoopUnit, false, plotsToIgnoreForZOC))
			{
				if (bTurnChange || bWarChange)
					m_knownUnits.insert(std::make_pair(pLoopUnit->getOwner(), pLoopUnit->GetID()));
				AddFogDanger(pLoopUnit->plot(), eLoopTeam, 2, false);
			}
		}

		// for each city
		for (CvCity* pLoopCity = loopPlayer.firstCity(&iLoop); pLoopCity != NULL; pLoopCity = loopPlayer.nextCity(&iLoop))
		{
			if (ShouldIgnoreCity(pLoopCity, false))
				continue;

			bool bIndirectFireAllowed = false; //this is an OUT parameter ...
			int iRange = pLoopCity->getBombardRange(bIndirectFireAllowed);
			CvPlot* pCityPlot = pLoopCity->plot();
			for (int i = RING0_PLOTS; i < RING_PLOTS[iRange]; i++)
			{
				CvPlot* pLoopPlot = iterateRingPlots(pCityPlot, i);
				if (pLoopPlot)
				{

					if (!bIndirectFireAllowed && !pCityPlot->canSeePlot(pLoopPlot, NO_TEAM, iRange, NO_DIRECTION))
						continue;
					AssignCityDangerValue(pLoopCity, pLoopPlot);
				}
			}

			AddFogDanger(pLoopCity->plot(), eLoopTeam, 3, true);
		}
	}

	// now compare the new known units with the previous known units
	if (bTurnChange)
	{
		for (UnitSet::iterator it = knownUnitsPrevUpdate.begin(); it != knownUnitsPrevUpdate.end(); ++it)
		{
			//might have made peace ...
			if (ShouldIgnorePlayer(it->first))
				continue;

			if (m_knownUnits.find(*it) == m_knownUnits.end())
			{
				//it's still there, but moved out of sight - nevertheless count it, a human would do that as well
				CvUnit* pVanishedUnit = GET_PLAYER(it->first).getUnit(it->second);

				//do not add it to the known units though, so next turn we will have forgotten about it
				if (pVanishedUnit)
				{
					if (UpdateDangerSingleUnit(pVanishedUnit, true, plotsToIgnoreForZOC))
						m_vanishedUnits.insert(*it);
				}
			}
		}
	}

	for(int iPlotLoop = 0; iPlotLoop < GC.getMap().numPlots(); iPlotLoop++)
	{
		CvPlot* pPlot = GC.getMap().plotByIndexUnchecked(iPlotLoop);

		if(pPlot->isRevealed(thisTeam))
		{
			//remember the plot based damage, but it depends on the unit's promotions also, so we won't apply it directly
			int iPlotDamage = 0;
			if (pPlot->getFeatureType() != NO_FEATURE)
				iPlotDamage += (GC.getFeatureInfo(pPlot->getFeatureType())->getTurnDamage());
			if (pPlot->getTerrainType() != NO_TERRAIN)
				iPlotDamage += (GC.getTerrainInfo(pPlot->getTerrainType())->getTurnDamage());

			if (pPlot->isWater())
			{
				CvCity* pOwner = pPlot->getEffectiveOwningCity();
				if (pOwner != NULL)
				{
					int iTempDamage = pPlot->getEffectiveOwningCity()->GetDeepWaterTileDamage();
					if (iTempDamage > 0)
					{
						//only affected BY adjacent plots
						for (int iI = 0; iI < NUM_DIRECTION_TYPES; iI++)
						{
							CvPlot* pAdjacentPlot = plotDirection(pPlot->getX(), pPlot->getY(), ((DirectionTypes)iI));

							if (pAdjacentPlot != NULL && pAdjacentPlot->isWater() && pAdjacentPlot->getEffectiveOwningCity() == pOwner && pAdjacentPlot->isBeingWorked())
							{
								iPlotDamage += iTempDamage;
								break;
							}
						}
					}
				}
			}

			//will we take damage here for sure?
			m_DangerPlots[iPlotLoop].m_bFlatPlotDamage = (iPlotDamage>0);

			ImprovementTypes eImprovement = pPlot->getRevealedImprovementType(thisTeam);
			if(eImprovement != NO_IMPROVEMENT)
			{
				int iDamage = GC.getImprovementInfo(eImprovement)->GetNearbyEnemyDamage();
				if (iDamage > /*10 in CP, 5 in VP*/ GD_INT_GET(ENEMY_HEAL_RATE) && !ShouldIgnoreCitadel(pPlot, false))
				{
					//citadel only affects adjacent plots, not this plot
					for(int iI = 0; iI < NUM_DIRECTION_TYPES; iI++)
					{
						CvPlot* pAdjacentPlot = plotDirection(pPlot->getX(), pPlot->getY(), ((DirectionTypes)iI));

						if(pAdjacentPlot != NULL)
							//unlike the rest, citadel damage applies with 100% certainty, so add twice the amount to drive the point home
							m_DangerPlots[pAdjacentPlot->GetPlotIndex()].m_iImprovementDamage += iDamage*2;
					}
				}

				//if we know there's a camp with units there but we can't see it, assume some danger
				ImprovementTypes eCamp = (ImprovementTypes)GD_INT_GET(BARBARIAN_CAMP_IMPROVEMENT);
				if (eImprovement == eCamp && !pPlot->isVisible(thisTeam))
					AddFogDanger(pPlot, BARBARIAN_TEAM, 3, false);
			}
		}
	}
}

/// Return the maximum amount of damage that could be dealt to a non-specific unit at this plot
int CvDangerPlots::GetDanger(const CvPlot& Plot, bool bFixedDamageOnly)
{
	if(m_DangerPlots.empty())
		return 0;

	return m_DangerPlots[Plot.GetPlotIndex()].GetDanger(bFixedDamageOnly);
}

/// Return the maximum amount of damage a city could take at this plot
int CvDangerPlots::GetDanger(const CvCity* pCity, const CvUnit* pPretendGarrison)
{
	if(m_DangerPlots.empty() || !pCity)
		return 0;

	return m_DangerPlots[pCity->plot()->GetPlotIndex()].GetDanger(pCity, pPretendGarrison);
}

/// Return the maximum amount of damage a unit could take at this plot
int CvDangerPlots::GetDanger(const CvPlot& Plot, const CvUnit* pUnit, const UnitIdContainer& unitsToIgnore, int iExtraDamage, AirActionType iAirAction)
{
	if(m_DangerPlots.empty() || !pUnit)
		return 0;

	return m_DangerPlots[Plot.GetPlotIndex()].GetDanger(pUnit, unitsToIgnore, iExtraDamage, iAirAction);
}

std::vector<CvUnit*> CvDangerPlots::GetPossibleAttackers(const CvPlot& Plot, TeamTypes eTeamForVisibilityCheck) const
{
	if(m_DangerPlots.empty())
		return std::vector<CvUnit*>();

	return m_DangerPlots[Plot.GetPlotIndex()].GetPossibleAttackers(eTeamForVisibilityCheck);
}

bool CvDangerPlots::IsKnownAttacker(const CvUnit* pUnit) const
{
	if (m_DangerPlots.empty()  || !pUnit || !pUnit->IsCanAttack())
		return false;

	return m_knownUnits.find(std::make_pair(pUnit->getOwner(), pUnit->GetID())) != m_knownUnits.end();
}

bool CvDangerPlots::AddKnownAttacker(const CvUnit* pUnit)
{
	if (m_DangerPlots.empty()  || !pUnit || !pUnit->IsCanAttack())
		return false;

	if (IsKnownAttacker(pUnit))
		return false;

	UpdateDangerSingleUnit(pUnit, false, PlotIndexContainer()); //for simplicity, assume no ZOC by owned units
	m_knownUnits.insert(std::make_pair(pUnit->getOwner(), pUnit->GetID()));
	return true;
}

/// Should this player be ignored when creating the danger plots?
bool CvDangerPlots::ShouldIgnorePlayer(PlayerTypes ePlayer)
{
	//if one is major and the other a minor (but no barbarian)
	if(GET_PLAYER(m_ePlayer).isMinorCiv() != GET_PLAYER(ePlayer).isMinorCiv() && !GET_PLAYER(ePlayer).isBarbarian() && !GET_PLAYER(m_ePlayer).isBarbarian())
	{
		CvPlayer* pMinor = NULL;
		CvPlayer* pMajor = NULL;

		if(GET_PLAYER(m_ePlayer).isMinorCiv())
		{
			pMinor = &GET_PLAYER(m_ePlayer);
			pMajor = &GET_PLAYER(ePlayer);
		}
		else
		{
			pMinor = &GET_PLAYER(ePlayer);
			pMajor = &GET_PLAYER(m_ePlayer);
		}

		if(pMinor->GetMinorCivAI()->IsFriends(pMajor->GetID()))
		{
			return true;
		}

		// if we're a major, we should ignore minors that are not at war with us
		if (!GET_PLAYER(m_ePlayer).isMinorCiv())
		{
			TeamTypes eMajorTeam = pMajor->getTeam();
			TeamTypes eMinorTeam = pMinor->getTeam();
			if (!GET_TEAM(eMajorTeam).isAtWar(eMinorTeam))
			{
				return true;
			}
		}
	}

	//ignore if at peace
	if ( !GET_TEAM(GET_PLAYER(m_ePlayer).getTeam()).isAtWar(GET_PLAYER(ePlayer).getTeam()) )
		 return true;

	return false;
}

/// Should this unit be ignored when creating the danger plots?
bool CvDangerPlots::ShouldIgnoreUnit(const CvUnit* pUnit, bool bIgnoreVisibility)
{
	//watch out: if this is called for a half-initialized unit, the pointer may be valid but the plot invalid
	if(m_DangerPlots.empty() || m_ePlayer==NO_PLAYER || !pUnit || !pUnit->plot())
		return true;

	if(!pUnit->IsCanAttack())
		return true;

	if (pUnit->isInvisible(GET_PLAYER(m_ePlayer).getTeam(), false))
		return true;

	//invisible but revealed camp/city. count the unit there anyways (for AI), humans would guess so
	if (!GET_PLAYER(m_ePlayer).isHuman())
	{
		bIgnoreVisibility |= pUnit->plot()->isCity();
		bIgnoreVisibility |= pUnit->plot()->getRevealedImprovementType(pUnit->getTeam()) == GD_INT_GET(BARBARIAN_CAMP_IMPROVEMENT);
	}

	if(!pUnit->plot()->isVisible(GET_PLAYER(m_ePlayer).getTeam()) && !bIgnoreVisibility)
		return true;

	return false;
}

/// Should this city be ignored when creating the danger plots?
bool CvDangerPlots::ShouldIgnoreCity(const CvCity* pCity, bool bIgnoreVisibility)
{
	//ignore nonexistent cities
	if(!pCity)
		return true;

	//never ignore then
	if (bIgnoreVisibility)
		return false;

	//ignore if neither the city plot nor an adjacent plot is revealed
	TeamTypes eTeam = GET_PLAYER(m_ePlayer).getTeam();
	return !pCity->isRevealed(eTeam, false, true);
}

/// Should this city be ignored when creating the danger plots?
bool CvDangerPlots::ShouldIgnoreCitadel(CvPlot* pCitadelPlot, bool bIgnoreVisibility)
{
	//ignore nonexistent citadels
	if(!pCitadelPlot)
		return true;

	//never ignore then
	if (bIgnoreVisibility)
		return false;

	// ignore unseen citadels
	if(!pCitadelPlot->isRevealed(GET_PLAYER(m_ePlayer).getTeam()))
		return true;

	// cannot be pillaged
	if (pCitadelPlot->IsImprovementPillaged())
		return true;

	// our own citadels aren't dangerous
	PlayerTypes eOwner = pCitadelPlot->getOwner();
	if(eOwner == m_ePlayer)
		return true;

	// must be at war
	if(eOwner != NO_PLAYER && !atWar(GET_PLAYER(m_ePlayer).getTeam(), GET_PLAYER(eOwner).getTeam()))
		return true;

	return false;
}

//	-----------------------------------------------------------------------------------------------
/// Contains the calculations to do the danger value for the plot according to the unit
void CvDangerPlots::AssignUnitDangerValue(const CvUnit* pUnit, CvPlot* pPlot)
{
	if (m_DangerPlots.empty() || !pUnit || !pPlot)
		return;

	DangerUnitVector& v = m_DangerPlots[pPlot->GetPlotIndex()].m_apUnits;
	DangerUnitVector::value_type element(pUnit->getOwner(), pUnit->GetID());

	//it may happen that we try to add an attacker that is already noted
	//the IsKnownAttacker() does not contain the units we didn't want to remember for next turn
	//because technically they are invisible, AI only tracks them because they were visible the turn before
	//but now we rediscovered the attacker. so, do a double check:
	if (std::find(v.begin(),v.end(),element) == v.end())
		v.push_back(element);
}

//	-----------------------------------------------------------------------------------------------
/// Contains the calculations to do the danger value for the plot according to the city
void CvDangerPlots::AssignCityDangerValue(const CvCity* pCity, CvPlot* pPlot)
{
	if (m_DangerPlots.empty() || !pCity || !pPlot)
		return;

	m_DangerPlots[pPlot->GetPlotIndex()].m_apCities.push_back( std::make_pair(pCity->getOwner(),pCity->GetID()) );
}

///
template<typename DangerPlots, typename Visitor>
void CvDangerPlots::Serialize(DangerPlots& dangerPlots, Visitor& visitor)
{
	visitor(dangerPlots.m_ePlayer);
	visitor(dangerPlots.m_knownUnits);
	visitor(dangerPlots.m_vanishedUnits);
}

/// reads in danger plots info
void CvDangerPlots::Read(FDataStream& kStream)
{
	CvStreamLoadVisitor serialVisitor(kStream);
	CvDangerPlots::Serialize(*this, serialVisitor);

	//need to update, after all only the known units were serialized
	m_bDirty = true;
	//use this as a marker for the update mode
	m_iTurnBuilt = -1;
}

/// writes out danger plots info
void CvDangerPlots::Write(FDataStream& kStream) const
{
	CvStreamSaveVisitor serialVisitor(kStream);
	CvDangerPlots::Serialize(*this, serialVisitor);
}

FDataStream& operator>>(FDataStream& stream, CvDangerPlots& dangerPlots)
{
	dangerPlots.Read(stream);
	return stream;
}
FDataStream& operator<<(FDataStream& stream, const CvDangerPlots& dangerPlots)
{
	dangerPlots.Write(stream);
	return stream;
}

//	-----------------------------------------------------------------------------------------------
void CvDangerPlots::SetDirty()
{
	m_bDirty = true;
}

// Get the maximum damage a non-specified unit could receive at this plot in the next turn
int CvDangerPlotContents::GetDanger(bool bFixedDamageOnly)
{
	if (!m_pPlot)
		return 0;

	int iPlotDamage = m_iImprovementDamage;
	
	// Damage from terrain & features (eg fallout) - since we don't know the unit, just assume 20
	if (m_bFlatPlotDamage)
		iPlotDamage += 20;

	if (bFixedDamageOnly)
		return iPlotDamage;

	//now add the contribution from units, ignore cities & fog, this is just an estimation anyway
	for (DangerUnitVector::iterator it = m_apUnits.begin(); it < m_apUnits.end(); ++it)
	{
		CvUnit* pUnit = GET_PLAYER(it->first).getUnit(it->second);

		if ( !pUnit || pUnit->isDelayedDeath() || pUnit->IsDead())
		{
			continue;
		}

		int iUnusedReferenceVariable = 0;
		CvPlot* pAttackerPlot = NULL;
		if (pUnit->IsCanAttackRanged())
		{
			if (pUnit->getDomainType() == DOMAIN_AIR)
			{
				iPlotDamage += pUnit->GetAirCombatDamage(NULL, NULL, 0, iUnusedReferenceVariable, false, 0, m_pPlot, NULL, true);
			}
			else
			{
				iPlotDamage += pUnit->GetRangeCombatDamage(NULL, NULL, 0, iUnusedReferenceVariable, false, 0, m_pPlot, NULL, true, true);
			}
		}
		else
		{
			if (m_pPlot->isAdjacent(pUnit->plot()))
				pAttackerPlot = pUnit->plot();

			//we don't know the defender strength, so assume it's equal to attacker strength!
			int iSelfDamage = 0;
			iPlotDamage += pUnit->getMeleeCombatDamage(
				pUnit->GetMaxAttackStrength(pAttackerPlot, m_pPlot, NULL, true, true),
				pUnit->GetBaseCombatStrength()*100, 
				iSelfDamage,
				false, NULL);

			if (pUnit->isRangedSupportFire())
			{
				iPlotDamage += pUnit->GetRangeCombatDamage(NULL, NULL, 0, iUnusedReferenceVariable, false, 0, m_pPlot, pAttackerPlot, true, true);
			}
		}
	}
		
	return iPlotDamage;
}

int CvDangerPlotContents::GetAirUnitDamage(const CvUnit* pUnit, AirActionType iAirAction)
{
	if (pUnit->getDomainType() != DOMAIN_AIR || pUnit->isSuicide())
		return 0;
	
	if (iAirAction == AIR_ACTION_INTERCEPT) // Max damage from a potential air sweep against our intercept
	{
		int iBestAirSweepDamage = 0;
		int iCurrentAirSweepDamage = 0;
		for (DangerUnitVector::iterator it = m_apUnits.begin(); it < m_apUnits.end(); ++it)
		{
			CvUnit* pAttacker = GET_PLAYER(it->first).getUnit(it->second);

			if (!pAttacker || !pAttacker->canAirSweep() || pAttacker->isDelayedDeath() || pAttacker->IsDead())
				continue;

			int iAttackerStrength = pAttacker->GetMaxRangedCombatStrength(pUnit, /*pCity*/ NULL, true);
			iAttackerStrength *= (100 + pAttacker->GetAirSweepCombatModifier());
			iAttackerStrength /= 100;

			int iDefenderStrength = pUnit->GetMaxRangedCombatStrength(pUnit, /*pCity*/ NULL, false);
			int iReceiverDamage = 0;
			iCurrentAirSweepDamage = pUnit->getMeleeCombatDamage(iDefenderStrength, iAttackerStrength, iReceiverDamage,
				/*bIncludeRand*/ false, pAttacker);

			// It's a slower to have this in the unit loop instead of after the best damage has been calculated, but it's also more accurate
			if (iCurrentAirSweepDamage >= pAttacker->GetCurrHitPoints() && iReceiverDamage >= pUnit->GetCurrHitPoints())
			{
				if (iReceiverDamage + pUnit->getDamage() > iCurrentAirSweepDamage + pAttacker->getDamage())
				{
					iCurrentAirSweepDamage = pUnit->GetCurrHitPoints() - 1;
				}
			}
			if (iCurrentAirSweepDamage > iBestAirSweepDamage)
			{
				iBestAirSweepDamage = iCurrentAirSweepDamage;
			}
		}
		return iBestAirSweepDamage;
	}
	else
	{
		CvUnit* pInterceptor = m_pPlot->GetBestInterceptor(pUnit->getOwner(), pUnit);
		if (pInterceptor)
		{
			// Air sweeps take modified damage from interceptors
			if (iAirAction == AIR_ACTION_SWEEP)
			{
				if (pInterceptor->getDomainType() != DOMAIN_AIR)
				{
					return (pInterceptor->GetInterceptionDamage(pUnit, false, m_pPlot) * (100 + /*-50*/ GD_INT_GET(AIR_SWEEP_INTERCEPTION_DAMAGE_MOD)))/100;
				}
				else
				{
					int iAttackerStrength = pUnit->GetMaxRangedCombatStrength(pInterceptor, /*pCity*/ NULL, true);
					iAttackerStrength *= (100 + pUnit->GetAirSweepCombatModifier());
					iAttackerStrength /= 100;
					int iDefenderStrength = pInterceptor->GetMaxRangedCombatStrength(pUnit, /*pCity*/ NULL, false);
					int iDamageDealt = 0; // referemce variable
					int iReceiveDamage = pInterceptor->getMeleeCombatDamage(iDefenderStrength, iAttackerStrength, iDamageDealt,
						/*bIncludeRand*/ false, pUnit);
					if (iReceiveDamage >= pUnit->GetCurrHitPoints() && iDamageDealt >= pInterceptor->GetCurrHitPoints())
					{
						if (iDamageDealt + pInterceptor->getDamage() > iReceiveDamage + pUnit->getDamage())
						{
							iReceiveDamage = pUnit->GetCurrHitPoints() - 1;
						}
					}
					return iReceiveDamage;
				}
			}
			else
			{
				// Always assume interception is successful
				return pInterceptor->GetInterceptionDamage(pUnit, false);
			}
		}
	}

	return 0;
}

// Get the maximum damage unit could receive at this plot in the next turn (update this with CvUnitCombat changes!)
int CvDangerPlotContents::GetDanger(const CvUnit* pUnit, const UnitIdContainer& unitsToIgnore, int iExtraDamage, AirActionType iAirAction)
{
	if (!m_pPlot || !pUnit)
		return 0;

	// Air units only take damage from interceptions
	if (pUnit->getDomainType() == DOMAIN_AIR)
		return GetAirUnitDamage(pUnit, iAirAction);

	//otherwise calculate from scratch
	int iPlotDamage = 0;

	CvCity* pFriendlyCity = m_pPlot->isFriendlyCity(*pUnit) ? m_pPlot->getPlotCity() : NULL;

	// Civilians can be captured
	if (pUnit->IsCivilianUnit())
	{
		// If plot contains an enemy unit, mark it as max danger
		if (m_pPlot->isEnemyUnit(pUnit->getOwner(),true,true))
			return MAX_INT;

		if (pFriendlyCity)
		{
			// Can't hide in a city forever
			if (pFriendlyCity->isInDangerOfFalling())
				return MAX_INT;
			else
				return 0;
		}

		//need to differentiate between plots that the enemy can move into and those merely under ranged attack
		for (DangerUnitVector::iterator it = m_apCaptureUnits.begin(); it < m_apCaptureUnits.end(); ++it)
		{
			CvUnit* pAttacker = GET_PLAYER(it->first).getUnit(it->second);
			if (pAttacker && !pAttacker->isDelayedDeath() && !pAttacker->IsDead())
				return MAX_INT;
		}

		//attacks are less important for civilians ...
		for (DangerUnitVector::iterator it = m_apUnits.begin(); it < m_apUnits.end(); ++it)
		{
			CvUnit* pAttacker = GET_PLAYER(it->first).getUnit(it->second);
			if ( pAttacker && !pAttacker->isDelayedDeath() && !pAttacker->IsDead() )
			{
				int iDummy = 0;
				int iDamage = TacticalAIHelpers::GetSimulatedDamageFromAttackOnUnit(pUnit, pAttacker, m_pPlot, pAttacker->plot(), iDummy, false, 0, true);
				if (!m_pPlot->IsKnownVisibleToTeam(pAttacker->getTeam()))
					iDamage = (iDamage * 80) / 100; //there's a chance they won't spot us
				iPlotDamage += iDamage;
			}
		}

		// Static damage
		iPlotDamage += m_iImprovementDamage;
		iPlotDamage += m_bFlatPlotDamage ? m_pPlot->getTurnDamage(pUnit->ignoreTerrainDamage(), pUnit->ignoreFeatureDamage(), pUnit->extraTerrainDamage(), pUnit->extraFeatureDamage()) : 0;

		// Potential damage from enemy units in fog 
		// Note: Visibility might have changed but tracking this is too expensive, this should be good enough
		iPlotDamage += m_iFogCount*FOG_DEFAULT_DANGER;

		// Damage from cities
		for (DangerCityVector::iterator it = m_apCities.begin(); it < m_apCities.end(); ++it)
		{
			CvCity* pCity = GET_PLAYER(it->first).getCity(it->second);
			if (!pCity || pCity->getTeam() == pUnit->getTeam())
				continue;

			iPlotDamage += pCity->rangeCombatDamage(pUnit, false, m_pPlot);
		}

		return iPlotDamage;
	}

	// Capturing a city with a garrisoned unit destroys the garrisoned unit
	if (pFriendlyCity)
	{
		int iCityDanger = GetDanger(pFriendlyCity, pUnit);
		if (iCityDanger + pFriendlyCity->getDamage() < pFriendlyCity->GetMaxHitPoints() + 50) //add a margin for error
		{
			if (pUnit->CanGarrison())
			{
				// Reconstruct the amount of damage the garrison would absorb for the city
				int iUnitShare = (iCityDanger*2*pUnit->GetMaxHitPoints()) / pFriendlyCity->GetMaxHitPoints();

				// Damage from features
				return iUnitShare + m_iImprovementDamage;
			}
			else
				return 0;
		}
		else
		{
			return MAX_INT;
		}
	}

	// Damage from units
	// EXTREMELY IMPORTANT THAT NO RNG IS USED FOR PREDICTION!
	// Otherwise a tooltip or similar can change the game state
	for (DangerUnitVector::iterator it = m_apUnits.begin(); it < m_apUnits.end(); ++it)
	{
		CvUnit* pAttacker = GET_PLAYER(it->first).getUnit(it->second);
		if (!pAttacker || pAttacker->isDelayedDeath() || pAttacker->IsDead())
			continue;

		//there should be only very few of these, if any
		if (std::find(unitsToIgnore.begin(),unitsToIgnore.end(),it->second) != unitsToIgnore.end())
			continue;

		//todo: if the attacker is an air unit and we have interceptors around, reduce the expected damage
		//but interceptions are hard to keep track of and bad for performance ...

		if (pAttacker->plot() != m_pPlot)
		{
			int iAttackerDamage = 0;
			int iEnemyRange = pAttacker->IsCanAttackRanged() ? pAttacker->GetRange() : 1;
			bool bOutOfRange = plotDistance(*m_pPlot, *pAttacker->plot()) > iEnemyRange;

			//if the attacker is not out of range, assume they need to move for the attack, so we don't know their plot
			//todo: consider whether the enemy units would block each other from attacking?
			int iDamage = TacticalAIHelpers::GetSimulatedDamageFromAttackOnUnit(pUnit, pAttacker, m_pPlot, bOutOfRange ? NULL : pAttacker->plot(), iAttackerDamage, false, iExtraDamage, true);

			if (!m_pPlot->IsKnownVisibleToTeam(pAttacker->getTeam()))
				iDamage = (iDamage * 80) / 100; //there's a chance they won't spot us

			if (iAttackerDamage >= pAttacker->GetCurrHitPoints() && !pAttacker->isSuicide())
				iDamage /= 2; //suicide attack with non suicide unit is unlikely

			iPlotDamage += iDamage;
		}
	}

	// Damage from cities
	for (DangerCityVector::iterator it = m_apCities.begin(); it < m_apCities.end(); ++it)
	{
		CvCity* pCity = GET_PLAYER(it->first).getCity(it->second);
		if (!pCity || pCity->getTeam() == pUnit->getTeam())
			continue;

		int iCityDamage = pCity->rangeCombatDamage(pUnit, false, m_pPlot, true);
		//if we cannot see a potential garrison, assume there is one ...
		if (!pCity->isVisible(pUnit->getTeam(), false))
			iCityDamage *= 2;

		iPlotDamage += iCityDamage;
	}

	// Potential damage from enemy units in fog 
	// Note: Visibility might have changed but tracking this is too expensive, this should be good enough
	iPlotDamage += m_iFogCount * FOG_DEFAULT_DANGER;

	// Damage from surrounding improvements (citadel) and the plot itself
	iPlotDamage += m_iImprovementDamage;
	iPlotDamage += m_bFlatPlotDamage ? m_pPlot->getTurnDamage(pUnit->ignoreTerrainDamage(), pUnit->ignoreFeatureDamage(), pUnit->extraTerrainDamage(), pUnit->extraFeatureDamage()) : 0;

	//done
	return iPlotDamage;
}

// Get the maximum damage city could receive this turn if it were in this plot
int CvDangerPlotContents::GetDanger(const CvCity* pCity, const CvUnit* pPretendGarrison)
{
	if (!m_pPlot || !pCity)
		return 0;

	int iPlotDamage = 0;
	CvPlot* pCityPlot = pCity->plot();
	const int iCityX = pCityPlot->getX();
	const int iCityY = pCityPlot->getY();

	CvCityGarrisonOverride guard(pCity,pPretendGarrison);

	//if we have a garrison, split the damage
	int iGarrisonMaxHP = pCity->GetGarrisonedUnit() ? pCity->GetGarrisonedUnit()->GetMaxHitPoints() : 0;
	int iGarrisonHPLeft = pCity->GetGarrisonedUnit() ? pCity->GetGarrisonedUnit()->GetCurrHitPoints() : 0;
	int iGarrisonDamage = 0; // called by reference

	for (DangerUnitVector::iterator it = m_apUnits.begin(); it < m_apUnits.end(); ++it)
	{
		CvUnit* pUnit = GET_PLAYER(it->first).getUnit(it->second);
		if (!pUnit || pUnit->isDelayedDeath() || pUnit->IsDead())
		{
			continue;
		}

		CvPlot* pAttackerPlot = NULL;
		if (pUnit->IsCanAttackRanged())
		{
			if (pUnit->getDomainType() == DOMAIN_AIR)
			{
				CvUnit* pInterceptor = m_pPlot->GetBestInterceptor(pUnit->getOwner(), pUnit);
				int iInterceptDamage = 0;
				if (pInterceptor)
				{
					// Always assume interception is successful
					iInterceptDamage = pInterceptor->GetInterceptionDamage(pUnit, false, m_pPlot);
				}
				iPlotDamage += pUnit->GetAirCombatDamage(NULL, pCity, (iGarrisonHPLeft <= 0 ? 0 : iGarrisonMaxHP), iGarrisonDamage, false, iInterceptDamage, m_pPlot);
				iGarrisonHPLeft -= iGarrisonDamage;
			}
			else
			{
				iPlotDamage += pUnit->GetRangeCombatDamage(NULL, pCity, (iGarrisonHPLeft <= 0 ? 0 : iGarrisonMaxHP), iGarrisonDamage, false, 0, m_pPlot);
				iGarrisonHPLeft -= iGarrisonDamage;
			}
		}
		else
		{
			if (plotDistance(iCityX, iCityY, pUnit->getX(), pUnit->getY()) == 1)
			{
				pAttackerPlot = pUnit->plot();
			}
			int iSelfDamageAttacker = 0;
			// check if the garrison is dead
			iPlotDamage += pUnit->getMeleeCombatDamageCity(pUnit->GetMaxAttackStrength(pAttackerPlot, pCityPlot, NULL, !pUnit->isNoCapture(), !pUnit->isNoCapture()),
				pCity, iSelfDamageAttacker, (iGarrisonHPLeft <= 0 ? 0 : iGarrisonMaxHP), iGarrisonDamage, false);
			iGarrisonHPLeft -= iGarrisonDamage;
		}
	}



	return iPlotDamage;
}

std::vector<CvUnit*> CvDangerPlotContents::GetPossibleAttackers(TeamTypes eTeamForVisibilityCheck) const
{
	//ignore cities
	std::vector<CvUnit*> vResult;
	for (DangerUnitVector::const_iterator it = m_apUnits.begin(); it < m_apUnits.end(); ++it)
	{
		CvUnit* pAttacker = GET_PLAYER(it->first).getUnit(it->second);
		if (pAttacker && !pAttacker->isDelayedDeath() && !pAttacker->IsDead())
		{
			if (eTeamForVisibilityCheck == NO_TEAM || pAttacker->plot()->isVisible(eTeamForVisibilityCheck))
				vResult.push_back(pAttacker);
		}
	}

	return vResult;
}
