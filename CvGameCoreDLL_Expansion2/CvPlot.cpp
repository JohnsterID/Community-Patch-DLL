﻿/*	-------------------------------------------------------------------------------------------------------
	© 1991-2012 Take-Two Interactive Software and its subsidiaries.  Developed by Firaxis Games.  
	Sid Meier's Civilization V, Civ, Civilization, 2K Games, Firaxis Games, Take-Two Interactive Software 
	and their respective logos are all trademarks of Take-Two interactive Software, Inc.  
	All other marks and trademarks are the property of their respective owners.  
	All rights reserved. 
	------------------------------------------------------------------------------------------------------- */

#include "CvGameCoreDLLPCH.h"
#include "CvPlot.h"
#include "CvCity.h"
#include "CvUnit.h"
#include "CvGlobals.h"
#include "CvArea.h"
#include "ICvDLLUserInterface.h"
#include "CvMap.h"
#include "CvPlayerAI.h"
#include "CvTeam.h"
#include "CvGameCoreUtils.h"
#include "CvRandom.h"
#include "CvInfos.h"
#include "CvImprovementClasses.h"
#include "CvAStar.h"
#include "CvEconomicAI.h"
#include "CvEnumSerialization.h"
#include "CvNotifications.h"
#include "CvMinorCivAI.h"
#include "CvUnitCombat.h"
#include "CvDLLUtilDefines.h"
#include "CvInfosSerializationHelper.h"
#include "CvBarbarians.h"
#include "CvDiplomacyAI.h"
#include "CvDllPlot.h"
#include "CvDllUnit.h"
#include "CvUnitMovement.h"
#include "CvTargeting.h"
#include "CvTypes.h"
// Include this after all other headers.
#include "LintFree.h"

// Public Functions...

//	--------------------------------------------------------------------------------
namespace FSerialization
{
void SyncPlots()
{
	if(GC.getGame().isNetworkMultiPlayer())
	{
		PlayerTypes authoritativePlayer = GC.getGame().getActivePlayer();
		CvMap& map = GC.getMap();
		for(int i = 0; i < map.numPlots(); ++i)
		{
			CvPlot* plot = map.plotByIndexUnchecked(i);
			CvSyncArchive<CvPlot>& archive = plot->getSyncArchive();
			archive.collectDeltas();
			if(archive.hasDeltas())
			{
				FMemoryStream memoryStream;
				std::vector<std::pair<std::string, std::string> > callStacks;
				archive.saveDelta(memoryStream, callStacks);
				gDLL->sendPlotSyncCheck(authoritativePlayer, plot->getX(), plot->getY(), memoryStream, callStacks);
			}
		}
	}
}

// clears ALL deltas for ALL plots
void ClearPlotDeltas()
{
	CvMap& map = GC.getMap();
	for (int i = 0; i < map.numPlots(); ++i)
	{
		CvPlot* plot = map.plotByIndexUnchecked(i);
		FAutoArchive& archive = plot->getSyncArchive();
		archive.clearDelta();
	}
}
}

//////////////////////////////////////////////////////////////////////////
// CvArchaeologyData serialization
//////////////////////////////////////////////////////////////////////////
template<typename ArchaeologyData, typename Visitor>
void CvArchaeologyData::Serialize(ArchaeologyData& archaeologyData, Visitor& visitor)
{
	visitor(archaeologyData.m_eArtifactType);
	visitor(archaeologyData.m_eEra);
	visitor(archaeologyData.m_ePlayer1);
	visitor(archaeologyData.m_ePlayer2);
	visitor(archaeologyData.m_eWork);
}

FDataStream& operator>>(FDataStream& loadFrom, CvArchaeologyData& writeTo)
{
	CvStreamLoadVisitor serialVisitor(loadFrom);
	CvArchaeologyData::Serialize(writeTo, serialVisitor);
	return loadFrom;
}

/// Serialization write
FDataStream& operator<<(FDataStream& saveTo, const CvArchaeologyData& readFrom)
{
	CvStreamSaveVisitor serialVisitor(saveTo);
	CvArchaeologyData::Serialize(readFrom, serialVisitor);
	return saveTo;
}

//////////////////////////////////////////////////////////////////////////
// CvPlot
//////////////////////////////////////////////////////////////////////////
CvPlot::CvPlot()
	: m_syncArchive()
	, m_szScriptData(NULL)
{
	if (GC.getGame().isNetworkMultiPlayer())
	{
		m_syncArchive.initSyncVars(*FNEW(CvSyncArchive<CvPlot>::SyncVars(*this), c_eCiv5GameplayDLL, 0));
	}
}


//	--------------------------------------------------------------------------------
CvPlot::~CvPlot()
{
	uninit();
}

//	--------------------------------------------------------------------------------
void CvPlot::init(int iX, int iY)
{
	//--------------------------------
	// Init non-saved data

	m_iX = iX;
	m_iY = iY;

	m_iPlotIndex = GC.getMap().plotNum(m_iX, m_iY);

	//--------------------------------
	// Init saved data
	reset();
}


//	--------------------------------------------------------------------------------
void CvPlot::uninit()
{
	SAFE_DELETE_ARRAY(m_szScriptData);

	m_pCenterUnit = NULL;

	m_units.clear();
}

//	--------------------------------------------------------------------------------
// FUNCTION: reset()
// Initializes data members that are serialized.
void CvPlot::reset()
{
	//--------------------------------
	// Uninit class
	uninit();

	m_iArea = -1;
	m_iLandmass = -1;
	m_iContinent = -1;
	m_iOwnershipDuration = 0;
	m_iImprovementDuration = 0;
	m_iUpgradeProgress = 0;
	m_iNumMajorCivsRevealed = 0;
	m_iCityRadiusCount = 0;
	m_iReconCount = 0;
	m_iRiverCrossingCount = 0;
	m_iResourceNum = 0;
	m_cContinentType = 0;

	m_bStartingPlot = false;
	m_bHills = false;
	m_bNEOfRiver = false;
	m_bWOfRiver = false;
	m_bNWOfRiver = false;
	m_bPotentialCityWork = false;
	m_bPlotLayoutDirty = false;
	m_bLayoutStateWorked = false;
	m_bImprovementPassable = false;
	m_bImprovementPillaged = false;
	m_bRoutePillaged = false;
	m_bBarbCampNotConverting = false;
	m_bRoughPlot = false;
	m_bResourceLinkedCityActive = false;
	m_bImprovedByGiftFromMajor = false;
	m_bIsImpassable = false;
	m_bImprovementEmbassy = false;

	m_bIsFreshwater = false;
	m_bIsAdjacentToLand = false;
	m_bIsAdjacentToWater = false;
	m_bIsLake = false;

	m_eOwner = NO_PLAYER;
	m_ePlotType = PLOT_OCEAN;
	m_eTerrainType = NO_TERRAIN;
	m_eFeatureType = NO_FEATURE;
	m_eResourceType = NO_RESOURCE;
	m_eImprovementType = NO_IMPROVEMENT;
	m_ePlayerBuiltImprovement = NO_PLAYER;
	m_ePlayerBuiltRoute = NO_PLAYER;
	m_ePlayerResponsibleForImprovement = NO_PLAYER;
	m_ePlayerResponsibleForRoute = NO_PLAYER;
	m_ePlayerThatClearedBarbCampHere = NO_PLAYER;
#if defined(MOD_BALANCE_CORE)
	m_iUnitPlotExperience = 0;
	m_iUnitPlotGAExperience = 0;
	m_iPlotChangeMoves = 0;
	m_iRestoreMoves = 0;
#endif
	m_eLandmarkCreditMinor = NO_PLAYER;
	m_ePlayerThatClearedDigHere = NO_PLAYER;
	m_ePlayerThatDestroyedCityHere = NO_PLAYER;
	m_eRouteType = NO_ROUTE;
#if defined(MOD_GLOBAL_STACKING_RULES)
	m_eUnitIncrement = 0;
#endif
	m_eWorldAnchor = NO_WORLD_ANCHOR;
	m_cWorldAnchorData = NO_WORLD_ANCHOR;
	m_eRiverEFlowDirection = NO_FLOWDIRECTION;
	m_eRiverSEFlowDirection = NO_FLOWDIRECTION;
	m_eRiverSWFlowDirection = NO_FLOWDIRECTION;
	m_cRiverCrossing = 0;

	m_bIsCity = false;
	m_owningCity.reset();
	m_owningCityOverride.reset();

	m_sSpawnedResourceX = -1;
	m_sSpawnedResourceY = -1;

	m_vExtraYields.clear();
	m_vRivers.clear();

	for (int iI = 0; iI < NUM_DIRECTION_TYPES; iI++)
	{
		m_vRivers.push_back(-1);
	}

	m_bfRevealed.ClearAll();

	for(int iI = 0; iI < NUM_YIELD_TYPES; ++iI)
	{
		m_aiYield[iI] = 0;
	}

	for(int iI = 0; iI < MAX_TEAMS; ++iI)
	{
		m_aiPlayerCityRadiusCount[iI] = 0;
		m_aiVisibilityCount[iI] = 0;
		m_aiKnownVisibilityCount[iI] = 0;
		m_aiRevealedOwner[iI] = -1;
		m_abResourceForceReveal[iI] = false;
		m_aeRevealedImprovementType[iI] = NO_IMPROVEMENT;
		m_aeRevealedRouteType[iI] = NO_ROUTE;
		m_aeHumanPlannedRouteState[iI] = NO_PLANNED_ROUTE;
#if defined(MOD_BALANCE_CORE)
		m_abIsImpassable[iI] = false;
		m_abStrategicRoute[iI] = false;
#endif
	}

	m_vInvisibleVisibilityUnitCount.clear();
	m_vInvisibleVisibilityCount.clear();

	m_kArchaeologyData.Reset();
	m_iNumTradeUnitRoute = 0;
	m_iLastTurnBuildChanged = 0;
}

//////////////////////////////////////
// graphical only setup
//////////////////////////////////////
void CvPlot::setupGraphical()
{
	if(!GC.IsGraphicsInitialized())
	{
		return;
	}

	updateSymbols();

	updateVisibility();
}

//	--------------------------------------------------------------------------------
void CvPlot::erase(bool bEraseUnits)
{
	FFastSmallFixedList<IDInfo, 25, true, c_eCiv5GameplayDLL > oldUnits;
	IDInfo* pUnitNode = NULL;
	CvCity* pCity = NULL;
	CvUnit* pLoopUnit = NULL;

	// kill units
	if (bEraseUnits)
	{
		oldUnits.clear();

		pUnitNode = headUnitNode();

		while (pUnitNode != NULL)
		{
			oldUnits.insertAtEnd(pUnitNode);
			pUnitNode = nextUnitNode(pUnitNode);
		}

		pUnitNode = oldUnits.head();

		while (pUnitNode != NULL)
		{
			pLoopUnit = GetPlayerUnit(*pUnitNode);
			pUnitNode = oldUnits.next(pUnitNode);

			if (pLoopUnit != NULL)
			{
				pLoopUnit->kill(false);
			}
		}
	}

	// kill cities
	pCity = getPlotCity();
	if (pCity != NULL)
	{
		pCity->kill();
	}

	setResourceType(NO_RESOURCE, 0);
	setImprovementType(NO_IMPROVEMENT);
	setRouteType(NO_ROUTE);
	setFeatureType(NO_FEATURE);

	// disable rivers
	setNEOfRiver(false, NO_FLOWDIRECTION);
	setNWOfRiver(false, NO_FLOWDIRECTION);
	setWOfRiver(false, NO_FLOWDIRECTION);
}

//	--------------------------------------------------------------------------------
void CvPlot::doTurn()
{
	if(isOwned())
	{
		changeOwnershipDuration(1);
	}

	if(getImprovementType() != NO_IMPROVEMENT)
	{
		changeImprovementDuration(1);
	}

	verifyUnitValidPlot();

	// Clear world anchor
	SetWorldAnchor(NO_WORLD_ANCHOR);

	// XXX
#ifdef _DEBUG
	{
		IDInfo* pUnitNode;
		CvUnit* pLoopUnit;

		pUnitNode = headUnitNode();

		while(pUnitNode != NULL)
		{
			pLoopUnit = GetPlayerUnit(*pUnitNode);
			pUnitNode = nextUnitNode(pUnitNode);

			if(pLoopUnit)
			{
				ASSERT_DEBUG(pLoopUnit->atPlot(*this), "pLoopUnit is expected to be at the current plot instance");
				if(!pLoopUnit->atPlot(*this))
				{
					removeUnit(pLoopUnit, true);
				}
			}
		}
	}
#endif
	// XXX
}


//	--------------------------------------------------------------------------------
void CvPlot::doImprovement()
{
	CvCity* pCity = NULL;
	CvString strBuffer;
	int iI = 0;
	int iResourceNum = 0;
	CvTeam& thisTeam = GET_TEAM(getTeam());

	ASSERT_DEBUG(isBeingWorked() && isOwned());

	if(m_eImprovementType != NO_IMPROVEMENT)
	{
		if(m_eResourceType == NO_RESOURCE)
		{
			ASSERT_DEBUG((0 < GC.getNumResourceInfos()), "GC.getNumResourceInfos() is not greater than zero but an array is being allocated in CvPlot::doImprovement");
			int iNumResourceInfos = GC.getNumResourceInfos();
			CvImprovementEntry* thisImprovementInfo = GC.getImprovementInfo((ImprovementTypes)m_eImprovementType);
			for(iI = 0; iI < iNumResourceInfos; ++iI)
			{
				CvResourceInfo* thisResourceInfo = GC.getResourceInfo((ResourceTypes) iI);
				if (thisResourceInfo)
				{
					if(thisTeam.IsResourceRevealed((ResourceTypes)iI))
					{
						if(thisImprovementInfo->GetImprovementResourceDiscoverRand(iI) > 0)
						{
							if (GC.getGame().randRangeExclusive(0, thisImprovementInfo->GetImprovementResourceDiscoverRand(iI), CvSeeder(GetPseudoRandomSeed()).mix(iI)) == 0)
							{
								iResourceNum = GC.getMap().getRandomResourceQuantity((ResourceTypes)iI);
								setResourceType((ResourceTypes)iI, iResourceNum);
								if(getOwner() == GC.getGame().getActivePlayer())
								{
									pCity = GC.getMap().findCity(getX(), getY(), getOwner(), NO_TEAM, false);
									if(pCity != NULL)
									{
										if(strcmp(thisResourceInfo->GetType(), "RESOURCE_ARTIFACTS") == 0)
										{
											strBuffer = GetLocalizedText("TXT_KEY_MISC_DISCOVERED_ARTIFACTS_NEAR", pCity->getNameKey());
										}
										else if(strcmp(thisResourceInfo->GetType(), "RESOURCE_HIDDEN_ARTIFACTS") == 0)
										{
											strBuffer = GetLocalizedText("TXT_KEY_MISC_DISCOVERED_HIDDEN_ARTIFACTS_NEAR", pCity->getNameKey());
										}
										else
										{
											strBuffer = GetLocalizedText("TXT_KEY_MISC_DISCOVERED_NEW_RESOURCE", thisResourceInfo->GetTextKey(), pCity->getNameKey());
										}

										GC.GetEngineUserInterface()->AddCityMessage(0, pCity->GetIDInfo(), getOwner(), false, /*10*/ GD_INT_GET(EVENT_MESSAGE_TIME), strBuffer/*, "AS2D_DISCOVERRESOURCE", MESSAGE_TYPE_MINOR_EVENT, thisResourceInfo.GetButton(), (ColorTypes)GC.getInfoTypeForString("COLOR_WHITE"), getX(), getY(), true, true*/);
									}
								}

								break;
							}
						}
					}
				}
			}
		}
	}

	doImprovementUpgrade();
}

//	--------------------------------------------------------------------------------
void CvPlot::doImprovementUpgrade()
{
	if(getImprovementType() != NO_IMPROVEMENT)
	{
		ImprovementTypes eImprovementUpdrade = (ImprovementTypes)GC.getImprovementInfo(getImprovementType())->GetImprovementUpgrade();
		if(eImprovementUpdrade != NO_IMPROVEMENT)
		{
			if(isBeingWorked() || GC.getImprovementInfo(eImprovementUpdrade)->IsOutsideBorders())
			{
				changeUpgradeProgress(GET_PLAYER(getOwner()).getImprovementUpgradeRate());

				if(getUpgradeProgress() >= (GC.getGame().getImprovementUpgradeTime(getImprovementType(), this)) * 100)
				{
					setImprovementType(eImprovementUpdrade, GetPlayerThatBuiltImprovement());
				}
			}
		}
	}
}

//	--------------------------------------------------------------------------------
FogOfWarModeTypes CvPlot::GetActiveFogOfWarMode() const
{
	CvGame& Game = GC.getGame();
	const TeamTypes activeTeam = Game.getActiveTeam();
	ASSERT_DEBUG(activeTeam != NO_TEAM);

	FogOfWarModeTypes eFOWMode = FOGOFWARMODE_UNEXPLORED;

	bool bIsDebug = Game.isDebugMode();
	if(bIsDebug || activeTeam != NO_TEAM)
	{
		if(isRevealed(activeTeam, bIsDebug))
		{
			eFOWMode = (isVisible(activeTeam, bIsDebug))? FOGOFWARMODE_OFF : FOGOFWARMODE_NOVIS;
		}
	}

	return eFOWMode;
}

//	--------------------------------------------------------------------------------
/// The fog state for the plot has changed, send an update message.
void CvPlot::updateFog(bool bDefer)
{
	if(!GC.IsGraphicsInitialized())
	{
		return;
	}

	FogOfWarModeTypes eFOWMode = GetActiveFogOfWarMode();

	if(!bDefer)
	{
		//apparently this hides/reveals any units as well. no need to touch them manually. in fact it can lead to infinity recursion?
		gDLL->GameplayFOWChanged(getX(), getY(), eFOWMode, false);
	}
	else
	{
		GC.getMap().deferredFogPlots().insert(this);
	}
}

//	---------------------------------------------------------------------------
void CvPlot::updateVisibility()
{
	if(!GC.IsGraphicsInitialized())
	{
		return;
	}

	setLayoutDirty(true);

	// Any unit that has visibility rules (it can become invisible to another player) needs to update itself.
	const TeamTypes eActiveTeam = GC.getGame().getActiveTeam();
	ASSERT_DEBUG(eActiveTeam != NO_TEAM);
	if (eActiveTeam != NO_TEAM)
	{
		IDInfo* pUnitNode = headUnitNode();
		while (pUnitNode != NULL)
		{
			CvUnit* pLoopUnit = GetPlayerUnit(*pUnitNode);
			pUnitNode = nextUnitNode(pUnitNode);

			if (pLoopUnit)
			{
				InvisibleTypes eInvisibleType = pLoopUnit->getInvisibleType();
				if (eInvisibleType != NO_INVISIBLE)
				{
					// This unit has visibility rules, send a message that it needs to update itself.
					CvInterfacePtr<ICvUnit1> pDllUnit(new CvDllUnit(pLoopUnit));
					gDLL->GameplayUnitVisibility(pDllUnit.get(), (pLoopUnit->getTeam() == eActiveTeam)?true:isInvisibleVisible(eActiveTeam, eInvisibleType), true, 0.01f);
				}
				if (pLoopUnit->IsHiddenByNearbyUnit(this))
				{
					CvInterfacePtr<ICvUnit1> pDllUnit(new CvDllUnit(pLoopUnit));
					gDLL->GameplayUnitVisibility(pDllUnit.get(), (pLoopUnit->getTeam() == eActiveTeam) ? true : isInvisibleVisibleUnit(eActiveTeam), true, 0.01f);
				}
			}
		}

		// Must update the other layers as well
		CvPlotManager& kPlotManager = GC.getMap().plotManager();
		int iNumLayers = kPlotManager.GetNumLayers();
		for (int iLayer = 0; iLayer < iNumLayers; ++iLayer)
		{	
			const CvIDInfoFixedVector &kUnits = kPlotManager.GetUnitsByIndex(m_iX, m_iY, iLayer);
			for (CvIDInfoFixedVector::const_iterator itrUnit = kUnits.begin(); itrUnit != kUnits.end(); ++itrUnit)
			{
				CvUnit* pLoopUnit = ::GetPlayerUnit(*itrUnit);
				if (pLoopUnit)
				{
					InvisibleTypes eInvisibleType = pLoopUnit->getInvisibleType();
					if (eInvisibleType != NO_INVISIBLE)
					{
						// This unit has visibility rules, send a message that it needs to update itself.
						CvInterfacePtr<ICvUnit1> pDllUnit(new CvDllUnit(pLoopUnit));
						gDLL->GameplayUnitVisibility(pDllUnit.get(), (pLoopUnit->getTeam() == eActiveTeam)?true:isInvisibleVisible(eActiveTeam, eInvisibleType), true, 0.01f);
					}
					if (pLoopUnit->IsHiddenByNearbyUnit(this))
					{
						// This unit has visibility rules, send a message that it needs to update itself.
						CvInterfacePtr<ICvUnit1> pDllUnit(new CvDllUnit(pLoopUnit));
						gDLL->GameplayUnitVisibility(pDllUnit.get(), (pLoopUnit->getTeam() == eActiveTeam) ? true : isInvisibleVisibleUnit(eActiveTeam), true, 0.01f);
					}
				}
			}
		}
	}
}

//	--------------------------------------------------------------------------------
void CvPlot::updateSymbols()
{
	CvInterfacePtr<ICvPlot1> pDllPlot(new CvDllPlot(this));
	gDLL->GameplayYieldMightHaveChanged(pDllPlot.get());
}


//	--------------------------------------------------------------------------------
void CvPlot::updateCenterUnit()
{
	if(!GC.IsGraphicsInitialized())
	{
		return;
	}

	if(!isActiveVisible())
	{
		setCenterUnit(NULL);
		return;
	}

	setCenterUnit(getSelectedUnit());

	TeamTypes eActiveTeam = GC.getGame().getActiveTeam();

	if(!getCenterUnit())
	{
		CvUnit* hBestDefender = getBestDefender(NO_PLAYER, GC.getGame().getActivePlayer());
		if(hBestDefender && hBestDefender->getDomainType() != DOMAIN_AIR && !hBestDefender->isInvisible(eActiveTeam,false))
			setCenterUnit(hBestDefender);
	}

	// okay, all of the other checks failed - if there is any unit here, make it the center unit
	if(!getCenterUnit())
	{
		IDInfo* pUnitNode = headUnitNode();

		if(pUnitNode != NULL)
		{
			CvUnit* pLoopUnit = GetPlayerUnit(*pUnitNode);
			if(pLoopUnit && !pLoopUnit->IsGarrisoned() && pLoopUnit->getDomainType() != DOMAIN_AIR && !pLoopUnit->isInvisible(eActiveTeam,false))
			{
				setCenterUnit(pLoopUnit);
			}
		}
	}

	CvUnit* pCenterUnit = getCenterUnit();
	IDInfo* pUnitNode = headUnitNode();
	while(pUnitNode != NULL)
	{
		CvUnit* pLoopUnit = GetPlayerUnit(*pUnitNode);
		pUnitNode = nextUnitNode(pUnitNode);

		if(pLoopUnit)
		{
			CvInterfacePtr<ICvUnit1> pDllUnit(new CvDllUnit(pLoopUnit));

			if(pCenterUnit == pLoopUnit)
			{
				gDLL->GameplayUnitVisibility(pDllUnit.get(), true, false, 0.01f);
			}
			else
			{
				gDLL->GameplayUnitVisibility(pDllUnit.get(), false, false, 0.01f);
			}
		}
	}
}


//	--------------------------------------------------------------------------------
void CvPlot::verifyUnitValidPlot(PlayerTypes eForSpecificPlayer, bool bWakeUp)
{
	vector<IDInfo> oldUnitList;

	IDInfo* pUnitNode = headUnitNode();
	while(pUnitNode != NULL)
	{
		oldUnitList.push_back(*pUnitNode);
		pUnitNode = nextUnitNode(pUnitNode);
	}

	for(size_t iVectorLoop = 0; iVectorLoop < oldUnitList.size(); ++iVectorLoop)
	{
		CvUnit* pLoopUnit = GetPlayerUnit(oldUnitList[iVectorLoop]);
		if(pLoopUnit != NULL)
		{
			if (eForSpecificPlayer == NO_PLAYER || (eForSpecificPlayer != NO_PLAYER && pLoopUnit->getOwner() == eForSpecificPlayer))
			{
				if (!pLoopUnit->isDelayedDeath())
				{
					if (pLoopUnit->atPlot(*this))
					{
						//if plot ownership changes we want to make sure that human units
						//which would suffer attrition or cause annoyance are woken up
						if (bWakeUp)
							pLoopUnit->SetActivityType(ACTIVITY_AWAKE);

						if (!(pLoopUnit->isCargo()))
						{
							if (!(pLoopUnit->isInCombat()))
							{
								if (!pLoopUnit->canEndTurnAtPlot(this))
								{
									if (!pLoopUnit->jumpToNearestValidPlot())
										pLoopUnit->kill(true);
								}
							}
						}
					}
				}
			}
		}
	}

	// Unit not allowed in a plot owned by someone?
	if(isOwned())
	{
		for(size_t iVectorLoop = 0; iVectorLoop < oldUnitList.size(); ++iVectorLoop)
		{
			CvUnit* pLoopUnit = GetPlayerUnit(oldUnitList[iVectorLoop]);
			if(pLoopUnit != NULL)
			{
				if(!pLoopUnit->isDelayedDeath())
				{
					if(pLoopUnit->atPlot(*this))  // it may have jumped
					{
						if(!(pLoopUnit->isInCombat()))
						{
							if(pLoopUnit->getTeam() != getTeam()) // && getTeam() == NO_TEAM)// || !GET_TEAM(getTeam()).isVassal(pLoopUnit->getTeam())))
							{
								if(isVisibleEnemyUnit(pLoopUnit))
								{
									if(!(pLoopUnit->isInvisible(getTeam(), false)))
									{
										if (!pLoopUnit->jumpToNearestValidPlot())
											pLoopUnit->kill(true);
									}
								}
							}
						}
					}
				}
			}
		}
	}
#if defined(MOD_GLOBAL_STACKING_RULES)
	else
	{
		for(size_t iVectorLoop = 0; iVectorLoop < oldUnitList.size(); ++iVectorLoop)
		{
			CvUnit* pLoopUnit = GetPlayerUnit(oldUnitList[iVectorLoop]);
			if(pLoopUnit != NULL)
			{
				if(!pLoopUnit->isDelayedDeath())
				{
					if(pLoopUnit->atPlot(*this))  // it may have jumped
					{
						if(!(pLoopUnit->isInCombat()))
						{
							for(size_t iVectorLoop2 = iVectorLoop+1; iVectorLoop2 < oldUnitList.size(); ++iVectorLoop2)
							{
								CvUnit* pLoopUnit2 = GetPlayerUnit(oldUnitList[iVectorLoop2]);
								if(pLoopUnit2 != NULL)
								{
									if(!pLoopUnit2->isDelayedDeath())
									{
										if(pLoopUnit2->atPlot(*this))  // it may have jumped
										{
											if(!(pLoopUnit2->isInCombat()))
											{
												if(atWar(pLoopUnit->getTeam(), pLoopUnit2->getTeam()))
												{
													// We have to evict the weaker of pLoopUnit and pLoopUnit2
													if (pLoopUnit->GetPower() < pLoopUnit2->GetPower())
													{
														CUSTOMLOG("Evicting player %i's %s at (%i, %i)", pLoopUnit->getOwner(), pLoopUnit->getName().c_str(), getX(), getY());
														if (!pLoopUnit->jumpToNearestValidPlot())
															pLoopUnit->kill(true);
													}
													else
													{
														CUSTOMLOG("Evicting player %i's %s at (%i, %i)", pLoopUnit2->getOwner(), pLoopUnit2->getName().c_str(), getX(), getY());
														if (!pLoopUnit2->jumpToNearestValidPlot())
															pLoopUnit2->kill(true);
													}
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
#endif
}

//	--------------------------------------------------------------------------------
// Left-over method, primarily because it is exposed to Lua.
void CvPlot::nukeExplosion(int iDamageLevel, CvUnit*)
{
	CvUnitCombat::ApplyNuclearExplosionDamage(this, iDamageLevel);
}

//	--------------------------------------------------------------------------------
bool CvPlot::isAdjacentToArea(int iAreaID) const
{
	CvPlot** aNeighbors = GC.getMap().getNeighborsUnchecked(m_iPlotIndex);
	for(int iI = 0; iI < NUM_DIRECTION_TYPES; ++iI)
	{
		if(aNeighbors[iI] != NULL)
		{
			if(aNeighbors[iI]->getArea() == iAreaID)
			{
				return true;
			}
		}
	}

	return false;
}

//	--------------------------------------------------------------------------------
bool CvPlot::isAdjacentToArea(const CvArea* pArea) const
{
	return isAdjacentToArea(pArea->GetID());
}

//	--------------------------------------------------------------------------------
bool CvPlot::isAdjacentToLandmass(int iLandmassID) const
{
	CvPlot** aNeighbors = GC.getMap().getNeighborsUnchecked(m_iPlotIndex);
	for(int iI = 0; iI < NUM_DIRECTION_TYPES; ++iI)
	{
		if(aNeighbors[iI] != NULL)
		{
			if(aNeighbors[iI]->getLandmass() == iLandmassID)
			{
				return true;
			}
		}
	}

	return false;
}

//	--------------------------------------------------------------------------------
bool CvPlot::shareAdjacentArea(const CvPlot* pPlot) const
{
	int iLastArea = -1;
	for(int iI = 0; iI < NUM_DIRECTION_TYPES; ++iI)
	{
		CvPlot* pAdjacentPlot = plotDirection(getX(), getY(), ((DirectionTypes)iI));

		if(pAdjacentPlot != NULL)
		{
			int iCurrArea = pAdjacentPlot->getArea();

			if(iCurrArea != iLastArea)
			{
				if(pPlot->isAdjacentToArea(iCurrArea))
				{
					return true;
				}

				iLastArea = iCurrArea;
			}
		}
	}

	return false;
}

//	--------------------------------------------------------------------------------
bool CvPlot::isAdjacent(const CvPlot* pPlot) const
{
	if(pPlot == NULL)
		return false;

	//or is it maybe faster to check distance == 1?
	CvPlot** aPlotsToCheck = GC.getMap().getNeighborsUnchecked(this);
	for(int iI=0; iI<NUM_DIRECTION_TYPES; iI++)
	{
		CvPlot* pAdjacentPlot = aPlotsToCheck[iI];
		if(pAdjacentPlot == pPlot)
			return true;
	}

	return false;
}

//	--------------------------------------------------------------------------------
//! A tile is considered to be on an international border if it is adjacent to at
//!  least one other tile with a different owner
bool CvPlot::isInternationalBorder() const
{
	CvPlot** aNeighbors = GC.getMap().getNeighborsUnchecked(m_iPlotIndex);
	for (int iI = 0; iI < NUM_DIRECTION_TYPES; ++iI)
	{
		if (aNeighbors[iI] != NULL)
		{
			if (getOwner() != aNeighbors[iI]->getOwner())
			{
				return true;
			}
		}
	}

	return false;
}

//	--------------------------------------------------------------------------------
bool CvPlot::isDeepWater() const
{
	if(isWater())
	{
		TerrainTypes eDeepWater = (TerrainTypes) GD_INT_GET(DEEP_WATER_TERRAIN);
		if(getTerrainType() == eDeepWater)
		{
			return true;
		}
	}
	return false;
}

//	--------------------------------------------------------------------------------
bool CvPlot::isShallowWater() const
{
	if(isWater())
	{
		TerrainTypes eShallowWater = (TerrainTypes) GD_INT_GET(SHALLOW_WATER_TERRAIN);
		if(getTerrainType() == eShallowWater)
		{
			return true;
		}
	}
	return false;
}

//	--------------------------------------------------------------------------------
bool CvPlot::isAdjacentToShallowWater() const
{
	//the result of this check should be cached (see updateWaterFlags)
	//but right now it is called only rarely, so the inefficiency doesn't matter much
	TerrainTypes eShallowWater = (TerrainTypes) GD_INT_GET(SHALLOW_WATER_TERRAIN);

	CvPlot** aPlotsToCheck = GC.getMap().getNeighborsUnchecked(this);
	for(int iI=0; iI<NUM_DIRECTION_TYPES; iI++)
	{
		CvPlot* pAdjacentPlot = aPlotsToCheck[iI];
		if(pAdjacentPlot && pAdjacentPlot->getTerrainType() == eShallowWater)
		{
			return true;
		}
	}

	return false;
}


#if defined(MOD_PROMOTIONS_CROSS_ICE)
//	--------------------------------------------------------------------------------
bool CvPlot::isAdjacentToIce() const
{
	for(int iI = 0; iI < NUM_DIRECTION_TYPES; ++iI)
	{
		CvPlot* pAdjacentPlot = plotDirection(getX(), getY(), ((DirectionTypes)iI));

		if(pAdjacentPlot != NULL)
		{
			if(pAdjacentPlot->isIce())
			{
				return true;
			}
		}
	}

	return false;
}
#endif

//	--------------------------------------------------------------------------------
CvLandmass* CvPlot::GetLargestAdjacentWater() const
{
	if(isWater())
		return NULL;

	CvLandmass* pResult = NULL;
	int iMaxSize = 0;

	for(int iI = 0; iI < NUM_DIRECTION_TYPES; ++iI)
	{
		CvPlot* pAdjacentPlot = plotDirection(getX(), getY(), ((DirectionTypes)iI));

		if(pAdjacentPlot && pAdjacentPlot->isWater())
		{
			CvLandmass* pAdjacentBodyOfWater = pAdjacentPlot->landmass();
			if (pAdjacentBodyOfWater && pAdjacentBodyOfWater->getNumTiles() > iMaxSize)
			{
				iMaxSize = pAdjacentBodyOfWater->getNumTiles();
				pResult = pAdjacentBodyOfWater;
			}
		}
	}

	return pResult;
}

//	--------------------------------------------------------------------------------
CvArea* CvPlot::GetLargestAdjacentWaterArea() const
{
	if(isWater())
		return NULL;

	CvArea* pResult = NULL;
	int iMaxSize = 0;

	for(int iI = 0; iI < NUM_DIRECTION_TYPES; ++iI)
	{
		CvPlot* pAdjacentPlot = plotDirection(getX(), getY(), ((DirectionTypes)iI));

		if(pAdjacentPlot && pAdjacentPlot->isWater())
		{
			CvArea* pAdjacentBodyOfWater = pAdjacentPlot->area();
			if (pAdjacentBodyOfWater && pAdjacentBodyOfWater->getNumTiles() > iMaxSize)
			{
				iMaxSize = pAdjacentBodyOfWater->getNumTiles();
				pResult = pAdjacentBodyOfWater;
			}
		}
	}

	return pResult;
}

//	--------------------------------------------------------------------------------
bool CvPlot::isVisibleWorked() const
{
	if(isBeingWorked())
	{
		if((getTeam() == GC.getGame().getActiveTeam()) || GC.getGame().isDebugMode())
		{
			return true;
		}
	}

	return false;
}


//	--------------------------------------------------------------------------------
bool CvPlot::isWithinTeamCityRadius(TeamTypes eTeam, PlayerTypes eIgnorePlayer) const
{
	for(int iI = 0; iI < MAX_PLAYERS; ++iI)
	{
		if(GET_PLAYER((PlayerTypes)iI).isAlive())
		{
			if(GET_PLAYER((PlayerTypes)iI).getTeam() == eTeam)
			{
				if((eIgnorePlayer == NO_PLAYER) || (((PlayerTypes)iI) != eIgnorePlayer))
				{
					if(isPlayerCityRadius((PlayerTypes)iI))
					{
						return true;
					}
				}
			}
		}
	}

	return false;
}


//	--------------------------------------------------------------------------------
bool CvPlot::isLake(bool bUseCachedValue) const
{
	if (!bUseCachedValue)
		updateWaterFlags();

	return m_bIsLake;
}

bool CvPlot::isFreshWater(bool bUseCachedValue) const
{
	if (!bUseCachedValue)
		updateWaterFlags();
	return m_bIsFreshwater;
}

//	--------------------------------------------------------------------------------
bool CvPlot::isCoastalLand(int iMinWaterSize, bool bUseCachedValue, bool bCheckCanals) const
{
	if (!bUseCachedValue)
		updateWaterFlags();

	if (m_bIsAdjacentToWater)
	{
		// fast check for ocean or 1-size water bodies
		if (iMinWaterSize < 2)
			return true;

		//otherwise check the size of the water body
		CvPlot** aPlotsToCheck = GC.getMap().getNeighborsUnchecked(this);
		for (int iCount=0; iCount<NUM_DIRECTION_TYPES; iCount++)
		{
			const CvPlot* pAdjacentPlot = aPlotsToCheck[iCount];
			if (pAdjacentPlot && pAdjacentPlot->isWater())
			{
				if (pAdjacentPlot->getFeatureType() == FEATURE_ICE && !pAdjacentPlot->isOwned())
					continue;

				//look at the "landmass", not at the area - areas may be very small and multiple areas make one body of water
				CvLandmass* pAdjacentBodyOfWater = GC.getMap().getLandmassById(pAdjacentPlot->getLandmass());
				if (pAdjacentBodyOfWater && pAdjacentBodyOfWater->getNumTiles() >= iMinWaterSize)
					return true;
			}
		}


		//If not checking for canals, abort!
		// Also, water tiles are not land, so always return false
		if (!bCheckCanals || !MOD_GLOBAL_PASSABLE_FORTS || isWater())
			return false;

		//check for areas of water connected by canals
		//starting with the plot itself, we loop through all adjacent plots and add them to a list if they are water tiles
		//or if there is an owned fort, citadel or city on them
		//we do this repeatedly until we reach the required amount of plots or until every item of the list is checked
		std::vector<const CvPlot*> vAccessibleWaterPlots(1, this);
		TeamTypes eTeam = getTeam();
		unsigned int iNumPlotsChecked = 0;
		do
		{
			const CvPlot* pPlotBeingChecked = vAccessibleWaterPlots[iNumPlotsChecked];
			CvPlot** aPlotsToCheck = GC.getMap().getNeighborsUnchecked(pPlotBeingChecked);
			for (int iCount = 0; iCount < NUM_DIRECTION_TYPES; iCount++)
			{
				const CvPlot* pAdjacentPlot = aPlotsToCheck[iCount];
				if (!pAdjacentPlot)
					continue;

				// If water, must not be unowned ice
				if (pAdjacentPlot->isWater())
				{
					if (pAdjacentPlot->getFeatureType() == FEATURE_ICE && !pAdjacentPlot->isOwned())
						continue;
				}
				//If land, must be owned city, fort or citadel
				else if (pAdjacentPlot->getTeam()==eTeam)
				{
					if (!pAdjacentPlot->isCity() && (!pAdjacentPlot->IsImprovementPassable() || pAdjacentPlot->IsImprovementPillaged()))
						continue;

					// Must be adjacent to water
					if (!pAdjacentPlot->isAdjacentToWater())
						continue;
				}
				else
				{
					// Land tile not owned by us
					continue;
				}

				// add the plot to the list if it's not already in it
				if (std::find(vAccessibleWaterPlots.begin(), vAccessibleWaterPlots.end(), pAdjacentPlot) == vAccessibleWaterPlots.end())
				{
					vAccessibleWaterPlots.push_back(pAdjacentPlot);
					if (static_cast<int>(vAccessibleWaterPlots.size())-1 >= iMinWaterSize) // minus 1 because the first element in the list is the plot from which we started
						return true;
				}
			}
			iNumPlotsChecked++;
		} 
		while (iNumPlotsChecked != vAccessibleWaterPlots.size());
	}
	return false;
}

//	--------------------------------------------------------------------------------
bool CvPlot::isAdjacentToLand(bool bUseCachedValue) const
{
	if (!bUseCachedValue)
		updateWaterFlags();

	return m_bIsAdjacentToLand;
}

//	--------------------------------------------------------------------------------
bool CvPlot::isAdjacentToWater() const
{
	for (int iI = 0; iI < NUM_DIRECTION_TYPES; ++iI)
	{
		CvPlot* pAdjacentPlot = plotDirection(getX(), getY(), ((DirectionTypes)iI));

		if (pAdjacentPlot)
		{
			if (pAdjacentPlot->isWater())
				return true;
		}
	}

	return false;
}

void CvPlot::updateWaterFlags() const
{
	//------- first check lakes and coasts
	if (isWater())
	{
		CvLandmass* pLandmass = GC.getMap().getLandmassById(m_iLandmass);
		m_bIsLake = pLandmass ? pLandmass->isLake() : false;
		m_bIsAdjacentToWater = false; //by definition
		m_bIsAdjacentToLand = false; //may be set to true later

		CvPlot** aPlotsToCheck = GC.getMap().getNeighborsUnchecked(this);
		for(int iCount=0; iCount<NUM_DIRECTION_TYPES; iCount++)
		{
			const CvPlot* pAdjacentPlot = aPlotsToCheck[iCount];
			if(pAdjacentPlot && !pAdjacentPlot->isWater())
			{
				m_bIsAdjacentToLand = true;
				break;
			}
		}
	}
	else //land plots
	{
		m_bIsLake = false;
		m_bIsAdjacentToWater = false; //maybe set to true later
		m_bIsAdjacentToLand = false; //always false for land plots (by definition)

		CvPlot** aPlotsToCheck = GC.getMap().getNeighborsUnchecked(this);
		for(int iCount=0; iCount<NUM_DIRECTION_TYPES; iCount++)
		{
			const CvPlot* pAdjacentPlot = aPlotsToCheck[iCount];
			if(pAdjacentPlot && pAdjacentPlot->isWater() && pAdjacentPlot->getFeatureType()!=FEATURE_ICE)
			{
				m_bIsAdjacentToWater = true;
				break;
			}
		}
	}

	//------ finally freshwater
	
	m_bIsFreshwater = isRiver();
	//isImpassable() removed this check, Gazebo
	if(isWater() || m_bIsFreshwater)
		return;

	//now the more complex checks
	CvPlot** aPlotsToCheck = GC.getMap().getNeighborsUnchecked(this);
	//+1 means we check the plot itself also!
	for(int iCount=0; iCount<NUM_DIRECTION_TYPES+1; iCount++)
	{
		const CvPlot* pLoopPlot = aPlotsToCheck[iCount];

		if(pLoopPlot != NULL)
		{
			if(pLoopPlot->isLake())
			{
				m_bIsFreshwater = true;
				return;
			}

			FeatureTypes feature_type = pLoopPlot->getFeatureType();

			if(feature_type != NO_FEATURE)
			{
				if(GC.getFeatureInfo(feature_type)->isAddsFreshWater())
				{
					m_bIsFreshwater = true;
					return;
				}
			}

			ImprovementTypes improvement_type = pLoopPlot->getImprovementType();

			if(improvement_type != NO_IMPROVEMENT)
			{
				if(GC.getImprovementInfo(improvement_type)->IsAddsFreshWater())
				{
					m_bIsFreshwater = true;
					return;
				}
			}

			if (pLoopPlot->isCity() && pLoopPlot->getPlotCity()->isAddsFreshWater())
			{
				m_bIsFreshwater = true;
				return;
			}
		}
	}
}

//	--------------------------------------------------------------------------------
bool CvPlot::isRiverCrossingFlowClockwise(DirectionTypes eDirection) const
{
	CvPlot* pPlot = NULL;
	switch(eDirection)
	{
	case DIRECTION_NORTHEAST:
		pPlot = plotDirection(getX(), getY(), DIRECTION_NORTHEAST);
		if(pPlot != NULL)
		{
			return (pPlot->getRiverSWFlowDirection() == FLOWDIRECTION_SOUTHEAST);
		}
		break;
	case DIRECTION_EAST:
		return (getRiverEFlowDirection() == FLOWDIRECTION_SOUTH);
		break;
	case DIRECTION_SOUTHEAST:
		return (getRiverSEFlowDirection() == FLOWDIRECTION_SOUTHWEST);
		break;
	case DIRECTION_SOUTHWEST:
		return (getRiverSWFlowDirection() == FLOWDIRECTION_NORTHWEST);
		break;
	case DIRECTION_WEST:
		pPlot = plotDirection(getX(), getY(), DIRECTION_WEST);
		if(pPlot != NULL)
		{
			return (pPlot->getRiverEFlowDirection() == FLOWDIRECTION_NORTH);
		}
		break;
	case DIRECTION_NORTHWEST:
		pPlot = plotDirection(getX(), getY(), DIRECTION_NORTHWEST);
		if(pPlot != NULL)
		{
			return (pPlot->getRiverSEFlowDirection() == FLOWDIRECTION_NORTHEAST);
		}
		break;
	default:
		ASSERT_DEBUG(false);
		break;
	}

	return false;
}


//	--------------------------------------------------------------------------------
bool CvPlot::isRiverSide() const
{
	CvPlot* pLoopPlot = NULL;
	int iI = 0;

	for(iI = 0; iI < NUM_DIRECTION_TYPES; ++iI)
	{
		pLoopPlot = plotDirection(getX(), getY(), ((DirectionTypes)iI));

		if(pLoopPlot != NULL)
		{
			if(isRiverCrossing(directionXY(this, pLoopPlot)))
			{
				return true;
			}
		}
	}

	return false;
}

//	--------------------------------------------------------------------------------
bool CvPlot::isRiverConnection(DirectionTypes eDirection) const
{
	switch(eDirection)
	{
	case NO_DIRECTION:
		return false;
		break;
	case DIRECTION_NORTHEAST:
		return (isRiverCrossing(DIRECTION_NORTHWEST) || isRiverCrossing(DIRECTION_EAST));
		break;

	case DIRECTION_EAST:
		return (isRiverCrossing(DIRECTION_NORTHEAST) || isRiverCrossing(DIRECTION_SOUTHEAST));
		break;

	case DIRECTION_SOUTHEAST:
		return (isRiverCrossing(DIRECTION_EAST) || isRiverCrossing(DIRECTION_SOUTHWEST));
		break;

	case DIRECTION_SOUTHWEST:
		return (isRiverCrossing(DIRECTION_SOUTHEAST) || isRiverCrossing(DIRECTION_WEST));
		break;

	case DIRECTION_WEST:
		return (isRiverCrossing(DIRECTION_NORTHWEST) || isRiverCrossing(DIRECTION_SOUTHWEST));
		break;

	case DIRECTION_NORTHWEST:
		return (isRiverCrossing(DIRECTION_NORTHEAST) || isRiverCrossing(DIRECTION_WEST));
		break;

	default:
		ASSERT_DEBUG(false);
		break;
	}

	return false;
}

//	--------------------------------------------------------------------------------
CvPlot* CvPlot::getNeighboringPlot(DirectionTypes eDirection) const
{
	return plotDirection(getX(), getY(), eDirection);
}

//	--------------------------------------------------------------------------------
/// Is there a river on this side of the plot?
bool CvPlot::IsRiverSide(DirectionTypes eDirection) const
{
	ASSERT_DEBUG(eDirection != NO_DIRECTION, "eDirection is not assigned a valid value");
	CvPlot* pAdjacentPlot = plotDirection(getX(), getY(), eDirection);
	switch (eDirection)
	{
		case DIRECTION_NORTHEAST:
		{
			return pAdjacentPlot ? pAdjacentPlot->isNEOfRiver() : false;
		}
		case DIRECTION_EAST:
		{
			return isWOfRiver();
		}
		case DIRECTION_SOUTHEAST:
		{
			return isNWOfRiver();
		}
		case DIRECTION_SOUTHWEST:
		{
			return isNEOfRiver();
		}
		case DIRECTION_WEST:
		{
			return pAdjacentPlot ? pAdjacentPlot->isWOfRiver() : false;
		}
		case DIRECTION_NORTHWEST:
		{
			return pAdjacentPlot ? pAdjacentPlot->isNWOfRiver() : false;
		}
		default:
			return false;
	}
}

//	--------------------------------------------------------------------------------
/// Is there a river on this side of the plot?
bool CvPlot::IsLakeSide(DirectionTypes eDirection) const
{
	ASSERT_DEBUG(eDirection != NO_DIRECTION, "eDirection is not assigned a valid value");
	CvPlot* pAdjacentPlot = plotDirection(getX(), getY(), eDirection);
	return pAdjacentPlot && pAdjacentPlot->isLake();
}

//	--------------------------------------------------------------------------------
/// We don't care about river flow or whether it crosses a river.
/// As long as both plots are on the same side of a river system, it counts.
bool CvPlot::IsAlongSameRiver(const CvPlot* pToPlot) const
{
	ASSERT_DEBUG(pToPlot);
	DirectionTypes eDirection = directionXY(this, pToPlot);
	DirectionTypes eNextDirection = static_cast<DirectionTypes>((eDirection + 1) % 6);
	DirectionTypes ePrevDirection = static_cast<DirectionTypes>((eDirection + 5) % 6);
	DirectionTypes eAdjNextDirection = static_cast<DirectionTypes>((eDirection + 4) % 6);
	DirectionTypes eAdjPrevDirection = static_cast<DirectionTypes>((eDirection + 2) % 6);

	if (IsRiverSide(eNextDirection) && pToPlot->IsRiverSide(eAdjPrevDirection))
		return true;

	if (IsRiverSide(ePrevDirection) && pToPlot->IsRiverSide(eAdjNextDirection))
		return true;

	return false;
}

//	--------------------------------------------------------------------------------
CvPlot* CvPlot::getNearestLandPlotInternal(int iDistance) const
{
	if(iDistance > GC.getMap().getGridHeight() && iDistance > GC.getMap().getGridWidth())
	{
		return NULL;
	}

	for(int iDX = -iDistance; iDX <= iDistance; iDX++)
	{
		for(int iDY = -iDistance; iDY <= iDistance; iDY++)
		{
			// bkw - revisit this, it works but is inefficient
			CvPlot* pPlot = plotXY(getX(), getY(), iDX, iDY);
			if(pPlot != NULL && !pPlot->isWater() && plotDistance(getX(), getY(), pPlot->getX(), pPlot->getY()) == iDistance)
			{
				return pPlot;
			}
		}
	}
	return getNearestLandPlotInternal(iDistance + 1);
}


//	--------------------------------------------------------------------------------
int CvPlot::getNearestLandArea() const
{
	CvPlot* pPlot = getNearestLandPlot();
	return pPlot ? pPlot->getArea() : -1;
}


//	--------------------------------------------------------------------------------
CvPlot* CvPlot::getNearestLandPlot() const
{
	return getNearestLandPlotInternal(0);
}


//	--------------------------------------------------------------------------------
int CvPlot::seeFromLevel(TeamTypes eTeam) const
{
	int iLevel = 0;

	ASSERT_DEBUG(getTerrainType() != NO_TERRAIN, "TerrainType is not assigned a valid value");

	// Plot recon-ed?
	if(getReconCount() > 0)
	{
		// By my reckoning, this just needs to be a big number
		iLevel = 10;
	}
	// Normal visibility
	else
	{
		iLevel = (getTerrainType()!=NO_TERRAIN) ? GC.getTerrainInfo(getTerrainType())->getSeeFromLevel() : 0;
	}

	if (isMountain())
	{
		iLevel += /*2*/ GD_INT_GET(MOUNTAIN_SEE_FROM_CHANGE);
	}
	else if (isHills())
	{
		iLevel += /*1*/ GD_INT_GET(HILLS_SEE_FROM_CHANGE);
	}
	else if (isWater())
	{
		iLevel += /*1*/ GD_INT_GET(SEAWATER_SEE_FROM_CHANGE);

		if (eTeam != NO_TEAM && GET_TEAM(eTeam).isExtraWaterSeeFrom())
			iLevel++;
	}

	// Land plots are "higher" than water plots, limiting visibility and range attacks from water onto land
	if (MOD_BALANCE_VP && !isWater())
		iLevel++;

	return iLevel;
}


//	--------------------------------------------------------------------------------
int CvPlot::seeThroughLevel(bool bIncludeShubbery) const
{
	int iLevel = (getTerrainType()!=NO_TERRAIN) ? GC.getTerrainInfo(getTerrainType())->getSeeThroughLevel() : 0;

	if (bIncludeShubbery && getFeatureType() != NO_FEATURE)
	{
		iLevel += GC.getFeatureInfo(getFeatureType())->getSeeThroughChange();
	}

	if (isMountain() && getFeatureType() == NO_FEATURE) //natural wonders are features on mountain sometimes
	{
		iLevel += /*2*/ GD_INT_GET(MOUNTAIN_SEE_THROUGH_CHANGE);
	}
	else if (isHills())
	{
		iLevel += /*1*/ GD_INT_GET(HILLS_SEE_THROUGH_CHANGE);
	}
	else if (isWater())
	{
		iLevel += /*1*/ GD_INT_GET(SEAWATER_SEE_THROUGH_CHANGE);
	}

	// Land plots are "higher" than water plots, limiting visibility and range attacks from water onto land
	if (MOD_BALANCE_VP && !isWater())
		iLevel++;

	return iLevel;
}


//	--------------------------------------------------------------------------------
void CvPlot::changeSeeFromSight(TeamTypes eTeam, DirectionTypes eDirection, int iFromLevel, bool bIncrement, InvisibleTypes eSeeInvisible, CvUnit* pUnit)
{
	CvPlot* pPlot = NULL;
	int iThroughLevel = seeThroughLevel();

	if(iFromLevel >= iThroughLevel)
	{
		pPlot = plotDirection(getX(), getY(), eDirection);

		if(pPlot != NULL)
		{
			if((iFromLevel > iThroughLevel) || (pPlot->seeFromLevel(eTeam) > iFromLevel))
			{
				pPlot->changeVisibilityCount(eTeam, ((bIncrement) ? 1 : -1), eSeeInvisible, true, false, pUnit);
			}
		}
	}
}

//	--------------------------------------------------------------------------------
// while this looks more complex than the previous version, it should run much faster
void CvPlot::changeAdjacentSight(TeamTypes eTeam, int iRange, bool bIncrement, InvisibleTypes eSeeInvisible, DirectionTypes eFacingDirection, CvUnit* pUnit)
{
	//do nothing if range is negative, this is invalid
	if (iRange < 0)
		return;

	//range zero is dangerous, it can lead to unit stacking problems, should happen only with trade units
	if (iRange==0)
	{
		//change the visibility of this plot only
		changeVisibilityCount(eTeam, ((bIncrement) ? 1 : -1), eSeeInvisible, true, false, pUnit);
		return;
	}

	vector<int>& scratchpad = GC.getMap().GetVisibilityScratchpad();

	bool bBasedOnUnit = (pUnit != NULL);

	//check one extra outer ring
	int iRangeWithOneExtraRing = iRange + 1;

	// start in the center going NE
	int thisRing = 0;
	DirectionTypes eDirectionOfNextTileToCheck = DIRECTION_NORTHWEST;
	CvPlot* pPlotToCheck = this;
	int iDX = 0;
	int iDY = 0;
	int iCenterLevel = seeFromLevel(eTeam);
	int iPlotCounter = 0;
	int iMaxPlotNumberOnThisRing = 0;

	while(thisRing <= iRangeWithOneExtraRing)
	{
		if(pPlotToCheck)
		{
			//always reveal adjacent plots when using line of sight - but then clear it out
			if(thisRing <= 1)
			{
				if(eFacingDirection != NO_DIRECTION)
				{
					pPlotToCheck->changeVisibilityCount(eTeam, 1, eSeeInvisible, false /*bInformExplorationTracking*/, false, pUnit);
					pPlotToCheck->changeVisibilityCount(eTeam, -1, eSeeInvisible, false /*bInformExplorationTracking*/, false, pUnit);
				}
			}

			// see if this plot is in the visibility wedge
			if(shouldProcessDisplacementPlot(iDX, iDY, iRange, eFacingDirection))
			{
				if(thisRing != 0)
				{
					CvPlot* pFirstInwardPlot = NULL;
					CvPlot* pSecondInwardPlot = NULL;
					const int INVALID_RING = -1;
					const int HALF_BLOCKED = 0x01000000;
					int iRingOfFirstInwardPlot = INVALID_RING;
					int iRingOfSecondInwardPlot = INVALID_RING;
					int iFirstInwardLevel = INVALID_RING;
					int iSecondInwardLevel = INVALID_RING;
					bool bFirstHalfBlocked = false;
					bool bSecondHalfBlocked = false;

					// try to look at the two plot inwards
					switch(eDirectionOfNextTileToCheck)
					{
					case NO_DIRECTION:
						UNREACHABLE();
					case DIRECTION_NORTHEAST:
						pFirstInwardPlot = plotDirection(pPlotToCheck->getX(),pPlotToCheck->getY(),DIRECTION_EAST);
						pSecondInwardPlot = plotDirection(pPlotToCheck->getX(),pPlotToCheck->getY(),DIRECTION_SOUTHEAST);
						break;
					case DIRECTION_EAST:
						pFirstInwardPlot = plotDirection(pPlotToCheck->getX(),pPlotToCheck->getY(),DIRECTION_SOUTHWEST);
						pSecondInwardPlot = plotDirection(pPlotToCheck->getX(),pPlotToCheck->getY(),DIRECTION_SOUTHEAST);
						break;
					case DIRECTION_SOUTHEAST:
						pFirstInwardPlot = plotDirection(pPlotToCheck->getX(),pPlotToCheck->getY(),DIRECTION_SOUTHWEST);
						pSecondInwardPlot = plotDirection(pPlotToCheck->getX(),pPlotToCheck->getY(),DIRECTION_WEST);
						break;
					case DIRECTION_SOUTHWEST:
						pFirstInwardPlot = plotDirection(pPlotToCheck->getX(),pPlotToCheck->getY(),DIRECTION_WEST);
						pSecondInwardPlot = plotDirection(pPlotToCheck->getX(),pPlotToCheck->getY(),DIRECTION_NORTHWEST);
						break;
					case DIRECTION_WEST:
						pFirstInwardPlot = plotDirection(pPlotToCheck->getX(),pPlotToCheck->getY(),DIRECTION_NORTHWEST);
						pSecondInwardPlot = plotDirection(pPlotToCheck->getX(),pPlotToCheck->getY(),DIRECTION_NORTHEAST);
						break;
					case DIRECTION_NORTHWEST:
						pFirstInwardPlot = plotDirection(pPlotToCheck->getX(),pPlotToCheck->getY(),DIRECTION_EAST);
						pSecondInwardPlot = plotDirection(pPlotToCheck->getX(),pPlotToCheck->getY(),DIRECTION_NORTHEAST);
						break;
					}
					if(pFirstInwardPlot)
					{
						iRingOfFirstInwardPlot = plotDistance(getX(),getY(),pFirstInwardPlot->getX(),pFirstInwardPlot->getY());
						if(iRingOfFirstInwardPlot == thisRing - 1)
						{
							iFirstInwardLevel = scratchpad[ pFirstInwardPlot->GetPlotIndex() ];
							if(iFirstInwardLevel >= HALF_BLOCKED)
							{
								iFirstInwardLevel -= HALF_BLOCKED;
								bFirstHalfBlocked = true;
							}
						}
					}
					if(pSecondInwardPlot)
					{
						iRingOfSecondInwardPlot = plotDistance(getX(),getY(),pSecondInwardPlot->getX(),pSecondInwardPlot->getY());
						if(iRingOfSecondInwardPlot == thisRing - 1)
						{
							iSecondInwardLevel = scratchpad[ pSecondInwardPlot->GetPlotIndex() ];
							if(iSecondInwardLevel >= HALF_BLOCKED)
							{
								iSecondInwardLevel -= HALF_BLOCKED;
								bSecondHalfBlocked = true;
							}
						}
					}
					int iThisPlotLevel = pPlotToCheck->seeThroughLevel(thisRing != iRangeWithOneExtraRing);
					if(iFirstInwardLevel != INVALID_RING && iSecondInwardLevel != INVALID_RING && iFirstInwardLevel != iSecondInwardLevel && !bFirstHalfBlocked && !bSecondHalfBlocked)
					{
						double fP0X = (double) getX();
						double fP0Y = (double) getY();
						double fP1X = (double) pPlotToCheck->getX();
						double fP1Y = (double) pPlotToCheck->getY();
						if(getY() & 1)
						{
							fP0X += 0.5;
						}
						if(pPlotToCheck->getY() & 1)
						{
							fP1X += 0.5;
						}

						double a = fP1Y - fP0Y;
						double b = fP0X - fP1X;
						double c = fP0Y * fP1X - fP1Y * fP0X;

						double fFirstInwardX = (double) pFirstInwardPlot->getX();
						double fFirstInwardY = (double) pFirstInwardPlot->getY();
						if(pFirstInwardPlot->getY() & 1)
						{
							fFirstInwardX += 0.5;
						}
						double fFirstDist = a * fFirstInwardX + b * fFirstInwardY + c;
						fFirstDist = abs(fFirstDist);
						// skip the extra distance since it is the same for both equations

						double fSecondInwardX = (double) pSecondInwardPlot->getX();
						double fSecondInwardY = (double) pSecondInwardPlot->getY();
						if(pSecondInwardPlot->getY() & 1)
						{
							fSecondInwardX += 0.5;
						}
						double fSecondDist = a * fSecondInwardX + b * fSecondInwardY + c;
						fSecondDist = abs(fSecondDist);
						// skip the extra distance since it is the same for both equations

						if(fFirstDist - fSecondDist > 0.05)  // we are closer to the second point
						{
							int iHighestLevel = (iSecondInwardLevel > iThisPlotLevel) ? iSecondInwardLevel : iThisPlotLevel;
							scratchpad[ pPlotToCheck->GetPlotIndex() ] = iHighestLevel;
							if(iSecondInwardLevel < iThisPlotLevel || ((iCenterLevel >= iSecondInwardLevel) && (thisRing < iRangeWithOneExtraRing)))
							{
								pPlotToCheck->changeVisibilityCount(eTeam, ((bIncrement) ? 1 : -1), eSeeInvisible, true, bBasedOnUnit && thisRing < 2, pUnit);
							}
						}
						else if(fSecondDist - fFirstDist > 0.05)   // we are closer to the first point
						{
							int iHighestLevel = (iFirstInwardLevel > iThisPlotLevel) ? iFirstInwardLevel : iThisPlotLevel;
							scratchpad[ pPlotToCheck->GetPlotIndex() ] = iHighestLevel;
							if(iFirstInwardLevel < iThisPlotLevel || ((iCenterLevel >= iFirstInwardLevel) && (thisRing < iRangeWithOneExtraRing)))
							{
								pPlotToCheck->changeVisibilityCount(eTeam, ((bIncrement) ? 1 : -1), eSeeInvisible, true, bBasedOnUnit && thisRing < 2, pUnit);
							}
						}
						else
						{
							int iHighestInwardLevel = (iFirstInwardLevel > iSecondInwardLevel) ? iFirstInwardLevel : iSecondInwardLevel;
							int iLowestInwardLevel = (iFirstInwardLevel > iSecondInwardLevel) ? iSecondInwardLevel : iFirstInwardLevel;
							int iHighestLevel = (iHighestInwardLevel > iThisPlotLevel) ? iHighestInwardLevel : iThisPlotLevel;
							int iHighestLowestLevel = (iLowestInwardLevel > iThisPlotLevel) ? iLowestInwardLevel : iThisPlotLevel;
							if(iHighestInwardLevel > iThisPlotLevel)
							{
								scratchpad[ pPlotToCheck->GetPlotIndex() ] = iHighestLowestLevel + HALF_BLOCKED;
							}
							else
							{
								scratchpad[ pPlotToCheck->GetPlotIndex() ] = iHighestLevel;
							}
							if(iLowestInwardLevel < iThisPlotLevel || ((iCenterLevel >= iLowestInwardLevel) && (thisRing < iRangeWithOneExtraRing)))
							{
								pPlotToCheck->changeVisibilityCount(eTeam, ((bIncrement) ? 1 : -1), eSeeInvisible, true, bBasedOnUnit && thisRing < 2, pUnit);
							}
						}
					}
					else if(iFirstInwardLevel != INVALID_RING && !bFirstHalfBlocked)
					{
						int iHighestLevel = (iFirstInwardLevel > iThisPlotLevel) ? iFirstInwardLevel : iThisPlotLevel;
						scratchpad[ pPlotToCheck->GetPlotIndex() ] = iHighestLevel;
						if(iFirstInwardLevel < iThisPlotLevel || ((iCenterLevel >= iFirstInwardLevel) && (thisRing < iRangeWithOneExtraRing)))
						{
							pPlotToCheck->changeVisibilityCount(eTeam, ((bIncrement) ? 1 : -1), eSeeInvisible, true, bBasedOnUnit && thisRing < 2, pUnit);
						}
					}
					else if(iSecondInwardLevel != INVALID_RING && !bSecondHalfBlocked)
					{
						int iHighestLevel = (iSecondInwardLevel > iThisPlotLevel) ? iSecondInwardLevel : iThisPlotLevel;
						scratchpad[ pPlotToCheck->GetPlotIndex() ] = iHighestLevel;
						if(iSecondInwardLevel < iThisPlotLevel || ((iCenterLevel >= iSecondInwardLevel) && (thisRing < iRangeWithOneExtraRing)))
						{
							pPlotToCheck->changeVisibilityCount(eTeam, ((bIncrement) ? 1 : -1), eSeeInvisible, true, bBasedOnUnit && thisRing < 2, pUnit);
						}
					}
					else if(iFirstInwardLevel != INVALID_RING)
					{
						int iHighestLevel = (iFirstInwardLevel > iThisPlotLevel) ? iFirstInwardLevel : iThisPlotLevel;
						scratchpad[ pPlotToCheck->GetPlotIndex() ] = iHighestLevel;
						if(iFirstInwardLevel < iThisPlotLevel || ((iCenterLevel >= iFirstInwardLevel) && (thisRing < iRangeWithOneExtraRing)))
						{
							pPlotToCheck->changeVisibilityCount(eTeam, ((bIncrement) ? 1 : -1), eSeeInvisible, true, bBasedOnUnit && thisRing < 2, pUnit);
						}
					}
					else if(iSecondInwardLevel != INVALID_RING)
					{
						int iHighestLevel = (iSecondInwardLevel > iThisPlotLevel) ? iSecondInwardLevel : iThisPlotLevel;
						scratchpad[ pPlotToCheck->GetPlotIndex() ] = iHighestLevel;
						if(iSecondInwardLevel < iThisPlotLevel || ((iCenterLevel >= iSecondInwardLevel) && (thisRing < iRangeWithOneExtraRing)))
						{
							pPlotToCheck->changeVisibilityCount(eTeam, ((bIncrement) ? 1 : -1), eSeeInvisible, true, bBasedOnUnit && thisRing < 2, pUnit);
						}
					}
					else // I have no idea how this can happen, but...
					{
						scratchpad[ pPlotToCheck->GetPlotIndex() ] = iThisPlotLevel;
					}
				}
				else // this is the center point
				{
					pPlotToCheck->changeVisibilityCount(eTeam, ((bIncrement) ? 1 : -1), eSeeInvisible, true, bBasedOnUnit && thisRing < 2, pUnit);
					scratchpad[ pPlotToCheck->GetPlotIndex() ] = 0;
				}
			}

		}

		int iNextDX = 0;
		int iNextDY = 0;
		if(iPlotCounter >= iMaxPlotNumberOnThisRing)  // we have processed all of the plots in this ring
		{
			// if that fails go out one ring in the NE direction traveling E
			eDirectionOfNextTileToCheck = DIRECTION_NORTHEAST;
			iNextDX = iDX + GC.getPlotDirectionX()[eDirectionOfNextTileToCheck];
			iNextDY = iDY + GC.getPlotDirectionY()[eDirectionOfNextTileToCheck];
			thisRing++;
			iMaxPlotNumberOnThisRing += thisRing * 6;
			eDirectionOfNextTileToCheck = DIRECTION_EAST;
			// (if that is further out than the extended range, we are done)
		}
		else
		{
			iNextDX = iDX + GC.getPlotDirectionX()[eDirectionOfNextTileToCheck];
			iNextDY = iDY + GC.getPlotDirectionY()[eDirectionOfNextTileToCheck];
			if(hexDistance(iNextDX,iNextDY) > thisRing)
			{
				// try to turn right
				eDirectionOfNextTileToCheck = GC.getTurnRightDirection(eDirectionOfNextTileToCheck);
				iNextDX = iDX + GC.getPlotDirectionX()[eDirectionOfNextTileToCheck];
				iNextDY = iDY + GC.getPlotDirectionY()[eDirectionOfNextTileToCheck];
			}
		}

		iPlotCounter++;

		iDX = iNextDX;
		iDY = iNextDY;
		pPlotToCheck = plotXYWithRangeCheck(getX(),getY(),iDX,iDY,iRangeWithOneExtraRing);
	}

}

//	--------------------------------------------------------------------------------
// Implementation based on changeAdjacentSight
void CvPlot::ChangeKnownAdjacentSight(TeamTypes eTeam, TeamTypes eMinorCivAlly, int iRange, DirectionTypes eFacingDirection)
{
	//do nothing if range is negative, this is invalid
	if (iRange < 0)
		return;

	//range zero is dangerous, it can lead to unit stacking problems, should happen only with trade units
	if (iRange == 0)
	{
		//change the visibility of this plot only
		IncreaseKnownVisibilityCount(eTeam, eMinorCivAlly);
		return;
	}

	vector<int>& scratchpad = GC.getMap().GetKnownVisibilityScratchpad();

	//check one extra outer ring
	int iRangeWithOneExtraRing = iRange + 1;

	// start in the center going NE
	int thisRing = 0;
	DirectionTypes eDirectionOfNextTileToCheck = DIRECTION_NORTHWEST;
	CvPlot* pPlotToCheck = this;
	int iDX = 0;
	int iDY = 0;
	int iCenterLevel = seeFromLevel(eTeam);
	int iPlotCounter = 0;
	int iMaxPlotNumberOnThisRing = 0;

	while (thisRing <= iRangeWithOneExtraRing)
	{
		if (pPlotToCheck)
		{
			// see if this plot is in the visibility wedge
			if (shouldProcessDisplacementPlot(iDX, iDY, iRange, eFacingDirection))
			{
				if (thisRing != 0)
				{
					CvPlot* pFirstInwardPlot = NULL;
					CvPlot* pSecondInwardPlot = NULL;
					const int INVALID_RING = -1;
					const int HALF_BLOCKED = 0x01000000;
					int iRingOfFirstInwardPlot = INVALID_RING;
					int iRingOfSecondInwardPlot = INVALID_RING;
					int iFirstInwardLevel = INVALID_RING;
					int iSecondInwardLevel = INVALID_RING;
					bool bFirstHalfBlocked = false;
					bool bSecondHalfBlocked = false;

					// try to look at the two plot inwards
					switch (eDirectionOfNextTileToCheck)
					{
					case NO_DIRECTION:
						UNREACHABLE();
					case DIRECTION_NORTHEAST:
						pFirstInwardPlot = plotDirection(pPlotToCheck->getX(), pPlotToCheck->getY(), DIRECTION_EAST);
						pSecondInwardPlot = plotDirection(pPlotToCheck->getX(), pPlotToCheck->getY(), DIRECTION_SOUTHEAST);
						break;
					case DIRECTION_EAST:
						pFirstInwardPlot = plotDirection(pPlotToCheck->getX(), pPlotToCheck->getY(), DIRECTION_SOUTHWEST);
						pSecondInwardPlot = plotDirection(pPlotToCheck->getX(), pPlotToCheck->getY(), DIRECTION_SOUTHEAST);
						break;
					case DIRECTION_SOUTHEAST:
						pFirstInwardPlot = plotDirection(pPlotToCheck->getX(), pPlotToCheck->getY(), DIRECTION_SOUTHWEST);
						pSecondInwardPlot = plotDirection(pPlotToCheck->getX(), pPlotToCheck->getY(), DIRECTION_WEST);
						break;
					case DIRECTION_SOUTHWEST:
						pFirstInwardPlot = plotDirection(pPlotToCheck->getX(), pPlotToCheck->getY(), DIRECTION_WEST);
						pSecondInwardPlot = plotDirection(pPlotToCheck->getX(), pPlotToCheck->getY(), DIRECTION_NORTHWEST);
						break;
					case DIRECTION_WEST:
						pFirstInwardPlot = plotDirection(pPlotToCheck->getX(), pPlotToCheck->getY(), DIRECTION_NORTHWEST);
						pSecondInwardPlot = plotDirection(pPlotToCheck->getX(), pPlotToCheck->getY(), DIRECTION_NORTHEAST);
						break;
					case DIRECTION_NORTHWEST:
						pFirstInwardPlot = plotDirection(pPlotToCheck->getX(), pPlotToCheck->getY(), DIRECTION_EAST);
						pSecondInwardPlot = plotDirection(pPlotToCheck->getX(), pPlotToCheck->getY(), DIRECTION_NORTHEAST);
						break;
					}
					if (pFirstInwardPlot)
					{
						iRingOfFirstInwardPlot = plotDistance(getX(), getY(), pFirstInwardPlot->getX(), pFirstInwardPlot->getY());
						if (iRingOfFirstInwardPlot == thisRing - 1)
						{
							iFirstInwardLevel = scratchpad[pFirstInwardPlot->GetPlotIndex()];
							if (iFirstInwardLevel >= HALF_BLOCKED)
							{
								iFirstInwardLevel -= HALF_BLOCKED;
								bFirstHalfBlocked = true;
							}
						}
					}
					if (pSecondInwardPlot)
					{
						iRingOfSecondInwardPlot = plotDistance(getX(), getY(), pSecondInwardPlot->getX(), pSecondInwardPlot->getY());
						if (iRingOfSecondInwardPlot == thisRing - 1)
						{
							iSecondInwardLevel = scratchpad[pSecondInwardPlot->GetPlotIndex()];
							if (iSecondInwardLevel >= HALF_BLOCKED)
							{
								iSecondInwardLevel -= HALF_BLOCKED;
								bSecondHalfBlocked = true;
							}
						}
					}
					int iThisPlotLevel = pPlotToCheck->seeThroughLevel(thisRing != iRangeWithOneExtraRing);
					if (iFirstInwardLevel != INVALID_RING && iSecondInwardLevel != INVALID_RING && iFirstInwardLevel != iSecondInwardLevel && !bFirstHalfBlocked && !bSecondHalfBlocked)
					{
						double fP0X = (double)getX();
						double fP0Y = (double)getY();
						double fP1X = (double)pPlotToCheck->getX();
						double fP1Y = (double)pPlotToCheck->getY();
						if (getY() & 1)
						{
							fP0X += 0.5;
						}
						if (pPlotToCheck->getY() & 1)
						{
							fP1X += 0.5;
						}

						double a = fP1Y - fP0Y;
						double b = fP0X - fP1X;
						double c = fP0Y * fP1X - fP1Y * fP0X;

						double fFirstInwardX = (double)pFirstInwardPlot->getX();
						double fFirstInwardY = (double)pFirstInwardPlot->getY();
						if (pFirstInwardPlot->getY() & 1)
						{
							fFirstInwardX += 0.5;
						}
						double fFirstDist = a * fFirstInwardX + b * fFirstInwardY + c;
						fFirstDist = abs(fFirstDist);
						// skip the extra distance since it is the same for both equations

						double fSecondInwardX = (double)pSecondInwardPlot->getX();
						double fSecondInwardY = (double)pSecondInwardPlot->getY();
						if (pSecondInwardPlot->getY() & 1)
						{
							fSecondInwardX += 0.5;
						}
						double fSecondDist = a * fSecondInwardX + b * fSecondInwardY + c;
						fSecondDist = abs(fSecondDist);
						// skip the extra distance since it is the same for both equations

						if (fFirstDist - fSecondDist > 0.05)  // we are closer to the second point
						{
							int iHighestLevel = (iSecondInwardLevel > iThisPlotLevel) ? iSecondInwardLevel : iThisPlotLevel;
							scratchpad[pPlotToCheck->GetPlotIndex()] = iHighestLevel;
							if (iSecondInwardLevel < iThisPlotLevel || ((iCenterLevel >= iSecondInwardLevel) && (thisRing < iRangeWithOneExtraRing)))
							{
								pPlotToCheck->IncreaseKnownVisibilityCount(eTeam, eMinorCivAlly);
							}
						}
						else if (fSecondDist - fFirstDist > 0.05)   // we are closer to the first point
						{
							int iHighestLevel = (iFirstInwardLevel > iThisPlotLevel) ? iFirstInwardLevel : iThisPlotLevel;
							scratchpad[pPlotToCheck->GetPlotIndex()] = iHighestLevel;
							if (iFirstInwardLevel < iThisPlotLevel || ((iCenterLevel >= iFirstInwardLevel) && (thisRing < iRangeWithOneExtraRing)))
							{
								pPlotToCheck->IncreaseKnownVisibilityCount(eTeam, eMinorCivAlly);
							}
						}
						else
						{
							int iHighestInwardLevel = (iFirstInwardLevel > iSecondInwardLevel) ? iFirstInwardLevel : iSecondInwardLevel;
							int iLowestInwardLevel = (iFirstInwardLevel > iSecondInwardLevel) ? iSecondInwardLevel : iFirstInwardLevel;
							int iHighestLevel = (iHighestInwardLevel > iThisPlotLevel) ? iHighestInwardLevel : iThisPlotLevel;
							int iHighestLowestLevel = (iLowestInwardLevel > iThisPlotLevel) ? iLowestInwardLevel : iThisPlotLevel;
							if (iHighestInwardLevel > iThisPlotLevel)
							{
								scratchpad[pPlotToCheck->GetPlotIndex()] = iHighestLowestLevel + HALF_BLOCKED;
							}
							else
							{
								scratchpad[pPlotToCheck->GetPlotIndex()] = iHighestLevel;
							}
							if (iLowestInwardLevel < iThisPlotLevel || ((iCenterLevel >= iLowestInwardLevel) && (thisRing < iRangeWithOneExtraRing)))
							{
								pPlotToCheck->IncreaseKnownVisibilityCount(eTeam, eMinorCivAlly);
							}
						}
					}
					else if (iFirstInwardLevel != INVALID_RING && !bFirstHalfBlocked)
					{
						int iHighestLevel = (iFirstInwardLevel > iThisPlotLevel) ? iFirstInwardLevel : iThisPlotLevel;
						scratchpad[pPlotToCheck->GetPlotIndex()] = iHighestLevel;
						if (iFirstInwardLevel < iThisPlotLevel || ((iCenterLevel >= iFirstInwardLevel) && (thisRing < iRangeWithOneExtraRing)))
						{
							pPlotToCheck->IncreaseKnownVisibilityCount(eTeam, eMinorCivAlly);
						}
					}
					else if (iSecondInwardLevel != INVALID_RING && !bSecondHalfBlocked)
					{
						int iHighestLevel = (iSecondInwardLevel > iThisPlotLevel) ? iSecondInwardLevel : iThisPlotLevel;
						scratchpad[pPlotToCheck->GetPlotIndex()] = iHighestLevel;
						if (iSecondInwardLevel < iThisPlotLevel || ((iCenterLevel >= iSecondInwardLevel) && (thisRing < iRangeWithOneExtraRing)))
						{
							pPlotToCheck->IncreaseKnownVisibilityCount(eTeam, eMinorCivAlly);
						}
					}
					else if (iFirstInwardLevel != INVALID_RING)
					{
						int iHighestLevel = (iFirstInwardLevel > iThisPlotLevel) ? iFirstInwardLevel : iThisPlotLevel;
						scratchpad[pPlotToCheck->GetPlotIndex()] = iHighestLevel;
						if (iFirstInwardLevel < iThisPlotLevel || ((iCenterLevel >= iFirstInwardLevel) && (thisRing < iRangeWithOneExtraRing)))
						{
							pPlotToCheck->IncreaseKnownVisibilityCount(eTeam, eMinorCivAlly);
						}
					}
					else if (iSecondInwardLevel != INVALID_RING)
					{
						int iHighestLevel = (iSecondInwardLevel > iThisPlotLevel) ? iSecondInwardLevel : iThisPlotLevel;
						scratchpad[pPlotToCheck->GetPlotIndex()] = iHighestLevel;
						if (iSecondInwardLevel < iThisPlotLevel || ((iCenterLevel >= iSecondInwardLevel) && (thisRing < iRangeWithOneExtraRing)))
						{
							pPlotToCheck->IncreaseKnownVisibilityCount(eTeam, eMinorCivAlly);
						}
					}
					else // I have no idea how this can happen, but...
					{
						scratchpad[pPlotToCheck->GetPlotIndex()] = iThisPlotLevel;
					}
				}
				else // this is the center point
				{
					pPlotToCheck->IncreaseKnownVisibilityCount(eTeam, eMinorCivAlly);
					scratchpad[pPlotToCheck->GetPlotIndex()] = 0;
				}
			}

		}

		int iNextDX = 0;
		int iNextDY = 0;
		if (iPlotCounter >= iMaxPlotNumberOnThisRing)  // we have processed all of the plots in this ring
		{
			// if that fails go out one ring in the NE direction traveling E
			eDirectionOfNextTileToCheck = DIRECTION_NORTHEAST;
			iNextDX = iDX + GC.getPlotDirectionX()[eDirectionOfNextTileToCheck];
			iNextDY = iDY + GC.getPlotDirectionY()[eDirectionOfNextTileToCheck];
			thisRing++;
			iMaxPlotNumberOnThisRing += thisRing * 6;
			eDirectionOfNextTileToCheck = DIRECTION_EAST;
			// (if that is further out than the extended range, we are done)
		}
		else
		{
			iNextDX = iDX + GC.getPlotDirectionX()[eDirectionOfNextTileToCheck];
			iNextDY = iDY + GC.getPlotDirectionY()[eDirectionOfNextTileToCheck];
			if (hexDistance(iNextDX, iNextDY) > thisRing)
			{
				// try to turn right
				eDirectionOfNextTileToCheck = GC.getTurnRightDirection(eDirectionOfNextTileToCheck);
				iNextDX = iDX + GC.getPlotDirectionX()[eDirectionOfNextTileToCheck];
				iNextDY = iDY + GC.getPlotDirectionY()[eDirectionOfNextTileToCheck];
			}
		}

		iPlotCounter++;

		iDX = iNextDX;
		iDY = iNextDY;
		pPlotToCheck = plotXYWithRangeCheck(getX(), getY(), iDX, iDY, iRangeWithOneExtraRing);
	}

}

void CvPlot::changeEspionageSight(TeamTypes eTeam, CvCity* pCity, int iRange, bool bIncrement)
{
	//do nothing if range is negative, this is invalid
	if (iRange < 0)
		return;

	if (iRange == 0)
	{
		//change the visibility of this plot only
		changeVisibilityCount(eTeam, ((bIncrement) ? 1 : -1), NO_INVISIBLE, true, false);
		return;
	}

	for (int iI = RING0_PLOTS; iI < RING_PLOTS[iRange]; iI++)
	{
		CvPlot* pLoopPlot = pCity->GetCityCitizens()->GetCityPlotFromIndex(iI);
		if (!pLoopPlot)
			continue;

		//if it belongs to the city
		if (pCity->GetID() == pLoopPlot->getOwningCityID())
		{
			pLoopPlot->changeVisibilityCount(eTeam, ((bIncrement) ? 1 : -1), NO_INVISIBLE, true, false);
		}	
	}
}

//	--------------------------------------------------------------------------------
bool CvPlot::canSeePlot(const CvPlot* pPlot, TeamTypes eTeam, int iRange, DirectionTypes eFacingDirection) const
{
	if(pPlot == NULL)
	{
		return false;
	}

	if(pPlot == this)
	{
		return true;
	}

	int startX = getX();
	int startY = getY();
	int destX = pPlot->getX();
	int destY = pPlot->getY();

	int iDistance = plotDistance(startX, startY, destX,  destY);
	if(iDistance <= iRange)
	{
		//find displacement
		int dy = destY - startY;

		int iX1 = xToHexspaceX(destX,  destY);
		int iX2 = xToHexspaceX(startX, startY);
		 
		int dx = iX1 - iX2;

		dx = dxWrap(dx); //world wrap
		dy = dyWrap(dy);

		//check if in facing direction
		if(shouldProcessDisplacementPlot(dx, dy, iRange, eFacingDirection))
		{
			if(iDistance == 1)
			{
				return true;
			}

			//check if anything blocking the plot
			if (CvTargeting::CanSeeDisplacementPlot(startX, startY, dx, dy, seeFromLevel(eTeam)))
			{
				return true;
			}
		}
	}

	return false;
}

//	--------------------------------------------------------------------------------
bool CvPlot::shouldProcessDisplacementPlot(int dx, int dy, int, DirectionTypes eFacingDirection) const
{
	if(eFacingDirection == NO_DIRECTION)
	{
		return true;
	}
	else if((dx == 0) && (dy == 0)) //always process this plot
	{
		return true;
	}
	else
	{
		//							NE					E		SE					SW					W		NW
		double displacements[6][2] = { {0.5f, 0.866025f}, {1, 0}, {0.5f, -0.866025f}, {-0.5f, -0.866025f}, {-1, 0}, {-0.5f, -0.866025f}};

		double directionX = displacements[eFacingDirection][0];
		double directionY = displacements[eFacingDirection][1];

		//compute angle off of direction
		double crossProduct = directionX * dy - directionY * dx; //cross product
		double dotProduct = directionX * dx + directionY * dy; //dot product

		double theta = atan2(crossProduct, dotProduct);
		double spread = 75 * (double) M_PI / 180;
		if((abs(dx) <= 1) && (abs(dy) <= 1)) //close plots use wider spread
		{
			spread = 90 * (double) M_PI / 180;
		}

		return (theta >= -spread / 2) && (theta <= spread / 2);
	}
}

//	--------------------------------------------------------------------------------
void CvPlot::updateSight(bool bIncrement)
{
	IDInfo* pUnitNode = NULL;
	CvUnit* pLoopUnit = NULL;
	int iLoop = 0;
	int iI = 0;

	// Owned
	if(isOwned())
	{
		changeAdjacentSight(getTeam(), /*1*/ GD_INT_GET(PLOT_VISIBILITY_RANGE), bIncrement, NO_INVISIBLE, NO_DIRECTION);

		// if this tile is owned by a minor share the visibility with my ally
		PlayerTypes ownerID = getOwner();
		if(ownerID >= MAX_MAJOR_CIVS && ownerID != BARBARIAN_PLAYER)
		{
			CvPlayer& thisPlayer = GET_PLAYER(ownerID);
			CvMinorCivAI* pMinorCivAI = thisPlayer.GetMinorCivAI();
			if(pMinorCivAI && pMinorCivAI->GetAlly() != NO_PLAYER)
			{
				changeAdjacentSight(GET_PLAYER(pMinorCivAI->GetAlly()).getTeam(), /*1*/ GD_INT_GET(PLOT_VISIBILITY_RANGE), bIncrement, NO_INVISIBLE, NO_DIRECTION);
			}
		}
	}

	pUnitNode = headUnitNode();

	// Unit
	while(pUnitNode != NULL)
	{
		pLoopUnit = GetPlayerUnit(*pUnitNode);
		pUnitNode = nextUnitNode(pUnitNode);

		if(pLoopUnit)
		{
			if (pLoopUnit->canChangeVisibility())
				changeAdjacentSight(pLoopUnit->getTeam(), pLoopUnit->visibilityRange(), bIncrement, pLoopUnit->getSeeInvisibleType(), pLoopUnit->getFacingDirection(true), pLoopUnit);
		}
	}

	if(getReconCount() > 0)
	{
		for(iI = 0; iI < MAX_PLAYERS; ++iI)
		{
			for(pLoopUnit = GET_PLAYER((PlayerTypes)iI).firstUnit(&iLoop); pLoopUnit != NULL; pLoopUnit = GET_PLAYER((PlayerTypes)iI).nextUnit(&iLoop))
			{
				if(pLoopUnit->getReconPlot() == this && pLoopUnit->canChangeVisibility())
				{
					changeAdjacentSight(pLoopUnit->getTeam(), pLoopUnit->reconRange(), bIncrement, pLoopUnit->getSeeInvisibleType(), pLoopUnit->getFacingDirection(true), pLoopUnit);
				}
			}
		}
	}
}


//	--------------------------------------------------------------------------------
void CvPlot::updateSeeFromSight(bool bIncrement, bool bRecalculate)
{
	CvPlot* pLoopPlot = NULL;
	int iDX = 0;
	int iDY = 0;

	int iRange = 1 + /*1*/ GD_INT_GET(UNIT_VISIBILITY_RANGE);
#if defined(MOD_PROMOTIONS_VARIABLE_RECON)
	int iReconRange = 1 + /*6*/ GD_INT_GET(RECON_VISIBILITY_RANGE);
#endif
	for(int iPromotion = 0; iPromotion < GC.getNumPromotionInfos(); ++iPromotion)
	{
		const PromotionTypes ePromotion = static_cast<PromotionTypes>(iPromotion);
		CvPromotionEntry* pkPromotionInfo = GC.getPromotionInfo(ePromotion);
		if(pkPromotionInfo)
		{
			iRange += pkPromotionInfo->GetVisibilityChange();
#if defined(MOD_PROMOTIONS_VARIABLE_RECON)
			iReconRange += std::max(0, pkPromotionInfo->GetReconChange());
#endif
		}
	}

	iRange = std::max(iReconRange, iRange);
	iRange = std::min(8, iRange); // I don't care, I'm not looking more than 8 out, deal

	for(iDX = -iRange; iDX <= iRange; iDX++)
	{
		for(iDY = -iRange; iDY <= iRange; iDY++)
		{
			pLoopPlot = plotXYWithRangeCheck(getX(), getY(), iDX, iDY, iRange);

			if(pLoopPlot != NULL)
			{
				pLoopPlot->updateSight(bIncrement);

#if defined(MOD_BALANCE_CORE)
				//hack: don't do this during map generation
				if (bRecalculate && GC.getGame().getElapsedGameTurns()>0)
					GC.getMap().LineOfSightChanged(pLoopPlot);
#endif
			}
		}
	}
}


//	--------------------------------------------------------------------------------
bool CvPlot::canHaveResource(ResourceTypes eResource, bool bIgnoreLatitude, bool bIgnoreCiv) const
{
	ASSERT_DEBUG(getTerrainType() != NO_TERRAIN, "TerrainType is not assigned a valid value");

	if(eResource == NO_RESOURCE)
	{
		return true;
	}

	if(getResourceType() != NO_RESOURCE)
	{
		return false;
	}

	if(isMountain())
	{
		return false;
	}

	CvResourceInfo& thisResourceInfo = *GC.getResourceInfo(eResource);

	if(getFeatureType() != NO_FEATURE)
	{
		if(!(thisResourceInfo.isFeature(getFeatureType())))
		{
			return false;
		}

		if(!(thisResourceInfo.isFeatureTerrain(getTerrainType())))
		{
			return false;
		}
	}
	else
	{
		if(!(thisResourceInfo.isTerrain(getTerrainType())))
		{
			return false;
		}
	}

	if(isHills())
	{
		if(!(thisResourceInfo.isHills()))
		{
			return false;
		}
	}
	else if(isFlatlands())
	{
		if(!(thisResourceInfo.isFlatlands()))
		{
			return false;
		}
	}

	if(thisResourceInfo.isNoRiverSide())
	{
		if(isRiver())
		{
			return false;
		}
	}

	if(thisResourceInfo.getMinAreaSize() != -1)
	{
		//better check landmass size, one landmass may have multiple areas
		if(landmass()->getNumTiles() < thisResourceInfo.getMinAreaSize())
		{
			return false;
		}
	}

	if(!bIgnoreCiv && thisResourceInfo.isOnlyMinorCivs())
	{
		return false;
	}

	if (!bIgnoreCiv && thisResourceInfo.GetRequiredCivilization() != NO_CIVILIZATION)
	{
		return false;
	}

	if(!bIgnoreLatitude)
	{
		if(getLatitude() > thisResourceInfo.getMaxLatitude())
		{
			return false;
		}

		if(getLatitude() < thisResourceInfo.getMinLatitude())
		{
			return false;
		}
	}

	if(!isPotentialCityWork())
	{
		return false;
	}

	TerrainTypes eShallowWater = (TerrainTypes) GD_INT_GET(SHALLOW_WATER_TERRAIN);
	if(getTerrainType() == eShallowWater)
	{
		if(!isAdjacentToLand(false))
		{
			return false;
		}
	}


	return true;
}

//	--------------------------------------------------------------------------------
bool CvPlot::CanSpawnResource(PlayerTypes ePlayer, bool bIgnoreTech, bool bIsLand) const
{
	if (isCity())
		return false;

	if (!isValidMovePlot(ePlayer))
		return false;

	if (bIsLand && isWater())
		return false;

	if (!bIsLand && !isWater())
		return false;

	if (IsNaturalWonder())
		return false;

	if (isMountain())
		return false;

	if (getFeatureType() == FEATURE_OASIS)
		return false;

	if (bIgnoreTech && getResourceType() != NO_RESOURCE)
		return false;

	if (!bIgnoreTech && getResourceType(GET_PLAYER(ePlayer).getTeam()) != NO_RESOURCE)
		return false;

	return true;
}

//	--------------------------------------------------------------------------------
bool CvPlot::canHaveImprovement(ImprovementTypes eImprovement, PlayerTypes ePlayer, bool, bool bCheckAdjacency, bool bTestXAdjacent) const
{
	CvPlot* pLoopPlot = NULL;
	bool bValid = false;
	int iI = 0;

	CvImprovementEntry* pkImprovementInfo = GC.getImprovementInfo(eImprovement);
	if(pkImprovementInfo == NULL)
	{
		return false;
	}

#if defined(MOD_GLOBAL_ALPINE_PASSES)
	if (MOD_GLOBAL_ALPINE_PASSES && pkImprovementInfo->IsMountainsMakesValid() && isMountain())
	{
		return true;
	}
#endif

	if(getFeatureType() != NO_FEATURE)
	{
		if (pkImprovementInfo->GetCreatedFeature() != NO_FEATURE && getFeatureType() == pkImprovementInfo->GetCreatedFeature())
		{
			return false;
		}
		if (GC.getFeatureInfo(getFeatureType())->IsNaturalWonder())
		{
			return false;
		}
	}

	bValid = false;

	if(isCity())
	{
		return false;
	}

	if(!isValidMovePlot(ePlayer))
	{
		return false;
	}

	if(pkImprovementInfo->IsWater() != isWater())
	{
		return false;
	}

	if(getFeatureType() != NO_FEATURE)
	{
		if(GC.getFeatureInfo(getFeatureType())->isNoImprovement())
		{
			return false;
		}
	}

	ResourceTypes thisResource = getResourceType( ePlayer!=NO_PLAYER ? GET_PLAYER(ePlayer).getTeam() : NO_TEAM );
	// The functionality of this line is different in Civ 4: in that game a "Valid" Resource ALLOWS an Improvement on a Tile.  In Civ 5 this makes a Resource REQUIRE a certain Improvement
	if(thisResource != NO_RESOURCE)
	{
		// If there IS a valid resource here then set validity to true
		if(pkImprovementInfo->IsImprovementResourceMakesValid(thisResource))
		{
			bValid = true;
		}
		// Some improvements can ignore resource requirements, but otherwise not satisfying the requirements is an automatic fail
		else if (!pkImprovementInfo->IsBuildableOnResources())
		{
			return false;
		}
	}
	else
	{
		// If the improvement requires a resource, and there is none here, then it's an automatic fail
		if (pkImprovementInfo->IsRequiresResource())
		{
			return false;
		}
	}

	if(pkImprovementInfo->IsNoAdjacentCity() && IsAdjacentCity())
	{
		return false;
	}

	if(pkImprovementInfo->IsNoFreshWater() && isFreshWater())
	{
		return false;
	}

	if(pkImprovementInfo->IsRequiresFlatlands() && !isFlatlands())
	{
		return false;
	}

	if(pkImprovementInfo->IsRequiresFlatlandsOrFreshWater() && !isFlatlands() && !isFreshWater())
	{
		return false;
	}

	if(pkImprovementInfo->IsRequiresFeature() && (getFeatureType() == NO_FEATURE))
	{

		//Polder-specific code for lakes
		bool bLake = false;
		if (MOD_BALANCE_CORE && pkImprovementInfo->IsAdjacentLake())
		{
			for(iI = 0; iI < NUM_DIRECTION_TYPES; ++iI)
			{
				pLoopPlot = plotDirection(getX(), getY(), ((DirectionTypes)iI));

				if(pLoopPlot != NULL)
				{
					if(pLoopPlot->isLake())
					{
						bLake = true;
						break;
					}
				}
			}
		}
		if (!bLake)
			return false;
	}

	if (MOD_BALANCE_CORE && pkImprovementInfo->IsAdjacentLake())
	{
		for(iI = 0; iI < NUM_DIRECTION_TYPES; ++iI)
		{
			pLoopPlot = plotDirection(getX(), getY(), ((DirectionTypes)iI));

			if(pLoopPlot != NULL)
			{
				if(pLoopPlot->isLake())
				{
					bValid = true;
					break;
				}
			}
		}
	}

	if(pkImprovementInfo->IsRequiresImprovement())
	{
		if (getImprovementType() == NO_IMPROVEMENT)
		{
			return false;
		}
		else
		{
			bValid = false;
		}
	}

	if(pkImprovementInfo->IsCoastal() && !isCoastalLand(/*10*/ GD_INT_GET(MIN_WATER_SIZE_FOR_OCEAN)))
	{
		return false;
	}

	if(pkImprovementInfo->IsCoastMakesValid() && (isWater() && !isLake()))
	{
		bValid = true;
	}

	if(pkImprovementInfo->IsAdjacentCity() && IsAdjacentCity())
	{
		bValid = true;
	}

	if(pkImprovementInfo->IsHillsMakesValid() && isHills())
	{
		bValid = true;
	}

	if(pkImprovementInfo->IsWaterAdjacencyMakesValid())
	{
		if (isCoastalLand() || isFreshWater() || isRiver()) 
		{
			bValid = true;
		}
	}

	if(pkImprovementInfo->IsFreshWaterMakesValid() && isFreshWater())
	{
		bValid = true;
	}

	if(pkImprovementInfo->IsRiverSideMakesValid() && isRiver())
	{
		bValid = true;
	}

	if(pkImprovementInfo->GetTerrainMakesValid(getTerrainType()))
	{
		bValid = true;
	}

	if((getImprovementType() != NO_IMPROVEMENT) && pkImprovementInfo->GetImprovementMakesValid(getImprovementType()))
	{
		bValid = true;
	}

	if((getFeatureType() != NO_FEATURE) && pkImprovementInfo->GetFeatureMakesValid(getFeatureType()))
	{
		bValid = true;
	}

	int iAdjacentSameImprovementMakesValid = pkImprovementInfo->GetXSameAdjacentMakesValid();
	if (iAdjacentSameImprovementMakesValid > 0)
	{
		int iAdjacentSameImprovement = 0;

		if (bTestXAdjacent)
		{
			bValid = true;
		}
		else
		{
			for (iI = 0; iI < NUM_DIRECTION_TYPES; ++iI)
			{
				pLoopPlot = plotDirection(getX(), getY(), ((DirectionTypes)iI));

				if (pLoopPlot && pLoopPlot->getImprovementType() == eImprovement)
				{
					iAdjacentSameImprovement++;

					if (iAdjacentSameImprovement >= iAdjacentSameImprovementMakesValid)
					{
						bValid = true;
						break;
					}
				}
			}
		}
	}

	if (!bValid)
	{
		return false;
	}

	if(pkImprovementInfo->IsRiverSideMakesValid())
	{
		bValid = false;

		for(iI = 0; iI < NUM_DIRECTION_TYPES; ++iI)
		{
			pLoopPlot = plotDirection(getX(), getY(), ((DirectionTypes)iI));

			if(pLoopPlot != NULL)
			{
				if(isRiverCrossing(directionXY(this, pLoopPlot)))
				{
					if(pLoopPlot->getImprovementType() != eImprovement)
					{
						bValid = true;
						break;
					}
				}
			}
		}

		if(!bValid)
		{
			return false;
		}
	}

	int iRequiredAdjacentLand = pkImprovementInfo->GetRequiresXAdjacentLand();
	if (iRequiredAdjacentLand > -1)
	{
		int iAdjacentLand = 0;

		for(iI = 0; iI < NUM_DIRECTION_TYPES; ++iI)
		{
			pLoopPlot = plotDirection(getX(), getY(), ((DirectionTypes)iI));

			if(pLoopPlot != NULL)
			{
				if (!pLoopPlot->isWater())
				{
					iAdjacentLand++;
				}
			}
		}

		if (iAdjacentLand < iRequiredAdjacentLand)
		{
			return false;
		}
	}

	int iRequiredAdjacentWater = pkImprovementInfo->GetRequiresXAdjacentWater();
	if (iRequiredAdjacentWater > -1)
	{
		int iAdjacentWater = 0;

		for(iI = 0; iI < NUM_DIRECTION_TYPES; ++iI)
		{
			pLoopPlot = plotDirection(getX(), getY(), ((DirectionTypes)iI));

			if(pLoopPlot != NULL)
			{
				if (pLoopPlot->isWater())
				{
					iAdjacentWater++;
				}
			}
		}

		if (iAdjacentWater < iRequiredAdjacentWater)
		{
			return false;
		}
	}

	for(iI = 0; iI < NUM_YIELD_TYPES; ++iI)
	{
		//Simplification - errata yields not worth considering.
		if ((YieldTypes)iI > YIELD_CULTURE_LOCAL && !MOD_BALANCE_CORE_JFD)
			break;

		if (pkImprovementInfo->GetPrereqNatureYield(iI) > 0 && calculateNatureYield(((YieldTypes)iI), ePlayer, getFeatureType(), getResourceType(GET_PLAYER(ePlayer).getTeam()), NULL) < pkImprovementInfo->GetPrereqNatureYield(iI))
		{
			return false;
		}
	}

	if (MOD_EVENTS_PLOT)
	{
		if (GAMEEVENTINVOKE_TESTALL(GAMEEVENT_PlotCanImprove, getX(), getY(), eImprovement) == GAMEEVENTRETURN_FALSE)
			return false;
	}

	if (!bCheckAdjacency)
		return true;

	bool bHasLuxuryRequirement = pkImprovementInfo->IsAdjacentLuxury();
	bool bHasNoTwoAdjacencyRequirement = pkImprovementInfo->IsNoTwoAdjacent();
	if (bHasLuxuryRequirement || bHasNoTwoAdjacencyRequirement)
	{
		bool bLuxuryRequirementMet = !bHasLuxuryRequirement;
		for (int iI = 0; iI < NUM_DIRECTION_TYPES; ++iI)
		{
			CvPlot* pAdjacentPlot = plotDirection(getX(), getY(), ((DirectionTypes)iI));
			if (pAdjacentPlot != NULL)
			{
				if (bHasLuxuryRequirement)
				{
					ResourceTypes eResource = pAdjacentPlot->getResourceType(GET_PLAYER(ePlayer).getTeam());
					if (eResource != NO_RESOURCE)
					{
						CvResourceInfo* pkResourceInfo = GC.getResourceInfo(eResource);
						if (pkResourceInfo && pkResourceInfo->getResourceUsage() == RESOURCEUSAGE_LUXURY)
						{
							bLuxuryRequirementMet = true;
						}
					}
				}
				if (bHasNoTwoAdjacencyRequirement)
				{
					ImprovementTypes eAdjacentImprovement = pAdjacentPlot->getImprovementType();
					if (eAdjacentImprovement != NO_IMPROVEMENT)
					{
						if (eAdjacentImprovement == eImprovement)
						{
							return false;
						}

						CvImprovementEntry* pkImprovement2 = GC.getImprovementInfo(eAdjacentImprovement);
						if (pkImprovement2->GetImprovementMakesValid(eImprovement))
						{
							return false;
						}
					}

					int iBuildProgress = pAdjacentPlot->getBuildProgress(GetBuildTypeFromImprovement(eImprovement));
					if (iBuildProgress > 0)
					{
						return false;
					}
				}
			}
		}
		if (bHasLuxuryRequirement && !bLuxuryRequirementMet)
		{
			return false;
		}
	}

	return true;
}

BuildTypes CvPlot::GetBuildTypeFromImprovement(ImprovementTypes eImprovement) const
{
	for (int iBuildIndex = 0; iBuildIndex < GC.getNumBuildInfos(); iBuildIndex++)
	{
		BuildTypes eBuild = (BuildTypes)iBuildIndex;
		CvBuildInfo* pkBuild = GC.getBuildInfo(eBuild);

		if (NULL != pkBuild && eImprovement == (ImprovementTypes)pkBuild->getImprovement())
		{
			return eBuild;
		}
	}

	return NO_BUILD;
}


//	--------------------------------------------------------------------------------
bool CvPlot::canBuild(BuildTypes eBuild, PlayerTypes ePlayer, bool bTestVisible, bool bTestPlotOwner, bool bTestXAdjacent) const
{
	static const ImprovementTypes eFeitoria = (ImprovementTypes)GC.getInfoTypeForString("IMPROVEMENT_FEITORIA");
	TeamTypes eTeam = GET_PLAYER(ePlayer).getTeam();

	ImprovementTypes eImprovement;
	ImprovementTypes eFinalImprovementType;
	RouteTypes eRoute;
	bool bValid = false;

	// Can't build nothing!
	if(eBuild == NO_BUILD)
	{
		return false;
	}

	// Repairing an Improvement that's been pillaged
	CvBuildInfo& thisBuildInfo = *GC.getBuildInfo(eBuild);
	if(thisBuildInfo.isRepair())
	{
#if defined(MOD_NO_REPAIR_FOREIGN_LANDS)
		if(MOD_NO_REPAIR_FOREIGN_LANDS)
		{
			//Can't repair outside of owned territory.
			if(ePlayer != NO_PLAYER && getOwner() != NO_PLAYER && getOwner() != ePlayer)
			{
				return false;
			}
		}
#endif
		if(IsImprovementPillaged() || IsRoutePillaged())
		{
			bValid = true;
		}
		else
		{
			return false;
		}
	}
	if(thisBuildInfo.IsRemoveRoute())
	{
		if(!getPlotCity() && getRouteType() != NO_ROUTE)
		{
			if(getOwner() == ePlayer)
			{
				bValid = true;
			}
			else if(getOwner() == NO_PLAYER && GetPlayerResponsibleForRoute() == ePlayer)
			{
				bValid = true;
			}
			else if(getOwner() == NO_PLAYER && (GetPlayerResponsibleForRoute() == NO_PLAYER || !GET_PLAYER(GetPlayerResponsibleForRoute()).isAlive()))
			{
				bValid = true;
			}
			else
			{
				return false;
			}
		}
		else if(isCity())
		{
			return false;
		}
		else if(getRouteType() != NO_ROUTE)
		{
			if(getOwner() == ePlayer)
			{
				bValid = true;
			}
			else if(getOwner() == NO_PLAYER && GetPlayerResponsibleForRoute() == ePlayer)
			{
				bValid = true;
			}
			else if(getOwner() == NO_PLAYER && (GetPlayerResponsibleForRoute() == NO_PLAYER || !GET_PLAYER(GetPlayerResponsibleForRoute()).isAlive()))
			{
				bValid = true;
			}
		}
	}

	eImprovement = ((ImprovementTypes)(thisBuildInfo.getImprovement()));

	// Improvement
	if(eImprovement != NO_IMPROVEMENT)
	{
		// Player must be able to build this Improvement
		if(!canHaveImprovement(eImprovement, ePlayer, bTestVisible, false, bTestXAdjacent))
		{
			return false;
		}

		// Already an improvement here
		if(getImprovementType() != NO_IMPROVEMENT)
		{

			if(GC.getImprovementInfo(getImprovementType())->IsPermanent())
			{
				return false;
			}

			if(getImprovementType() == eImprovement)
			{
				return false;
			}

			if (eFeitoria != NO_IMPROVEMENT && getImprovementType() == eFeitoria && getOwner() != NO_PLAYER && GET_PLAYER(getOwner()).isMinorCiv())
				return false;

			eFinalImprovementType = finalImprovementUpgrade(getImprovementType());

			if(eFinalImprovementType != NO_IMPROVEMENT)
			{
				if(eFinalImprovementType == finalImprovementUpgrade(eImprovement))
				{
					if(!IsImprovementPillaged())
					{
						return false;
					}
				}
			}
		}

		// Requirements on adjacent plots?
		if (!bTestVisible)
		{
			CvImprovementEntry *pkImprovement = GC.getImprovementInfo(eImprovement);
			bool bHasLuxuryRequirement = pkImprovement->IsAdjacentLuxury();
			bool bHasNoAdjacencyRequirement = pkImprovement->IsNoTwoAdjacent();
			if (pkImprovement && (bHasLuxuryRequirement || bHasNoAdjacencyRequirement))
			{
				bool bLuxuryRequirementMet = !bHasLuxuryRequirement;
				for (int iI = 0; iI < NUM_DIRECTION_TYPES; ++iI)
				{
					CvPlot *pAdjacentPlot = plotDirection(getX(), getY(), ((DirectionTypes)iI));
					if (pAdjacentPlot != NULL)
					{
						if (bHasLuxuryRequirement)
						{
							ResourceTypes eResource = pAdjacentPlot->getResourceType(eTeam);
							if (eResource != NO_RESOURCE)
							{
								CvResourceInfo *pkResourceInfo = GC.getResourceInfo(eResource);
								if (pkResourceInfo && pkResourceInfo->getResourceUsage() == RESOURCEUSAGE_LUXURY)
								{
									bLuxuryRequirementMet = true;
								}
							}
						}
						if (bHasNoAdjacencyRequirement)
						{
							ImprovementTypes eAdjacentImprovement =  pAdjacentPlot->getImprovementType();
							if (eAdjacentImprovement != NO_IMPROVEMENT)
							{
								if (eAdjacentImprovement == eImprovement)
								{
									return false;
								}
								CvImprovementEntry* pkImprovement2 = GC.getImprovementInfo(eAdjacentImprovement);
								if (pkImprovement2 && pkImprovement2->GetImprovementMakesValid(eImprovement))
								{
									return false;
								}
							}
							int iBuildProgress = pAdjacentPlot->getBuildProgress(eBuild);
							if (iBuildProgress > 0)
							{
								return false;
							}
						}
					}
				}
				if (bHasLuxuryRequirement && !bLuxuryRequirementMet)
				{
					return false;
				}
			}
		}

		if(!bTestVisible)
		{
			if(!GC.getImprovementInfo(eImprovement)->IsIgnoreOwnership())
			{
				// Gifts for minors can ignore borders requirements
				if(bTestPlotOwner)
				{
					// Outside Borders - Can be built in or outside our lands, but not in other lands
					if(GC.getImprovementInfo(eImprovement)->IsOutsideBorders())
					{
						if (getTeam() != eTeam && getTeam() != NO_TEAM)
						{
							return false;
						}
					}
					// In Adjacent Friendly - Can be built in or adjacent to our lands
					else if (GC.getImprovementInfo(eImprovement)->IsInAdjacentFriendly())
					{
						//citadels only in adjacent _unowned_ territory
						if (getTeam() == NO_TEAM)
						{
							if (!isAdjacentTeam(eTeam, false))
								return false;
						}
						else if (getTeam() != eTeam)
							return false;
					}
					// Only City State Territory - Can only be built in City-State territory (not our own lands)
					else if (GC.getImprovementInfo(eImprovement)->IsOnlyCityStateTerritory())
					{
						if (isOwned() && GET_PLAYER(getOwner()).isMinorCiv())
						{
							// If this is an embassy, check for existing embassies
							if (GC.getImprovementInfo(eImprovement)->IsEmbassy())
							{
								for (int iI = 0; iI < GC.getNumImprovementInfos(); iI++)
								{
									ImprovementTypes eOtherEmbassyImprovement = (ImprovementTypes)iI;
									if (!GC.getImprovementInfo(eOtherEmbassyImprovement)->IsEmbassy())
										continue;

									if (GET_PLAYER(getOwner()).getImprovementCount(eOtherEmbassyImprovement, false) > 0)
										return false;
								}
							}
						}
						else
							return false;
					}
					else if(getTeam() != eTeam) 
					{//only buildable in own culture
						return false;
					}
				}
			}
		}

		bValid = true;
	}

	eRoute = ((RouteTypes)(GC.getBuildInfo(eBuild)->getRoute()));

	// Route
	if(eRoute != NO_ROUTE)
	{
		// can't build roads in cities
		if (isCity())
			return false;

		if(getRouteType() != NO_ROUTE)
		{
			if (isWater() && !thisBuildInfo.IsWater())
			{
				return false;
			}

			if(eRoute == getRouteType())
			{
				if(!IsRoutePillaged())
				{
					return false;
				}
			}
		}

		//can't build roads in enemy territory and saddle them with the maintenance
		if (getOwner() != NO_PLAYER && ePlayer != NO_PLAYER)
		{
			CvPlayer& kOwner = GET_PLAYER(getOwner());
			if (kOwner.isMajorCiv() && kOwner.getTeam() != GET_PLAYER(ePlayer).getTeam())
			{
				if (!GET_TEAM(kOwner.getTeam()).IsVassal(GET_PLAYER(ePlayer).getTeam()))
					return false;
			}
		}

		bValid = true;
	}

	// In general, cannot clear features in the lands of players that aren't on our team or that we're at war with
	if(getFeatureType() != NO_FEATURE)
	{
		if(GC.getBuildInfo(eBuild)->isFeatureRemove(getFeatureType()))
		{
#if defined(MOD_BALANCE_CORE)
			if(getFeatureType() == FEATURE_FALLOUT && GC.getBuildInfo(eBuild)->isFeatureRemove(FEATURE_FALLOUT))
			{
				bValid = true;
			}
			else
#endif
			if(bTestPlotOwner)
			{
				if(isOwned() && (eTeam != getTeam()) && !atWar(eTeam, getTeam()))
				{
					if (eImprovement == NO_IMPROVEMENT)
					{
						return false;
					}

					// Some improvements are exceptions
					if (!GC.getImprovementInfo(eImprovement)->IsIgnoreOwnership() &&
						!GC.getImprovementInfo(eImprovement)->IsOnlyCityStateTerritory())
					{
						return false;
					}
				}
			}

			bValid = true;
		}
	}

	return bValid;
}


//	--------------------------------------------------------------------------------
int CvPlot::getBuildTime(BuildTypes eBuild, PlayerTypes ePlayer) const
{
	if (MOD_CIV6_WORKER)
	{
		return 0;
	}

	ASSERT_DEBUG(getTerrainType() != NO_TERRAIN, "TerrainType is not assigned a valid value");

	CvBuildInfo* pkBuildInfo = GC.getBuildInfo(eBuild);
	int iTime = pkBuildInfo->getTime();
	if (ePlayer != NO_PLAYER)
	{
		TeamTypes eTeam = GET_PLAYER(ePlayer).getTeam();
		if (eTeam != NO_TEAM)
		{
			iTime += GET_TEAM(eTeam).getBuildTimeChange(eBuild);
		}
	}

	if(getFeatureType() != NO_FEATURE)
	{
		iTime += pkBuildInfo->getFeatureTime(getFeatureType());
	}

	// If repair, take the shorter time of repair and improve (but not for builds that kill the builder)
	// We keep track of the improve time first, then compare at the end after all multipliers
	if (pkBuildInfo->isRepair())
	{
		int iImproveTime = -1;
		RouteTypes eRoute = getRouteType();
		ImprovementTypes eImprovement = getImprovementType();

		for (int iBuildLoop = 0; iBuildLoop < GC.getNumBuildInfos(); iBuildLoop++)
		{
			BuildTypes eBuild = static_cast<BuildTypes>(iBuildLoop);
			CvBuildInfo* pkBuildInfoLoop = GC.getBuildInfo(eBuild);
			if (!pkBuildInfoLoop)
				continue;

			if (IsImprovementPillaged())
			{
				if (pkBuildInfoLoop->getImprovement() == eImprovement)
				{
					if (pkBuildInfoLoop->isKill())
						continue;

					iImproveTime = pkBuildInfoLoop->getTime();
					break;
				}
			}
			else if (IsRoutePillaged())
			{
				if (pkBuildInfoLoop->getRoute() == eRoute)
				{
					if (pkBuildInfoLoop->isKill())
						continue;

					iImproveTime = pkBuildInfoLoop->getTime();
					break;
				}
			}
		}

		if (iImproveTime != -1)
		{
			// Repair action is not affected by game speed, but improve does
			iImproveTime *= GC.getGame().getGameSpeedInfo().getBuildPercent();
			iImproveTime /= 100;

			iTime = min(iTime, iImproveTime);
		}
	}
	else
	{
		iTime *= GC.getGame().getGameSpeedInfo().getBuildPercent();
		iTime /= 100;
	}

	iTime *= std::max(0, (GC.getTerrainInfo(getTerrainType())->getBuildModifier() + 100));
	iTime /= 100;

	iTime *= GC.getGame().getStartEraInfo().getBuildPercent();
	iTime /= 100;

	return iTime;
}


//	--------------------------------------------------------------------------------
int CvPlot::getBuildTurnsLeft(BuildTypes eBuild, PlayerTypes ePlayer, int iNowExtra, int iThenExtra) const
{
	int iBuildLeft = getBuildTime(eBuild, ePlayer);
	if(iBuildLeft == 0)
		return 0;

	int iNowBuildRate = iNowExtra;
	int iThenBuildRate = iThenExtra;

	const IDInfo* pUnitNode = headUnitNode();
	while(pUnitNode != NULL)
	{
		const CvUnit* pLoopUnit = GetPlayerUnit(*pUnitNode);
		pUnitNode = nextUnitNode(pUnitNode);

		if(pLoopUnit && pLoopUnit->getBuildType() == eBuild)
		{
			if(pLoopUnit->canMove())
			{
				iNowBuildRate += pLoopUnit->workRate(false);
			}
			iThenBuildRate += pLoopUnit->workRate(true);
		}
	}

	if(iThenBuildRate == 0)
	{
		//this means it will take forever under current circumstances
		return INT_MAX;
	}

	iBuildLeft -= getBuildProgress(eBuild);
	iBuildLeft -= iNowBuildRate;
	iBuildLeft = std::max(0, iBuildLeft);

	int iTurnsLeft = (iBuildLeft / iThenBuildRate);
	//round up
	if(iTurnsLeft * iThenBuildRate < iBuildLeft)
		iTurnsLeft++;

	return iTurnsLeft;
}


//	--------------------------------------------------------------------------------
int CvPlot::getBuildTurnsTotal(BuildTypes eBuild, PlayerTypes ePlayer) const
{
	int iBuildRate = 0;
	int iBuildTime = getBuildTime(eBuild, ePlayer);

	const IDInfo* pUnitNode = headUnitNode();
	while (pUnitNode)
	{
		const CvUnit* pLoopUnit = GetPlayerUnit(*pUnitNode);
		pUnitNode = nextUnitNode(pUnitNode);

		if (pLoopUnit && pLoopUnit->getBuildType() == eBuild)
			iBuildRate += pLoopUnit->workRate(true);
	}

	if (iBuildRate == 0)
	{
		//this means it will take forever under current circumstances
		return INT_MAX;
	}

	iBuildTime = std::max(1, iBuildTime);

	// Rounds up
	return (iBuildTime - 1) / iBuildRate + 1;
}


//	--------------------------------------------------------------------------------
int CvPlot::getFeatureProduction(BuildTypes eBuild, PlayerTypes ePlayer, CvCity** ppCity) const
{
	int iProduction = 0;

	TeamTypes eTeam = GET_PLAYER(ePlayer).getTeam();

	if(getFeatureType() == NO_FEATURE)
	{
		return 0;
	}

	CvCity* pEffectiveOwningCity = getEffectiveOwningCity();

	if(pEffectiveOwningCity == NULL)
	{
		pEffectiveOwningCity = GC.getMap().findCity(getX(), getY(), NO_PLAYER, eTeam, false);
	}

	if (ppCity)
	{
		*ppCity = pEffectiveOwningCity;
	}

	if(pEffectiveOwningCity == NULL)
	{
		return 0;
	}

	// Base value
	if(GET_PLAYER(ePlayer).GetAllFeatureProduction() > 0)
	{
		iProduction = GET_PLAYER(ePlayer).GetAllFeatureProduction();
	}
	else
	{
		iProduction = GC.getBuildInfo(eBuild)->getFeatureProduction(getFeatureType());
	}

	if (MOD_BALANCE_CORE_SETTLER_ADVANCED)
	{
		iProduction *= std::max(0, (GET_PLAYER(ePlayer).getFeatureProductionModifier()));
		iProduction /= 100;
	}
	else
	{
		iProduction *= std::max(0, (GET_PLAYER(ePlayer).getFeatureProductionModifier() + 100));
		iProduction /= 100;
	}

	iProduction *= GC.getGame().getGameSpeedInfo().getFeatureProductionPercent();
	iProduction /= 100;

	if (getTeam() != eTeam)
	{
		iProduction *= /*67*/ GD_INT_GET(DIFFERENT_TEAM_FEATURE_PRODUCTION_PERCENT);
		iProduction /= 100;
	}

	return std::max(0, iProduction);
}


//	--------------------------------------------------------------------------------
CvUnit* CvPlot::getBestGarrison(PlayerTypes eOwner) const
{
	const IDInfo* pUnitNode = headUnitNode();
	CvUnit* pLoopUnit = NULL;
	CvUnit* pBestUnit = NULL;

	//we don't consider promotions here ...
	int iBestBaseCS = -1;
	while(pUnitNode != NULL)
	{
		pLoopUnit = GetPlayerUnit(*pUnitNode);
		pUnitNode = nextUnitNode(pUnitNode);

		if(pLoopUnit && (pLoopUnit->getOwner() == eOwner) && pLoopUnit->CanGarrison() && !pLoopUnit->isDelayedDeath())
		{
			int iBaseCS = max(pLoopUnit->GetBaseCombatStrength(),pLoopUnit->GetBaseRangedCombatStrength());
			//naval units are not the best garrison
			if (!pLoopUnit->isNativeDomain(this))
				iBaseCS = 0;

			if(iBaseCS>iBestBaseCS)
			{
				pBestUnit = pLoopUnit;
				iBestBaseCS = iBaseCS;
			}
		}
	}

	return pBestUnit;
}

//	--------------------------------------------------------------------------------
CvUnit* CvPlot::getBestDefender(PlayerTypes eOwner, PlayerTypes eAttackingPlayer, const CvUnit* pAttacker, bool bTestAtWar, bool bIgnoreVisibility, bool bTestCanMove, bool bNoncombatAllowed) const
{
	const IDInfo* pUnitNode = headUnitNode();
	CvUnit* pLoopUnit = NULL;
	CvUnit* pBestUnit = NULL;

	//early out
	if (eAttackingPlayer != NO_PLAYER && !bIgnoreVisibility && !isVisible(GET_PLAYER(eAttackingPlayer).getTeam()))
		return NULL;

	while(pUnitNode != NULL)
	{
		//for performance, avoid looking up unit pointers if we can
		if (eOwner != NO_PLAYER && pUnitNode->eOwner != eOwner)
		{
			pUnitNode = nextUnitNode(pUnitNode);
			continue;
		}

		pLoopUnit = GetPlayerUnit(*pUnitNode);
		pUnitNode = nextUnitNode(pUnitNode);

		if(pLoopUnit && (bNoncombatAllowed || pLoopUnit->IsCanDefend()) && pLoopUnit != pAttacker)	// Does the unit exist, and can it fight, or do we care if it can't fight?
		{
			if(eAttackingPlayer == NO_PLAYER || bIgnoreVisibility || !pLoopUnit->isInvisible(GET_PLAYER(eAttackingPlayer).getTeam(), false))
			{
				if(!bTestAtWar || eAttackingPlayer == NO_PLAYER || pLoopUnit->isEnemy(GET_PLAYER(eAttackingPlayer).getTeam(), this) || (NULL != pAttacker && pAttacker->isEnemy(GET_PLAYER(pLoopUnit->getOwner()).getTeam(), this)))
				{
					if(!bTestCanMove || (pLoopUnit->canMove() && !(pLoopUnit->isCargo())))
					{
						if(pLoopUnit->isBetterDefenderThan(pBestUnit, pAttacker))
						{
							pBestUnit = pLoopUnit;
						}
					}
				}
			}
		}
	}

	return pBestUnit;
}

//	--------------------------------------------------------------------------------
bool CvPlot::HasAirCover(PlayerTypes eDefendingPlayer) const
{
	if (eDefendingPlayer == NO_PLAYER)
		return false;

	CvPlayerAI& kPlayer = GET_PLAYER(eDefendingPlayer);
	const std::vector<std::pair<int, int>>& possibleUnits = kPlayer.GetPossibleInterceptors();
	for (std::vector<std::pair<int, int>>::const_iterator it = possibleUnits.begin(); it != possibleUnits.end(); ++it)
	{
		CvPlot* pInterceptorPlot = GC.getMap().plotByIndexUnchecked(it->second);
		CvUnit* pInterceptorUnit = kPlayer.getUnit(it->first);

		if (!pInterceptorUnit || pInterceptorUnit->isDelayedDeath())
			continue;

		// Must not have already intercepted this turn
		if (!pInterceptorUnit->canInterceptNow())
			continue;

		// Test range
		int iDistance = plotDistance(*pInterceptorPlot, *this);
		if (iDistance <= pInterceptorUnit->GetAirInterceptRange())
			return true;
	}

	return false;
}

//	--------------------------------------------------------------------------------
int CvPlot::GetInterceptorCount(PlayerTypes eAttackingPlayer, CvUnit* pAttackingUnit /* = NULL */, bool bLandInterceptorsOnly /*false*/, bool bVisibleInterceptorsOnly /*false*/) const
{
	int iCount = 0;
	GetBestInterceptor(eAttackingPlayer, pAttackingUnit,bLandInterceptorsOnly,bVisibleInterceptorsOnly,&iCount);
	return iCount;
}


//	--------------------------------------------------------------------------------
CvUnit* CvPlot::GetBestInterceptor(PlayerTypes eAttackingPlayer, const CvUnit* pAttackingUnit /* = NULL */, 
	bool bLandInterceptorsOnly /*false*/, bool bVisibleInterceptorsOnly /*false*/, int* piNumPossibleInterceptors) const
{
	if (eAttackingPlayer == NO_PLAYER)
		return NULL;

	VALIDATE_OBJECT();
	CvUnit* pBestUnit = 0;
	int iBestValue = 0;
	int iBestDistance = INT_MAX;

	// Loop through all players' Units (that we're at war with) to see if they can intercept
	// Note that the barbarians are not included here, to they can never intercept
	const std::vector<PlayerTypes>& vEnemies = GET_PLAYER(eAttackingPlayer).GetPlayersAtWarWith();

	for(size_t iI = 0; iI < vEnemies.size(); iI++)
	{
		CvPlayerAI& kLoopPlayer = GET_PLAYER(vEnemies[iI]);
		TeamTypes eLoopTeam = kLoopPlayer.getTeam();

		//stealth unit? no intercept
		if(pAttackingUnit && pAttackingUnit->isInvisible(eLoopTeam, false, false))
			continue;

		const std::vector<std::pair<int, int>>& possibleUnits = kLoopPlayer.GetPossibleInterceptors();
		for (std::vector<std::pair<int, int>>::const_iterator it = possibleUnits.begin(); it != possibleUnits.end(); ++it)
		{
			CvPlot* pInterceptorPlot = GC.getMap().plotByIndexUnchecked(it->second);
			if (bVisibleInterceptorsOnly && !pInterceptorPlot->isVisible(GET_PLAYER(eAttackingPlayer).getTeam()))
				continue;

			//first a very rough distance check to avoid expensive unit lookup
			int iDistance = plotDistance(*pInterceptorPlot, *this);
			if (iDistance > 11)
				continue;

			CvUnit* pInterceptorUnit = kLoopPlayer.getUnit(it->first);
			if (!pInterceptorUnit || pInterceptorUnit->isDelayedDeath())
				continue;

			// Must not have already intercepted this turn
			if (!pInterceptorUnit->canInterceptNow())
				continue;

			// Check conditions
			if (bLandInterceptorsOnly && pInterceptorUnit->getDomainType() != DOMAIN_LAND)
				continue;
			if (iDistance > pInterceptorUnit->GetAirInterceptRange())
				continue;
			
			//do not violate neutral players' airspace
			if (isOwned() && !kLoopPlayer.IsAtWarWith(getOwner()) && !IsFriendlyTerritory(kLoopPlayer.GetID()))
				continue;

			// we're fine with truncation here; take promotions boosting intercept strength into account
			int attackStrength = (pInterceptorUnit->GetBestAttackStrength() * (100 + pInterceptorUnit->GetInterceptionCombatModifier())) / 100;
			
			// interceptionProbability contains product of actual intercept chance and health percentage; lets be careful with air units at low health in case of air sweeps
			int healthFactor = pInterceptorUnit->interceptionProbability();
			if (pInterceptorUnit->getDomainType() == DOMAIN_AIR)
				healthFactor = (healthFactor * (pInterceptorUnit->GetCurrHitPoints() * 100) / pInterceptorUnit->GetMaxHitPoints()) / 100;
			
			int iValue = attackStrength * healthFactor;

			if (iValue>0 && piNumPossibleInterceptors)
				(*piNumPossibleInterceptors)++;

			if( iValue>iBestValue || (iValue==iBestValue && iDistance<iBestDistance) )
			{
				iBestDistance = iDistance;
				iBestValue = iValue;
				pBestUnit = pInterceptorUnit;
			}
		}
	}

	return pBestUnit;
}

CvCity* CvPlot::GetNukeInterceptor(PlayerTypes eAttackingPlayer) const
{
	if (eAttackingPlayer == NO_PLAYER)
		return NULL;

	CvCity* pCity = getOwningCity();
	if (pCity == NULL)
		return NULL;

	CvPlayerAI& kPlayer = GET_PLAYER(pCity->getOwner());
	if (kPlayer.isMinorCiv() || kPlayer.isBarbarian())
		return NULL;

	if (pCity->getNukeInterceptionChance() <= 0)
		return NULL;

	return pCity;
}


//	--------------------------------------------------------------------------------
CvUnit* CvPlot::getSelectedUnit() const
{
	const IDInfo* pUnitNode = headUnitNode();
	CvUnit* pLoopUnit = NULL;

	while(pUnitNode != NULL)
	{
		pLoopUnit = GetPlayerUnit(*pUnitNode);
		pUnitNode = nextUnitNode(pUnitNode);

		if(pLoopUnit && pLoopUnit->IsSelected())
		{
			return pLoopUnit;
		}
	}

	return NULL;
}


//	--------------------------------------------------------------------------------
int CvPlot::getUnitPower(PlayerTypes eOwner) const
{
	const IDInfo* pUnitNode = headUnitNode();
	const CvUnit* pLoopUnit = NULL;
	int iCount = 0;

	while(pUnitNode != NULL)
	{
		pLoopUnit = GetPlayerUnit(*pUnitNode);
		pUnitNode = nextUnitNode(pUnitNode);

		if(!pLoopUnit) continue;

		if((eOwner == NO_PLAYER) || (pLoopUnit->getOwner() == eOwner))
		{
			for(int iI = 0; iI < GC.getNumFlavorTypes(); iI++)
			{
				if(GC.getFlavorTypes((FlavorTypes)iI) == "FLAVOR_OFFENSE" ||
				        GC.getFlavorTypes((FlavorTypes)iI) == "FLAVOR_DEFENSE")
				{
					iCount += pLoopUnit->getUnitInfo().GetFlavorValue(iI);
				}
			}
		}
	}

	return iCount;
}

int CvPlot::GetEffectiveFlankingBonus(const CvUnit* pUnit, const CvUnit* pOtherUnit, const CvPlot* pOtherUnitPlot) const
{
	if (!pUnit || !pOtherUnit)
		return 0;

	const CvPlot* pOtherPlot = pOtherUnitPlot ? pOtherUnitPlot : pOtherUnit->plot();

	//our units are the enemy's enemies ...
	int iNumUnitsAdjacentToOther = pOtherPlot->GetNumEnemyUnitsAdjacent( pOtherUnit->getTeam(), pOtherUnit->getDomainType(), pUnit, true);
	int iNumUnitsAdjacentToHere = GetNumEnemyUnitsAdjacent( pUnit->getTeam(), pUnit->getDomainType(), pOtherUnit, true);

	if (iNumUnitsAdjacentToOther > iNumUnitsAdjacentToHere)
		return (pUnit->GetFlankAttackModifier() + /*10*/ GD_INT_GET(BONUS_PER_ADJACENT_FRIEND)) * (iNumUnitsAdjacentToOther - iNumUnitsAdjacentToHere);

	return 0;
}

int CvPlot::GetEffectiveFlankingBonusAtRange(const CvUnit* pAttackingUnit, const CvUnit* pDefendingUnit) const
{
	// note that this plot is the plot that the ranged unit is ATTACKING, not the plot that the ranged unit is located
	
	if (!pAttackingUnit || !pDefendingUnit)
		return 0;

	// ranged units can't get flanked when they attack, but their target can be
	int iNumUnitsAdjacentToHere = GetNumEnemyUnitsAdjacent( pDefendingUnit->getTeam(), pDefendingUnit->getDomainType(), pAttackingUnit, true);

	return (pAttackingUnit->GetFlankAttackModifier() + /*10*/ GD_INT_GET(BONUS_PER_ADJACENT_FRIEND)) * (iNumUnitsAdjacentToHere);
}

//	--------------------------------------------------------------------------------
int CvPlot::defenseModifier(TeamTypes eDefender, bool bIgnoreImprovement, bool bIgnoreFeature, bool bForHelp) const
{
	// Cities also give a boost - damage is split between city and unit - assume a flat 100% defense bonus for simplicity
	if (isCity())
		return 100;

	int iModifier = 0;

	// Plot type
	if(isHills() || isMountain())
		iModifier += /*25 in CP, 10 in VP*/ GD_INT_GET(HILLS_EXTRA_DEFENSE);

	// Feature
	if(!bIgnoreFeature && getFeatureType() != NO_FEATURE)
		iModifier += GC.getFeatureInfo(getFeatureType())->getDefenseModifier();

	// Terrain
	if(getTerrainType() != NO_TERRAIN)
		iModifier += GC.getTerrainInfo(getTerrainType())->getDefenseModifier();

	if (!bIgnoreImprovement)
	{
		// Improvements count extra, but include them for tooltips only if the tile is revealed
		ImprovementTypes eImprovement = bForHelp ? getRevealedImprovementType(GC.getGame().getActiveTeam()) : getImprovementType();
		if(eImprovement != NO_IMPROVEMENT && !IsImprovementPillaged())
		{
			//only friendly or unowned fortresses can be used for combat, but include them in the tooltips always
			if(bForHelp || (eDefender != NO_TEAM && (getTeam() == NO_TEAM || getTeam() == eDefender)))
			{
				CvImprovementEntry* pkImprovement = GC.getImprovementInfo(eImprovement);
				if (pkImprovement)
					iModifier += pkImprovement->GetDefenseModifier();
			}
		}
	}

	return iModifier;
}

//	---------------------------------------------------------------------------
int CvPlot::movementCost(const CvUnit* pUnit, const CvPlot* pFromPlot, int iMovesRemaining) const
{
	int iMaxMoves = pUnit->baseMoves( needsEmbarkation(pUnit) )*GD_INT_GET(MOVE_DENOMINATOR);

	if (plotDistance(*this,*pFromPlot)>1)
		return iMaxMoves;

	return CvUnitMovement::MovementCost(pUnit, pFromPlot, this, iMovesRemaining, iMaxMoves);
}

//	---------------------------------------------------------------------------
int CvPlot::MovementCostNoZOC(const CvUnit* pUnit, const CvPlot* pFromPlot, int iMovesRemaining) const
{
	int iMaxMoves = pUnit->baseMoves( needsEmbarkation(pUnit) )*GD_INT_GET(MOVE_DENOMINATOR);

	if (plotDistance(*this,*pFromPlot)>1)
		return iMaxMoves;

	return CvUnitMovement::MovementCostNoZOC(pUnit, pFromPlot, this, iMovesRemaining, iMaxMoves);
}

//	--------------------------------------------------------------------------------
bool CvPlot::IsAllowsWalkWater() const
{
	ImprovementTypes eImprovement = getImprovementType();
	if (eImprovement != NO_IMPROVEMENT)
	{
		CvImprovementEntry *pkEntry = GC.getImprovementInfo(eImprovement);
		if (pkEntry)
			return pkEntry->IsAllowsWalkWater();
	}
	return false;
}

bool CvPlot::needsEmbarkation(const CvUnit* pUnit) const
{
	//embarkation only on water plots
	if (!isWater() || isIce() || IsAllowsWalkWater())
		return false;

	if (!pUnit)
		return true;

	//only land units need to embark
	if (pUnit->getDomainType() != DOMAIN_LAND || pUnit->canMoveAllTerrain())
		return false;

	if (pUnit->IsEmbarkDeepWater())
		return isDeepWater();

	//some units can flip between different types
	if (pUnit->isConvertUnit())
		return false;

	//we know it's a land unit and a water plot by now
	return true;
}

//	--------------------------------------------------------------------------------
int CvPlot::getExtraMovePathCost() const
{
	return GC.getGame().getPlotExtraCost(getX(), getY());
}


//	--------------------------------------------------------------------------------
void CvPlot::changeExtraMovePathCost(int iChange)
{
	GC.getGame().changePlotExtraCost(getX(), getY(), iChange);
}

//	--------------------------------------------------------------------------------
bool CvPlot::isAdjacentOwned() const
{
	CvPlot** aPlotsToCheck = GC.getMap().getNeighborsUnchecked(this);
	for(int iI=0; iI<NUM_DIRECTION_TYPES; iI++)
	{
		CvPlot* pAdjacentPlot = aPlotsToCheck[iI];
		if(pAdjacentPlot != NULL)
		{
			if(pAdjacentPlot->getOwner() != NO_PLAYER)
			{
				return true;
			}
		}
	}

	return false;
}


//	--------------------------------------------------------------------------------
bool CvPlot::isAdjacentPlayer(PlayerTypes ePlayer, bool bLandOnly) const
{
	CvPlot** aPlotsToCheck = GC.getMap().getNeighborsUnchecked(this);
	for(int iI=0; iI<NUM_DIRECTION_TYPES; iI++)
	{
		CvPlot* pAdjacentPlot = aPlotsToCheck[iI];
		if(pAdjacentPlot != NULL)
		{
			if(pAdjacentPlot->getOwner() == ePlayer)
			{
				if(!bLandOnly || !(pAdjacentPlot->isWater()))
				{
					return true;
				}
			}
		}
	}

	return false;
}

//	--------------------------------------------------------------------------------
bool CvPlot::IsAdjacentOwnedByTeamOtherThan(TeamTypes eTeam, bool bAllowNoTeam, bool bIgnoreImpassable, bool bIgnoreMinor, bool bIgnoreVassal) const
{
	CvPlot** aPlotsToCheck = GC.getMap().getNeighborsUnchecked(this);
	for (int iI = 0; iI < NUM_DIRECTION_TYPES; iI++)
	{
		CvPlot* pAdjacentPlot = aPlotsToCheck[iI];
		if (!pAdjacentPlot)
			continue;

		if (bIgnoreImpassable && pAdjacentPlot->isImpassable(pAdjacentPlot->getTeam()))
			continue;

		if (bIgnoreMinor && pAdjacentPlot->getOwner() != NO_PLAYER && GET_PLAYER(pAdjacentPlot->getOwner()).isMinorCiv())
			continue;

		if (bIgnoreVassal && pAdjacentPlot->getTeam() != NO_TEAM && GET_TEAM(pAdjacentPlot->getTeam()).IsVassal(eTeam))
			continue;

		if (pAdjacentPlot->getTeam() != eTeam && (pAdjacentPlot->getTeam() != NO_TEAM || bAllowNoTeam))
			return true;
	}

	return false;
}

//	--------------------------------------------------------------------------------
bool CvPlot::IsAdjacentOwnedByUnfriendly(PlayerTypes ePlayer, vector<PlayerTypes>& vUnfriendlyMajors) const
{
	CvPlot** aPlotsToCheck = GC.getMap().getNeighborsUnchecked(this);
	set<PlayerTypes> adjacentPlayers;
	for (int iI = 0; iI < NUM_DIRECTION_TYPES; iI++)
	{
		CvPlot* pAdjacentPlot = aPlotsToCheck[iI];
		if (pAdjacentPlot != NULL)
		{
			PlayerTypes ePlotOwner = pAdjacentPlot->getOwner();
			if (ePlotOwner != NO_PLAYER && GET_PLAYER(ePlotOwner).getTeam() != GET_PLAYER(ePlayer).getTeam())
			{
				if (pAdjacentPlot->isImpassable(GET_PLAYER(ePlotOwner).getTeam()))
					continue;

				// Directly insert ePlotOwner without checking if it already exists
				adjacentPlayers.insert(ePlotOwner);
			}
		}
	}

	if (adjacentPlayers.size() > 0)
	{
		if (GET_PLAYER(ePlayer).isBarbarian())
			return true;

		// Were we already passed a set of players to check? (performance optimization)
		if (!vUnfriendlyMajors.empty())
		{
			for (set<PlayerTypes>::iterator it = adjacentPlayers.begin(); it != adjacentPlayers.end(); ++it)
			{
				if (std::find(vUnfriendlyMajors.begin(), vUnfriendlyMajors.end(), *it) != vUnfriendlyMajors.end())
					return true;
			}
		}
		else
		{
			if (GET_PLAYER(ePlayer).isMajorCiv())
			{
				for (set<PlayerTypes>::iterator it = adjacentPlayers.begin(); it != adjacentPlayers.end(); ++it)
				{
					if (GET_PLAYER(ePlayer).GetDiplomacyAI()->IsPotentialMilitaryTargetOrThreat(*it, false))
						return true;
				}
			}
			else if (GET_PLAYER(ePlayer).isMinorCiv())
			{
				for (set<PlayerTypes>::iterator it = adjacentPlayers.begin(); it != adjacentPlayers.end(); ++it)
				{
					TeamTypes eAdjacentTeam = GET_PLAYER(*it).getTeam();
					if (GET_PLAYER(ePlayer).IsAtWarWith(*it) || GET_PLAYER(ePlayer).GetMinorCivAI()->IsWaryOfTeam(eAdjacentTeam)
						|| GET_PLAYER(ePlayer).GetMinorCivAI()->GetJerkTurnsRemaining(eAdjacentTeam) > 0)
					{
						return true;
					}
				}
			}
		}
	}

	return false;
}

//	--------------------------------------------------------------------------------
bool CvPlot::IsAdjacentOwnedByEnemy(TeamTypes eTeam) const
{
	CvPlot** aPlotsToCheck = GC.getMap().getNeighborsUnchecked(this);
	for(int iI=0; iI<NUM_DIRECTION_TYPES; iI++)
	{
		CvPlot* pAdjacentPlot = aPlotsToCheck[iI];
		if(pAdjacentPlot != NULL)
		{
			if(pAdjacentPlot->getTeam() != NO_TEAM && GET_TEAM(eTeam).isAtWar(pAdjacentPlot->getTeam()))
			{
				return true;
			}
		}
	}

	return false;
}

//	--------------------------------------------------------------------------------
bool CvPlot::isAdjacentOwnedByVassal(TeamTypes eTeam, bool bLandOnly) const
{
	CvPlot** aPlotsToCheck = GC.getMap().getNeighborsUnchecked(this);
	for (int iI=0; iI<NUM_DIRECTION_TYPES; iI++)
	{
		CvPlot* pAdjacentPlot = aPlotsToCheck[iI];
		if(pAdjacentPlot != NULL)
		{
			if(pAdjacentPlot->getTeam() != NO_TEAM && GET_TEAM(pAdjacentPlot->getTeam()).IsVassal(eTeam))
			{
				if(!bLandOnly || !(pAdjacentPlot->isWater()))
				{
					return true;
				}
			}
		}
	}

	return false;
}

//	--------------------------------------------------------------------------------
bool CvPlot::isAdjacentTeam(TeamTypes eTeam, bool bLandOnly) const
{
	CvPlot** aPlotsToCheck = GC.getMap().getNeighborsUnchecked(this);
	for(int iI=0; iI<NUM_DIRECTION_TYPES; iI++)
	{
		CvPlot* pAdjacentPlot = aPlotsToCheck[iI];
		if(pAdjacentPlot != NULL)
		{
			if(pAdjacentPlot->getTeam() == eTeam)
			{
				if(!bLandOnly || !(pAdjacentPlot->isWater()))
				{
					return true;
				}
			}
		}
	}

	return false;
}

bool CvPlot::IsAdjacentCity(TeamTypes eTeam) const
{
	for (int iI = 0; iI < NUM_DIRECTION_TYPES; ++iI)
	{
		CvPlot* pAdjacentPlot = plotDirection(getX(), getY(), ((DirectionTypes)iI));
		if (pAdjacentPlot && pAdjacentPlot->isCity())
		{
			if (eTeam==NO_TEAM || pAdjacentPlot->getTeam()==eTeam)
				return true;
		}
	}

	return false;
}

//	--------------------------------------------------------------------------------
CvCity* CvPlot::GetAdjacentFriendlyCity(TeamTypes eTeam, bool bLandOnly) const
{
	for(int iI = 0; iI < NUM_DIRECTION_TYPES; ++iI)
	{
		CvPlot* pAdjacentPlot = plotDirection(getX(), getY(), ((DirectionTypes)iI));

		if(pAdjacentPlot != NULL)
		{
			if(pAdjacentPlot->getTeam() == eTeam && pAdjacentPlot->isCity())
			{
				if(!bLandOnly || !(pAdjacentPlot->isWater()))
				{
					CvCity* pCity = pAdjacentPlot->getPlotCity();
					if(pCity)
					{
						if(pCity->getTeam() == eTeam)
						{
							return pCity;
						}
					}
				}
			}
		}
	}

	return NULL;
}

//	--------------------------------------------------------------------------------
CvCity* CvPlot::GetAdjacentCity() const
{
	for(int iI = 0; iI < NUM_DIRECTION_TYPES; ++iI)
	{
		CvPlot* pAdjacentPlot = plotDirection(getX(), getY(), ((DirectionTypes)iI));

		if(pAdjacentPlot != NULL)
		{
			if(pAdjacentPlot->isCity())
			{
				CvCity* pCity = pAdjacentPlot->getPlotCity();
				if(pCity)
				{
					return pCity;
				}
			}
		}
	}

	return NULL;
}

//	--------------------------------------------------------------------------------
/// Number of adjacent tiles owned by another team (or unowned)
int CvPlot::GetNumAdjacentDifferentTeam(TeamTypes eTeam, DomainTypes eDomain, bool bCountUnowned) const
{
	int iRtnValue = 0;

	CvPlot** aNeighbors = GC.getMap().getNeighborsUnchecked(GetPlotIndex());
	for(int iI = 0; iI < NUM_DIRECTION_TYPES; ++iI)
	{
		CvPlot* pAdjacentPlot = aNeighbors[iI];
		if(pAdjacentPlot != NULL)
		{
			if(eDomain!=NO_DOMAIN && pAdjacentPlot->getDomain()!=eDomain)
			{
				continue;
			}

			if(pAdjacentPlot->getTeam() != eTeam && (bCountUnowned || pAdjacentPlot->isOwned()) && !pAdjacentPlot->isImpassable(pAdjacentPlot->getTeam()))
			{
				iRtnValue++;
			}
		}
	}

	return iRtnValue;
}

int CvPlot::GetNumAdjacentMountains() const
{
	int iNumMountains = 0;

	for (int iI = 0; iI < NUM_DIRECTION_TYPES; ++iI)
	{
		CvPlot* pAdjacentPlot = plotDirection(getX(), getY(), ((DirectionTypes)iI));

		if(pAdjacentPlot != NULL)
		{
			if (pAdjacentPlot->isMountain())
			{
				iNumMountains++;
			}
		}
	}
	return iNumMountains;
}

int CvPlot::GetSeaBlockadeScore(PlayerTypes ePlayer) const
{
	int iScore = 0;
	int iRange = min(5,max(0, /*2 in CP, 1 in VP*/ GD_INT_GET(NAVAL_PLOT_BLOCKADE_RANGE)));

	for(int iI = 0; iI < RING_PLOTS[iRange]; iI++)
	{
		CvPlot* pLoopPlot = iterateRingPlots(getX(), getY(), iI);
		if(pLoopPlot == NULL || pLoopPlot->getDomain() != DOMAIN_SEA || pLoopPlot->getArea() != getArea())
			continue;

		if (GET_PLAYER(ePlayer).IsAtWarWith(pLoopPlot->getOwner()))
		{
			//there should really be a function that gives you a weighted score of all yields ...
			iScore++;
			if (pLoopPlot->getResourceType(GET_PLAYER(ePlayer).getTeam()) != NO_RESOURCE)
				iScore++;
			if (pLoopPlot->getFeatureType() != NO_FEATURE)
				iScore++;
			if (pLoopPlot->getImprovementType() != NO_IMPROVEMENT)
				iScore++;
		}

		iScore++;
	}

	return iScore;
}

int CvPlot::countPassableNeighbors(DomainTypes eDomain, CvPlot** aPassableNeighbors) const
{
	int iPassable = 0;

	for (int iI = 0; iI < NUM_DIRECTION_TYPES; ++iI)
	{
		CvPlot* pAdjacentPlot = plotDirection(getX(), getY(), ((DirectionTypes)iI));
		if(pAdjacentPlot != NULL)
		{
			if ( (eDomain==NO_DOMAIN || eDomain==pAdjacentPlot->getDomain()) && !pAdjacentPlot->isImpassable(pAdjacentPlot->getTeam()) )
			{
				if (aPassableNeighbors)
					aPassableNeighbors[iPassable] = pAdjacentPlot;
				iPassable++;
			}
		}
	}
	return iPassable;
}

bool CvPlot::IsBorderLand(PlayerTypes eDefendingPlayer) const
{
	vector<PlayerTypes> vUnfriendlyMajors;

	//check distance to all major players' cities
	//if homefront for at least one ...
	for (int iPlayerLoop = 0; iPlayerLoop < MAX_MAJOR_CIVS; iPlayerLoop++)
	{
		PlayerTypes eLoopPlayer = (PlayerTypes)iPlayerLoop;
		if (GET_PLAYER(eLoopPlayer).getTeam() == GET_PLAYER(eDefendingPlayer).getTeam())
			continue;

		if (!GET_PLAYER(eLoopPlayer).isAlive() || GET_PLAYER(eLoopPlayer).getNumCities() <= 0)
			continue;

		if (GET_PLAYER(eDefendingPlayer).isMajorCiv())
		{
			if (GET_PLAYER(eDefendingPlayer).GetDiplomacyAI()->GetCivApproach(eLoopPlayer)!= CIV_APPROACH_FRIENDLY)
				vUnfriendlyMajors.push_back(eLoopPlayer);
			else
				continue;
		}
		else if (GET_PLAYER(eDefendingPlayer).isMinorCiv())
		{
			TeamTypes eLoopTeam = GET_PLAYER(eLoopPlayer).getTeam();
			if (GET_PLAYER(eDefendingPlayer).IsAtWarWith(eLoopPlayer) || GET_PLAYER(eDefendingPlayer).GetMinorCivAI()->IsWaryOfTeam(eLoopTeam)
				|| GET_PLAYER(eDefendingPlayer).GetMinorCivAI()->GetJerkTurnsRemaining(eLoopTeam) > 0)
			{
				vUnfriendlyMajors.push_back(eLoopPlayer);
			}
			else
				continue;			
		}

		if (IsCloseToCity(eLoopPlayer))
			return true;
	}

	//alternatively see if an adjacent plot is owned by an unfriendly player
	//only check adjacent plots, everything else is too expensive
	return IsAdjacentOwnedByUnfriendly(eDefendingPlayer, vUnfriendlyMajors);
}

bool CvPlot::IsBorderLand(PlayerTypes eDefendingPlayer, vector<PlayerTypes>& vUnfriendlyMajors) const
{
	// Were we already passed a set of players to check? (performance optimization)
	if (!vUnfriendlyMajors.empty())
	{
		for (std::vector<PlayerTypes>::iterator it = vUnfriendlyMajors.begin(); it != vUnfriendlyMajors.end(); it++)
		{
			if (IsCloseToCity(*it))
				return true;
		}
	}
	else
	{
		//check distance to all major players' cities
		//if homefront for at least one ...
		for (int iPlayerLoop = 0; iPlayerLoop < MAX_MAJOR_CIVS; iPlayerLoop++)
		{
			PlayerTypes eLoopPlayer = (PlayerTypes)iPlayerLoop;
			if (GET_PLAYER(eLoopPlayer).getTeam() == GET_PLAYER(eDefendingPlayer).getTeam())
				continue;

			if (!GET_PLAYER(eLoopPlayer).isAlive() || GET_PLAYER(eLoopPlayer).getNumCities() <= 0)
				continue;

			if (GET_PLAYER(eDefendingPlayer).isMajorCiv())
			{
				if (GET_PLAYER(eDefendingPlayer).GetDiplomacyAI()->IsPotentialMilitaryTargetOrThreat(eLoopPlayer, false))
					vUnfriendlyMajors.push_back(eLoopPlayer);
				else
					continue;
			}
			else if (GET_PLAYER(eDefendingPlayer).isMinorCiv())
			{
				TeamTypes eLoopTeam = GET_PLAYER(eLoopPlayer).getTeam();
				if (GET_PLAYER(eDefendingPlayer).IsAtWarWith(eLoopPlayer) || GET_PLAYER(eDefendingPlayer).GetMinorCivAI()->IsWaryOfTeam(eLoopTeam)
					|| GET_PLAYER(eDefendingPlayer).GetMinorCivAI()->GetJerkTurnsRemaining(eLoopTeam) > 0)
				{
					vUnfriendlyMajors.push_back(eLoopPlayer);
				}
				else
					continue;			
			}

			if (IsCloseToCity(eLoopPlayer))
				return true;
		}
	}

	//alternatively see if an adjacent plot is owned by an unfriendly player
	//only check adjacent plots, everything else is too expensive
	return IsAdjacentOwnedByUnfriendly(eDefendingPlayer, vUnfriendlyMajors);
}

bool CvPlot::IsChokePoint() const
{
	if(isImpassable(BARBARIAN_TEAM))
		return false;

	CvPlot* aPassableNeighbors[NUM_DIRECTION_TYPES];
	int iPassable = countPassableNeighbors(DOMAIN_LAND, aPassableNeighbors);

	//a plot is a chokepoint if it has between two and four passable land plots as neighbors
	if (iPassable<2 || iPassable>4)
		return false;

	//each adjacent passable plot must have at least 3 passable neighbors (anti peninsula / mountain valley check)
	int iPassableNoDeadEnd = 0;
	CvPlot* aPassableNeighborsNoDeadEnd[NUM_DIRECTION_TYPES];
	for (int iI = 0; iI<iPassable; iI++)
	{
		if (aPassableNeighbors[iI]->countPassableNeighbors(DOMAIN_LAND,NULL)>2)
		{
			aPassableNeighborsNoDeadEnd[iPassableNoDeadEnd] = aPassableNeighbors[iI];
			iPassableNoDeadEnd++;
		}
	}

	if (iPassableNoDeadEnd<2)
	{
		return false;
	}
	else if (iPassableNoDeadEnd==2)
	{
		//check they are not adjacent
		return !aPassableNeighborsNoDeadEnd[0]->isAdjacent(aPassableNeighborsNoDeadEnd[1]);
	}
	else if (iPassableNoDeadEnd==3)
	{
		//three passable plots. not more than one pair may be adjacent
		int AB = aPassableNeighborsNoDeadEnd[0]->isAdjacent(aPassableNeighborsNoDeadEnd[1]) ? 1 : 0;
		int AC = aPassableNeighborsNoDeadEnd[0]->isAdjacent(aPassableNeighborsNoDeadEnd[2]) ? 1 : 0;
		int BC = aPassableNeighborsNoDeadEnd[1]->isAdjacent(aPassableNeighborsNoDeadEnd[2]) ? 1 : 0;

		return (AB+AC+BC)<2;
	}
	else if (iPassableNoDeadEnd==4)
	{
		//four passable plots. not more than two pairs may be adjacent
		int AB = aPassableNeighborsNoDeadEnd[0]->isAdjacent(aPassableNeighborsNoDeadEnd[1]) ? 1 : 0;
		int AC = aPassableNeighborsNoDeadEnd[0]->isAdjacent(aPassableNeighborsNoDeadEnd[2]) ? 1 : 0;
		int AD = aPassableNeighborsNoDeadEnd[0]->isAdjacent(aPassableNeighborsNoDeadEnd[3]) ? 1 : 0;
		int BC = aPassableNeighborsNoDeadEnd[1]->isAdjacent(aPassableNeighborsNoDeadEnd[2]) ? 1 : 0;
		int BD = aPassableNeighborsNoDeadEnd[1]->isAdjacent(aPassableNeighborsNoDeadEnd[3]) ? 1 : 0;
		int CD = aPassableNeighborsNoDeadEnd[2]->isAdjacent(aPassableNeighborsNoDeadEnd[3]) ? 1 : 0;

		return (AB+AC+AD+BC+BD+CD)<3;
	}

	return false;
}

bool CvPlot::IsWaterAreaSeparator() const
{
	//only passable land plots
	if(isWater() || isImpassable(BARBARIAN_TEAM))
		return false;

	//for simplicity we simply require a different water area on both sides
	int iFirstWaterArea = -1;

	CvPlot** aPlotsToCheck = GC.getMap().getNeighborsUnchecked(this);
	for (int iCount = 0; iCount < NUM_DIRECTION_TYPES; iCount++)
	{
		const CvPlot* pLoopPlot = aPlotsToCheck[iCount];
		if(pLoopPlot==NULL || !pLoopPlot->isWater())
			continue;

		if (iFirstWaterArea == -1)
		{
			iFirstWaterArea = pLoopPlot->getArea();
		}
		else if (pLoopPlot->getArea() != iFirstWaterArea)
		{
			return true;
		}
	}

	return false;
}

//	--------------------------------------------------------------------------------
void CvPlot::plotAction(PlotUnitFunc func, int iData1, int iData2, PlayerTypes eOwner, TeamTypes eTeam)
{
	IDInfo* pUnitNode = NULL;
	CvUnit* pLoopUnit = NULL;

	pUnitNode = headUnitNode();

	while(pUnitNode != NULL)
	{
		pLoopUnit = GetPlayerUnit(*pUnitNode);
		pUnitNode = nextUnitNode(pUnitNode);

		if(pLoopUnit)
		{
			if((eOwner == NO_PLAYER) || (pLoopUnit->getOwner() == eOwner))
			{
				if((eTeam == NO_TEAM) || (pLoopUnit->getTeam() == eTeam))
				{
					func(pLoopUnit, iData1, iData2);
				}
			}
		}
	}
}


//	--------------------------------------------------------------------------------
int CvPlot::plotCount(ConstPlotUnitFunc funcA, int iData1A, int iData2A, PlayerTypes eOwner, TeamTypes eTeam, ConstPlotUnitFunc funcB, int iData1B, int iData2B) const
{
	const IDInfo* pUnitNode = NULL;
	const CvUnit* pLoopUnit = NULL;
	int iCount = 0;

	iCount = 0;

	pUnitNode = headUnitNode();

	while(pUnitNode != NULL)
	{
		pLoopUnit = GetPlayerUnit(*pUnitNode);
		pUnitNode = nextUnitNode(pUnitNode);

		if(pLoopUnit)
		{
			if((eOwner == NO_PLAYER) || (pLoopUnit->getOwner() == eOwner))
			{
				if((eTeam == NO_TEAM) || (pLoopUnit->getTeam() == eTeam))
				{
					if((funcA == NULL) || funcA(pLoopUnit, iData1A, iData2A))
					{
						if((funcB == NULL) || funcB(pLoopUnit, iData1B, iData2B))
						{
							iCount++;
						}
					}
				}
			}
		}
	}

	return iCount;
}

//	--------------------------------------------------------------------------------
CvUnit* CvPlot::plotCheck(ConstPlotUnitFunc funcA, int iData1A, int iData2A, PlayerTypes eOwner, TeamTypes eTeam, ConstPlotUnitFunc funcB, int iData1B, int iData2B) const
{
	const IDInfo* pUnitNode = headUnitNode();
	CvUnit* pLoopUnit = NULL;

	while(pUnitNode != NULL)
	{
		pLoopUnit = GetPlayerUnit(*pUnitNode);
		pUnitNode = nextUnitNode(pUnitNode);

		if(pLoopUnit)
		{
			if((eOwner == NO_PLAYER) || (pLoopUnit->getOwner() == eOwner))
			{
				if((eTeam == NO_TEAM) || (pLoopUnit->getTeam() == eTeam))
				{
					if(funcA(pLoopUnit, iData1A, iData2A))
					{
						if((funcB == NULL) || funcB(pLoopUnit, iData1B, iData2B))
						{
							return pLoopUnit;
						}
					}
				}
			}
		}
	}

	return NULL;
}

//	--------------------------------------------------------------------------------
bool CvPlot::isOwned() const
{
	return getOwner() != NO_PLAYER;
}


//	--------------------------------------------------------------------------------
bool CvPlot::isBarbarian() const
{
	return getOwner() == BARBARIAN_PLAYER;
}


//	--------------------------------------------------------------------------------
bool CvPlot::isRevealedBarbarian() const
{
	return getRevealedOwner(GC.getGame().getActiveTeam(), true) == BARBARIAN_PLAYER;
}

//	--------------------------------------------------------------------------------
bool CvPlot::HasBarbarianCamp()
{
	return getImprovementType() == GD_INT_GET(BARBARIAN_CAMP_IMPROVEMENT);
}

//	--------------------------------------------------------------------------------
bool CvPlot::HasDig()
{
	return getResourceType() == GD_INT_GET(ARTIFACT_RESOURCE);
}

bool CvPlot::isVisible(TeamTypes eTeam, bool bDebug) const
{
	if (bDebug && GC.getGame().isDebugMode())
		return true;
	else
		return isVisible(eTeam);
}

bool CvPlot::isVisible(TeamTypes eTeam) const
{
	if (eTeam == NO_TEAM)
		return false;

	return getVisibilityCount(eTeam) > 0;
}

//	--------------------------------------------------------------------------------
bool CvPlot::isActiveVisible() const
{
	if (GET_PLAYER(GC.getGame().getActivePlayer()).isObserver() && CvPreGame::quickCombat() && CvPreGame::quickMovement())
		//this is relevant for animations etc, do not show them in observer mode, makes everything faster
		return false;

	return isVisible(GC.getGame().getActiveTeam());
}

//	--------------------------------------------------------------------------------
bool CvPlot::isVisibleToAnyTeam(bool bNoMinor) const
{
	//barbarians are excluded here!
	for(int iI = 0; iI < MAX_CIV_TEAMS; ++iI)
	{
		//Skip observer here.
		if(GET_TEAM((TeamTypes)iI).isObserver())
		{
			continue;
		}

		if (bNoMinor && GET_TEAM((TeamTypes)iI).isMinorCiv())
		{
			continue;
		}

		if(GET_TEAM((TeamTypes)iI).isAlive())
		{
			if(isVisible(((TeamTypes)iI)))
			{
				return true;
			}
		}
	}

	return false;
}


//	--------------------------------------------------------------------------------
bool CvPlot::isVisibleToWatchingHuman() const
{
	for(int iI = 0; iI < MAX_CIV_PLAYERS; ++iI)
	{
		CvPlayerAI& thisPlayer = GET_PLAYER((PlayerTypes)iI);
		if( (thisPlayer.isAlive() && thisPlayer.isHuman()) || ( CvPreGame::slotStatus((PlayerTypes)iI) == SS_OBSERVER && CvPreGame::slotClaim((PlayerTypes)iI) == SLOTCLAIM_ASSIGNED) )
		{
			if(isVisible(thisPlayer.getTeam()))
			{
				return true;
			}
		}
	}

	return false;
}


//	--------------------------------------------------------------------------------
bool CvPlot::isAdjacentVisible(TeamTypes eTeam, bool bDebug) const
{
	CvPlot** aPlotsToCheck = GC.getMap().getNeighborsUnchecked(this);
	for(int iI=0; iI<NUM_DIRECTION_TYPES; iI++)
	{
		CvPlot* pAdjacentPlot = aPlotsToCheck[iI];
		if(pAdjacentPlot != NULL)
		{
			if(pAdjacentPlot->isVisible(eTeam, bDebug))
			{
				return true;
			}
		}
	}

	return false;
}

//	--------------------------------------------------------------------------------
bool CvPlot::isAdjacentNonvisible(TeamTypes eTeam) const
{
	CvPlot** aPlotsToCheck = GC.getMap().getNeighborsUnchecked(this);
	for(int iI=0; iI<NUM_DIRECTION_TYPES; iI++)
	{
		CvPlot* pAdjacentPlot = aPlotsToCheck[iI];
		if(pAdjacentPlot != NULL)
		{
			if(!pAdjacentPlot->isVisible(eTeam))
			{
				return true;
			}
		}
	}

	return false;
}

//	--------------------------------------------------------------------------------
int CvPlot::countMatchingAdjacentPlots(DomainTypes eDomain, PlayerTypes eOwningPlayer, PlayerTypes eWarPlayer, PlayerTypes eInvisiblePlayer) const
{
	int iMatchingAdjacentCount = 0;
	CvPlot** aNeighbors = GC.getMap().getNeighborsUnchecked(this);
	for (int i = 0; i < NUM_DIRECTION_TYPES; i++)
	{
		CvPlot* pNeighbor = aNeighbors[i];
		if (!pNeighbor)
			continue;

		if (eDomain != NO_DOMAIN && pNeighbor->getDomain() != eDomain)
			continue;

		if (eOwningPlayer != NO_PLAYER && pNeighbor->getOwner() != eOwningPlayer)
			continue;

		if (eWarPlayer != NO_PLAYER && !GET_PLAYER(eWarPlayer).IsAtWarWith(pNeighbor->getOwner()))
			continue;

		if (eInvisiblePlayer != NO_PLAYER && pNeighbor->isVisible(GET_PLAYER(eInvisiblePlayer).getTeam()))
			continue;

		iMatchingAdjacentCount++;
	}

	return iMatchingAdjacentCount;
}

//	--------------------------------------------------------------------------------
bool CvPlot::isGoody(TeamTypes eTeam) const
{
	if((eTeam != NO_TEAM) && (GET_TEAM(eTeam).isBarbarian() || GET_TEAM(eTeam).isMinorCiv()))
	{
		return false;
	}

	return ((getImprovementType() == NO_IMPROVEMENT) ? false : GC.getImprovementInfo(getImprovementType())->IsGoody());
}


//	--------------------------------------------------------------------------------
bool CvPlot::isRevealedGoody(TeamTypes eTeam) const
{
	if(eTeam == NO_TEAM)
	{
		return isGoody();
	}

	if(GET_TEAM(eTeam).isBarbarian() || GET_TEAM(eTeam).isMinorCiv())
	{
		return false;
	}

	if(!isRevealed(eTeam))
	{
		return false;
	}

	return ((getRevealedImprovementType(eTeam) == NO_IMPROVEMENT) ? false : GC.getImprovementInfo(getRevealedImprovementType(eTeam))->IsGoody());
}


//	--------------------------------------------------------------------------------
void CvPlot::removeGoody()
{
	setImprovementType(NO_IMPROVEMENT);
	// Make sure the players redo their goody hut searches
	for(int i = 0; i < MAX_MAJOR_CIVS; i++)
	{
		if(GET_PLAYER((PlayerTypes)i).isAlive())
			GET_PLAYER((PlayerTypes)i).GetEconomicAI()->UpdateExplorePlotsLocally(this);
	}
}

bool CvPlot::isCity() const
{
	return m_bIsCity;
}

bool CvPlot::isEnemyCity(const CvUnit& kUnit) const
{
	if(isCity())
		return kUnit.isEnemy(GET_PLAYER(m_owningCity.eOwner).getTeam(), this);

	return false;
}

//	--------------------------------------------------------------------------------
bool CvPlot::isFriendlyCity(const CvUnit& kUnit) const
{
	if(!isCity())
	{
		return false;
	}

	TeamTypes ePlotTeam = getTeam();

	if(NO_TEAM != ePlotTeam)
	{
		TeamTypes eTeam = GET_PLAYER(kUnit.getCombatOwner(ePlotTeam, *this)).getTeam();

		if(eTeam == ePlotTeam)
		{
			return true;
		}

		if(GET_TEAM(ePlotTeam).IsAllowsOpenBordersToTeam(eTeam))
		{
			return true;
		}
	}

	return false;
}

bool CvPlot::isFortification(TeamTypes eOccupyingTeam) const
{
	if (isCity())
		return true;

	if (MOD_GLOBAL_NO_FOLLOWUP_FROM_CITIES)
	{
		// If the attacker is in a fort or citadel or other improvement with NoFollowUp, don't advance
		TeamTypes eOwnerTeam = getTeam();
		if ((eOwnerTeam == NO_TEAM || eOwnerTeam == eOccupyingTeam) && getImprovementType() != NO_IMPROVEMENT && !IsImprovementPillaged())
		{
			CvImprovementEntry* pImprovementInfo = GC.getImprovementInfo(getImprovementType());
			if (pImprovementInfo && pImprovementInfo->IsNoFollowUp())
				return true;
		}
	}

	return false;
}

bool CvPlot::isCoastalCityOrPassableImprovement(PlayerTypes ePlayer, bool bCityMustBeFriendly, bool bImprovementMustBeFriendly) const
{
	bool bIsCity = isCity() && isCoastalLand();
	// Good enough
	if (bIsCity)
	{
		if (bCityMustBeFriendly)
			return IsFriendlyTerritory(ePlayer);
		else
			return true;
	}

	bool bIsPassableImprovement = false;
	if (MOD_GLOBAL_PASSABLE_FORTS)
		bIsPassableImprovement = IsImprovementPassable() && !IsImprovementPillaged() && isOwned() && isCoastalLand();

	// Good enough
	if (bIsPassableImprovement)
	{
		if (bImprovementMustBeFriendly)
			return IsFriendlyTerritory(ePlayer);
		else
			return true;
	}

	return false;
}

//	--------------------------------------------------------------------------------
/// Is this a plot that's friendly to our team? (owned by us or someone we have Open Borders with)
bool CvPlot::IsFriendlyTerritory(PlayerTypes ePlayer) const
{
	TeamTypes ePlotOwner = getTeam();
	// Nobody owns this plot
	if (ePlotOwner == NO_TEAM)
		return false;

	// Special cases
	if (ePlayer == NO_PLAYER || ePlayer==BARBARIAN_PLAYER)
		return false;

	// Our territory
	TeamTypes eTeam = GET_PLAYER(ePlayer).getTeam();
	if(ePlotOwner == eTeam)
		return true;

	// Major's territory we have OB with
	if (GET_TEAM(ePlotOwner).IsAllowsOpenBordersToTeam(eTeam))
		return true;

	// City State's territory we've earned OB with
	if(!GET_TEAM(eTeam).isMinorCiv())
	{
		if(GET_TEAM(ePlotOwner).isMinorCiv())
		{
			PlayerTypes eCityState = GET_TEAM(ePlotOwner).getLeaderID();

			if(GET_PLAYER(eCityState).GetMinorCivAI()->IsPlayerHasOpenBorders(ePlayer))
			{
				return true;
			}
		}
	}

	return false;
}

//	--------------------------------------------------------------------------------
bool CvPlot::isBeingWorked() const
{
	CvCity* pOwningCity = getEffectiveOwningCity();
	if(pOwningCity != NULL)
	{
		return pOwningCity->GetCityCitizens()->IsWorkingPlot(this);
	}

	return false;
}


//	--------------------------------------------------------------------------------
bool CvPlot::isUnit() const
{
	return (getNumUnits() > 0);
}

//	-----------------------------------------------------------------------------------------------
static bool isEnemy(const CvUnit* pUnit, TeamTypes eOtherTeam, bool bAlwaysHostile)
{
	if(pUnit->canCoexistWithEnemyUnit(eOtherTeam))
	{
		return false;
	}

	TeamTypes eOurTeam = GET_PLAYER(pUnit->getCombatOwner(eOtherTeam, *(pUnit->plot()))).getTeam();
	return (bAlwaysHostile ? eOtherTeam != eOurTeam : atWar(eOtherTeam, eOurTeam));
}

//	-----------------------------------------------------------------------------------------------
static bool isOtherTeam(const CvUnit* pUnit, TeamTypes eOtherTeam)
{
	if(pUnit->canCoexistWithEnemyUnit(eOtherTeam))
	{
		return false;
	}

	return (pUnit->getTeam() != eOtherTeam);
}

//	-----------------------------------------------------------------------------------------------
bool CvPlot::isVisibleEnemyDefender(const CvUnit* pUnit) const
{
	TeamTypes eTeam = GET_PLAYER(pUnit->getOwner()).getTeam();

	if (!isVisible(eTeam))
		return false;

	ASSERT_DEBUG(pUnit, "Source unit must be valid");
	const IDInfo* pUnitNode = m_units.head();
	if(pUnit && pUnitNode)
	{
		bool bAlwaysHostile = pUnit->isAlwaysHostile(*this);

		do
		{
			const CvUnit* pLoopUnit = GetPlayerUnit(*pUnitNode);
			pUnitNode = m_units.next(pUnitNode);

			if(pLoopUnit)
			{
				if(pLoopUnit->IsCanDefend() && isEnemy(pLoopUnit, eTeam, bAlwaysHostile))
				{
					return true;
				}
			}
		}
		while(pUnitNode != NULL);
	}

	return false;
}

//	-----------------------------------------------------------------------------------------------
//  ignores embarked units!
//	-----------------------------------------------------------------------------------------------
CvUnit* CvPlot::getVisibleEnemyDefender(PlayerTypes ePlayer) const
{
	TeamTypes eTeam = GET_PLAYER(ePlayer).getTeam();

	if (!isVisible(eTeam))
		return NULL;

	const IDInfo* pUnitNode = m_units.head();
	if(pUnitNode)
	{
		do
		{
			CvUnit* pLoopUnit = GetPlayerUnit(*pUnitNode);
			pUnitNode = m_units.next(pUnitNode);

			if(pLoopUnit && !pLoopUnit->isInvisible(eTeam, false))
			{
				if(pLoopUnit->IsCanDefend() && isEnemy(pLoopUnit, eTeam, false))
				{
					return pLoopUnit;
				}
			}
		}
		while(pUnitNode != NULL);
	}

	return NULL;
}

//	-----------------------------------------------------------------------------------------------
int CvPlot::getNumDefenders(PlayerTypes ePlayer) const
{
	const IDInfo* pUnitNode = m_units.head();
	if(pUnitNode != NULL)
	{
		int iCount = 0;
		do
		{
			const CvUnit* pLoopUnit = GetPlayerUnit(*pUnitNode);
			pUnitNode = m_units.next(pUnitNode);

			if(pLoopUnit)
			{
				if( (ePlayer==NO_PLAYER || pLoopUnit->getOwner()==ePlayer) && pLoopUnit->IsCanDefend() )
				{
					++iCount;
				}
			}
		}
		while(pUnitNode != NULL);
		return iCount;
	}

	return 0;
}
#if defined(MOD_BALANCE_CORE)
//	-----------------------------------------------------------------------------------------------
int CvPlot::getNumNavalDefenders(PlayerTypes ePlayer) const
{
	const IDInfo* pUnitNode = m_units.head();
	if(pUnitNode != NULL)
	{
		int iCount = 0;
		do
		{
			const CvUnit* pLoopUnit = GetPlayerUnit(*pUnitNode);
			pUnitNode = m_units.next(pUnitNode);

			if(pLoopUnit)
			{
				if( (ePlayer==NO_PLAYER || pLoopUnit->getOwner()==ePlayer) && pLoopUnit->IsCanDefend() && !pLoopUnit->isEmbarked() && (pLoopUnit->getDomainType() != DOMAIN_SEA))
				{
					++iCount;
				}
			}
		}
		while(pUnitNode != NULL);
		return iCount;
	}

	return 0;
}
#endif
//	-----------------------------------------------------------------------------------------------
int CvPlot::getNumVisibleEnemyDefenders(const CvUnit* pUnit) const
{
	TeamTypes eTeam = GET_PLAYER(pUnit->getOwner()).getTeam();

	if (!isVisible(eTeam))
		return 0;

	ASSERT_DEBUG(pUnit, "Source unit must be valid");
	const IDInfo* pUnitNode = m_units.head();
	if(pUnit && pUnitNode)
	{
		bool bAlwaysHostile = pUnit->isAlwaysHostile(*this);
		int iCount = 0;

		do
		{
			const CvUnit* pLoopUnit = GetPlayerUnit(*pUnitNode);
			pUnitNode = m_units.next(pUnitNode);

			if(pLoopUnit && !pLoopUnit->isInvisible(eTeam, false) && !pLoopUnit->IsDead())
			{
				if(pLoopUnit->IsCanDefend() && isEnemy(pLoopUnit, eTeam, bAlwaysHostile))
				{
					++iCount;
				}
			}
		}
		while(pUnitNode != NULL);
		return iCount;
	}
	return 0;
}

int CvPlot::getNumUnitsOfAIType(UnitAITypes eType, PlayerTypes ePlayer) const
{
	const IDInfo* pUnitNode = m_units.head();
	int iCount = 0;
	while(pUnitNode != NULL)
	{
		const CvUnit* pLoopUnit = GetPlayerUnit(*pUnitNode);
		pUnitNode = m_units.next(pUnitNode);

		if(pLoopUnit && pLoopUnit->AI_getUnitAIType()==eType)
		if(ePlayer==NO_PLAYER || pLoopUnit->getOwner()==ePlayer)
			++iCount;
	}

	return iCount;
}

//	-----------------------------------------------------------------------------------------------
CvUnit* CvPlot::getFirstUnitOfAITypeSameTeam(TeamTypes eTeam, UnitAITypes eType) const
{
	const IDInfo* pUnitNode = m_units.head();
	while (pUnitNode)
	{
		CvUnit* pLoopUnit = GetPlayerUnit(*pUnitNode);
		pUnitNode = m_units.next(pUnitNode);

		if (pLoopUnit && pLoopUnit->AI_getUnitAIType() == eType)
			if (eTeam == NO_TEAM || pLoopUnit->getTeam() == eTeam)
				return pLoopUnit;
	}

	return NULL;
}

//	-----------------------------------------------------------------------------------------------
CvUnit* CvPlot::getFirstUnitOfAITypeOtherTeam(TeamTypes eTeam, UnitAITypes eType) const
{
	const IDInfo* pUnitNode = m_units.head();
	while (pUnitNode)
	{
		CvUnit* pLoopUnit = GetPlayerUnit(*pUnitNode);
		pUnitNode = m_units.next(pUnitNode);

		if (pLoopUnit && pLoopUnit->AI_getUnitAIType() == eType)
			if (eTeam == NO_TEAM || pLoopUnit->getTeam() != eTeam)
				return pLoopUnit;
	}

	return NULL;
}

//	-----------------------------------------------------------------------------------------------
bool CvPlot::isVisibleEnemyUnit(PlayerTypes ePlayer) const
{
	TeamTypes eTeam = GET_PLAYER(ePlayer).getTeam();

	if (!isVisible(eTeam))
		return false;

	ASSERT_DEBUG(ePlayer != NO_PLAYER, "Source player must be valid");
	const IDInfo* pUnitNode = m_units.head();
	if(pUnitNode)
	{
		do
		{
			//performance optimization, avoid unit lookup if same player
			if (pUnitNode->eOwner == ePlayer)
			{
				pUnitNode = m_units.next(pUnitNode);
				continue;
			}

			const CvUnit* pLoopUnit = GetPlayerUnit(*pUnitNode);
			pUnitNode = m_units.next(pUnitNode);

			if(pLoopUnit && !pLoopUnit->isInvisible(eTeam, false) && !pLoopUnit->IsDead())
			{
				if(isEnemy(pLoopUnit, eTeam, false))
				{
					return true;
				}
			}
		}
		while(pUnitNode != NULL);
	}

	return false;
}

//	-----------------------------------------------------------------------------------------------
bool CvPlot::isVisibleEnemyUnit(const CvUnit* pUnit) const
{
	TeamTypes eTeam = GET_PLAYER(pUnit->getOwner()).getTeam();

	if (!isVisible(eTeam))
		return false;

	ASSERT_DEBUG(pUnit, "Source unit must be valid");
	const IDInfo* pUnitNode = m_units.head();
	if(pUnit && pUnitNode)
	{
		bool bAlwaysHostile = pUnit->isAlwaysHostile(*this);

		do
		{
			//performance optimization, avoid unit lookup if same player
			if (pUnitNode->eOwner == pUnit->getOwner())
			{
				pUnitNode = m_units.next(pUnitNode);
				continue;
			}

			const CvUnit* pLoopUnit = GetPlayerUnit(*pUnitNode);
			pUnitNode = m_units.next(pUnitNode);

			if(pLoopUnit && !pLoopUnit->isInvisible(eTeam, false) && !pLoopUnit->IsDead())
			{
				if(isEnemy(pLoopUnit, eTeam, bAlwaysHostile))
				{
					return true;
				}
			}
		}
		while(pUnitNode != NULL);
	}

	return false;
}

//	-----------------------------------------------------------------------------------------------
bool CvPlot::isVisibleOtherUnit(PlayerTypes ePlayer) const
{
	TeamTypes eTeam = GET_PLAYER(ePlayer).getTeam();

	if (!isVisible(eTeam))
		return false;

	ASSERT_DEBUG(ePlayer != NO_PLAYER, "Source player must be valid");
	const IDInfo* pUnitNode = m_units.head();
	if(pUnitNode)
	{
		do
		{
			const CvUnit* pLoopUnit = GetPlayerUnit(*pUnitNode);
			pUnitNode = m_units.next(pUnitNode);

			if(pLoopUnit && !pLoopUnit->isInvisible(eTeam, false) && !pLoopUnit->IsDead())
			{
				if(isOtherTeam(pLoopUnit, eTeam))
				{
					return true;
				}
			}
		}
		while(pUnitNode != NULL);
	}

	return false;
}

bool CvPlot::isEnemyUnit(PlayerTypes ePlayer, bool bCombatOnly, bool bCheckVisibility, bool bIgnoreBarbs, bool bIgnoreEmbarked) const
{
	TeamTypes eTeam = GET_PLAYER(ePlayer).getTeam();

	if (bCheckVisibility && !isVisible(eTeam))
		return false;

	ASSERT_DEBUG(ePlayer != NO_PLAYER, "Source player must be valid");
	const IDInfo* pUnitNode = m_units.head();
	if(pUnitNode)
	{
		do
		{
			//performance optimization: skip the expensive unit lookup for our own units
			if (pUnitNode->eOwner == ePlayer)
			{
				pUnitNode = m_units.next(pUnitNode);
				continue;
			}

			const CvUnit* pLoopUnit = GetPlayerUnit(*pUnitNode);
			pUnitNode = m_units.next(pUnitNode);

			if(pLoopUnit && !pLoopUnit->isInvisible(eTeam, false) && !pLoopUnit->IsDead())
			{
				//airplanes not included
				if (bCombatOnly && !pLoopUnit->IsCanDefend())
					continue;

				if (bIgnoreBarbs && pLoopUnit->isBarbarian())
					continue;

				if (bIgnoreEmbarked && !pLoopUnit->isNativeDomain(pLoopUnit->plot()))
					continue;

				if(isOtherTeam(pLoopUnit, eTeam) && isEnemy(pLoopUnit,eTeam,false))
					return true;
			}
		}
		while(pUnitNode != NULL);
	}

	return false;
}


bool CvPlot::isNeutralUnit(PlayerTypes ePlayer, bool bCombat, bool bCheckVisibility, bool bIgnoreMinors) const
{
	TeamTypes eTeam = GET_PLAYER(ePlayer).getTeam();

	if (bCheckVisibility && !isVisible(eTeam))
		return false;

	ASSERT_DEBUG(ePlayer != NO_PLAYER, "Source player must be valid");
	const IDInfo* pUnitNode = m_units.head();
	if(pUnitNode)
	{
		do
		{
			const CvUnit* pLoopUnit = GetPlayerUnit(*pUnitNode);
			pUnitNode = m_units.next(pUnitNode);

			if(pLoopUnit && !pLoopUnit->isInvisible(eTeam, false) && !pLoopUnit->IsDead())
			{
				//airplanes not included
				if (bCombat && !pLoopUnit->IsCanDefend())
					continue;

				if (bIgnoreMinors && GET_PLAYER(pLoopUnit->getOwner()).isMinorCiv())
					continue;

				if(isOtherTeam(pLoopUnit, eTeam) && !isEnemy(pLoopUnit,eTeam,false))
					return true;
			}
		}
		while(pUnitNode != NULL);
	}

	return false;
}

bool CvPlot::isNeutralUnitAdjacent(PlayerTypes ePlayer, bool bCombat, bool bCheckVisibility, bool bIgnoreMinors) const
{
	CvPlot** aPlotsToCheck = GC.getMap().getNeighborsUnchecked(this);
	for (int iI = 0; iI < NUM_DIRECTION_TYPES; iI++)
	{
		CvPlot* pAdjacentPlot = aPlotsToCheck[iI];
		if (pAdjacentPlot != NULL)
			if (pAdjacentPlot->isNeutralUnit(ePlayer, bCombat, bCheckVisibility, bIgnoreMinors))
				return true;
	}

	return false;
}

bool CvPlot::isFriendlyUnit(PlayerTypes ePlayer, bool bCombatOnly, bool bSamePlayer) const
{
	TeamTypes eTeam = GET_PLAYER(ePlayer).getTeam();

	//can't have a friendly unit there if it's not visible
	if (!isVisible(eTeam))
		return false;

	ASSERT_DEBUG(ePlayer != NO_PLAYER, "Source player must be valid");
	const IDInfo* pUnitNode = m_units.head();
	if(pUnitNode)
	{
		do
		{
			const CvUnit* pLoopUnit = GetPlayerUnit(*pUnitNode);
			pUnitNode = m_units.next(pUnitNode);

			if(pLoopUnit && !pLoopUnit->isInvisible(eTeam, false) && !pLoopUnit->IsDead())
			{
				//airplanes not included
				if (bCombatOnly && (!pLoopUnit->IsCanDefend() || !pLoopUnit->isNativeDomain(this)))
					continue;

				if(bSamePlayer && pLoopUnit->getOwner()==ePlayer)
					return true;

				if (!bSamePlayer && pLoopUnit->getTeam() == eTeam)
					return true;
			}
		}
		while(pUnitNode != NULL);
	}

	return false;
}

//	---------------------------------------------------------------------------
//	Return true if any fighting is occurring in the plot.
bool CvPlot::isFighting() const
{
	return (isUnitFighting() || isCityFighting());
}

//	---------------------------------------------------------------------------
//	Return true if any units are fighting in the plot.
bool CvPlot::isUnitFighting() const
{
	const IDInfo* pUnitNode = m_units.head();
	if(pUnitNode)
	{
		do
		{
			const CvUnit* pLoopUnit = GetPlayerUnit(*pUnitNode);
			pUnitNode = m_units.next(pUnitNode);

			if(pLoopUnit && pLoopUnit->isFighting())
			{
				return true;
			}
		}
		while(pUnitNode != NULL);
	}

	return false;
}

//	---------------------------------------------------------------------------
//	Return true if any cities are fighting in the plot.
bool CvPlot::isCityFighting() const
{
	CvCity* pkCity = getPlotCity();
	if(pkCity)
		return pkCity->isFighting();

	return false;
}

//	---------------------------------------------------------------------------
bool CvPlot::canHaveFeature(FeatureTypes eFeature) const
{
	ASSERT_DEBUG(getTerrainType() != NO_TERRAIN, "TerrainType is not assigned a valid value");

	if(eFeature == NO_FEATURE)
	{
		return true;
	}

	if(getFeatureType() != NO_FEATURE)
	{
		return false;
	}

	if(isMountain())
	{
		return false;
	}

	if(isCity())
	{
		return false;
	}

	CvFeatureInfo* pkFeature = GC.getFeatureInfo(eFeature);
	if(pkFeature == NULL)
	{
		return false;
	}

	if(!pkFeature->isTerrain(getTerrainType()))
	{
		return false;
	}

	if(pkFeature->isNoCoast() && isCoastalLand())
	{
		return false;
	}

	if(pkFeature->isNoRiver() && isRiver())
	{
		return false;
	}

	if(pkFeature->isRequiresFlatlands() && isHills())
	{
		return false;
	}

	if(pkFeature->isNoAdjacent())
	{
		for(int iI = 0; iI < NUM_DIRECTION_TYPES; ++iI)
		{
			CvPlot* pAdjacentPlot = plotDirection(getX(), getY(), ((DirectionTypes)iI));

			if(pAdjacentPlot != NULL)
			{
				if(pAdjacentPlot->getFeatureType() == eFeature)
				{
					return false;
				}
			}
		}
	}

	if(!isRiver() && pkFeature->isRequiresRiver())
	{
		return false;
	}

	return true;
}


//	--------------------------------------------------------------------------------
bool CvPlot::isRoute() const
{
	return ((RouteTypes)m_eRouteType != NO_ROUTE);
}


//	--------------------------------------------------------------------------------
bool CvPlot::isValidRoute(const CvUnit* pUnit) const
{
	if((RouteTypes)m_eRouteType != NO_ROUTE && !m_bRoutePillaged)
	{
		if (!pUnit)
			return true;

		if (pUnit->getDomainType() == getDomain())
			return !pUnit->isEnemy(getTeam(), this) || pUnit->isEnemyRoute();
	}

	return false;
}

//	--------------------------------------------------------------------------------
void CvPlot::SetCityConnection(PlayerTypes ePlayer, bool bActive, bool bIndustrial)
{
	if (ePlayer == NO_PLAYER)
		return;

	if( GET_PLAYER(ePlayer).UpdateCityConnection(this,bActive,bIndustrial) )
	{
		for(int iI = 0; iI < MAX_TEAMS; ++iI)
		{
			if ( GET_TEAM((TeamTypes)iI).isObserver() || ((GET_TEAM((TeamTypes)iI).isAlive()) && GC.getGame().getActiveTeam() == (TeamTypes)iI) )
			{
				if(isVisible((TeamTypes)iI))
				{
					setLayoutDirty(true);
				}
			}
		}
#if defined(MOD_BALANCE_CORE)
		updateYield();
#endif
	}
}


//	--------------------------------------------------------------------------------
bool CvPlot::IsCityConnection(PlayerTypes ePlayer, bool bIndustrial) const
{
	if (ePlayer == NO_PLAYER)
		ePlayer = getOwner();

	//hack: for unowned plots, pretend there is no connection
	if (ePlayer == NO_PLAYER)
		return false;

	return GET_PLAYER(ePlayer).IsCityConnectionPlot(this, bIndustrial);
}

#if defined(MOD_BALANCE_CORE)
//	--------------------------------------------------------------------------------
void CvPlot::ChangeNumTradeUnitRoute(int iChange)
{
	m_iNumTradeUnitRoute = m_iNumTradeUnitRoute + iChange;
}
void CvPlot::SetNumTradeUnitRoute(int iNewValue)
{
	m_iNumTradeUnitRoute = iNewValue;
}
int CvPlot::GetNumTradeUnitRoute() const
{
	return m_iNumTradeUnitRoute;
}

//	--------------------------------------------------------------------------------
bool CvPlot::IsTradeUnitRoute() const
{
	return GetNumTradeUnitRoute() > 0;
}
#endif

//	--------------------------------------------------------------------------------
bool CvPlot::at(int iX, int iY) const
{
	return ((getX() == iX) && (getY() == iY));
}


//	--------------------------------------------------------------------------------
int CvPlot::getLatitude() const
{
	int iLatitude = 0;

	if(GC.getMap().isWrapX() || !(GC.getMap().isWrapY()))
	{
		iLatitude = ((getY() * 100) / GC.getMap().getGridHeight());
	}
	else
	{
		iLatitude = ((getX() * 100) / GC.getMap().getGridWidth());
	}

	iLatitude = ((iLatitude * (GC.getMap().getTopLatitude() - GC.getMap().getBottomLatitude())) / 100);

	return abs(iLatitude + GC.getMap().getBottomLatitude());
}


//	--------------------------------------------------------------------------------
CvArea* CvPlot::area() const
{
	return GC.getMap().getAreaById(getArea());
}


//	--------------------------------------------------------------------------------
const std::vector<int>& CvPlot::getAllAdjacentAreas() const
{
	static std::vector<int> result;
	//better safe than sorry
	gDLL->GetGameCoreLock();
	result.clear();

	CvPlot** aNeighbors = GC.getMap().getNeighborsUnchecked(this);
	for(int iI = 0; iI < NUM_DIRECTION_TYPES; ++iI)
	{
		CvPlot* pAdjacentPlot = aNeighbors[iI];
		//exclude our own area! also, simply deduplication, but no guarantee for unique values in result
		if(pAdjacentPlot != NULL && pAdjacentPlot->getArea() != getArea())
			if (result.empty() || result.back()!=pAdjacentPlot->getArea())
				result.push_back(pAdjacentPlot->getArea());
	}

	gDLL->ReleaseGameCoreLock();
	return result;
}

bool CvPlot::hasSharedAdjacentArea(const CvPlot* pOther, bool bAllowLand, bool bAllowWater) const
{
	if (!pOther)
		return false;

	std::vector<int> myAreas = getAllAdjacentAreas();
	std::vector<int> theirAreas = pOther->getAllAdjacentAreas();

	//fancy stl version works only under conditions
	if (bAllowLand && bAllowWater)
	{
		std::vector<int> shared(MAX(myAreas.size(), theirAreas.size()));
		std::vector<int>::iterator result = std::set_intersection(myAreas.begin(), myAreas.end(), theirAreas.begin(), theirAreas.end(), shared.begin());
		return (result != shared.begin());
	}

	//manual version
	for (vector<int>::iterator i1 = myAreas.begin(); i1 != myAreas.end(); ++i1)
	{
		CvArea* a1 = GC.getMap().getAreaById(*i1);
		if (!bAllowWater && a1->isWater())
			continue;
		if (!bAllowLand && !a1->isWater())
			continue;

		for (vector<int>::iterator i2 = theirAreas.begin(); i2 != theirAreas.end(); ++i2)
		{
			CvArea* a2 = GC.getMap().getAreaById(*i2);

			//don't need to check for water/land again, id is enough
			if (a1->GetID() == a2->GetID())
				return true;
		}
	}

	return false;
}
	

//	--------------------------------------------------------------------------------
void CvPlot::setArea(int iNewValue)
{
	if(getArea() != iNewValue)
	{
		if(area() != NULL)
		{
			processArea(area(), -1);
		}

		if (MOD_EVENTS_TERRAFORMING)
		{
			GAMEEVENTINVOKE_HOOK(GAMEEVENT_TerraformingPlot, TERRAFORMINGEVENT_AREA, m_iX, m_iY, 0, iNewValue, m_iArea, -1, -1);
		}

		m_iArea = iNewValue;

		if(area() != NULL)
		{
			processArea(area(), 1);
			updateYield();
		}
	}
}

//	--------------------------------------------------------------------------------
void CvPlot::setLandmass(int iNewValue)
{
	if(m_iLandmass != iNewValue)
	{
		// cleanup old one
		CvLandmass* pLandmass = GC.getMap().getLandmassById(m_iLandmass);
		if(pLandmass != NULL)
		{
			pLandmass->changeNumTiles(-1);
			pLandmass->ChangeCentroidX(-m_iX);
			pLandmass->ChangeCentroidY(-m_iY);
		}

#if defined(MOD_EVENTS_TERRAFORMING)
		if (MOD_EVENTS_TERRAFORMING) {
			GAMEEVENTINVOKE_HOOK(GAMEEVENT_TerraformingPlot, TERRAFORMINGEVENT_LANDMASS, m_iX, m_iY, 0, iNewValue, m_iLandmass, -1, -1);
		}
#endif

		m_iLandmass = iNewValue;

		pLandmass = GC.getMap().getLandmassById(m_iLandmass);
		if(pLandmass != NULL)
		{
			pLandmass->changeNumTiles(1);
			pLandmass->ChangeCentroidX(m_iX);
			pLandmass->ChangeCentroidY(m_iY);
		}
	}
}

CvLandmass * CvPlot::landmass() const
{
	return GC.getMap().getLandmassById(m_iLandmass);
}


//	--------------------------------------------------------------------------------
void CvPlot::setContinent(int iNewValue)
{
	if (m_iContinent != iNewValue)
	{
		// cleanup old one
		CvContinent* pContinent = GC.getMap().getContinentById(m_iContinent);
		if (pContinent != NULL)
		{
			pContinent->changeNumTiles(-1);
			pContinent->ChangeCentroidX(-m_iX);
			pContinent->ChangeCentroidY(-m_iY);
		}

		m_iContinent = iNewValue;

		pContinent = GC.getMap().getContinentById(m_iContinent);
		if (pContinent != NULL)
		{
			pContinent->changeNumTiles(1);
			pContinent->ChangeCentroidX(m_iX);
			pContinent->ChangeCentroidY(m_iY);
		}
	}
}

CvContinent* CvPlot::continent() const
{
	return GC.getMap().getContinentById(m_iContinent);
}

//	--------------------------------------------------------------------------------
int CvPlot::GetRiverID(DirectionTypes eDirection) const
{
	return m_vRivers[eDirection];
}

//	--------------------------------------------------------------------------------
void CvPlot::SetRiverID(DirectionTypes eDirection, int iRiverID)
{
	m_vRivers[eDirection] = iRiverID;
}

//	--------------------------------------------------------------------------------
CvRiver* CvPlot::GetRiver(DirectionTypes eDirection) const
{
	return GC.getMap().GetRiverById(m_vRivers[eDirection]);
}

//	--------------------------------------------------------------------------------
bool CvPlot::HasSharedRiver(const CvPlot* pOther) const
{
	for (int iI = 0; iI < NUM_DIRECTION_TYPES; iI++)
	{
		int iRiverID = GetRiverID((DirectionTypes)iI);
		if (iRiverID != -1)
		{
			for (int iJ = 0; iJ < NUM_DIRECTION_TYPES; iJ++)
			{
				if (pOther->GetRiverID((DirectionTypes)iJ) == iRiverID)
				{
					return true;
				}
			}
		}
	}
	return false;
}

//	--------------------------------------------------------------------------------
std::vector<int> CvPlot::getAllAdjacentLandmasses() const
{
	std::vector<int> result;

	CvPlot** aNeighbors = GC.getMap().getNeighborsUnchecked(this);
	for(int iI = 0; iI < NUM_DIRECTION_TYPES; ++iI)
	{
		CvPlot* pAdjacentPlot = aNeighbors[iI];
		//exclude our own area! also, simply deduplication, but no guarantee for unique values in result
		if(pAdjacentPlot != NULL && pAdjacentPlot->getLandmass() != getLandmass())
			if (result.empty() || result.back()!=pAdjacentPlot->getLandmass())
				result.push_back(pAdjacentPlot->getLandmass());
	}

	return result;
}

bool CvPlot::hasSharedAdjacentLandmass(const CvPlot* pOther, bool bAllowLand, bool bAllowWater) const
{
	if (!pOther)
		return false;

	std::vector<int> myLandmasses = getAllAdjacentLandmasses();
	std::vector<int> theirLandmasses = pOther->getAllAdjacentLandmasses();

	//fancy stl version works only under conditions
	if (bAllowLand && bAllowWater)
	{
		std::vector<int> shared(MAX(myLandmasses.size(), theirLandmasses.size()));
		std::vector<int>::iterator result = std::set_intersection(myLandmasses.begin(), myLandmasses.end(), theirLandmasses.begin(), theirLandmasses.end(), shared.begin());
		return (result != shared.begin());
	}

	//manual version
	for (vector<int>::iterator i1 = myLandmasses.begin(); i1 != myLandmasses.end(); ++i1)
	{
		CvLandmass* a1 = GC.getMap().getLandmassById(*i1);
		if (!bAllowWater && a1->isWater())
			continue;
		if (!bAllowLand && !a1->isWater())
			continue;

		for (vector<int>::iterator i2 = theirLandmasses.begin(); i2 != theirLandmasses.end(); ++i2)
		{
			CvLandmass* a2 = GC.getMap().getLandmassById(*i2);

			//don't need to check for water/land again, id is enough
			if (a1->GetID() == a2->GetID())
				return true;
		}
	}

	return false;
}


//	--------------------------------------------------------------------------------
int CvPlot::getOwnershipDuration() const
{
	return m_iOwnershipDuration;
}


//	--------------------------------------------------------------------------------
bool CvPlot::isOwnershipScore() const
{
	return (getOwnershipDuration() >= /*20*/ GD_INT_GET(OWNERSHIP_SCORE_DURATION_THRESHOLD));
}


//	--------------------------------------------------------------------------------
void CvPlot::setOwnershipDuration(int iNewValue)
{
	bool bOldOwnershipScore = false;

	if(getOwnershipDuration() != iNewValue)
	{
		bOldOwnershipScore = isOwnershipScore();

		m_iOwnershipDuration = iNewValue;
		ASSERT_DEBUG(getOwnershipDuration() >= 0);

		if(bOldOwnershipScore != isOwnershipScore())
		{
			if(isOwned())
			{
				if(!isWater())
				{
					GET_PLAYER(getOwner()).changeTotalLandScored((isOwnershipScore()) ? 1 : -1);
				}
			}
		}
	}
}


//	--------------------------------------------------------------------------------
void CvPlot::changeOwnershipDuration(int iChange)
{
	setOwnershipDuration(getOwnershipDuration() + iChange);
}


//	--------------------------------------------------------------------------------
int CvPlot::getImprovementDuration() const
{
	return m_iImprovementDuration;
}


//	--------------------------------------------------------------------------------
void CvPlot::setImprovementDuration(int iNewValue)
{
	m_iImprovementDuration = iNewValue;
	ASSERT_DEBUG(getImprovementDuration() >= 0);
}


//	--------------------------------------------------------------------------------
void CvPlot::changeImprovementDuration(int iChange)
{
	setImprovementDuration(getImprovementDuration() + iChange);
}


//	--------------------------------------------------------------------------------
int CvPlot::getUpgradeProgress() const
{
	return m_iUpgradeProgress;
}


//	--------------------------------------------------------------------------------
int CvPlot::getUpgradeTimeLeft(ImprovementTypes eImprovement, PlayerTypes ePlayer) const
{
	int iUpgradeLeft = 0;
	int iUpgradeRate = 0;
	int iTurnsLeft = 0;

	iUpgradeLeft = ((100 * GC.getGame().getImprovementUpgradeTime(eImprovement, this)) - ((getImprovementType() == eImprovement) ? getUpgradeProgress() : 0));
	iUpgradeLeft /= 100;

	if(ePlayer == NO_PLAYER)
	{
		return iUpgradeLeft;
	}

	iUpgradeRate = GET_PLAYER(ePlayer).getImprovementUpgradeRate();

	if(iUpgradeRate == 0)
	{
		return iUpgradeLeft;
	}

	// Upgrade rate is stored at 100x
	iTurnsLeft = (100 * iUpgradeLeft / iUpgradeRate);

	if((iTurnsLeft * iUpgradeRate) < iUpgradeLeft)
	{
		iTurnsLeft++;
	}

	return std::max(1, iTurnsLeft);
}


//	--------------------------------------------------------------------------------
void CvPlot::setUpgradeProgress(int iNewValue)
{
	m_iUpgradeProgress = iNewValue;
	ASSERT_DEBUG(getUpgradeProgress() >= 0);
}


//	--------------------------------------------------------------------------------
void CvPlot::changeUpgradeProgress(int iChange)
{
	setUpgradeProgress(getUpgradeProgress() + iChange);
}

//	--------------------------------------------------------------------------------
fraction CvPlot::ComputeFractionalYieldFromAdjacentImprovement(CvImprovementEntry& kImprovement, YieldTypes eYield) const
{
	CvPlot* pAdjacentPlot = NULL;
	fraction fRtnValue = 0;

	if (kImprovement.IsYieldPerXAdjacentImprovement(eYield))
	{
		for(int iI = 0; iI < NUM_DIRECTION_TYPES; iI++)
		{
			pAdjacentPlot = plotDirection(getX(), getY(), static_cast<DirectionTypes>(iI));
			if(pAdjacentPlot && !pAdjacentPlot->IsImprovementPillaged())
			{
				ImprovementTypes eAdjacentImprovement = pAdjacentPlot->getImprovementType();
				if (eAdjacentImprovement != NO_IMPROVEMENT)
				{
					fRtnValue += kImprovement.GetYieldPerXAdjacentImprovement(eYield, eAdjacentImprovement);
				}
			}
		}
	}

	return fRtnValue;
}

//	--------------------------------------------------------------------------------
fraction CvPlot::ComputeFractionalYieldFromAdjacentTerrain(CvImprovementEntry& kImprovement, YieldTypes eYield) const
{
	CvPlot* pAdjacentPlot = NULL;
	fraction fRtnValue = 0;

	if (kImprovement.IsYieldPerXAdjacentTerrain(eYield))
	{
		for (int iI = 0; iI < NUM_DIRECTION_TYPES; iI++)
		{
			pAdjacentPlot = plotDirection(getX(), getY(), static_cast<DirectionTypes>(iI));

			if (!pAdjacentPlot)
				continue;

			fRtnValue += kImprovement.GetYieldPerXAdjacentTerrain(eYield, pAdjacentPlot->getTerrainType());
		}
	}

	return fRtnValue;
}

int CvPlot::ComputeYieldFromAdjacentResource(CvImprovementEntry& kImprovement, YieldTypes eYield, TeamTypes eTeam) const
{
	CvPlot* pAdjacentPlot = NULL;
	int iRtnValue = 0;

	for(int iJ = 0; iJ < GC.getNumResourceInfos(); iJ++)
	{
		ResourceTypes eResource = (ResourceTypes)iJ;
		if(eResource != NO_RESOURCE)
		{
			if(kImprovement.GetAdjacentResourceYieldChanges(eResource, eYield) > 0)
			{
				for(int iI = 0; iI < NUM_DIRECTION_TYPES; iI++)
				{
					pAdjacentPlot = plotDirection(getX(), getY(), ((DirectionTypes)iI));
					if(pAdjacentPlot && pAdjacentPlot->getResourceType(eTeam) == eResource)
					{
						iRtnValue += kImprovement.GetAdjacentResourceYieldChanges(eResource, eYield);
					}
				}
			}
		}
	}

	return iRtnValue;
}
int CvPlot::ComputeYieldFromAdjacentTerrain(CvImprovementEntry& kImprovement, YieldTypes eYield) const
{
	CvPlot* pAdjacentPlot = NULL;
	int iRtnValue = 0;

	for(int iJ = 0; iJ < GC.getNumTerrainInfos(); iJ++)
	{
		TerrainTypes eTerrain = (TerrainTypes)iJ;
		if(eTerrain != NO_TERRAIN)
		{
			if(kImprovement.GetAdjacentTerrainYieldChanges(eTerrain, eYield) > 0)
			{
				for(int iI = 0; iI < NUM_DIRECTION_TYPES; iI++)
				{
					pAdjacentPlot = plotDirection(getX(), getY(), ((DirectionTypes)iI));
					if(pAdjacentPlot && pAdjacentPlot->getTerrainType() == eTerrain)
					{
						iRtnValue += kImprovement.GetAdjacentTerrainYieldChanges(eTerrain, eYield);
					}
				}
			}
		}
	}

	return iRtnValue;
}

int CvPlot::ComputeYieldFromAdjacentFeature(CvImprovementEntry& kImprovement, YieldTypes eYield) const
{
	CvPlot* pAdjacentPlot = NULL;
	int iRtnValue = 0;

	for(int iJ = 0; iJ < GC.getNumFeatureInfos(); iJ++)
	{
		FeatureTypes eFeature = (FeatureTypes)iJ;
		if (eFeature != NO_FEATURE)
		{
			if (kImprovement.GetAdjacentFeatureYieldChanges(eFeature, eYield) > 0)
			{
				for(int iI = 0; iI < NUM_DIRECTION_TYPES; iI++)
				{
					pAdjacentPlot = plotDirection(getX(), getY(), ((DirectionTypes)iI));
					if (pAdjacentPlot && pAdjacentPlot->getFeatureType() == eFeature)
					{
						iRtnValue += kImprovement.GetAdjacentFeatureYieldChanges(eFeature, eYield);
					}
				}
			}
		}
	}

	return iRtnValue;
}

//	--------------------------------------------------------------------------------
#if defined(MOD_GLOBAL_STACKING_RULES)
int CvPlot::getStackingUnits() const
{
    const IDInfo* pUnitNode = headUnitNode();
    CvUnit* pLoopUnit = NULL;

    while(pUnitNode != NULL)
    {
      pLoopUnit = GetPlayerUnit(*pUnitNode);
      pUnitNode = nextUnitNode(pUnitNode);

      if(pLoopUnit && pLoopUnit->getNumberStackingUnits() > 0)
      {
        return pLoopUnit->getNumberStackingUnits();
      }
    }
    return 0;
}
//	--------------------------------------------------------------------------------
int CvPlot::getAdditionalUnitsFromImprovement() const
{
	if(MOD_GLOBAL_STACKING_RULES)
	{
		return m_eUnitIncrement;
	}

	return 0;
}

//	--------------------------------------------------------------------------------
void CvPlot::calculateAdditionalUnitsFromImprovement()
{
	if(MOD_GLOBAL_STACKING_RULES)
	{
		if (getImprovementType() != NO_IMPROVEMENT)
		{
			if (IsImprovementPillaged())
			{
				m_eUnitIncrement = 0;
			}
			else
			{
				m_eUnitIncrement = GC.getImprovementInfo(getImprovementType())->GetAdditionalUnits();
			}
		}
		else
		{
			m_eUnitIncrement = 0;
		}
	}
}
#endif

//	--------------------------------------------------------------------------------
int CvPlot::getNumMajorCivsRevealed() const
{
	return m_iNumMajorCivsRevealed;
}

//	--------------------------------------------------------------------------------
void CvPlot::setNumMajorCivsRevealed(int iNewValue)
{
	m_iNumMajorCivsRevealed = iNewValue;
	ASSERT_DEBUG(getNumMajorCivsRevealed() >= 0);
}


//	--------------------------------------------------------------------------------
void CvPlot::changeNumMajorCivsRevealed(int iChange)
{
	setNumMajorCivsRevealed(getNumMajorCivsRevealed() + iChange);
}


//	--------------------------------------------------------------------------------
int CvPlot::getCityRadiusCount() const
{
	return m_iCityRadiusCount;
}


//	--------------------------------------------------------------------------------
int CvPlot::isCityRadius() const
{
	return (getCityRadiusCount() > 0);
}


//	--------------------------------------------------------------------------------
void CvPlot::changeCityRadiusCount(int iChange)
{
	m_iCityRadiusCount = (m_iCityRadiusCount + iChange);
	ASSERT_DEBUG(getCityRadiusCount() >= 0);
}


//	--------------------------------------------------------------------------------
bool CvPlot::isStartingPlot() const
{
	return m_bStartingPlot;
}


//	--------------------------------------------------------------------------------
void CvPlot::setStartingPlot(bool bNewValue)
{
	m_bStartingPlot = bNewValue;
}


//	--------------------------------------------------------------------------------
bool CvPlot::isNEOfRiver() const
{
	return m_bNEOfRiver;
}


//	--------------------------------------------------------------------------------
void CvPlot::setNEOfRiver(bool bNewValue, FlowDirectionTypes eRiverDir)
{
	// Nothing is being changed here
	if (isNEOfRiver() == bNewValue && eRiverDir == getRiverSWFlowDirection())
		return;

	ASSERT_DEBUG(eRiverDir == FLOWDIRECTION_SOUTHEAST || eRiverDir == FLOWDIRECTION_NORTHWEST || eRiverDir == NO_FLOWDIRECTION, "setting invalid flow direction");

	// This adds or removes a river segment
	if (isNEOfRiver() != bNewValue)
	{
		if (MOD_EVENTS_TERRAFORMING)
		{
			// Not sure if this works. The parameters for TERRAFORMINGEVENT_RIVER seem to be different from other terraform types.
			GAMEEVENTINVOKE_HOOK(GAMEEVENT_TerraformingPlot, TERRAFORMINGEVENT_RIVER, m_iX, m_iY, DIRECTION_NORTHEAST, bNewValue, isNEOfRiver(), eRiverDir, getRiverSWFlowDirection());
		}

		m_bNEOfRiver = bNewValue;

		updateRiverCrossing(DIRECTION_SOUTHWEST);
		updateYield();

		// Also update the plot across the river (if exists)
		CvPlot* pAdjacentPlot = plotDirection(getX(), getY(), DIRECTION_SOUTHWEST);
		if (pAdjacentPlot)
		{
			pAdjacentPlot->updateRiverCrossing(DIRECTION_NORTHEAST);
			pAdjacentPlot->updateYield();
		}

		if (area())
		{
			area()->changeNumRiverEdges(bNewValue ? 1 : -1);
		}
	}

	m_eRiverSWFlowDirection = eRiverDir;
}


//	--------------------------------------------------------------------------------
bool CvPlot::isWOfRiver() const
{
	return m_bWOfRiver;
}


//	--------------------------------------------------------------------------------
void CvPlot::setWOfRiver(bool bNewValue, FlowDirectionTypes eRiverDir)
{
	// Nothing is being changed here
	if (isWOfRiver() == bNewValue && eRiverDir == getRiverEFlowDirection())
		return;

	ASSERT_DEBUG(eRiverDir == FLOWDIRECTION_NORTH || eRiverDir == FLOWDIRECTION_SOUTH || eRiverDir == NO_FLOWDIRECTION, "setting invalid flow direction");

	// This adds or removes a river segment
	if (isWOfRiver() != bNewValue)
	{
		if (MOD_EVENTS_TERRAFORMING)
		{
			// Not sure if this works. The parameters for TERRAFORMINGEVENT_RIVER seem to be different from other terraform types.
			GAMEEVENTINVOKE_HOOK(GAMEEVENT_TerraformingPlot, TERRAFORMINGEVENT_RIVER, m_iX, m_iY, DIRECTION_WEST, bNewValue, isWOfRiver(), eRiverDir, getRiverEFlowDirection());
		}

		m_bWOfRiver = bNewValue;

		updateRiverCrossing(DIRECTION_EAST);
		updateYield();

		// Also update the plot across the river (if exists)
		CvPlot* pAdjacentPlot = plotDirection(getX(), getY(), DIRECTION_EAST);
		if (pAdjacentPlot)
		{
			pAdjacentPlot->updateRiverCrossing(DIRECTION_WEST);
			pAdjacentPlot->updateYield();
		}

		if (area())
		{
			area()->changeNumRiverEdges(bNewValue ? 1 : -1);
		}
	}

	m_eRiverEFlowDirection = eRiverDir;
}


//	--------------------------------------------------------------------------------
bool CvPlot::isNWOfRiver() const
{
	return m_bNWOfRiver;
}


//	--------------------------------------------------------------------------------
void CvPlot::setNWOfRiver(bool bNewValue, FlowDirectionTypes eRiverDir)
{
	// Nothing is being changed here
	if (isNWOfRiver() == bNewValue && eRiverDir == getRiverSEFlowDirection())
		return;

	ASSERT_DEBUG(eRiverDir == FLOWDIRECTION_NORTHEAST || eRiverDir == FLOWDIRECTION_SOUTHWEST || eRiverDir == NO_FLOWDIRECTION, "setting invalid flow direction");

	// This adds or removes a river segment
	if (isNWOfRiver() != bNewValue)
	{
		if (MOD_EVENTS_TERRAFORMING)
		{
			// Not sure if this works. The parameters for TERRAFORMINGEVENT_RIVER seem to be different from other terraform types.
			GAMEEVENTINVOKE_HOOK(GAMEEVENT_TerraformingPlot, TERRAFORMINGEVENT_RIVER, m_iX, m_iY, DIRECTION_NORTHWEST, bNewValue, isNWOfRiver(), eRiverDir, getRiverSEFlowDirection());
		}

		m_bNWOfRiver = bNewValue;

		updateRiverCrossing(DIRECTION_SOUTHEAST);
		updateYield();

		// Also update the plot across the river (if exists)
		CvPlot* pAdjacentPlot = plotDirection(getX(), getY(), DIRECTION_SOUTHEAST);
		if (pAdjacentPlot)
		{
			pAdjacentPlot->updateRiverCrossing(DIRECTION_NORTHWEST);
			pAdjacentPlot->updateYield();
		}

		if (area())
		{
			area()->changeNumRiverEdges(bNewValue ? 1 : -1);
		}
	}

	m_eRiverSEFlowDirection = eRiverDir;
}


//	--------------------------------------------------------------------------------
FlowDirectionTypes CvPlot::getRiverEFlowDirection() const
{
	return (FlowDirectionTypes)m_eRiverEFlowDirection;
}


//	--------------------------------------------------------------------------------
FlowDirectionTypes CvPlot::getRiverSEFlowDirection() const
{
	return (FlowDirectionTypes)m_eRiverSEFlowDirection;
}

//	--------------------------------------------------------------------------------
FlowDirectionTypes CvPlot::getRiverSWFlowDirection() const
{
	return (FlowDirectionTypes)m_eRiverSWFlowDirection;
}


//	--------------------------------------------------------------------------------
// This function finds an *inland* corner of this plot at which to place a river.
// It then returns the plot with that corner at its SE.

CvPlot* CvPlot::getInlandCorner() const
{
	CvPlot* pRiverPlot = NULL; // will be a plot through whose SE corner we want the river to run
	int aiShuffle[4] = {0,1,2,3};
	shuffleArray(aiShuffle, 4, GC.getGame().getMapRand());

	for (int iI = 0; iI < 4; ++iI)
	{
		switch (aiShuffle[iI])
		{
		case 0:
			pRiverPlot = GC.getMap().plotCheckInvalid(getX(), getY());
			break;
		case 1:
			pRiverPlot = plotDirection(getX(), getY(), DIRECTION_NORTHEAST);
			break;
		case 2:
			pRiverPlot = plotDirection(getX(), getY(), DIRECTION_NORTHWEST);
			break;
		case 3:
			pRiverPlot = plotDirection(getX(), getY(), DIRECTION_WEST);
			break;
		}

		if (pRiverPlot != NULL && !pRiverPlot->hasCoastAtSECorner())
		{
			break;
		}
		else
		{
			pRiverPlot = NULL;
		}
	}

	return pRiverPlot;
}


//	--------------------------------------------------------------------------------
bool CvPlot::hasCoastAtSECorner() const
{
	CvPlot* pAdjacentPlot = NULL;

	if(isWater())
	{
		return true;
	}

	pAdjacentPlot = plotDirection(getX(), getY(), DIRECTION_EAST);
	if(pAdjacentPlot != NULL && pAdjacentPlot->isWater())
	{
		return true;
	}

	pAdjacentPlot = plotDirection(getX(), getY(), DIRECTION_SOUTHEAST);
	return pAdjacentPlot != NULL && pAdjacentPlot->isWater();
}


//	--------------------------------------------------------------------------------
bool CvPlot::isPotentialCityWork() const
{
	return m_bPotentialCityWork;
}


//	--------------------------------------------------------------------------------
bool CvPlot::isPotentialCityWorkForArea(CvArea* pArea) const
{
	for (int iI = 0; iI < MAX_CITY_PLOTS; ++iI)
	{
		CvPlot* pLoopPlot = iterateRingPlots(getX(), getY(), iI);

		if (pLoopPlot)
		{
			if (!pLoopPlot->isWater() || /*0*/ GD_INT_GET(WATER_POTENTIAL_CITY_WORK_FOR_AREA)>0)
			{
				if (pLoopPlot->area() == pArea)
				{
					return true;
				}
			}
		}
	}

	return false;
}

bool CvPlot::IsResourceImprovedForOwner(bool bIgnoreTechPrereq, bool bFoundingCity)
{
	if (!isOwned())
		return false;

	// resource revealed? only check if we do not ignore tech prereqs
	ResourceTypes eResource = getResourceType(bIgnoreTechPrereq ? NO_TEAM : getTeam());
	if (eResource == NO_RESOURCE)
		return false;

	// tech requirements met?
	if (!GET_TEAM(getTeam()).IsResourceImproveable(eResource) && !bIgnoreTechPrereq)
		return false;

	// cities always connect resources
	if (isCity() || bFoundingCity)
		return true;

	ImprovementTypes eImprovement = getImprovementType();
	if (eImprovement == NO_IMPROVEMENT || IsImprovementPillaged())
		return false;

	return GC.getImprovementInfo(eImprovement)->IsConnectsResource(eResource);
}

//	--------------------------------------------------------------------------------
void CvPlot::updatePotentialCityWork()
{
	CvPlot* pLoopPlot = NULL;
	bool bValid = false;
	int iI = 0;

	bValid = false;


	for(iI = 0; iI < MAX_CITY_PLOTS; ++iI)
	{
		pLoopPlot = iterateRingPlots(getX(), getY(), iI);

		if(pLoopPlot != NULL)
		{
			if(!(pLoopPlot->isWater()))
			{
				bValid = true;
				break;
			}
		}
	}

	if(isPotentialCityWork() != bValid)
	{
		m_bPotentialCityWork = bValid;

		updateYield();
	}
}

//	--------------------------------------------------------------------------------
void CvPlot::setOwner(PlayerTypes eNewValue, int iAcquiringCityID, bool bCheckUnits, bool, bool bFoundingCity)
{
	CvString strBuffer;
	int iI = 0;
	ImprovementTypes eLandmarkImprovement = (ImprovementTypes)GC.getInfoTypeForString("IMPROVEMENT_LANDMARK");

	// Remove effects for old owner before changing the member
	if(getOwner() != eNewValue)
	{
		PlayerTypes eOldOwner = getOwner();
		TeamTypes eOldTeam = getTeam();
		PlayerTypes eBuilder = GetPlayerThatBuiltImprovement();
		ImprovementTypes eImprovement = getImprovementType();

		GC.getGame().addReplayMessage(REPLAY_MESSAGE_PLOT_OWNER_CHANGE, eNewValue, "", getX(), getY());

		CvCity* pOldCity = getPlotCity();
		CvCity* pOldOwningCity = getEffectiveOwningCity();

		{
			setOwnershipDuration(0);
			FeatureTypes eFeature = getFeatureType();

			if (eFeature != NO_FEATURE)
			{
				CvFeatureInfo* pFeatureInfo = GC.getFeatureInfo(eFeature);
				if (pFeatureInfo && eNewValue != NO_PLAYER)
				{
					if (pFeatureInfo->getInBorderHappiness() > 0)
						GET_PLAYER(eNewValue).SetNaturalWonderOwned(eFeature, true);

					PromotionTypes eFreePromotion = (PromotionTypes)pFeatureInfo->getPromotionIfOwned();
					if (eFreePromotion != NO_PROMOTION)
					{
						if (!GET_PLAYER(eNewValue).IsFreePromotion(eFreePromotion))
						{
							GET_PLAYER(eNewValue).ChangeFreePromotionCount(eFreePromotion, 1);
						}
					}
				}
			}

			// Plot was owned by someone else
			if (eOldOwner != NO_PLAYER)
			{
				if (eFeature != NO_FEATURE)
				{
					CvFeatureInfo* pFeatureInfo = GC.getFeatureInfo(eFeature);
					if (pFeatureInfo)
					{
						if (pFeatureInfo->getInBorderHappiness() > 0)
							GET_PLAYER(eOldOwner).SetNaturalWonderOwned(eFeature, false);

						PromotionTypes eFreePromotion = (PromotionTypes)pFeatureInfo->getPromotionIfOwned();
						if (eFreePromotion != NO_PROMOTION)
						{
							if (GET_PLAYER(eOldOwner).IsFreePromotion(eFreePromotion))
							{
								GET_PLAYER(eOldOwner).ChangeFreePromotionCount(eFreePromotion, -1);
							}

						}
					}
				}
				changeAdjacentSight(eOldTeam, /*1*/ GD_INT_GET(PLOT_VISIBILITY_RANGE), false, NO_INVISIBLE, NO_DIRECTION);

				// if this tile is owned by a minor share the visibility with my ally
				if (eOldOwner >= MAX_MAJOR_CIVS && eOldOwner != BARBARIAN_PLAYER)
				{
					CvPlayer& thisPlayer = GET_PLAYER(eOldOwner);
					CvMinorCivAI* pMinorCivAI = thisPlayer.GetMinorCivAI();
					if (pMinorCivAI && pMinorCivAI->GetAlly() != NO_PLAYER)
					{
						changeAdjacentSight(GET_PLAYER(pMinorCivAI->GetAlly()).getTeam(), /*1*/ GD_INT_GET(PLOT_VISIBILITY_RANGE), false, NO_INVISIBLE, NO_DIRECTION);
					}
				}

				if (area())
				{
					area()->changeNumOwnedTiles(-1);
				}
				GC.getMap().changeOwnedPlots(-1);

				if (!isWater())
				{
					GET_PLAYER(eOldOwner).changeTotalLand(-1);
					GET_TEAM(eOldTeam).changeTotalLand(-1);

					if (isOwnershipScore())
					{
						GET_PLAYER(eOldOwner).changeTotalLandScored(-1);
					}
				}

				// Improvement is here
				if (eImprovement != NO_IMPROVEMENT)
				{
					GET_PLAYER(eOldOwner).changeImprovementCount(eImprovement, -1, eOldOwner == eBuilder);
					if (pOldOwningCity->GetID() != iAcquiringCityID)
					{
						pOldOwningCity->ChangeImprovementCount(eImprovement, -1);
					}

					// Remove siphoned resources
					CvImprovementEntry* pImprovementInfo = GC.getImprovementInfo(eImprovement);
					if (pImprovementInfo != NULL && pImprovementInfo->GetLuxuryCopiesSiphonedFromMinor() > 0)
					{
						if (eBuilder != NO_PLAYER)
						{
							if (GET_PLAYER(eOldOwner).isMinorCiv())
							{
								GET_PLAYER(eBuilder).changeSiphonLuxuryCount(eOldOwner, -1 * pImprovementInfo->GetLuxuryCopiesSiphonedFromMinor());
								GET_PLAYER(eOldOwner).GetMinorCivAI()->SetSiphoned(eBuilder, false);
							}
						}
					}
					if (pImprovementInfo->GetGrantsVision() > 0 && eBuilder != NO_PLAYER)
					{
						int iPlotVisRange = pImprovementInfo->GetGrantsVision();
						changeAdjacentSight(GET_PLAYER(eBuilder).getTeam(), iPlotVisRange, false, NO_INVISIBLE, NO_DIRECTION, NULL);
					}
					if (pImprovementInfo->GetUnitPlotExperience() > 0)
					{
						ChangeUnitPlotExperience(pImprovementInfo->GetUnitPlotExperience() * -1);
					}
					if (pImprovementInfo->GetGAUnitPlotExperience() > 0)
					{
						ChangeUnitPlotGAExperience(-1 * pImprovementInfo->GetGAUnitPlotExperience());
					}
					if (pImprovementInfo->GetMovesChange() != 0)
					{
						ChangePlotMovesChange(-1 * pImprovementInfo->GetMovesChange());
					}
					if (pImprovementInfo->IsRestoreMoves())
					{
						ChangeRestoreMovesCount(-1);
					}

					// Embassy extra vote in WC mod
					if (pImprovementInfo != NULL && pImprovementInfo->GetCityStateExtraVote() > 0)
					{
						if (eBuilder != NO_PLAYER)
						{
							if (GET_PLAYER(eOldOwner).isMinorCiv())
							{
								GET_PLAYER(eBuilder).ChangeImprovementLeagueVotes(pImprovementInfo->GetCityStateExtraVote() * -1);
							}
						}
					}

					// Remove credit for building Landmark if player took it from the minor
					if (eImprovement == eLandmarkImprovement && eOldOwner == GetLandmarkCreditMinor() && GET_PLAYER(eBuilder).getTeam() == GET_PLAYER(eNewValue).getTeam())
						SetLandmarkCreditMinor(NO_PLAYER);
				}

				// Remove Resource Quantity from total
				bool bIgnoreTechPrereq = IsImprovedByGiftFromMajor();
				if (getResourceType(eOldTeam, bIgnoreTechPrereq) != NO_RESOURCE)
				{
					if (IsResourceImprovedForOwner(bIgnoreTechPrereq))
					{
						GET_PLAYER(eOldOwner).removeResourcesOnPlotFromTotal(this, false, bIgnoreTechPrereq);
					}
					else
					{
						GET_PLAYER(eOldOwner).removeResourcesOnPlotFromUnimproved(this, false, bIgnoreTechPrereq);
					}
				}
			}

			// This plot is ABOUT TO BE owned. Pop Goody Huts/remove barb camps, etc. Otherwise it will try to increase/reduce the # of Improvements we have in our borders, and these guys shouldn't apply to that count
			if(eNewValue != NO_PLAYER)
			{
				// Pop Goody Huts here
				if(isGoody())
				{
					GET_PLAYER(eNewValue).doGoody(this, NULL);
					eImprovement = NO_IMPROVEMENT;
				}

				// If there's a camp here, clear it
				if(eImprovement == GD_INT_GET(BARBARIAN_CAMP_IMPROVEMENT))
				{
					setImprovementType(NO_IMPROVEMENT);
					CvBarbarians::DoBarbCampCleared(this, eNewValue);
					SetPlayerThatClearedBarbCampHere(eNewValue);
					eImprovement = NO_IMPROVEMENT;
				}

				// Transfer responsibility of routes and improvements if the plot is now owned by someone else
				if (eImprovement != NO_IMPROVEMENT)
				{
					SetPlayerResponsibleForImprovement(eNewValue);
				}
				if (getRouteType() != NO_ROUTE && !isCity())
				{
					SetPlayerResponsibleForRoute(eNewValue);
				}
			}
			else
			{
				// Transfer responsibility of improvements back to the original builder if the plot is now unowned, and the improvement can be built outside of borders
				// If the improvement is not supposed to function outside borders, then no one is responsible for it
				// Original builder of route is not stored in memory, so the responsible player remains the previous owner
				if (eImprovement != NO_IMPROVEMENT)
				{
					CvImprovementEntry* pkImprovementInfo = GC.getImprovementInfo(eImprovement);
					if (pkImprovementInfo && pkImprovementInfo->IsOutsideBorders())
					{
						SetPlayerResponsibleForImprovement(eBuilder);
					}
					SetPlayerResponsibleForImprovement(NO_PLAYER);
				}

				// if we don't have an owner, there cannot be a city. this also does bookkeeping for route maintenance
				if (pOldCity)
					setIsCity(false, pOldCity->GetID(), pOldCity->getWorkPlotDistance());
			}

			// ACTUALLY CHANGE OWNERSHIP HERE
			setOwningCity(eNewValue, iAcquiringCityID);
			m_eOwner = eNewValue;

			if (MOD_EVENTS_TILE_IMPROVEMENTS) {
				GAMEEVENTINVOKE_HOOK(GAMEEVENT_TileOwnershipChanged, getX(), getY(), getOwner(), eOldOwner);
				// lua modders likely will want to mess with improvements/resources on the tile, so do this *before* that is calculated and re-check
				eBuilder = GetPlayerThatBuiltImprovement();
				eImprovement = getImprovementType();
			}

			// Post ownership switch
			if (isOwned())
			{
				CvPlayerAI& newPlayer = GET_PLAYER(eNewValue);

				changeAdjacentSight(getTeam(), /*1*/ GD_INT_GET(PLOT_VISIBILITY_RANGE), true, NO_INVISIBLE, NO_DIRECTION);

				// if this tile is owned by a minor share the visibility with my ally
				if (eNewValue >= MAX_MAJOR_CIVS && eNewValue != BARBARIAN_PLAYER)
				{
					CvPlayer& thisPlayer = GET_PLAYER(eNewValue);
					CvMinorCivAI* pMinorCivAI = thisPlayer.GetMinorCivAI();
					if (pMinorCivAI && pMinorCivAI->GetAlly() != NO_PLAYER)
					{
						changeAdjacentSight(GET_PLAYER(pMinorCivAI->GetAlly()).getTeam(), /*1*/ GD_INT_GET(PLOT_VISIBILITY_RANGE), true, NO_INVISIBLE, NO_DIRECTION);
					}
				}

				if (area())
				{
					area()->changeNumOwnedTiles(1);
				}
				GC.getMap().changeOwnedPlots(1);

				if (!isWater())
				{
					GET_PLAYER(getOwner()).changeTotalLand(1);
					GET_TEAM(getTeam()).changeTotalLand(1);

					if (isOwnershipScore())
					{
						GET_PLAYER(getOwner()).changeTotalLandScored(1);
					}
				}

				// Improvement is here
				if(eImprovement != NO_IMPROVEMENT)
				{
					GET_PLAYER(eNewValue).changeImprovementCount(eImprovement, 1, getOwner() == eBuilder);
					// city owner changes hands later or maybe earlier? ugh
					if (!pOldOwningCity || pOldOwningCity->GetID() != iAcquiringCityID)
					{
						CvCity* pNewOwningCity = ::GetPlayerCity(IDInfo(eNewValue, iAcquiringCityID));
						pNewOwningCity->ChangeImprovementCount(eImprovement, 1);
					}

					// Add siphoned resources
					CvImprovementEntry* pImprovementInfo = GC.getImprovementInfo(eImprovement);
					if (pImprovementInfo != NULL && pImprovementInfo->GetLuxuryCopiesSiphonedFromMinor() > 0)
					{
						if (eBuilder != NO_PLAYER)
						{
							if (GET_PLAYER(eNewValue).isMinorCiv())
							{
								GET_PLAYER(eBuilder).changeSiphonLuxuryCount(eNewValue, pImprovementInfo->GetLuxuryCopiesSiphonedFromMinor());
								GET_PLAYER(eNewValue).GetMinorCivAI()->SetSiphoned(eBuilder, true);
							}
						}
					}

					//Did someone else build this, and now you own it? Let's shift that around.
					if(pImprovementInfo->GetGrantsVision() > 0 && eBuilder != NO_PLAYER && getOwner() != eBuilder)
					{
						int iPlotVisRange = pImprovementInfo->GetGrantsVision();
						changeAdjacentSight(GET_PLAYER(eBuilder).getTeam(), iPlotVisRange, false, NO_INVISIBLE, NO_DIRECTION, NULL);
						changeAdjacentSight(GET_PLAYER(getOwner()).getTeam(), iPlotVisRange, true, NO_INVISIBLE, NO_DIRECTION, NULL);
					}
					if (pImprovementInfo->GetUnitPlotExperience() > 0 && getOwner() != eBuilder)
					{
						ChangeUnitPlotExperience(pImprovementInfo->GetUnitPlotExperience());
					}
					if (pImprovementInfo->GetGAUnitPlotExperience() > 0 && getOwner() != eBuilder)
					{
						ChangeUnitPlotGAExperience(pImprovementInfo->GetGAUnitPlotExperience());
					}
					if (pImprovementInfo->GetMovesChange() != 0)
					{
						ChangePlotMovesChange(pImprovementInfo->GetMovesChange());
					}
					if (pImprovementInfo->IsRestoreMoves())
					{
						ChangeRestoreMovesCount(1);
					}

					//Resource from improvement - change ownership if needed.
					ResourceTypes eResourceFromImprovement = (ResourceTypes)pImprovementInfo->GetResourceFromImprovement();
					int iQuantity = pImprovementInfo->GetResourceQuantityFromImprovement();
					if(iQuantity <= 0)
					{
						iQuantity = 1;
					}

					if(eResourceFromImprovement != NO_RESOURCE && (getResourceType() != NO_RESOURCE && getResourceType() != eResourceFromImprovement))
					{
						setResourceType(eResourceFromImprovement, iQuantity);
					}

					// Embassy is here (somehow- city-state conquest/reconquest, perhaps?) Add vote
					if (pImprovementInfo != NULL)
					{
						if (pImprovementInfo->GetCityStateExtraVote() > 0)
						{
							if (eBuilder != NO_PLAYER)
							{
								if (GET_PLAYER(eNewValue).isMinorCiv())
								{
									GET_PLAYER(eBuilder).ChangeImprovementLeagueVotes(pImprovementInfo->GetCityStateExtraVote());
								}
							}
						}
						if (pImprovementInfo->IsEmbassy())
							SetImprovementEmbassy(true);
						else
							SetImprovementEmbassy(false);
					}
				}

				// Add Resource Quantity to total
				if (getResourceType(getTeam()) != NO_RESOURCE)
				{
					if (IsResourceImprovedForOwner(false, bFoundingCity))
					{
						GET_PLAYER(getOwner()).addResourcesOnPlotToTotal(this);
					}
					else
					{
						GET_PLAYER(getOwner()).addResourcesOnPlotToUnimproved(this);
					}
				}

				// update the high water mark for player (if human)
				int iMaxCityHighWaterMark = /*3*/ GD_INT_GET(MAX_CITY_DIST_HIGHWATER_MARK);
				if(newPlayer.isHuman() && newPlayer.GetCityDistanceHighwaterMark() < iMaxCityHighWaterMark)
				{
					int bestHighWaterMark = newPlayer.GetCityDistanceHighwaterMark();
					for(int iDX = -iMaxCityHighWaterMark; iDX <= iMaxCityHighWaterMark && bestHighWaterMark < iMaxCityHighWaterMark; iDX++)
					{
						for(int iDY = -iMaxCityHighWaterMark; iDY <= iMaxCityHighWaterMark && bestHighWaterMark < iMaxCityHighWaterMark; iDY++)
						{
							int hexRange = 0;

							// I'm assuming iDX and iDY are in hex-space
							if((iDX >= 0) == (iDY >= 0))  // the signs match
							{
								int iAbsDX = iDX >= 0 ? iDX : -iDX;
								int iAbsDY = iDY >= 0 ? iDY : -iDY;
								hexRange = iAbsDX + iAbsDY;
							}
							else
							{
								int iAbsDX = iDX >= 0 ? iDX : -iDX;
								int iAbsDY = iDY >= 0 ? iDY : -iDY;
								hexRange = iAbsDX >= iAbsDY ? iAbsDX : iAbsDY;
							}

							if(hexRange > bestHighWaterMark && hexRange <= iMaxCityHighWaterMark)
							{
								CvPlot* pLoopPlot = plotXY(getX(), getY(), iDX, iDY);
								if(pLoopPlot)
								{
									CvCity* thisCity = pLoopPlot->getPlotCity();
									if(thisCity)
									{
										if(thisCity->getOwner() == eNewValue)
										{
											bestHighWaterMark = hexRange;
										}
									}
								}
							}
						}
					}
					if(bestHighWaterMark > newPlayer.GetCityDistanceHighwaterMark())
					{
						newPlayer.SetCityDistanceHighwaterMark(bestHighWaterMark);
						if(GC.GetEngineUserInterface()->isCityScreenUp())
						{
							CvInterfacePtr<ICvCity1> pHeadSelectedCity(GC.GetEngineUserInterface()->getHeadSelectedCity());
							if(pHeadSelectedCity)
							{
								CvCity* pkHeadSelectedCity = GC.UnwrapCityPointer(pHeadSelectedCity.get());
								CvInterfacePtr<ICvPlot1> pDllPlot = GC.WrapPlotPointer(pkHeadSelectedCity->plot());
								GC.GetEngineUserInterface()->lookAt(pDllPlot.get(), CAMERALOOKAT_CITY_ZOOM_IN);
							}
						}
					}
				}
			}

			for(iI = 0; iI < MAX_TEAMS; ++iI)
			{
				if (GET_TEAM((TeamTypes)iI).isObserver() || GET_TEAM((TeamTypes)iI).isAlive())
				{
					updateRevealedOwner((TeamTypes)iI);
				}
			}

			updateYield();

			// Update happiness if there's a luxury or natural wonder here
			if(isOwned())
			{
				bool bShouldUpdateHappiness = false;

				if(getFeatureType() != NO_FEATURE)
				{
					if(GC.getFeatureInfo(getFeatureType())->IsNaturalWonder(true))
					{
						bShouldUpdateHappiness = true;
					}
				}

				if(!bShouldUpdateHappiness && getResourceType(getTeam()) != NO_RESOURCE)
				{
					if(GC.getResourceInfo(getResourceType(getTeam()))->getResourceUsage() == RESOURCEUSAGE_LUXURY)
					{
						if(eImprovement != NO_IMPROVEMENT)
						{
							bShouldUpdateHappiness = true;
						}
					}
				}

				if(bShouldUpdateHappiness)
				{
					GET_PLAYER(getOwner()).CalculateNetHappiness();
				}
			}

			if(bCheckUnits)
			{
				verifyUnitValidPlot(NO_PLAYER, true);
			}

			if(GC.getGame().isDebugMode())
			{
				CvInterfacePtr<ICvPlot1> pDllPlot = GC.WrapPlotPointer(this);
				GC.GetEngineUserInterface()->UpdateCountryBorder(pDllPlot.get());
			}
		}

		CvInterfacePtr<ICvPlot1> pDllPlot = GC.WrapPlotPointer(this);
		GC.GetEngineUserInterface()->UpdateCountryBorder(pDllPlot.get());
		GC.GetEngineUserInterface()->setDirty(NationalBorders_DIRTY_BIT, true);
		updateSymbols();
		setOwningCity(eNewValue, iAcquiringCityID);
	}

	// Sometimes we already own the plot but it's a different city
	// Note that this will stop the previous owning city from working the plot
	else if (getOwningCityID() != iAcquiringCityID)
	{
		// if there's a resource on this plot, we need to make sure resource counts are updated for both old and new city
		ResourceTypes eResource = getResourceType(getTeam());
		if (eResource != NO_RESOURCE)
		{
			bool bPreviouslyImproved = IsResourceImprovedForOwner();
			if (bPreviouslyImproved)
			{
				GET_PLAYER(getOwner()).removeResourcesOnPlotFromTotal(this);
			}
			else
			{
				GET_PLAYER(getOwner()).removeResourcesOnPlotFromUnimproved(this);
			}
			// chance ownership of the plot
			setOwningCity(eNewValue, iAcquiringCityID);
			// settling a city on the plot improves the resource
			bool bNowImproved = IsResourceImprovedForOwner(false, bFoundingCity);
			if (bNowImproved)
			{
				GET_PLAYER(getOwner()).addResourcesOnPlotToTotal(this);
			}
			else
			{
				GET_PLAYER(getOwner()).addResourcesOnPlotToUnimproved(this);
			}
		}
		else
		{
			setOwningCity(eNewValue, iAcquiringCityID);
		}
	}
}

//	--------------------------------------------------------------------------------
/// Is this Plot within a certain range of any of a player's Cities?
bool CvPlot::IsCloseToCity(PlayerTypes ePlayer) const
{
	if (ePlayer == NO_PLAYER)
		return false;

	//do not use estimated turns here, performance is not good
	int iDistance = GET_PLAYER(ePlayer).GetCityDistanceInPlots(this);
	int iRange = /*5*/ GD_INT_GET(AI_DIPLO_PLOT_RANGE_FROM_CITY_HOME_FRONT);
	return (iDistance <= iRange);
}

//	--------------------------------------------------------------------------------
bool CvPlot::isBlockaded(PlayerTypes eForPlayer)
{
	if (isCity())
		return false;

	if (isEnemyUnit(eForPlayer, true, true))
		return true;

	if (isFriendlyUnit(eForPlayer, true, false) || isNeutralUnit(eForPlayer, true, true))
		return false;

	int iLandRange = (MOD_ADJACENT_BLOCKADE) ? 1 : 0;
	int iRange = isWater() ? range(/*2 in CP, 1 in VP*/ GD_INT_GET(NAVAL_PLOT_BLOCKADE_RANGE),0,3) : iLandRange;

	for (int i = RING0_PLOTS; i < RING_PLOTS[iRange]; i++)
	{
		CvPlot* pNeighbor = iterateRingPlots(this, i);
		//landmass change is equivalent to domain change
		if (pNeighbor && pNeighbor->getLandmass() == getLandmass())
		{
			CvUnit* pEnemy = pNeighbor->getBestDefender(NO_PLAYER, eForPlayer, NULL, true, true);
			//no halo around embarked units
			if (pEnemy && pEnemy->isNativeDomain(pNeighbor) && pEnemy->canEndTurnAtPlot(this))
				return true;
		}
	}

	return false;
}

//	--------------------------------------------------------------------------------
bool CvPlot::isFlatlands() const
{
	return (getPlotType() == PLOT_LAND);
}

//	--------------------------------------------------------------------------------
void CvPlot::setPlotType(PlotTypes eNewValue, bool bRecalculate, bool bRebuildGraphics, bool bEraseUnitsIfWater)
{
	CvArea* pNewArea = NULL;
	CvArea* pCurrArea = NULL;
	CvArea* pLastArea = NULL;
	CvPlot* pLoopPlot = NULL;
	bool bWasWater = false;
	bool bWasDeepWater = false;
	bool bRecalculateAreas = false;
	int iAreaCount = 0;
	int iI = 0;

	if (eNewValue <= NO_PLOT || eNewValue >= NUM_PLOT_TYPES) return;

	if(getPlotType() != eNewValue)
	{
		if((getPlotType() == PLOT_OCEAN) || (eNewValue == PLOT_OCEAN))
		{
			erase(bEraseUnitsIfWater);
		}

		bWasWater = isWater();
		bWasDeepWater = isDeepWater();

		updateSeeFromSight(false,bRecalculate);

#if defined(MOD_EVENTS_TERRAFORMING)
		if (MOD_EVENTS_TERRAFORMING) {
			GAMEEVENTINVOKE_HOOK(GAMEEVENT_TerraformingPlot, TERRAFORMINGEVENT_PLOT, m_iX, m_iY, 0, eNewValue, m_ePlotType, -1, -1);
		}
#endif

		m_ePlotType = eNewValue;

		updateYield();
#if defined(MOD_BALANCE_CORE)
		updateImpassable();
#endif

		updateSeeFromSight(true,bRecalculate);

		if((getTerrainType() == NO_TERRAIN) || (GC.getTerrainInfo(getTerrainType())->isWater() != isWater()))
		{
			if(isWater())
			{
				if(isAdjacentToLand(false))
				{
					setTerrainType(((TerrainTypes)(GD_INT_GET(SHALLOW_WATER_TERRAIN))), bRecalculate, bRebuildGraphics);
					m_bIsAdjacentToLand = true;
				}
				else
				{
					setTerrainType(((TerrainTypes)(GD_INT_GET(DEEP_WATER_TERRAIN))), bRecalculate, bRebuildGraphics);
					m_bIsAdjacentToLand = false;
				}
			}
			else
			{
				setTerrainType(((TerrainTypes)(GD_INT_GET(LAND_TERRAIN))), bRecalculate, bRebuildGraphics);
			}
		}

		if (bWasDeepWater != isDeepWater() && bRecalculate)
		{
			GC.getMap().recalculateContinents();
		}

		if(bWasWater != isWater())
		{
			if(bRecalculate)
			{

				for(iI = 0; iI < NUM_DIRECTION_TYPES; ++iI)
				{
					pLoopPlot = plotDirection(getX(), getY(), ((DirectionTypes)iI));

					if(pLoopPlot != NULL)
					{
						if(pLoopPlot->isWater())
						{
							if(pLoopPlot->isAdjacentToLand(false))
							{
								pLoopPlot->setTerrainType(((TerrainTypes)(GD_INT_GET(SHALLOW_WATER_TERRAIN))), bRecalculate, bRebuildGraphics);
								m_bIsAdjacentToLand = true;
							}
							else
							{
								pLoopPlot->setTerrainType(((TerrainTypes)(GD_INT_GET(DEEP_WATER_TERRAIN))), bRecalculate, bRebuildGraphics);
								m_bIsAdjacentToLand = false;
							}
						}
					}
				}
			}

			for(iI = 0; iI < NUM_DIRECTION_TYPES; ++iI)
			{
				pLoopPlot = plotDirection(getX(), getY(), ((DirectionTypes)iI));

				if(pLoopPlot != NULL)
				{
					pLoopPlot->updateYield();
				}
			}


			for(iI = 0; iI < MAX_CITY_PLOTS; ++iI)
			{
				pLoopPlot = iterateRingPlots(getX(), getY(), iI);

				if(pLoopPlot != NULL)
				{
					pLoopPlot->updatePotentialCityWork();
				}
			}

			GC.getMap().changeLandPlots((isWater()) ? -1 : 1);

			if(getResourceType() != NO_RESOURCE)
			{
				GC.getMap().changeNumResourcesOnLand(getResourceType(), ((isWater()) ? -1 : 1));
			}

			if(isOwned())
			{
				GET_PLAYER(getOwner()).changeTotalLand((isWater()) ? -1 : 1);
				GET_TEAM(getTeam()).changeTotalLand((isWater()) ? -1 : 1);
			}

			if(bRecalculate)
			{
				pNewArea = NULL;
				bRecalculateAreas = false;

				if(isWater())
				{
					for(iI = 0; iI < NUM_DIRECTION_TYPES; ++iI)
					{
						pLoopPlot = plotDirection(getX(), getY(), ((DirectionTypes)iI));

						if(pLoopPlot != NULL && pLoopPlot->getArea()!=-1)
						{
							if(pLoopPlot->area()->isWater())
							{
								if(pNewArea == NULL)
								{
									pNewArea = pLoopPlot->area();
								}
								else if(pNewArea != pLoopPlot->area())
								{
									bRecalculateAreas = true;
									break;
								}
							}
						}
					}
				}
				else
				{
					for(iI = 0; iI < NUM_DIRECTION_TYPES; ++iI)
					{
						pLoopPlot = plotDirection(getX(), getY(), ((DirectionTypes)iI));

						if(pLoopPlot != NULL && pLoopPlot->getArea()!=-1)
						{
							if(!(pLoopPlot->area()->isWater()))
							{
								if(pNewArea == NULL)
								{
									pNewArea = pLoopPlot->area();
								}
								else if(pNewArea != pLoopPlot->area())
								{
									bRecalculateAreas = true;
									break;
								}
							}
						}
					}
				}

				if(!bRecalculateAreas)
				{
					pLoopPlot = plotDirection(getX(), getY(), ((DirectionTypes)(NUM_DIRECTION_TYPES - 1)));

					if(pLoopPlot != NULL)
					{
						pLastArea = pLoopPlot->area();
					}
					else
					{
						pLastArea = NULL;
					}

					iAreaCount = 0;

					for(iI = 0; iI < NUM_DIRECTION_TYPES; ++iI)
					{
						pLoopPlot = plotDirection(getX(), getY(), ((DirectionTypes)iI));

						if(pLoopPlot != NULL)
						{
							pCurrArea = pLoopPlot->area();
						}
						else
						{
							pCurrArea = NULL;
						}

						if(pCurrArea != pLastArea)
						{
							iAreaCount++;
						}

						pLastArea = pCurrArea;
					}

					if(iAreaCount > 2)
					{
						bRecalculateAreas = true;
					}
				}

				if(bRecalculateAreas)
				{
					GC.getMap().recalculateAreas();
				}
				else
				{
					setArea(-1);

					if((area() != NULL) && (area()->getNumTiles() == 1))
					{
						GC.getMap().deleteArea(getArea());
					}

					if(pNewArea == NULL)
					{
						pNewArea = GC.getMap().addArea();
						pNewArea->init(pNewArea->GetID(), isWater());
					}

					setArea(pNewArea->GetID());
				}
			}
		}

		if(bRebuildGraphics && GC.IsGraphicsInitialized())
		{
			//Update terrain graphical
			setLayoutDirty(true);
		}
	}
}

//	--------------------------------------------------------------------------------
void CvPlot::setTerrainType(TerrainTypes eNewValue, bool bRecalculate, bool bRebuildGraphics)
{
	bool bUpdateSight = false;

	if (eNewValue <= NO_TERRAIN || eNewValue >= NUM_TERRAIN_TYPES) return;

	TerrainTypes eOldValue = getTerrainType();

	if(eOldValue != eNewValue)
	{
		bUpdateSight = (getTerrainType() != NO_TERRAIN) &&
		        (eNewValue != NO_TERRAIN) &&
		        ((GC.getTerrainInfo(getTerrainType())->getSeeFromLevel() != GC.getTerrainInfo(eNewValue)->getSeeFromLevel()) ||
		         (GC.getTerrainInfo(getTerrainType())->getSeeThroughLevel() != GC.getTerrainInfo(eNewValue)->getSeeThroughLevel()));

		if(bUpdateSight)
		{
			updateSeeFromSight(false,bRecalculate);
		}

#if defined(MOD_EVENTS_TERRAFORMING)
		if (MOD_EVENTS_TERRAFORMING) {
			GAMEEVENTINVOKE_HOOK(GAMEEVENT_TerraformingPlot, TERRAFORMINGEVENT_TERRAIN, m_iX, m_iY, 0, eNewValue, m_eTerrainType, -1, -1);
		}
#endif

		m_eTerrainType = eNewValue;

		updateYield();
		updateImpassable();

#if defined(MOD_BALANCE_CORE)
		CvCity* pOwningCity = getEffectiveOwningCity();

		if(pOwningCity != NULL)
		{
			for(int iI = 0; iI < NUM_YIELD_TYPES; ++iI)
			{
				//Simplification - errata yields not worth considering.
				if ((YieldTypes)iI > YIELD_CULTURE_LOCAL && !MOD_BALANCE_CORE_JFD)
					break;

				pOwningCity->UpdateYieldPerXTerrain((YieldTypes)iI, getTerrainType());
				pOwningCity->UpdateYieldPerXTerrainFromReligion((YieldTypes)iI, getTerrainType());
			}
		}
#endif

		if(bUpdateSight)
		{
			updateSeeFromSight(true,bRecalculate);
		}

		const bool bTypeIsWater = GC.getTerrainInfo(getTerrainType())->isWater();
		if(bTypeIsWater != isWater())
		{
			setPlotType((bTypeIsWater)? PLOT_OCEAN : PLOT_LAND, bRecalculate, bRebuildGraphics);
		}

		for (int iJ = 0; iJ < NUM_DIRECTION_TYPES; ++iJ)
		{
			CvPlot* pAdjacentPlot = plotDirection(getX(), getY(), static_cast<DirectionTypes>(iJ));
			if (pAdjacentPlot && pAdjacentPlot->getImprovementType() != NO_IMPROVEMENT && !pAdjacentPlot->IsImprovementPillaged())
			{
				ImprovementTypes eAdjacentImprovement = pAdjacentPlot->getImprovementType();
				CvImprovementEntry& adjacentImprovementEntry = *GC.getImprovementInfo(eAdjacentImprovement);
				if (adjacentImprovementEntry.IsYieldPerXAdjacentTerrain())
				{
					for (int iK = 0; iK < NUM_YIELD_TYPES; ++iK)
					{
						YieldTypes eYield = static_cast<YieldTypes>(iK);
						// Simplification - errata yields not worth considering.
						if (eYield > YIELD_CULTURE_LOCAL && !MOD_BALANCE_CORE_JFD)
							break;

						if (adjacentImprovementEntry.GetYieldPerXAdjacentTerrain(eYield, eOldValue) > 0
							|| adjacentImprovementEntry.GetYieldPerXAdjacentTerrain(eYield, eNewValue) > 0)
						{
							pAdjacentPlot->updateYield();
							break;
						}
					}
				}
			}
		}
	}
}

//	--------------------------------------------------------------------------------
void CvPlot::setFeatureType(FeatureTypes eNewValue)
{
	FeatureTypes eOldFeature;
	bool bUpdateSight = false;

	if (eNewValue < NO_FEATURE) return;
	if (eNewValue > NO_FEATURE && GC.getFeatureInfo(eNewValue) == NULL) return;

	eOldFeature = getFeatureType();

	if(eOldFeature != eNewValue)
	{
		bUpdateSight = (eOldFeature == NO_FEATURE) ||
		        (eNewValue == NO_FEATURE) ||
		        (GC.getFeatureInfo(eOldFeature)->getSeeThroughChange() != GC.getFeatureInfo(eNewValue)->getSeeThroughChange());

		if(bUpdateSight)
		{
			updateSeeFromSight(false,true);
		}

		CvInterfacePtr<ICvPlot1> pDllPlot(new CvDllPlot(this));
		gDLL->GameplayFeatureChanged(pDllPlot.get(), eNewValue);

		if (MOD_EVENTS_TERRAFORMING) {
			GAMEEVENTINVOKE_HOOK(GAMEEVENT_TerraformingPlot, TERRAFORMINGEVENT_FEATURE, m_iX, m_iY, 0, eNewValue, m_eFeatureType, -1, -1);
		}

		m_eFeatureType = eNewValue;
		CvCity* pOwningCity = getEffectiveOwningCity();
		if(pOwningCity != NULL)
		{
			//City already working this plot? Adjust features being worked as needed.
			if(pOwningCity->GetCityCitizens()->IsWorkingPlot(this))
			{
				//New feature over old? Remove old, add new.
				if(eOldFeature != NO_FEATURE)
				{
					pOwningCity->ChangeNumFeatureWorked(eOldFeature, -1);
					//We added new improvement (wasn't deleted) - add here.
					if(eNewValue != NO_FEATURE)
					{
						pOwningCity->ChangeNumFeatureWorked(eNewValue, 1);
					}
					else
					{
						if(getTerrainType() != NO_TERRAIN)
						{
							pOwningCity->ChangeNumFeaturelessTerrainWorked(getTerrainType(), 1);
						}
					}
				}
				//New improvement over nothing? Add it in.
				else if(eNewValue != NO_FEATURE)
				{
					pOwningCity->ChangeNumFeatureWorked(eNewValue, 1);
				}
			}
		}
		updateYield();
		updateImpassable();

		if(pOwningCity != NULL)
		{
			for(int iI = 0; iI < NUM_YIELD_TYPES; ++iI)
			{
				//Simplification - errata yields not worth considering.
				if ((YieldTypes)iI > YIELD_CULTURE_LOCAL && !MOD_BALANCE_CORE_JFD)
					break;

				pOwningCity->UpdateYieldPerXFeature((YieldTypes)iI, eNewValue);
				pOwningCity->UpdateYieldPerXUnimprovedFeature((YieldTypes)iI);
			}
		}

		if (getOwner() != NO_PLAYER)
		{
			CvPlayer& kOwner = GET_PLAYER(getOwner());
			if (eOldFeature != NO_FEATURE)
				kOwner.UpdateCityFeatureCount(eOldFeature);
			if (eNewValue != NO_FEATURE)
				kOwner.UpdateCityFeatureCount(eNewValue);
		}

		if(bUpdateSight)
		{
			updateSeeFromSight(true,true);
		}

		if(eNewValue != NO_FEATURE)
		{
			// Now a Natural Wonder here
			if((eOldFeature == NO_FEATURE || !GC.getFeatureInfo(eOldFeature)->IsNaturalWonder(true)) && GC.getFeatureInfo(eNewValue)->IsNaturalWonder(true))
			{
				GC.getMap().ChangeNumNaturalWonders(1);
				GC.getMap().getAreaById(getArea())->ChangeNumNaturalWonders(1);
			}
		}
		if(eOldFeature != NO_FEATURE)
		{
			// Was a Natural Wonder, isn't any more
			if(GC.getFeatureInfo(eOldFeature)->IsNaturalWonder(true) && (eNewValue == NO_FEATURE || !GC.getFeatureInfo(eNewValue)->IsNaturalWonder(true)))
			{
				GC.getMap().ChangeNumNaturalWonders(-1);
				GC.getMap().getAreaById(getArea())->ChangeNumNaturalWonders(-1);
			}
		}

		if(eNewValue == NO_FEATURE)
		{
			if(getImprovementType() != NO_IMPROVEMENT)
			{
				if(GC.getImprovementInfo(getImprovementType())->IsRequiresFeature())
				{
					setImprovementType(NO_IMPROVEMENT);
				}
			}
		}

		if (MOD_PLOTS_EXTENSIONS)
		{
			// update adjacent tiles if there is a change for adjacent plot yields
			for (int iI = 0; iI < GC.getNumPlotInfos(); iI++)\
			{
				PlotTypes ePlot = (PlotTypes)iI;

				if (ePlot == NO_PLOT)
				{
					continue;
				}

				if ((eOldFeature != NO_FEATURE && GC.getPlotInfo(ePlot)->IsAdjacentFeatureYieldChange(eOldFeature)) || (eNewValue != NO_FEATURE && GC.getPlotInfo(ePlot)->IsAdjacentFeatureYieldChange(eNewValue)))
				{
					for (int iJ = 0; iJ < NUM_DIRECTION_TYPES; iJ++)
					{
						CvPlot* pAdjacentPlot = plotDirection(getX(), getY(), ((DirectionTypes)iJ));
						if (pAdjacentPlot && pAdjacentPlot->getPlotType() == ePlot)
						{
							pAdjacentPlot->updateYield();
						}
					}
				}
			}
		}

		if (MOD_EVENTS_TILE_IMPROVEMENTS) {
			GAMEEVENTINVOKE_HOOK(GAMEEVENT_TileFeatureChanged, getX(), getY(), getOwner(), eOldFeature, eNewValue);
		}
	}
}

//	--------------------------------------------------------------------------------
/// Does this plot have a natural wonder?
bool CvPlot::IsNaturalWonder(bool orPseudoNatural) const
{
	FeatureTypes eFeature = getFeatureType();
	if(eFeature == NO_FEATURE)
		return false;

	return GC.getFeatureInfo(eFeature)->IsNaturalWonder() || (orPseudoNatural && GC.getFeatureInfo(eFeature)->IsPseudoNaturalWonder());
}

//	--------------------------------------------------------------------------------
ResourceTypes CvPlot::getResourceType(TeamTypes eTeam, bool bIgnoreTechPrereq) const
{
	if(eTeam != NO_TEAM)
	{
		if(m_eResourceType != NO_RESOURCE)
		{
			if (MOD_BALANCE_CORE_BARBARIAN_THEFT && (getImprovementType() == GD_INT_GET(BARBARIAN_CAMP_IMPROVEMENT)))
				return NO_RESOURCE;

			CvGame& Game = GC.getGame();
			bool bDebug = Game.isDebugMode() || GET_TEAM(eTeam).isObserver();

			if(!bDebug && !bIgnoreTechPrereq && !GET_TEAM(eTeam).IsResourceRevealed((ResourceTypes)m_eResourceType) && !GET_TEAM(eTeam).isForceRevealedResource((ResourceTypes)m_eResourceType) && !IsResourceForceReveal(eTeam))
			{
				return NO_RESOURCE;
			}
		}
	}

	return (ResourceTypes)m_eResourceType;
}


//	--------------------------------------------------------------------------------
ResourceTypes CvPlot::getNonObsoleteResourceType(TeamTypes eTeam) const
{
	ResourceTypes eResource;

	ASSERT_DEBUG(eTeam != NO_TEAM);

	eResource = getResourceType(eTeam);

	if(eResource != NO_RESOURCE)
	{
		if((GC.getResourceInfo(eResource)->getTechObsolete() != NO_TECH) && GET_TEAM(eTeam).GetTeamTechs()->HasTech((TechTypes)(GC.getResourceInfo(eResource)->getTechObsolete())))
		{
			return NO_RESOURCE;
		}
	}

	return eResource;
}


//	--------------------------------------------------------------------------------
void CvPlot::setResourceType(ResourceTypes eNewValue, int iResourceNum, bool bForMinorCivPlot)
{
	if (eNewValue < NO_RESOURCE) return;
	if (eNewValue > NO_RESOURCE && GC.getResourceInfo(eNewValue) == NULL) return;

	ResourceTypes eOldValue = (ResourceTypes)m_eResourceType;
	if(eOldValue != eNewValue)
	{
		if (eNewValue != NO_RESOURCE)
		{
			CvResourceInfo* pkResourceInfo = GC.getResourceInfo(eNewValue);
			if (pkResourceInfo)
			{
				if (pkResourceInfo->isOnlyMinorCivs())
				{
					if (!bForMinorCivPlot)
					{
						ASSERT_DEBUG(false, "Tried to place a city-state unique luxury when not allowed.");
						return;
					}
				}
			}
		}

		if(eOldValue != NO_RESOURCE)
		{
			int iOldResourceNum = getNumResource();
			if(area())
			{
				area()->changeNumResources(eOldValue, -iOldResourceNum);
			}
			GC.getMap().changeNumResources(eOldValue, -iOldResourceNum);

			if(!isWater())
			{
				GC.getMap().changeNumResourcesOnLand(eOldValue, -iOldResourceNum);
			}

			if (isOwned())
			{
				PlayerTypes eOwner = getOwner();
				if (GET_PLAYER(eOwner).IsResourceRevealed(eOldValue))
				{
					if (IsResourceImprovedForOwner())
					{
						GET_PLAYER(eOwner).removeResourcesOnPlotFromTotal(this);
					}
					else
					{
						GET_PLAYER(eOwner).removeResourcesOnPlotFromUnimproved(this);
					}
				}
			}
		}

		// Dig cleared? To circumvent Firaxis hardcoding we need to mark whether a diplo penalty might apply now.
		if (eNewValue == NO_RESOURCE && getOwner() != NO_PLAYER && GET_PLAYER(getOwner()).isMajorCiv() && GET_PLAYER(getOwner()).isAlive())
		{
			if (m_eResourceType == GD_INT_GET(ARTIFACT_RESOURCE))
			{
				GET_PLAYER(getOwner()).GetDiplomacyAI()->SetWaitingForDigChoice(true);
			}
			// Hidden sites are ignored unless owner has unlocked Artistry or is human
			else if (m_eResourceType == GD_INT_GET(HIDDEN_ARTIFACT_RESOURCE) && (GET_PLAYER(getOwner()).isHuman() || GET_PLAYER(getOwner()).GetPlayerPolicies()->IsPolicyBranchUnlocked((PolicyBranchTypes)GC.getInfoTypeForString("POLICY_BRANCH_AESTHETICS", true))))
			{
				GET_PLAYER(getOwner()).GetDiplomacyAI()->SetWaitingForDigChoice(true);
			}
		}

		m_eResourceType = eNewValue; // !!! Here is where we actually change the value

		setNumResource(iResourceNum);

		if(eNewValue != NO_RESOURCE)
		{
			if(area())
			{
				area()->changeNumResources(eNewValue, iResourceNum);
			}
			GC.getMap().changeNumResources(eNewValue, iResourceNum);

			if(!isWater())
			{
				GC.getMap().changeNumResourcesOnLand(eNewValue, iResourceNum);
			}

			if (isOwned())
			{
				PlayerTypes eOwner = getOwner();
				if (GET_PLAYER(eOwner).IsResourceRevealed(eNewValue))
				{
					if (IsResourceImprovedForOwner())
					{
						GET_PLAYER(eOwner).addResourcesOnPlotToTotal(this);
					}
					else
					{
						GET_PLAYER(eOwner).addResourcesOnPlotToUnimproved(this);
					}
				}
			}
		}

		updateYield();

		if(GC.IsGraphicsInitialized())
		{
			setLayoutDirty(true);
		}
	}
}


//	--------------------------------------------------------------------------------
int CvPlot::getNumResource() const
{
	return m_iResourceNum;
}

//	--------------------------------------------------------------------------------
void CvPlot::setNumResource(int iNum)
{
	m_iResourceNum = iNum;
	ASSERT_DEBUG(getNumResource() >= 0);
	ASSERT_DEBUG(getResourceType() == NO_RESOURCE || m_iResourceNum > 0, "If a plot contains a Resource it should always have a quantity of at least 1.");
}

//	--------------------------------------------------------------------------------
void CvPlot::changeNumResource(int iChange)
{
	setNumResource(getNumResource() + iChange);
	ASSERT_DEBUG(getNumResource() >= 0);
}

//	--------------------------------------------------------------------------------
int CvPlot::getNumResourceForPlayer(PlayerTypes ePlayer, bool bExtraResources, bool bIgnoreTechPrereq) const
{
	ResourceTypes eResource = getResourceType(bIgnoreTechPrereq ? NO_TEAM : getTeam());
	if(eResource != NO_RESOURCE)
	{
		CvResourceInfo *pkResource = GC.getResourceInfo(eResource);
		if (pkResource)
		{
			if (GET_PLAYER(ePlayer).IsResourceRevealed(eResource) || bIgnoreTechPrereq)
			{
				if (bExtraResources)
				{
					if (pkResource->getResourceUsage() == RESOURCEUSAGE_LUXURY)
					{
						CvCity* pCity = getOwningCity();
						if (pCity)
						{
							if (pCity->IsExtraLuxuryResources())
							{
								return 1;
							}
						}
					}
					return 0;
				}
				else
				{
					int iRtnValue = m_iResourceNum;
					if (pkResource->getResourceUsage() == RESOURCEUSAGE_STRATEGIC)
					{
						int iQuantityMod = GET_PLAYER(ePlayer).GetPlayerTraits()->GetStrategicResourceQuantityModifier(getTerrainType());
						iRtnValue *= 100 + iQuantityMod;
						iRtnValue /= 100;
					}

					if (GET_PLAYER(ePlayer).GetPlayerTraits()->GetResourceQuantityModifier(eResource) > 0)
					{
						int iQuantityMod = GET_PLAYER(ePlayer).GetPlayerTraits()->GetResourceQuantityModifier(eResource);
						iRtnValue *= 100 + iQuantityMod;
						iRtnValue /= 100;
					}

					return iRtnValue;
				}
			}
		}
	}
	return 0;
}

//	--------------------------------------------------------------------------------
void CvPlot::removeMinorResources()
{
	if (GD_INT_GET(MINOR_CIV_MERCANTILE_RESOURCES_KEEP_ON_CAPTURE_DISABLED) == 1)
	{
		ResourceTypes eOldResource = getResourceType();
		if (eOldResource != NO_RESOURCE)
		{
			CvResourceInfo* pkResourceInfo = GC.getResourceInfo(eOldResource);
			if (pkResourceInfo && pkResourceInfo->isOnlyMinorCivs())
			{
				setResourceType(NO_RESOURCE, 0, true);
			}
		}
	}
}

//	--------------------------------------------------------------------------------
ImprovementTypes CvPlot::getImprovementType() const
{
	return (ImprovementTypes)m_eImprovementType;
}

//	--------------------------------------------------------------------------------
ImprovementTypes CvPlot::getImprovementTypeNeededToImproveResource(PlayerTypes ePlayer, bool bTestPlotOwner, bool bIgnoreSpecialImprovements)
{
	ASSERT_DEBUG(ePlayer == NO_PLAYER || ePlayer >= 0, "ePlayer is expected to be non-negative (invalid Index)");
	ASSERT_DEBUG(ePlayer == NO_PLAYER || ePlayer < MAX_MAJOR_CIVS, "ePlayer is expected to be within maximum bounds (invalid Index)");

	ResourceTypes eResource = NO_RESOURCE;
	if(ePlayer != NO_PLAYER)
	{
		eResource = getResourceType(GET_PLAYER(ePlayer).getTeam());
	}
	else
	{
		eResource = getResourceType();
	}

	if(eResource == NO_RESOURCE)
	{
		return NO_IMPROVEMENT;
	}

	if (IsResourceLinkedCityActive() && getImprovementType() != NO_IMPROVEMENT)
		return getImprovementType();		

	ImprovementTypes eImprovementNeeded = NO_IMPROVEMENT;

	// see if we can improve the resource
	for (int iBuildIndex = 0; iBuildIndex < GC.getNumBuildInfos(); iBuildIndex++)
	{
		BuildTypes eBuild = (BuildTypes)iBuildIndex;
		CvBuildInfo* pBuildInfo = GC.getBuildInfo(eBuild);
		if (pBuildInfo == NULL)
			continue;

		if (!canBuild(eBuild, ePlayer, false /*bTestVisible*/, bTestPlotOwner))
			continue;

		if (ePlayer != NO_PLAYER)
		{
			if (!GET_PLAYER(ePlayer).canBuild(this, eBuild, false /*bTestEra*/, false /*bTestVisible*/, false /*bTestGold*/, bTestPlotOwner))
			{
				continue;
			}
		}

		ImprovementTypes eImprovement = (ImprovementTypes)pBuildInfo->getImprovement();
		if (eImprovement == NO_IMPROVEMENT)
			continue;

		CvImprovementEntry* pImprovementInfo = GC.getImprovementInfo(eImprovement);
		if (pImprovementInfo == NULL)
			continue;

		if (!pImprovementInfo->IsConnectsResource(eResource))
			continue;

		if (bIgnoreSpecialImprovements && (pImprovementInfo->IsCreatedByGreatPerson() || pImprovementInfo->IsAdjacentCity()))
			continue;

		if (pImprovementInfo->IsWater() != isWater())
			continue;

		bool isCoast = isWater() && !isLake();
		if (pImprovementInfo->IsCoastMakesValid() && isCoast)
		{
			eImprovementNeeded = eImprovement;
			break;
		}

		eImprovementNeeded = eImprovement;
	}

	return eImprovementNeeded;
}

void CvPlot::setIsCity(bool bValue, int iCityID, int iWorkRange)
{
	//nothing to do
	if (isCity() == bValue)
		return;

	//sanitize
	iWorkRange = range(GET_PLAYER(getOwner()).getWorkPlotDistance(),1,5);

	//removing flag
	if(isCity() && !bValue)
	{
		// Is a route is here?  If so, we may now need to pay maintenance for it.
		if(getRouteType() != NO_ROUTE)
		{
			// Maintenance change!
			SetPlayerResponsibleForRoute(getOwner());
			SetPlayerThatBuiltRoute(getOwner());
		}

		// plot ownership will be changed in CvCity::preKill

		// do not call getPlotCity() here, it might be invalid
		for(int iI = 0; iI < RING_PLOTS[iWorkRange]; ++iI)
		{
			CvPlot* pLoopPlot = iterateRingPlots(getX(), getY(), iI);

			if(pLoopPlot != NULL)
			{
				pLoopPlot->changeCityRadiusCount(-1);
				pLoopPlot->changePlayerCityRadiusCount(getOwner(), -1);
			}
		}

#if defined(MOD_EVENTS_TERRAFORMING)
		if (MOD_EVENTS_TERRAFORMING)
		{
			GAMEEVENTINVOKE_HOOK(GAMEEVENT_TerraformingPlot, TERRAFORMINGEVENT_CITY, m_iX, m_iY, 0, -1, getOwner(), -1, getOwningCityID());
		}
#endif
	}
	
	//setting flag
	if(!isCity() && bValue)
	{
		//sanity check
		CvCity* pCity = GET_PLAYER(getOwner()).getCity(iCityID);
		ASSERT_DEBUG(pCity && pCity->plot() == this && m_owningCity == IDInfo(getOwner(), iCityID))

		//make sure this is correct
		m_owningCityOverride.reset();

		// do not call getPlotCity() here, it might be invalid
		for(int iI = 0; iI < RING_PLOTS[iWorkRange]; ++iI)
		{
			CvPlot* pLoopPlot = iterateRingPlots(getX(), getY(), iI);

			if(pLoopPlot != NULL)
			{
				pLoopPlot->changeCityRadiusCount(1);
				pLoopPlot->changePlayerCityRadiusCount(getOwner(), 1);
			}
		}
#if defined(MOD_BALANCE_CORE)
		if(isMountain())
		{
			ImprovementTypes eMachuPichu = (ImprovementTypes)GC.getInfoTypeForString("IMPROVEMENT_MOUNTAIN_CITY");
			setImprovementType(eMachuPichu);
		}
#endif
		// Is a route is here?  If we already own this plot, then we were paying maintenance, now we don't have to.
		if(getRouteType() != NO_ROUTE)
		{
			// Maintenance change!
			SetPlayerResponsibleForRoute(NO_PLAYER);
		}

#if defined(MOD_EVENTS_TERRAFORMING)
		if (MOD_EVENTS_TERRAFORMING)
		{
			GAMEEVENTINVOKE_HOOK(GAMEEVENT_TerraformingPlot, TERRAFORMINGEVENT_CITY, m_iX, m_iY, 0, getOwner(), -1, getOwningCityID(), -1);
		}
#endif
	}

	m_bIsCity = bValue;
	updateYield();
}

//	--------------------------------------------------------------------------------
void CvPlot::setImprovementType(ImprovementTypes eNewValue, PlayerTypes eBuilder)
{
	int iI = 0;
	ImprovementTypes eOldImprovement = getImprovementType();
	bool bNewImprovementGiftFromMajor = false; // If it is a gift from a major civ, our tech limitations do not apply

	if (eNewValue < NO_IMPROVEMENT) return;
	if (eNewValue > NO_IMPROVEMENT && GC.getImprovementInfo(eNewValue) == NULL) return;

	// Clear the pillage state if the improvement was replaced by any means
	bool bPillageStateChanged = IsImprovementPillaged();
	if (bPillageStateChanged)
		SetImprovementPillaged(false, false);

	// new farm resources: generic farm textures for asia and american are replaced with rice and maize farm texs. 
	// when a farm is built on those continents, check for plot's resource. if it's a generic, non-resource farm, change the art to another continent to get diverse generic graphics.
	// if it's built on rice or maize, change the continent art if needed. america 1, asia 2, africa 3, europe 4 (ocean 0, don't use that)
	if (eNewValue == 3) // id for farm 
	{
		int iContinentType = GetContinentType();
		ResourceTypes eResourceType = getResourceType();

		if (eResourceType == NO_RESOURCE && iContinentType < 3) { // only change it when current type is asia or america
			SetContinentType(iContinentType + 2);
		}
		if (eResourceType == (ResourceTypes)GC.getInfoTypeForString("RESOURCE_RICE", true) && iContinentType != 2) {
			SetContinentType(2);
		}
		if (eResourceType == (ResourceTypes)GC.getInfoTypeForString("RESOURCE_MAIZE", true) && iContinentType != 1) {
			SetContinentType(1);
		}
	}

	if (eBuilder != NO_PLAYER && eNewValue != NO_IMPROVEMENT && getOwner() != NO_PLAYER && GET_PLAYER(getOwner()).isMinorCiv() && GET_PLAYER(eBuilder).isMajorCiv() && !GC.getImprovementInfo(eNewValue)->IsCreatedByGreatPerson())
	{
		bNewImprovementGiftFromMajor = true;
	}

	if (eOldImprovement != eNewValue)
	{
		PlayerTypes eOldBuilder = GetPlayerThatBuiltImprovement();

		// If old improvement was a gift, it ignored our tech limits, so be sure to remove resources properly
		bool bOldImprovementConnectedResource = IsResourceImprovedForOwner(IsImprovedByGiftFromMajor());

		CvCity* pOwningCity = getEffectiveOwningCity();
		if (pOwningCity != NULL)
		{
			//City already working this plot? Adjust improvements being worked as needed.
			if (pOwningCity->GetCityCitizens()->IsWorkingPlot(this))
			{
				//New improvement over old? Remove old, add new.
				if (eOldImprovement != NO_IMPROVEMENT)
				{
					pOwningCity->ChangeNumImprovementWorked(eOldImprovement, -1);
					//We added new improvement (wasn't deleted) - add here.
					if (eNewValue != NO_IMPROVEMENT)
					{
						pOwningCity->ChangeNumImprovementWorked(eNewValue, 1);
					}
				}
				//New improvement over nothing? Add it in.
				else if (eNewValue != NO_IMPROVEMENT)
				{
					pOwningCity->ChangeNumImprovementWorked(eNewValue, 1);
				}
			}
			pOwningCity->GetCityCitizens()->SetDirty(true);
		}
		SetLandmarkCreditMinor(NO_PLAYER);
		PlayerTypes owningPlayerID = getOwner();
		if (eOldImprovement != NO_IMPROVEMENT)
		{
			CvImprovementEntry& oldImprovementEntry = *GC.getImprovementInfo(eOldImprovement);

			DomainTypes eTradeRouteDomain = NO_DOMAIN;
			if (oldImprovementEntry.IsAllowsWalkWater()) {
				eTradeRouteDomain = DOMAIN_LAND;
#if defined(MOD_GLOBAL_PASSABLE_FORTS)
			}
			else if (oldImprovementEntry.IsMakesPassable()) {
				eTradeRouteDomain = DOMAIN_SEA;
#endif
			}


			if (eTradeRouteDomain != NO_DOMAIN) {
				// Take away any trade routes of this domain that pass through the plot
				CvGameTrade* pTrade = GC.getGame().GetGameTrade();
				int iPlotX = getX();
				int iPlotY = getY();

				for (uint uiConnection = 0; uiConnection < pTrade->GetNumTradeConnections(); uiConnection++)
				{
					CvGameTrade* pTrade = GC.getGame().GetGameTrade();
					for (uint uiConnection = 0; uiConnection < pTrade->GetNumTradeConnections(); uiConnection++)
					{
						const TradeConnection* pConnection = &(pTrade->GetTradeConnection(uiConnection));
						if (pTrade->IsTradeRouteIndexEmpty(uiConnection))
						{
							continue;
						}
						for (uint ui = 0; ui < pConnection->m_aPlotList.size(); ui++)
						{
							if (pConnection->m_eDomain == eTradeRouteDomain) {
								TradeConnectionPlotList aPlotList = pConnection->m_aPlotList;

								for (uint uiPlotIndex = 0; uiPlotIndex < aPlotList.size(); uiPlotIndex++) {
									if (aPlotList[uiPlotIndex].m_iX == iPlotX && aPlotList[uiPlotIndex].m_iY == iPlotY)
									{
										CUSTOMLOG("Cancelling trade route for domain %i in plot (%i, %i) as enabling improvement destroyed", eTradeRouteDomain, iPlotX, iPlotY);
										pTrade->EndTradeRoute(pTrade->GetIndexFromID(pConnection->m_iID));
										break;
									}
								}
							}
						}
					}
				}
			}

			CvCity* pActualOwningCity = getOwningCity();
			if (pActualOwningCity)
			{
				for (int iI = 0; iI < NUM_DOMAIN_TYPES; iI++)
				{
					DomainTypes eDomain = (DomainTypes)iI;

					int iDomainProductionModifier = oldImprovementEntry.GetDomainProductionModifier(iI);
					if (iDomainProductionModifier != 0)
					{
						pActualOwningCity->changeDomainProductionModifier(eDomain, -iDomainProductionModifier);
					}

					int iDomainFreeExperience = oldImprovementEntry.GetDomainFreeExperience(iI);
					if (iDomainFreeExperience != 0)
					{
						pActualOwningCity->changeDomainFreeExperience(eDomain, -iDomainFreeExperience);
					}
				}
			}

			//must be false now
			SetImprovementPassable(false);
			//displace units which cannot stay here any longer (question: what if we replace one passable improvement with another? that let's ignore that case)
			for (int i = 0; i < getNumUnits(); i++)
			{
				CvUnit* pPotentiallyDisplaced = getUnitByIndex(i);
				//do not push around zombie units
				if (!pPotentiallyDisplaced->isDelayedDeath())
					pPotentiallyDisplaced->jumpToNearestValidPlotWithinRange(1);
			}

			// If this improvement can add culture to nearby improvements, update them as well
			if (area())
			{
				area()->changeNumImprovements(eOldImprovement, -1);
			}
			// Someone owns this plot
			if (isOwned())
			{
				CvPlayer& owningPlayer = GET_PLAYER(owningPlayerID);
				owningPlayer.changeImprovementCount(eOldImprovement, -1, eOldBuilder == owningPlayerID);
				pOwningCity->ChangeImprovementCount(eOldImprovement, -1);

				// Siphon resource changes
				if (oldImprovementEntry.GetLuxuryCopiesSiphonedFromMinor() > 0 && eOldBuilder != NO_PLAYER)
				{
					if (owningPlayer.isMinorCiv())
					{
						GET_PLAYER(eOldBuilder).changeSiphonLuxuryCount(owningPlayerID, -1 * oldImprovementEntry.GetLuxuryCopiesSiphonedFromMinor());
						owningPlayer.GetMinorCivAI()->SetSiphoned(eOldBuilder, false);
					}
				}
				if (oldImprovementEntry.GetGrantsVision() > 0 && eOldBuilder != NO_PLAYER)
				{
					int iPlotVisRange = oldImprovementEntry.GetGrantsVision();
					changeAdjacentSight(GET_PLAYER(eOldBuilder).getTeam(), iPlotVisRange, false, NO_INVISIBLE, NO_DIRECTION, NULL);
				}
				if (oldImprovementEntry.GetUnitPlotExperience() > 0)
				{
					ChangeUnitPlotExperience(-1 * oldImprovementEntry.GetUnitPlotExperience());
				}
				if (oldImprovementEntry.GetGAUnitPlotExperience() > 0)
				{
					ChangeUnitPlotGAExperience(-1 * oldImprovementEntry.GetGAUnitPlotExperience());
				}
				if (oldImprovementEntry.GetMovesChange() != 0)
				{
					ChangePlotMovesChange(-1 * oldImprovementEntry.GetMovesChange());
				}
				if (oldImprovementEntry.IsRestoreMoves())
				{
					ChangeRestoreMovesCount(-1);
				}

				//Resource from improvement - change ownership if needed.
				ResourceTypes eResourceFromImprovement = (ResourceTypes)oldImprovementEntry.GetResourceFromImprovement();
				int iQuantity = oldImprovementEntry.GetResourceQuantityFromImprovement();
				if (iQuantity <= 0)
				{
					iQuantity = 1;
				}
				if (eResourceFromImprovement != NO_RESOURCE && (getResourceType() != NO_RESOURCE && getResourceType() != eResourceFromImprovement))
				{
					setResourceType(eResourceFromImprovement, iQuantity);
				}
				// Embassy extra vote in WC mod
				if (oldImprovementEntry.GetCityStateExtraVote() > 0 && eOldBuilder != NO_PLAYER)
				{
					if (owningPlayer.isMinorCiv())
					{
						GET_PLAYER(eOldBuilder).ChangeImprovementLeagueVotes(oldImprovementEntry.GetCityStateExtraVote() * -1);
					}
				}
			}
			else
			{
				if (oldImprovementEntry.GetGrantsVision() > 0 && eOldBuilder != NO_PLAYER)
				{
					int iPlotVisRange = oldImprovementEntry.GetGrantsVision();
					changeAdjacentSight(GET_PLAYER(eOldBuilder).getTeam(), iPlotVisRange, false, NO_INVISIBLE, NO_DIRECTION, NULL);
				}
			}
			// Maintenance change!
			// Remove maintenance of the old improvement while we can still easily fetch the cost
			SetPlayerResponsibleForImprovement(NO_PLAYER);
		}

		m_eImprovementType = eNewValue;
		if (MOD_GLOBAL_STACKING_RULES)
		{
			calculateAdditionalUnitsFromImprovement();
		}
		//reset the counter
		setImprovementDuration(0);

		if (eOldImprovement != NO_IMPROVEMENT)
		{
			CvImprovementEntry& oldImprovementEntry = *GC.getImprovementInfo(eOldImprovement);

			// reset yields to adjacent plots
			for (int iJ = 0; iJ < NUM_DIRECTION_TYPES; ++iJ)
			{
				CvPlot* pAdjacentPlot = plotDirection(getX(), getY(), static_cast<DirectionTypes>(iJ));
				if (pAdjacentPlot && pAdjacentPlot->getImprovementType() != NO_IMPROVEMENT)
				{
					ImprovementTypes eAdjacentImprovement = pAdjacentPlot->getImprovementType();
					CvImprovementEntry& adjacentImprovementEntry = *GC.getImprovementInfo(eAdjacentImprovement);
					if (adjacentImprovementEntry.IsYieldPerXAdjacentImprovement())
					{
						for (int iK = 0; iK < NUM_YIELD_TYPES; ++iK)
						{
							YieldTypes eYield = static_cast<YieldTypes>(iK);
							// Simplification - errata yields not worth considering.
							if (eYield > YIELD_CULTURE_LOCAL && !MOD_BALANCE_CORE_JFD)
								break;

							if (adjacentImprovementEntry.GetYieldPerXAdjacentImprovement(eYield, eOldImprovement) > 0)
							{
								pAdjacentPlot->updateYield();
								break;
							}
						}
					}
				}
			}

			if (oldImprovementEntry.IsEmbassy())
				SetImprovementEmbassy(false);
		}

		// Reset who cleared a Barb camp here last (if we're putting a new one down)
		if (eNewValue == GD_INT_GET(BARBARIAN_CAMP_IMPROVEMENT))
		{
			SetPlayerThatClearedBarbCampHere(NO_PLAYER);
			
			// Alert the barbarian spawning code to this new camp
			CvBarbarians::ActivateBarbSpawner(this);

			if (MOD_EVENTS_BARBARIANS)
				GAMEEVENTINVOKE_HOOK(GAMEEVENT_BarbariansCampFounded, getX(), getY());
		}

		setUpgradeProgress(0);

		for(iI = 0; iI < MAX_TEAMS; ++iI)
		{
			if (GET_TEAM((TeamTypes)iI).isObserver() || GET_TEAM((TeamTypes)iI).isAlive())
			{
				if(isVisible((TeamTypes)iI))
				{
					setRevealedImprovementType((TeamTypes)iI, eNewValue);
				}
			}
		}

		if(m_eImprovementType != NO_IMPROVEMENT)
		{
			CvImprovementEntry& newImprovementEntry = *GC.getImprovementInfo(eNewValue);

			ResourceTypes eArtifactResourceType = static_cast<ResourceTypes>(GD_INT_GET(ARTIFACT_RESOURCE));
			ResourceTypes eHiddenArtifactResourceType = static_cast<ResourceTypes>(GD_INT_GET(HIDDEN_ARTIFACT_RESOURCE));
			if (newImprovementEntry.IsPermanent() || newImprovementEntry.IsCreatedByGreatPerson())
			{
				if (getOwner() != NO_PLAYER && (getResourceType(GET_PLAYER(getOwner()).getTeam()) == eArtifactResourceType || getResourceType(GET_PLAYER(getOwner()).getTeam()) == eHiddenArtifactResourceType))
				{
					if (MOD_BALANCE_CORE_ARCHAEOLOGY_FROM_GP && GetArchaeologicalRecord().m_eArtifactType != NO_GREAT_WORK_ARTIFACT_CLASS)
					{
						CvPlayer& kPlayer = GET_PLAYER(getOwner());
						kPlayer.SetNumArchaeologyChoices(kPlayer.GetNumArchaeologyChoices() + 1);
						kPlayer.GetCulture()->AddDigCompletePlot(this);

						if (kPlayer.isHuman())
						{
							CvNotifications* pNotifications = NULL;
							Localization::String locString;
							Localization::String locSummary;
							pNotifications = kPlayer.GetNotifications();
							if (pNotifications)
							{
								CvString strBuffer = GetLocalizedText("TXT_KEY_NOTIFICATION_CHOOSE_ARCHAEOLOGY");
								CvString strSummary = GetLocalizedText("TXT_KEY_NOTIFICATION_SUMMARY_CHOOSE_ARCHAEOLOGY");
								pNotifications->Add(NOTIFICATION_CHOOSE_ARCHAEOLOGY, strBuffer, strSummary, getX(), getY(), kPlayer.GetID());
								CancelActivePlayerEndTurn();
							}

							// Raiders of the Lost Ark achievement
							if (MOD_API_ACHIEVEMENTS)
							{
								const char* szCivKey = kPlayer.getCivilizationTypeKey();
								if (getOwner() != NO_PLAYER && !GC.getGame().isNetworkMultiPlayer() && strcmp(szCivKey, "CIVILIZATION_AMERICA") == 0)
								{
									CvPlayer &kPlotOwner = GET_PLAYER(getOwner());
									szCivKey = kPlotOwner.getCivilizationTypeKey();
									if (strcmp(szCivKey, "CIVILIZATION_EGYPT") == 0)
									{
										for (int i = 0; i < MAX_MAJOR_CIVS; i++)
										{
											CvPlayer &kLoopPlayer = GET_PLAYER((PlayerTypes)i);
											if (kLoopPlayer.GetID() != NO_PLAYER && kLoopPlayer.isAlive())
											{
												szCivKey = kLoopPlayer.getCivilizationTypeKey();
												if (strcmp(szCivKey, "CIVILIZATION_GERMANY"))
												{
													CvUnit *pLoopUnit = NULL;
													int iUnitLoop = 0;
													for (pLoopUnit = kLoopPlayer.firstUnit(&iUnitLoop); pLoopUnit != NULL; pLoopUnit = kLoopPlayer.nextUnit(&iUnitLoop))
													{
														if (strcmp(pLoopUnit->getUnitInfo().GetType(), "UNIT_ARCHAEOLOGIST") == 0)
														{
															if (plotDistance(pLoopUnit->getX(), pLoopUnit->getY(), getX(), getY()) <= 2)
															{
																gDLL->UnlockAchievement(ACHIEVEMENT_XP2_33);
															}
														}
													}
												}
											}
										}
									}
								}
							}
						}
						else
						{
							ArchaeologyChoiceType eChoice = kPlayer.GetCulture()->GetArchaeologyChoice(this);
							kPlayer.GetCulture()->DoArchaeologyChoice(eChoice);
						}
					}
					else
					{
						setResourceType(NO_RESOURCE, 0);
						ClearArchaeologicalRecord();
					}
				}
				else if (getResourceType() == eArtifactResourceType || getResourceType() == eHiddenArtifactResourceType)
				{
					setResourceType(NO_RESOURCE, 0);
					ClearArchaeologicalRecord();
				}
			}
			if (newImprovementEntry.GetHappinessOnConstruction() != 0 && eBuilder != NO_PLAYER)
			{
				GET_TEAM(GET_PLAYER(eBuilder).getTeam()).ChangeNumLandmarksBuilt(newImprovementEntry.GetHappinessOnConstruction());
				if (getOwner() != NO_PLAYER && getOwner() != eBuilder && GET_PLAYER(getOwner()).isMajorCiv())
				{
					GET_TEAM(GET_PLAYER(getOwner()).getTeam()).ChangeNumLandmarksBuilt(newImprovementEntry.GetHappinessOnConstruction());
				}
			}

			CvCity* pActualOwningCity = getOwningCity();
			if (pActualOwningCity)
			{
				for (int iI = 0; iI < NUM_DOMAIN_TYPES; iI++)
				{
					DomainTypes eDomain = (DomainTypes)iI;

					int iDomainProductionModifier = newImprovementEntry.GetDomainProductionModifier(iI);
					if (iDomainProductionModifier != 0)
					{
						pActualOwningCity->changeDomainProductionModifier(eDomain, iDomainProductionModifier);
					}

					int iDomainFreeExperience = newImprovementEntry.GetDomainFreeExperience(iI);
					if (iDomainFreeExperience != 0)
					{
						pActualOwningCity->changeDomainFreeExperience(eDomain, iDomainFreeExperience);
					}
				}
			}

			//remember this to improve pathfinding performance
			SetImprovementPassable(newImprovementEntry.IsMakesPassable());

			// update yields to adjacent plots
			for (int iJ = 0; iJ < NUM_DIRECTION_TYPES; ++iJ)
			{
				CvPlot* pAdjacentPlot = plotDirection(getX(), getY(), static_cast<DirectionTypes>(iJ));
				if (pAdjacentPlot && pAdjacentPlot->getImprovementType() != NO_IMPROVEMENT)
				{
					ImprovementTypes eAdjacentImprovement = pAdjacentPlot->getImprovementType();
					CvImprovementEntry& adjacentImprovementEntry = *GC.getImprovementInfo(eAdjacentImprovement);
					if (adjacentImprovementEntry.IsYieldPerXAdjacentImprovement())
					{
						for (int iK = 0; iK < NUM_YIELD_TYPES; ++iK)
						{
							YieldTypes eYield = static_cast<YieldTypes>(iK);
							//Simplification - errata yields not worth considering.
							if (eYield > YIELD_CULTURE_LOCAL && !MOD_BALANCE_CORE_JFD)
								break;

							if (adjacentImprovementEntry.GetYieldPerXAdjacentImprovement(eYield, eNewValue) > 0)
							{
								pAdjacentPlot->updateYield();
								break;
							}
						}
					}
				}
			}

			if(area())
			{
				area()->changeNumImprovements(eNewValue, 1);
			}

			bool bResourcesOnPlotChanged = false;
			if (MOD_IMPROVEMENTS_EXTENSIONS)
			{
				// creates feature
				if (newImprovementEntry.GetCreatedFeature() != NO_FEATURE)
				{
					setFeatureType(newImprovementEntry.GetCreatedFeature());
				}

				// chance to spawn a random resource, should be executed after creating feature
				int iResourceChance = newImprovementEntry.GetRandomResourceChance();
				if (iResourceChance > 0)
				{
					if (getResourceType() == NO_RESOURCE)
					{
						// first roll: can we get a resource on this plot?
						if (GC.getGame().randRangeInclusive(1, 100, GetPseudoRandomSeed().mix(GC.getGame().getGameTurn())) <= iResourceChance)
						{
							// get list of valid resources for the plot
							vector<ResourceTypes> vPossibleResources;
							for (int iI = 0; iI < GC.getNumResourceInfos(); iI++)
							{
								ResourceTypes eResource = (ResourceTypes)iI;
								CvResourceInfo* pResourceInfo = GC.getResourceInfo((ResourceTypes)iI);

								if (eResource != NO_RESOURCE && pResourceInfo)
								{
									if (canHaveResource(eResource, false, true) && GET_TEAM(getTeam()).IsResourceRevealed(eResource))
									{
										vPossibleResources.push_back(eResource);
									}
								}
							}

							// now let's add a resource.
							if (!vPossibleResources.empty())
							{
								// second roll: which resource do we get on this plot?
								uint uChoice = GC.getGame().urandLimitExclusive(vPossibleResources.size(), GetPseudoRandomSeed().mix(GC.getGame().getGameTurn()).mix(eNewValue));
								ResourceTypes eSelectedResource = vPossibleResources[uChoice];
								int iResourceQuantity = GC.getMap().getRandomResourceQuantity(eSelectedResource);
								setResourceType(eSelectedResource, iResourceQuantity);
								bResourcesOnPlotChanged = true;
								// notification stuff
								if (getOwner() == GC.getGame().getActivePlayer())
								{
									if (!CvPreGame::loadWBScenario() || GC.getGame().getGameTurn() > 0)
									{
										CvString strBuffer;
										CvResourceInfo* pSelectedResourceInfo = GC.getResourceInfo(eSelectedResource);
										ASSERT_DEBUG(pSelectedResourceInfo);
										NotificationTypes eNotificationType = NO_NOTIFICATION_TYPE;
										strBuffer = GetLocalizedText("TXT_KEY_NOTIFICATION_FOUND_RESOURCE", pSelectedResourceInfo->GetTextKey());

										CvString strSummary = GetLocalizedText("TXT_KEY_NOTIFICATION_SUMMARY_FOUND_RESOURCE", pSelectedResourceInfo->GetTextKey());

										switch (pSelectedResourceInfo->getResourceUsage())
										{
										case RESOURCEUSAGE_LUXURY:
											eNotificationType = NOTIFICATION_DISCOVERED_LUXURY_RESOURCE;
											break;
										case RESOURCEUSAGE_STRATEGIC:
											eNotificationType = NOTIFICATION_DISCOVERED_STRATEGIC_RESOURCE;
											break;
										case RESOURCEUSAGE_BONUS:
											eNotificationType = NOTIFICATION_DISCOVERED_BONUS_RESOURCE;
											break;
										}

										CvNotifications* pNotifications = GET_PLAYER(getOwner()).GetNotifications();
										if (pNotifications)
										{
											pNotifications->Add(eNotificationType, strBuffer, strSummary, getX(), getY(), eSelectedResource);
										}
									}
								}
							}
						}
					}
				}
			}
			if (eBuilder != NO_PLAYER)
			{
				if (newImprovementEntry.GetGrantsVision() > 0)
				{
					int iPlotVisRange = newImprovementEntry.GetGrantsVision();
					changeAdjacentSight(GET_PLAYER(eBuilder).getTeam(), iPlotVisRange, true, NO_INVISIBLE, NO_DIRECTION, NULL);
				}
				if (!isOwned() && newImprovementEntry.IsNewOwner())
				{
					int iBestCityID = -1;
					int iBestCityDistance = -1;
					int iDistance = 0;
					CvCity* pLoopCity = NULL;
					int iLoop = 0;
					for (pLoopCity = GET_PLAYER(eBuilder).firstCity(&iLoop); pLoopCity != NULL; pLoopCity = GET_PLAYER(eBuilder).nextCity(&iLoop))
					{
						CvPlot* pPlot = pLoopCity->plot();
						if (pPlot)
						{
							iDistance = plotDistance(getX(), getY(), pLoopCity->getX(), pLoopCity->getY());
							if (iBestCityDistance == -1 || iDistance < iBestCityDistance)
							{
								iBestCityID = pLoopCity->GetID();
								iBestCityDistance = iDistance;
								pOwningCity = pLoopCity;
							}
						}
					}
					setOwner(eBuilder, iBestCityID);
					owningPlayerID = eBuilder;
				}
				// Maintenance
				// If plot is unowned, and the improvement can be built outside of borders, the builder is responsible for the improvement
				// If improvement is not usually allowed to be built outside of borders, then no one is responsible
				if (newImprovementEntry.IsOutsideBorders())
				{
					SetPlayerResponsibleForImprovement(eBuilder);
				}
			}
			if(isOwned())
			{
				CvPlayer& owningPlayer = GET_PLAYER(owningPlayerID);
				owningPlayer.changeImprovementCount(eNewValue, 1, eBuilder == owningPlayerID);
				pOwningCity->ChangeImprovementCount(eNewValue, 1);

				//DLC_04 Achievement
				if (MOD_API_ACHIEVEMENTS)
				{
					if (owningPlayerID == GC.getGame().getActivePlayer() && strncmp(newImprovementEntry.GetType(), "IMPROVEMENT_MOTTE_BAILEY", 64) == 0)
					{
						//string compares are faster than testing if the mod is activated, so perform this after the compare test.
						if(gDLL->IsModActivated(CIV5_DLC_04_SCENARIO_MODID))
						{
							gDLL->UnlockAchievement(ACHIEVEMENT_SCENARIO_04_BUILD_MOTTE);
						}
					}
					
					// XP2 Achievement
					if (eBuilder != NO_PLAYER && !GC.getGame().isGameMultiPlayer())
					{
						if (GET_PLAYER(eBuilder).isHuman() && GET_PLAYER(eBuilder).isLocalPlayer() && strncmp(newImprovementEntry.GetType(), "IMPROVEMENT_FEITORIA", 64) == 0)
						{
							if (owningPlayer.isMinorCiv())
							{
								PlayerTypes eAlly = owningPlayer.GetMinorCivAI()->GetAlly();
								if (eAlly != NO_PLAYER && eAlly != eBuilder)
								{
									gDLL->UnlockAchievement(ACHIEVEMENT_XP2_24);
								}
							}
						}
					}
				}

				// Maintenance
				// If plot is owned, the plot owner is responsible for the improvement
				SetPlayerResponsibleForImprovement(owningPlayerID);

				// Siphon resource changes
				if(newImprovementEntry.GetLuxuryCopiesSiphonedFromMinor() > 0 && eBuilder != NO_PLAYER)
				{
					if (owningPlayer.isMinorCiv())
					{
						GET_PLAYER(eBuilder).changeSiphonLuxuryCount(owningPlayerID, newImprovementEntry.GetLuxuryCopiesSiphonedFromMinor());
						GET_PLAYER(owningPlayerID).GetMinorCivAI()->SetSiphoned(eBuilder, true);
					}
				}
				if (newImprovementEntry.GetUnitPlotExperience() > 0)
				{
					ChangeUnitPlotExperience(newImprovementEntry.GetUnitPlotExperience());
				}
				if (newImprovementEntry.GetGAUnitPlotExperience() > 0)
				{
					ChangeUnitPlotGAExperience(newImprovementEntry.GetGAUnitPlotExperience());
				}
				if (newImprovementEntry.GetMovesChange() != 0)
				{
					ChangePlotMovesChange(newImprovementEntry.GetMovesChange());
				}
				if (newImprovementEntry.IsRestoreMoves())
				{
					ChangeRestoreMovesCount(1);
				}

				//Resource from improvement - change ownership if needed.
				ResourceTypes eResourceFromImprovement = (ResourceTypes)newImprovementEntry.GetResourceFromImprovement();
				int iQuantity = newImprovementEntry.GetResourceQuantityFromImprovement();
				if(iQuantity <= 0)
				{
					iQuantity = 1;
				}

				if(eResourceFromImprovement != NO_RESOURCE)
				{
					setResourceType(eResourceFromImprovement, iQuantity);
					bResourcesOnPlotChanged = true;
				}

				// if the resources on the plot have changed, resource quantities have already been updated in setResourceType. otherwise, we do it now
				if (!bResourcesOnPlotChanged)
				{
					// Add Resource Quantity to total
					ResourceTypes eResource = getResourceType(getTeam()); // can we see the resource?
					if (eResource != NO_RESOURCE)
					{
						if (IsResourceImprovedForOwner(bNewImprovementGiftFromMajor) && !isCity())
						{
							owningPlayer.addResourcesOnPlotToTotal(this, false);
							owningPlayer.removeResourcesOnPlotFromUnimproved(this, false);

							if (GC.getResourceInfo(eResource)->getResourceUsage() == RESOURCEUSAGE_LUXURY)
							{
								owningPlayer.CalculateNetHappiness();
							}
						}
					}
					else
					{
						// if this is a gift from a major, add the resource even if we can't see it yet
						if (bNewImprovementGiftFromMajor && getResourceType() != NO_RESOURCE)
						{
							if (IsResourceImprovedForOwner(bNewImprovementGiftFromMajor))
							{
								owningPlayer.addResourcesOnPlotToTotal(this, false, bNewImprovementGiftFromMajor);
							}
						}
					}
				}

				// Embassy extra vote in WC mod
				if (newImprovementEntry.GetCityStateExtraVote() > 0 && eBuilder != NO_PLAYER)
				{
					if (owningPlayer.isMinorCiv())
					{
						if (owningPlayer.getImprovementCount(eNewValue) <= 1)
						{
							GET_PLAYER(eBuilder).ChangeImprovementLeagueVotes(newImprovementEntry.GetCityStateExtraVote());
						}
					}
				}
				if (newImprovementEntry.IsEmbassy())
					SetImprovementEmbassy(true);
				else
					SetImprovementEmbassy(false);
			}
			if (eBuilder != NO_PLAYER)
			{
				CvCity* pTargetCity = pOwningCity;
				if (pTargetCity == NULL)
				{
					pTargetCity = GET_PLAYER(eBuilder).getCapitalCity();
				}
				if (pTargetCity != NULL)
				{
					// call one for era scaling, and another for non-era scaling
					GET_PLAYER(eBuilder).doInstantYield(INSTANT_YIELD_TYPE_IMPROVEMENT_BUILD, false, NO_GREATPERSON, NO_BUILDING, 0, true, NO_PLAYER, NULL, false, pTargetCity);
					GET_PLAYER(eBuilder).doInstantYield(INSTANT_YIELD_TYPE_IMPROVEMENT_BUILD, false, NO_GREATPERSON, NO_BUILDING, 0, false, NO_PLAYER, NULL, false, pTargetCity);
				}
			}

			ResourceTypes eSpawnedResource = newImprovementEntry.SpawnsAdjacentResource();
			if (eBuilder != NO_PLAYER && eSpawnedResource != NO_RESOURCE)
			{
				CvPlot* pSpawnPlot = GetAdjacentResourceSpawnPlot(eBuilder);
				if (pSpawnPlot)
				{
					pSpawnPlot->setResourceType(eSpawnedResource, 1);
					SetSpawnedResourcePlot(pSpawnPlot);

					if (pSpawnPlot->getOwner() == NO_PLAYER)
					{
						int iBestCityID = -1;
						int iBestCityDistance = -1;
						int iDistance = 0;
						CvCity* pLoopCity = NULL;
						int iLoop = 0;
						for (pLoopCity = GET_PLAYER(eBuilder).firstCity(&iLoop); pLoopCity != NULL; pLoopCity = GET_PLAYER(eBuilder).nextCity(&iLoop))
						{
							CvPlot* pPlot = pLoopCity->plot();
							if (pPlot)
							{
								iDistance = plotDistance(getX(), getY(), pLoopCity->getX(), pLoopCity->getY());
								if (iBestCityDistance == -1 || iDistance < iBestCityDistance)
								{
									iBestCityID = pLoopCity->GetID();
									iBestCityDistance = iDistance;
								}
							}
						}
						pSpawnPlot->setOwner(eBuilder, iBestCityID);
					}
				}
			}
		}

		if (eOldImprovement != NO_IMPROVEMENT)
		{
			ResourceTypes eSpawnedResource = GC.getImprovementInfo(eOldImprovement)->SpawnsAdjacentResource();
			if (eSpawnedResource != NO_RESOURCE)
			{
				CvPlot* pSpawnedPlot = GetSpawnedResourcePlot();
				if (pSpawnedPlot)
				{
					pSpawnedPlot->setResourceType(NO_RESOURCE, 0);
					CvImprovementEntry* pkOldImprovementInfo = GC.getImprovementInfo(eOldImprovement);
					if (pkOldImprovementInfo && pkOldImprovementInfo->IsImprovementResourceMakesValid(eSpawnedResource))
						pSpawnedPlot->setImprovementType(NO_IMPROVEMENT);
					SetSpawnedResourcePlot(NULL);
				}
			}
		}

		// If we're removing an Improvement that hooked up a resource then we need to take away the bonus
		if(eOldImprovement != NO_IMPROVEMENT && !isCity())
		{
			if(isOwned())
			{
				CvPlayer& owningPlayer = GET_PLAYER(owningPlayerID);
				// Remove Resource Quantity from total
				if(getResourceType(getTeam()) != NO_RESOURCE)
				{
					if (bOldImprovementConnectedResource)
					{
						owningPlayer.removeResourcesOnPlotFromTotal(this);
						owningPlayer.addResourcesOnPlotToUnimproved(this);
					}
				}

				ResourceTypes eResource = getResourceType(getTeam());

				if(eResource != NO_RESOURCE)
				{
					if(GC.getImprovementInfo(eOldImprovement)->IsConnectsResource(eResource))
					{
						if(GC.getResourceInfo(eResource)->getResourceUsage() == RESOURCEUSAGE_LUXURY)
						{
							owningPlayer.CalculateNetHappiness();
						}
					}
				}
				//Resource from improvement - change ownership if needed.
				ResourceTypes eResourceFromImprovement = (ResourceTypes)GC.getImprovementInfo(eOldImprovement)->GetResourceFromImprovement();
				if (eResourceFromImprovement != NO_RESOURCE)
				{
					int iQuantity = GC.getImprovementInfo(eOldImprovement)->GetResourceQuantityFromImprovement();
					if (iQuantity <= 0)
					{
						iQuantity = 1;
					}
					if (getResourceType() != NO_RESOURCE && getResourceType() == eResourceFromImprovement)
					{
						setResourceType(NO_RESOURCE, 0);
					}
				}
			}
		}

		updateYield();
		if(eBuilder != NO_PLAYER && getOwner() == eBuilder)
		{
			CvImprovementEntry* pOldImprovement = eOldImprovement != NO_IMPROVEMENT ? GC.getImprovementInfo(eOldImprovement) : NULL;
			CvImprovementEntry* pNewImprovement = eNewValue != NO_IMPROVEMENT ? GC.getImprovementInfo(eNewValue) : NULL;
			if(pOldImprovement || pNewImprovement)
			{
				for(int iJ = 0; iJ < NUM_DIRECTION_TYPES; ++iJ)
				{
					CvPlot* pAdjacentPlot = plotDirection(getX(), getY(), ((DirectionTypes)iJ));
					if (pAdjacentPlot && pAdjacentPlot->getTerrainType() != NO_TERRAIN && pAdjacentPlot->getOwner() == eBuilder)
					{
						for(int iK = 0; iK < NUM_YIELD_TYPES; ++iK)
						{
							//Simplification - errata yields not worth considering.
							if (static_cast<YieldTypes>(iK) > YIELD_CULTURE_LOCAL && !MOD_BALANCE_CORE_JFD)
								break;

							if	(	( pOldImprovement && pOldImprovement->GetAdjacentTerrainYieldChanges(pAdjacentPlot->getTerrainType(), iK) > 0 )
								||	( pNewImprovement && pNewImprovement->GetAdjacentTerrainYieldChanges(pAdjacentPlot->getTerrainType(), iK) > 0 )
								)
							{
								pAdjacentPlot->updateYield();
								break;
							}
						}
					}
				}
			}
		}


		// Update the most recent builder
		if (eOldBuilder != eBuilder)
		{
			SetPlayerThatBuiltImprovement(eBuilder);
		}

		SetImprovedByGiftFromMajor(bNewImprovementGiftFromMajor); // Assumes that only one tile improvement can be on this plot at a time

		if(GC.getGame().isDebugMode())
		{
			setLayoutDirty(true);
		}
	}

	if (eOldImprovement != eNewValue || bPillageStateChanged)
	{
		if (MOD_EVENTS_TILE_IMPROVEMENTS)
		{
			GAMEEVENTINVOKE_HOOK(GAMEEVENT_TileImprovementChanged, getX(), getY(), getOwner(), eOldImprovement, eNewValue, IsImprovementPillaged());
		}

		if (MOD_IMPROVEMENTS_EXTENSIONS && eNewValue != NO_IMPROVEMENT)
		{
			// this should be called last
			if (GC.getImprovementInfo(eNewValue)->IsRemovesSelf())
			{
				setImprovementType(NO_IMPROVEMENT);
			}
		}
	}
}

CvPlot* CvPlot::GetAdjacentResourceSpawnPlot(PlayerTypes ePlayer) const
{
	vector<CvPlot*> aSpawnPlots;

	for (int iI = 0; iI < NUM_DIRECTION_TYPES; iI++)
	{
		CvPlot* pAdjacentPlot = plotDirection(getX(), getY(), (DirectionTypes)iI);
		if (!pAdjacentPlot)
			continue;

		if (!pAdjacentPlot->CanSpawnResource(ePlayer))
			continue;

		aSpawnPlots.push_back(pAdjacentPlot);
	}

	if (aSpawnPlots.empty())
		return NULL;

	int iRandomIndex = GC.getGame().urandLimitExclusive(aSpawnPlots.size(), GET_PLAYER(ePlayer).GetPseudoRandomSeed().mix(GetPseudoRandomSeed()));

	return aSpawnPlots[iRandomIndex];
}

void CvPlot::SetSpawnedResourcePlot(const CvPlot* pPlot)
{
	if (pPlot)
	{
		m_sSpawnedResourceX = pPlot->getX();
		m_sSpawnedResourceY = pPlot->getY();
	}
	else
	{
		m_sSpawnedResourceX = -1;
		m_sSpawnedResourceY = -1;
	}
}

CvPlot* CvPlot::GetSpawnedResourcePlot() const
{
	if (m_sSpawnedResourceX != -1 && m_sSpawnedResourceY != -1)
		return GC.getMap().plot(m_sSpawnedResourceX, m_sSpawnedResourceY);

	return NULL;
}

//	--------------------------------------------------------------------------------
bool CvPlot::IsImprovementEmbassy() const
{
	return m_bImprovementEmbassy;
}

//	--------------------------------------------------------------------------------
void CvPlot::SetImprovementEmbassy(bool bEmbassy)
{
	m_bImprovementEmbassy = bEmbassy;
}

//	--------------------------------------------------------------------------------
bool CvPlot::IsImprovementPassable() const
{
	return m_bImprovementPassable;
}

//	--------------------------------------------------------------------------------
void CvPlot::SetImprovementPassable(bool bPassable)
{
	m_bImprovementPassable = bPassable;
}

//	--------------------------------------------------------------------------------
bool CvPlot::IsImprovementPillaged() const
{
	return m_bImprovementPillaged;
}

//	--------------------------------------------------------------------------------
void CvPlot::SetImprovementPillaged(bool bPillaged, bool bEvents)
{
	bool bWasPillaged = m_bImprovementPillaged;

	if (bPillaged != bWasPillaged)
	{
		m_bImprovementPillaged = bPillaged;

		calculateAdditionalUnitsFromImprovement();

		if (getEffectiveOwningCity() != NULL)
		{
			if (bPillaged)
			{
				getEffectiveOwningCity()->ChangeNumPillagedPlots(1);
			}
			else
			{
				getEffectiveOwningCity()->ChangeNumPillagedPlots(-1);
			}
		}
		updateYield();

		// Quantified Resource changes
		if (getTeam() != NO_TEAM && getImprovementType() != NO_IMPROVEMENT)
		{
			if (getResourceType(getTeam()) != NO_RESOURCE)
			{
				if (GET_TEAM(getTeam()).IsResourceImproveable(getResourceType()))
				{
					if (GC.getImprovementInfo(getImprovementType())->IsConnectsResource(getResourceType()))
					{
						if (bPillaged)
						{
							GET_PLAYER(getOwner()).removeResourcesOnPlotFromTotal(this);
							GET_PLAYER(getOwner()).addResourcesOnPlotToUnimproved(this);
						}
						else
						{
							GET_PLAYER(getOwner()).removeResourcesOnPlotFromUnimproved(this);
							GET_PLAYER(getOwner()).addResourcesOnPlotToTotal(this);
						}
					}
				}
			}
			else if (getResourceType() != NO_RESOURCE && IsImprovedByGiftFromMajor())
			{
				// improvement gifted by major? update resources even if we can't see them ourselves
				if (GC.getImprovementInfo(getImprovementType())->IsConnectsResource(getResourceType()))
				{
					if (bPillaged)
					{
						GET_PLAYER(getOwner()).removeResourcesOnPlotFromTotal(this);
					}
					else
					{
						GET_PLAYER(getOwner()).addResourcesOnPlotToTotal(this);
					}
				}
			}
		}

		// Quantified Resource changes for improvements
		if (getImprovementType() != NO_IMPROVEMENT)
		{
			CvImprovementEntry* pImprovementInfo = GC.getImprovementInfo(getImprovementType());

			//Resource from improvement - change ownership if needed.
			ResourceTypes eResourceFromImprovement = static_cast<ResourceTypes>(pImprovementInfo->GetResourceFromImprovement());
			int iQuantity = pImprovementInfo->GetResourceQuantityFromImprovement();
			if (iQuantity <= 0)
			{
				iQuantity = 1;
			}

			int iChange = bPillaged ? -1 : 1;
			if (eResourceFromImprovement != NO_RESOURCE && getResourceType() != NO_RESOURCE && getResourceType() != eResourceFromImprovement)
			{
				GET_PLAYER(getOwner()).changeNumResourceTotal(eResourceFromImprovement, iChange * iQuantity, false, true, true);
			}

			int iMoves = pImprovementInfo->GetMovesChange();
			ChangePlotMovesChange(iMoves * iChange);

			if (pImprovementInfo->IsRestoreMoves())
			{
				ChangeRestoreMovesCount(iChange);
			}

			CvImprovementEntry& improvementEntry = *GC.getImprovementInfo(getImprovementType());
			ImprovementTypes eImprovement = getImprovementType();
			// update yields to adjacent plots
			for (int iJ = 0; iJ < NUM_DIRECTION_TYPES; ++iJ)
			{
				CvPlot* pAdjacentPlot = plotDirection(getX(), getY(), static_cast<DirectionTypes>(iJ));
				bool bAdjacentImprovement = pAdjacentPlot && pAdjacentPlot->getImprovementType() != NO_IMPROVEMENT;
				bool bCheckAdjacentTerrain = pAdjacentPlot && pAdjacentPlot->getTerrainType() != NO_TERRAIN && pAdjacentPlot->getOwner() == getOwner();
				if (bAdjacentImprovement || bCheckAdjacentTerrain)
				{
					ImprovementTypes eAdjacentImprovement = pAdjacentPlot->getImprovementType();
					if (eAdjacentImprovement != NO_IMPROVEMENT)
					{
						CvImprovementEntry& adjacentImprovementEntry = *GC.getImprovementInfo(eAdjacentImprovement);
						bool bCheckAdjacentAny = bAdjacentImprovement && adjacentImprovementEntry.IsYieldPerXAdjacentImprovement();
						for (int iK = 0; iK < NUM_YIELD_TYPES; ++iK)
						{
							YieldTypes eYield = static_cast<YieldTypes>(iK);
							// Simplification - errata yields not worth considering.
							if (eYield > YIELD_CULTURE_LOCAL && !MOD_BALANCE_CORE_JFD)
								break;

							if ((bCheckAdjacentAny && adjacentImprovementEntry.GetYieldPerXAdjacentImprovement(eYield, eImprovement) > 0)
								|| (bCheckAdjacentTerrain && improvementEntry.GetAdjacentTerrainYieldChanges(pAdjacentPlot->getTerrainType(), eYield) > 0)
								)
							{
								pAdjacentPlot->updateYield();
								break;
							}
						}
					}
				}
			}

			// Change to city Domain modifiers
			CvCity* pActualOwningCity = getOwningCity();
			if (pActualOwningCity)
			{
				for (int iI = 0; iI < NUM_DOMAIN_TYPES; iI++)
				{
					DomainTypes eDomain = (DomainTypes)iI;

					int iDomainProductionModifier = pImprovementInfo->GetDomainProductionModifier(iI);
					if (iDomainProductionModifier != 0)
					{
						pActualOwningCity->changeDomainProductionModifier(eDomain, iDomainProductionModifier * iChange);
					}

					int iDomainFreeExperience = pImprovementInfo->GetDomainFreeExperience(iI);
					if (iDomainFreeExperience != 0)
					{
						pActualOwningCity->changeDomainFreeExperience(eDomain, iDomainFreeExperience * iChange);
					}
				}
			}
		}
		
		if (bEvents && MOD_EVENTS_TILE_IMPROVEMENTS)
		{
			GAMEEVENTINVOKE_HOOK(GAMEEVENT_TileImprovementChanged, getX(), getY(), getOwner(), getImprovementType(), getImprovementType(), IsImprovementPillaged());
		}
	}

	if(bWasPillaged != m_bImprovementPillaged)
	{
		setLayoutDirty(true);
	}
}

//	--------------------------------------------------------------------------------
bool CvPlot::IsImprovedByGiftFromMajor() const
{
	return m_bImprovedByGiftFromMajor;
}

//	--------------------------------------------------------------------------------
void CvPlot::SetImprovedByGiftFromMajor(bool bValue)
{
	m_bImprovedByGiftFromMajor = bValue;
}

//	--------------------------------------------------------------------------------
/// Does this plot have a special improvement that we shouldn't remove?
bool CvPlot::HasSpecialImprovement() const
{
	// Gifted improvements (if we are a minor civ)
	if (getOwner() != NO_PLAYER)
	{
		CvPlayer* pOwner = &GET_PLAYER(getOwner());
		if (pOwner->isMinorCiv())
		{
			if (IsImprovedByGiftFromMajor())
			{
				return true;
			}
		}

		// Great person improvements
		ImprovementTypes eImprovement = getImprovementType();
		if (eImprovement != NO_IMPROVEMENT)
		{
			CvImprovementEntry* pImprovementInfo = GC.getImprovementInfo(eImprovement);
			//Works like GP improvement.
			if (pImprovementInfo && pImprovementInfo->IsCreatedByGreatPerson())
			{
				return true;
			}

			//Don't delete landmarks!
			ImprovementTypes eLandmark = (ImprovementTypes)GC.getInfoTypeForString("IMPROVEMENT_LANDMARK");
			if (eLandmark != NO_IMPROVEMENT && eImprovement == eLandmark)
			{
				return true;
			}
		}
	}

	return false;
}

//	--------------------------------------------------------------------------------
bool CvPlot::IsBarbarianCampNotConverting() const
{
	return m_bBarbCampNotConverting;
}

//	--------------------------------------------------------------------------------
void CvPlot::SetBarbarianCampNotConverting(bool bNotConverting)
{
	m_bBarbCampNotConverting = bNotConverting;
}

//	--------------------------------------------------------------------------------
GenericWorldAnchorTypes CvPlot::GetWorldAnchor() const
{
	return (GenericWorldAnchorTypes) m_eWorldAnchor;
}

//	--------------------------------------------------------------------------------
int CvPlot::GetWorldAnchorData() const
{
	return (int) m_cWorldAnchorData;
}

//	--------------------------------------------------------------------------------
void CvPlot::SetWorldAnchor(GenericWorldAnchorTypes eAnchor, int iData1)
{
	GenericWorldAnchorTypes eOldAnchor = (GenericWorldAnchorTypes) m_eWorldAnchor;

	// Set the new anchor
	m_eWorldAnchor = eAnchor;
	m_cWorldAnchorData = iData1;

	// Remove old
	if(eOldAnchor != NO_WORLD_ANCHOR)
		gDLL->GameplayWorldAnchor(eOldAnchor, /*bAdd*/ false, getX(), getY(), iData1);

	// Add new
	if(eAnchor == WORLD_ANCHOR_NATURAL_WONDER)
		gDLL->GameplayWorldAnchor(eAnchor, /*bAdd*/ true, getX(), getY(), iData1);
}


//	--------------------------------------------------------------------------------
RouteTypes CvPlot::getRouteType() const
{
	return (RouteTypes)m_eRouteType;
}


//	--------------------------------------------------------------------------------
void CvPlot::setRouteType(RouteTypes eNewValue, PlayerTypes eBuilder)
{
	RouteTypes eOldRoute = getRouteType();
	int iI = 0;

	if (eNewValue < NO_ROUTE) return;
	if (eNewValue > NO_ROUTE && GC.getRouteInfo(eNewValue) == NULL) return;

	if(eOldRoute != eNewValue || (eOldRoute == eNewValue && IsRoutePillaged()))
	{
		// Remove old effects
		if(eOldRoute != NO_ROUTE && !isCity())
		{
			// Maintenance change!
			// Remove maintenance while we can still easily access the cost
			SetPlayerResponsibleForRoute(NO_PLAYER);
		}

		if(eOldRoute != eNewValue)
			SetPlayerThatBuiltRoute(eBuilder);

		// Route switch here!
		m_eRouteType = eNewValue;

		// Apply new effects (maintenance and resource usage)
		if(eNewValue != NO_ROUTE && !isCity())
		{
			if (isOwned())
			{
				// If plot is owned, the plot owner is responsible for the route maintenance
				SetPlayerResponsibleForRoute(getOwner());
			}
			else
			{
				// If plot is unowned, the builder is responsible for the route maintenance
				SetPlayerResponsibleForRoute(eBuilder);
			}
		}

		// make sure this plot is not disabled
		// important to call this because city connection update is hooked up there
		SetRoutePillaged(false, false);

		for(iI = 0; iI < MAX_TEAMS; ++iI)
		{
			if (GET_TEAM((TeamTypes)iI).isObserver() || GET_TEAM((TeamTypes)iI).isAlive())
			{
				if(isVisible((TeamTypes)iI))
				{
					setRevealedRouteType((TeamTypes)iI, getRouteType());
				}
			}
		}

		updateYield();

#if defined(MOD_EVENTS_TILE_IMPROVEMENTS)
		if (MOD_EVENTS_TILE_IMPROVEMENTS) {
			GAMEEVENTINVOKE_HOOK(GAMEEVENT_TileRouteChanged, getX(), getY(), getOwner(), eOldRoute, eNewValue, IsRoutePillaged());
		}
#endif
	}
}

//	--------------------------------------------------------------------------------
bool CvPlot::IsRoutePillaged() const
{
	return m_bRoutePillaged;
}

//	--------------------------------------------------------------------------------
void CvPlot::SetRoutePillaged(bool bPillaged, bool bEvents)
{
	if(m_bRoutePillaged != bPillaged)
	{
		for(int iI = 0; iI < MAX_TEAMS; ++iI)
		{
			if ( GET_TEAM((TeamTypes)iI).isObserver() || ((GET_TEAM((TeamTypes)iI).isAlive()) && GC.getGame().getActiveTeam() == (TeamTypes)iI) )
			{
				if(isVisible((TeamTypes)iI))
				{
					setLayoutDirty(true);
				}
			}
		}
	}
#if defined(MOD_EVENTS_TILE_IMPROVEMENTS)
	else
	{
		bEvents = false;
	}
#endif

	//city connections will be recalculated on turn start for each player!
	m_bRoutePillaged = bPillaged;

#if defined(MOD_EVENTS_TILE_IMPROVEMENTS)
	if (bEvents && MOD_EVENTS_TILE_IMPROVEMENTS) {
		GAMEEVENTINVOKE_HOOK(GAMEEVENT_TileRouteChanged, getX(), getY(), getOwner(), getRouteType(), getRouteType(), IsRoutePillaged());
	}
#endif
}

//	--------------------------------------------------------------------------------
void CvPlot::updateCityRoute()
{
	RouteTypes eCityRoute;

	if(isCity())
	{
		ASSERT_DEBUG(isOwned(), "isOwned is expected to be true");

		eCityRoute = GET_PLAYER(getOwner()).getBestRoute();

		if(eCityRoute == NO_ROUTE)
		{
			eCityRoute = ((RouteTypes)(GD_INT_GET(INITIAL_CITY_ROUTE_TYPE)));
		}

		setRouteType(eCityRoute, getOwner());
	}
}

//	--------------------------------------------------------------------------------
/// Who built this Improvement?  Could be NO_PLAYER
PlayerTypes CvPlot::GetPlayerThatBuiltImprovement() const
{
	return (PlayerTypes) m_ePlayerBuiltImprovement;
}

//	--------------------------------------------------------------------------------
/// Who built this Improvement?  Could be NO_PLAYER
void CvPlot::SetPlayerThatBuiltImprovement(PlayerTypes eBuilder)
{
	m_ePlayerBuiltImprovement = eBuilder;
}

//	--------------------------------------------------------------------------------
/// Who built this Road?  Could be NO_PLAYER
PlayerTypes CvPlot::GetPlayerThatBuiltRoute() const
{
	return (PlayerTypes)m_ePlayerBuiltRoute;
}

//	--------------------------------------------------------------------------------
/// Who built this Improvement?  Could be NO_PLAYER
void CvPlot::SetPlayerThatBuiltRoute(PlayerTypes eBuilder)
{
	m_ePlayerBuiltRoute= eBuilder;
}

//	--------------------------------------------------------------------------------
/// Who pays maintenance for this Improvement?
PlayerTypes CvPlot::GetPlayerResponsibleForImprovement() const
{
	return (PlayerTypes) m_ePlayerResponsibleForImprovement;
}

//	--------------------------------------------------------------------------------
/// Who pays maintenance for this Improvement?
void CvPlot::SetPlayerResponsibleForImprovement(PlayerTypes eNewValue)
{
	PlayerTypes eOldValue = GetPlayerResponsibleForImprovement();
	if(eOldValue != eNewValue)
	{
		ImprovementTypes eImprovement = getImprovementType();
		if (eImprovement != NO_IMPROVEMENT)
		{
			CvImprovementEntry* pkImprovementInfo = GC.getImprovementInfo(eImprovement);

			if (pkImprovementInfo)
			{
				if (eOldValue != NO_PLAYER)
				{
					// Old owner no longer needs to pay maintenance
					if (MustPayMaintenanceHere(eOldValue))
					{
						GET_PLAYER(eOldValue).GetTreasury()->ChangeBaseImprovementGoldMaintenance(-pkImprovementInfo->GetGoldMaintenance());
					}

#if defined(MOD_IMPROVEMENTS_EXTENSIONS)
					if (MOD_IMPROVEMENTS_EXTENSIONS)
					{
						// Remember how many improvements we are responsible for (for UI)
						GET_PLAYER(eOldValue).changeResponsibleForImprovementCount(eImprovement, -1);
					}
#endif
				}

				if (eNewValue != NO_PLAYER)
				{
					// New owner needs to pay maintenance
					if (MustPayMaintenanceHere(eNewValue))
					{
						GET_PLAYER(eNewValue).GetTreasury()->ChangeBaseImprovementGoldMaintenance(pkImprovementInfo->GetGoldMaintenance());
					}

#if defined(MOD_IMPROVEMENTS_EXTENSIONS)
					if (MOD_IMPROVEMENTS_EXTENSIONS)
					{
						// Remember how many improvements we are responsible for (for UI)
						GET_PLAYER(eNewValue).changeResponsibleForImprovementCount(eImprovement, 1);
					}
#endif
				}

				if (MOD_IMPROVEMENTS_EXTENSIONS)
				{
					// Change resource quantity used
					for (int iResourceLoop = 0; iResourceLoop < GC.getNumResourceInfos(); iResourceLoop++)
					{
						int iNumResource = pkImprovementInfo->GetResourceQuantityRequirement(iResourceLoop);

						if (iNumResource > 0)
						{
							if (eOldValue != NO_PLAYER)
							{
								GET_PLAYER(eOldValue).changeNumResourceUsed((ResourceTypes)iResourceLoop, -iNumResource);
							}
							if (eNewValue != NO_PLAYER)
							{
								GET_PLAYER(eNewValue).changeNumResourceUsed((ResourceTypes)iResourceLoop, iNumResource);
							}
						}
					}
				}
			}
		}

		// Transfer responsibility
		m_ePlayerResponsibleForImprovement = eNewValue;
	}
}

//	--------------------------------------------------------------------------------
/// Who pays maintenance for this Route?
PlayerTypes CvPlot::GetPlayerResponsibleForRoute() const
{
	return (PlayerTypes) m_ePlayerResponsibleForRoute;
}

//	--------------------------------------------------------------------------------
/// Who pays maintenance for this Route?
void CvPlot::SetPlayerResponsibleForRoute(PlayerTypes eNewValue)
{
	PlayerTypes eOldValue = GetPlayerResponsibleForRoute();
	if (eOldValue != eNewValue)
	{
		RouteTypes eRoute = getRouteType();
		if (eRoute != NO_ROUTE)
		{
			CvRouteInfo* pkRouteInfo = GC.getRouteInfo(eRoute);

			if (pkRouteInfo)
			{
				if (eOldValue != NO_PLAYER)
				{
					// Old owner no longer needs to pay maintenance
					if (MustPayMaintenanceHere(eOldValue))
					{
						GET_PLAYER(eOldValue).GetTreasury()->ChangeBaseImprovementGoldMaintenance(-pkRouteInfo->GetGoldMaintenance());
					}

#if defined(MOD_IMPROVEMENTS_EXTENSIONS)
					if (MOD_IMPROVEMENTS_EXTENSIONS)
					{
						// Remember how many routes we are responsible for (for UI)
						GET_PLAYER(eOldValue).changeResponsibleForRouteCount(eRoute, -1);
					}
#endif
				}

				if (eNewValue != NO_PLAYER)
				{
					// New owner needs to pay maintenance
					if (MustPayMaintenanceHere(eNewValue))
					{
						GET_PLAYER(eNewValue).GetTreasury()->ChangeBaseImprovementGoldMaintenance(pkRouteInfo->GetGoldMaintenance());
					}

#if defined(MOD_IMPROVEMENTS_EXTENSIONS)
					if (MOD_IMPROVEMENTS_EXTENSIONS)
					{
						// Remember how many routes we are responsible for (for UI)
						GET_PLAYER(eNewValue).changeResponsibleForRouteCount(eRoute, 1);
					}
#endif
				}

				if (MOD_IMPROVEMENTS_EXTENSIONS)
				{
					// Change resource quantity used
					for (int iResourceLoop = 0; iResourceLoop < GC.getNumResourceInfos(); iResourceLoop++)
					{
						int iNumResource = pkRouteInfo->getResourceQuantityRequirement(iResourceLoop);

						if (iNumResource > 0)
						{
							if (eOldValue != NO_PLAYER)
							{
								GET_PLAYER(eOldValue).changeNumResourceUsed((ResourceTypes)iResourceLoop, -iNumResource);
							}
							if (eNewValue != NO_PLAYER)
							{
								GET_PLAYER(eNewValue).changeNumResourceUsed((ResourceTypes)iResourceLoop, iNumResource);
							}
						}
					}
				}
			}
		}

		// Transfer responsibility
		m_ePlayerResponsibleForRoute = eNewValue;
	}
}

//	--------------------------------------------------------------------------------
PlayerTypes CvPlot::GetPlayerThatClearedBarbCampHere() const
{
	return (PlayerTypes) m_ePlayerThatClearedBarbCampHere;
}

//	--------------------------------------------------------------------------------
void CvPlot::SetPlayerThatClearedBarbCampHere(PlayerTypes eNewValue)
{
	m_ePlayerThatClearedBarbCampHere = eNewValue;
}

//	--------------------------------------------------------------------------------
PlayerTypes CvPlot::GetLandmarkCreditMinor() const
{
	return (PlayerTypes) m_eLandmarkCreditMinor;
}

//	--------------------------------------------------------------------------------
void CvPlot::SetLandmarkCreditMinor(PlayerTypes eNewValue)
{
	m_eLandmarkCreditMinor = eNewValue;
}

//	--------------------------------------------------------------------------------
PlayerTypes CvPlot::GetPlayerThatClearedDigHere() const
{
	return (PlayerTypes) m_ePlayerThatClearedDigHere;
}

//	--------------------------------------------------------------------------------
void CvPlot::SetPlayerThatClearedDigHere(PlayerTypes eNewValue)
{
	m_ePlayerThatClearedDigHere = eNewValue;
}

//	--------------------------------------------------------------------------------
PlayerTypes CvPlot::GetPlayerThatDestroyedCityHere() const
{
	return (PlayerTypes)m_ePlayerThatDestroyedCityHere;
}

//	--------------------------------------------------------------------------------
void CvPlot::SetPlayerThatDestroyedCityHere(PlayerTypes eNewValue)
{
	m_ePlayerThatDestroyedCityHere = eNewValue;
}

//	--------------------------------------------------------------------------------
/// Link the resource on this plot to pCity. Note that this does NOT set the link to be active - this must be done manually
/*
void CvPlot::SetResourceLinkedCity(const CvCity* pCity)
{
	if (GetResourceLinkedCity() != pCity)
	{
		int iResourceChange = getNumResource();
		if (pCity == NULL)
		{
			if (GetResourceLinkedCity() != NULL)
				GetResourceLinkedCity()->ChangeNumResourceLocal(getResourceType(), -iResourceChange, true);

			//connected?
			ImprovementTypes eImprovement = getImprovementType();
			if (eImprovement != NO_IMPROVEMENT)
			{
				//connected?
				CvImprovementEntry* pImprovement = GC.getImprovementInfo(eImprovement);
				if (pImprovement && pImprovement->IsConnectsResource(getResourceType()))
				{
					SetResourceLinkedCityActive(false);
				}

			}
			m_ResourceLinkedCity.reset();
		}
		else if (GetResourceLinkedCity() != NULL)
		{
			GetResourceLinkedCity()->ChangeNumResourceLocal(getResourceType(), -iResourceChange, true);
			ImprovementTypes eImprovement = getImprovementType();
			if (eImprovement != NO_IMPROVEMENT)
			{
				//connected?
				CvImprovementEntry* pImprovement = GC.getImprovementInfo(eImprovement);
				if (pImprovement && pImprovement->IsConnectsResource(getResourceType()))
				{
					SetResourceLinkedCityActive(false);
				}

			}
		}
		
		if (pCity != NULL)
		{
			ASSERT_DEBUG(pCity->getOwner() == getOwner(), "Argument city pNewValue's owner is expected to be the same as the current instance");
			m_ResourceLinkedCity = pCity->GetIDInfo();
			GetResourceLinkedCity()->ChangeNumResourceLocal(getResourceType(), iResourceChange, true);
			ImprovementTypes eImprovement = getImprovementType();
			if (eImprovement != NO_IMPROVEMENT)
			{
				//connected?
				CvImprovementEntry* pImprovement = GC.getImprovementInfo(eImprovement);
				if (pImprovement && pImprovement->IsConnectsResource(getResourceType()))
				{
					SetResourceLinkedCityActive(true);
				}

			}
		}
	}
}
*/

//	--------------------------------------------------------------------------------
/// Is the Resource connection with the linked city active? (e.g. pillaging)
bool CvPlot::IsResourceLinkedCityActive() const
{
	return m_bResourceLinkedCityActive;
}

//	--------------------------------------------------------------------------------
/// Is the Resource connection with the linked city active? (e.g. pillaging)
void CvPlot::SetResourceLinkedCityActive(bool bValue)
{
	if (bValue != IsResourceLinkedCityActive())
	{
		m_bResourceLinkedCityActive = bValue;
	}
}

//	-----------------------------------------------------------------------------------------------
CvCity* CvPlot::getPlotCity() const
{
	if(m_bIsCity && !m_owningCity.isInvalid())
		return (GET_PLAYER((PlayerTypes)m_owningCity.eOwner).getCity(m_owningCity.iID));

	return NULL;
}

void CvPlot::setOwningCity(PlayerTypes ePlayer, int iCityID)
{

	CvCity* pOldCityOverride = getOwningCityOverride();
	CvCity*	pOldCity = getOwningCity();
	CvCity* pNewCity = ::GetPlayerCity(IDInfo(ePlayer, iCityID));

	m_owningCity = IDInfo(ePlayer, iCityID);
	
	// change improvement ownership
	ImprovementTypes eImprovement = getImprovementType();
	if (eImprovement != NO_IMPROVEMENT)
	{
		// improvement counts of override city are updated in setOwningCityOverride()
		if (!pOldCityOverride && pOldCity)
			pOldCity->ChangeImprovementCount(eImprovement, -1);

		if (pNewCity)
			pNewCity->ChangeImprovementCount(eImprovement, 1);
	}

	// if the plot is being worked and the city is about to change then put the citizen somewhere else
	if (pOldCityOverride && pOldCityOverride != pNewCity)
	{//change working plot now after the city cannot add the plot right back!
		//note that yields will be updated later
		if (pOldCityOverride->GetCityCitizens()->IsWorkingPlot(this))
		{
			pOldCityOverride->GetCityCitizens()->SetWorkingPlot(this, false, CvCity::YIELD_UPDATE_LOCAL);
			pOldCityOverride->GetCityCitizens()->DoAddBestCitizenFromUnassigned(CvCity::YIELD_UPDATE_GLOBAL);
		}
	}
	else if (pOldCity && pOldCity != pNewCity)
	{
		//change working plot now after the city cannot add the plot right back!
		//note that yields will be updated later
		if (pOldCity->GetCityCitizens()->IsWorkingPlot(this))
		{
			pOldCity->GetCityCitizens()->SetWorkingPlot(this, false, CvCity::YIELD_UPDATE_LOCAL);
			pOldCity->GetCityCitizens()->DoAddBestCitizenFromUnassigned(CvCity::YIELD_UPDATE_GLOBAL);
		}
	}

	//always reset the override as well
	setOwningCityOverride(NULL);

	if (pOldCity != pNewCity)
	{
		if (pOldCity)
		{
			pOldCity->RemoveFromPlotList(this);
		}
		if (pNewCity)
		{
			pNewCity->AddToPlotList(this);
		}
	}
}

int CvPlot::getOwningCityID() const
{
	return m_owningCity.iID;
}

CvCity* CvPlot::getOwningCity() const
{
	return ::GetPlayerCity(m_owningCity);
}

// --------------------------------------------------------------------------------
void CvPlot::updateOwningCity()
{
	//nothing to do
	if (!isOwned())
		return;

	//make sure to clear the override if it is invalid
	if (getOwningCityOverride() == NULL)
		setOwningCityOverride(NULL);

	//nothing to do
	if (getOwningCity())
		return;

	//find a city
	int iBestPlot = 0;
	CvCity* pBestCity = NULL;
	for(int iI = 0; iI < MAX_CITY_PLOTS; ++iI)
	{
		CvPlot* pLoopPlot = iterateRingPlots(getX(), getY(), iI);
		if(pLoopPlot != NULL)
		{
			CvCity* pLoopCity = pLoopPlot->getPlotCity();
			if(pLoopCity != NULL)
			{
				if(pLoopCity->getOwner() == getOwner())
				{
					if((pBestCity == NULL) ||
						    (GC.getRingFromLinearOffset()[iI] < GC.getRingFromLinearOffset()[iBestPlot]) ||
						    ((GC.getRingFromLinearOffset()[iI] == GC.getRingFromLinearOffset()[iBestPlot]) &&
						        ((pLoopCity->getGameTurnAcquired() < pBestCity->getGameTurnAcquired()) ||
						        ((pLoopCity->getGameTurnAcquired() == pBestCity->getGameTurnAcquired()) &&
						        (pLoopCity->GetID() < pBestCity->GetID())))))
					{
						iBestPlot = iI;
						pBestCity = pLoopCity;
					}
				}
			}
		}
	}

	if (pBestCity)
	{
		setOwningCity(getOwner(), pBestCity->GetID());
		updateYield();
		GC.GetEngineUserInterface()->setDirty(ColoredPlots_DIRTY_BIT, true);
	}
}


CvCity * CvPlot::getEffectiveOwningCity() const
{
	//no override
	if (m_owningCityOverride.isInvalid())
		return ::GetPlayerCity(m_owningCity);

	//with override
	return ::GetPlayerCity(m_owningCityOverride);
}

bool CvPlot::isEffectiveOwner(const CvCity * pCity) const
{
	//no override
	if (m_owningCityOverride.isInvalid())
		return m_owningCity == pCity->GetIDInfo();

	//with override
	return m_owningCityOverride == pCity->GetIDInfo();
}

//	--------------------------------------------------------------------------------
CvCity* CvPlot::getOwningCityOverride() const
{
	return ::GetPlayerCity(m_owningCityOverride);
}

//	--------------------------------------------------------------------------------
void CvPlot::setOwningCityOverride(CvCity* pNewValue)
{
	CvCity* pCurrentCity = getOwningCityOverride();
	if ( pNewValue != pCurrentCity )
	{
		if(pNewValue != NULL && pNewValue != getOwningCity())
		{
			m_owningCityOverride = pNewValue->GetIDInfo();
		}
		else
		{
			m_owningCityOverride.reset();
		}

		ImprovementTypes eImprovement = getImprovementType();
		if (eImprovement != NO_IMPROVEMENT)
		{
			if (pCurrentCity != NULL)
				pCurrentCity->ChangeImprovementCount(eImprovement, -1);
			if (pNewValue != NULL)
				pNewValue->ChangeImprovementCount(eImprovement, 1);
		}

		// Remove citizen from this plot if another city was using it
		if (pCurrentCity == NULL)
			pCurrentCity = getOwningCity();

		if (pCurrentCity != NULL && pCurrentCity->GetCityCitizens()->IsWorkingPlot(this))
		{
			pCurrentCity->GetCityCitizens()->SetWorkingPlot(this, false, CvCity::YIELD_UPDATE_LOCAL);
			pCurrentCity->GetCityCitizens()->SetForcedWorkingPlot(this, false);
			pCurrentCity->GetCityCitizens()->DoAddBestCitizenFromUnassigned(CvCity::YIELD_UPDATE_GLOBAL);
		}

		//if the effective owner changed, maybe the yield changes as well
		updateYield();

		GC.GetEngineUserInterface()->setDirty(ColoredPlots_DIRTY_BIT, true);
	}
}

//	--------------------------------------------------------------------------------
int CvPlot::getReconCount() const
{
	return m_iReconCount;
}


//	--------------------------------------------------------------------------------
void CvPlot::changeReconCount(int iChange)
{
	m_iReconCount = (m_iReconCount + iChange);
	ASSERT_DEBUG(getReconCount() >= 0);
}


//	--------------------------------------------------------------------------------
int CvPlot::getRiverCrossingCount() const
{
	return m_iRiverCrossingCount;
}


//	--------------------------------------------------------------------------------
void CvPlot::changeRiverCrossingCount(int iChange)
{
	m_iRiverCrossingCount = (m_iRiverCrossingCount + iChange);
	ASSERT_DEBUG(getRiverCrossingCount() >= 0);
}

//	--------------------------------------------------------------------------------
int CvPlot::getYield(YieldTypes eIndex) const
{
	ASSERT_DEBUG(eIndex >= 0, "eIndex is expected to be non-negative (invalid Index)");
	ASSERT_DEBUG(eIndex < NUM_YIELD_TYPES, "eIndex is expected to be within maximum bounds (invalid Index)");
	return (int)(m_aiYield[eIndex]);
}

void CvPlot::changeYield(YieldTypes eYield, int iChange)
{
	if (iChange == 0)
		return;

	for (size_t i = 0; i < m_vExtraYields.size(); i++)
	{
		if (m_vExtraYields[i].first == eYield)
		{
			m_vExtraYields[i].second += iChange;
			if (m_vExtraYields[i].second == 0)
				m_vExtraYields.erase( m_vExtraYields.begin()+i );

			//done!
			updateYield();
			return;
		}
	}

	//not found? add a new entry		
	m_vExtraYields.push_back( make_pair(eYield,iChange) );
    updateYield();
}

int CvPlot::calculateNatureYield(YieldTypes eYield, PlayerTypes ePlayer, FeatureTypes eFeature, ResourceTypes eResource, const CvCity* pOwningCity, bool bDisplay) const
{
	int iYield = 0;
	TeamTypes eTeam = (ePlayer != NO_PLAYER) ? GET_PLAYER(ePlayer).getTeam() : NO_TEAM;

	const TerrainTypes eTerrain = getTerrainType();
	const ImprovementTypes eImprovement = getImprovementType();
	const CvYieldInfo* pkYieldInfo = GC.getYieldInfo(eYield);
	const CvTerrainInfo* pkTerrainInfo = GC.getTerrainInfo(eTerrain);

	// impassable terrain has no base yield (but do allow coast)
	// if worked by a city, it should have a yield.
	if (!isValidMovePlot(ePlayer) && getOwner() != ePlayer && !isShallowWater())
	{
		iYield = 0;
	} 
	else
	{
		iYield = pkTerrainInfo->getYield(eYield);
		if (eYield == YIELD_PRODUCTION && eTerrain == TERRAIN_DESERT && !isHills() && eFeature == NO_FEATURE)
		{
			if (eResource != NO_RESOURCE && eImprovement != NO_IMPROVEMENT)
				iYield += /*0*/ GD_INT_GET(BALANCE_CORE_PRODUCTION_DESERT_IMPROVEMENT);
		}
	}

	iYield += GC.getPlotInfo(getPlotType())->getYield(eYield);
	iYield += GC.getGame().getPlotExtraYield(m_iX, m_iY, eYield);

	if (isHills())
		iYield += pkYieldInfo->getHillsChange();

	if (isMountain())
		iYield += pkYieldInfo->getMountainChange();

	if (isLake())
		iYield += pkYieldInfo->getLakeChange();

	if (eFeature != NO_FEATURE)
	{
		CvFeatureInfo* pkFeatureInfo = GC.getFeatureInfo(eFeature);
		int iYieldChange = pkFeatureInfo->getYieldChange(eYield);

		// Some Features REPLACE the Yield of the Plot instead of adding to it
		if (pkFeatureInfo->isYieldNotAdditive())
			iYield = iYieldChange;
		else
			iYield += iYieldChange;

		if (isRiver())
		{
			iYield += pkFeatureInfo->getRiverYieldChange(eYield);
		}

		if (isHills())
		{
			iYield += pkFeatureInfo->getHillsYieldChange(eYield);
		}

		if (isFreshWater())
		{
			iYield += pkFeatureInfo->getFreshWaterYieldChange(eYield);
		}

		if (isCoastalLand())
		{
			iYield += pkFeatureInfo->getCoastalLandYieldChange(eYield);
		}
	}
	else
	{
		if (isRiver())
		{
			iYield += pkTerrainInfo->getRiverYieldChange(eYield);
		}

		if (isHills())
		{
			iYield += pkTerrainInfo->getHillsYieldChange(eYield);
		}

		if (isFreshWater())
		{
			iYield += pkTerrainInfo->getFreshWaterYieldChange(eYield);
		}

		if (isCoastalLand())
		{
			iYield += pkTerrainInfo->getCoastalLandYieldChange(eYield);
		}
	}

	if (eTeam != NO_TEAM && eResource != NO_RESOURCE)
	{
		CvResourceInfo* pkResourceInfo = GC.getResourceInfo(eResource);
		if (pkResourceInfo)
			iYield += pkResourceInfo->getYieldChange(eYield);
	}

	if (MOD_PLOTS_EXTENSIONS)
	{
		PlotTypes ePlot = getPlotType();
		CvPlotInfo* pkPlotInfo = GC.getPlotInfo(ePlot);
		if (pkPlotInfo->IsAdjacentFeatureYieldChange())
		{
			// Yield from adjacent features
			bool bNaturalWonderPlot = IsNaturalWonder();
			for (int iI = 0; iI < NUM_DIRECTION_TYPES; ++iI)
			{
				DirectionTypes eDirection = static_cast<DirectionTypes>(iI);
				CvPlot* pAdjacentPlot = plotDirection(getX(), getY(), eDirection);
				if (!pAdjacentPlot)
					continue;

				FeatureTypes eAdjacentFeature = pAdjacentPlot->getFeatureType();
				if (eAdjacentFeature != NO_FEATURE)
				{
					iYield += pkPlotInfo->GetAdjacentFeatureYieldChange(eAdjacentFeature, eYield, bNaturalWonderPlot);
				}
			}
		}
	}

	if (pOwningCity && pOwningCity->plot() == this)
	{
		// VP: Set natural tile yields to 2 Food, 1 Production + resource yields + increases below, ignore everything above
		if (MOD_BALANCE_VP)
		{
			iYield = pkYieldInfo->getMinCity();

			if (eResource != NO_RESOURCE)
			{
				CvResourceInfo* pkResourceInfo = GC.getResourceInfo(eResource);
				if (pkResourceInfo)
				{
					iYield += pkResourceInfo->getYieldChange(eYield);
				}
			}

			if (isHills())
			{
				if (isFreshWater())
					iYield += pkYieldInfo->getMinCityHillFreshWater();
				else
					iYield += pkYieldInfo->getMinCityHillNoFreshWater();
			}
			else if (isMountain())
			{
				if (isFreshWater())
					iYield += pkYieldInfo->getMinCityMountainFreshWater();
				else
					iYield += pkYieldInfo->getMinCityMountainNoFreshWater();
			}
			else
			{
				if (isFreshWater())
					iYield += pkYieldInfo->getMinCityFlatFreshWater();
				else
					iYield += pkYieldInfo->getMinCityFlatNoFreshWater();
			}
		}
		// Community Patch only: Min. 2 Food & 1 Production for city center tile yields
		else
		{
			iYield = std::max(iYield, pkYieldInfo->getMinCity());
		}

		// Yields from garrison
		if (pOwningCity->HasGarrison())
		{
			CvUnit* pUnit = pOwningCity->GetGarrisonedUnit();
			iYield += pUnit->GetGarrisonYieldChange(eYield) * pUnit->GetBaseCombatStrength() / 8;
		}

		if (!bDisplay || pOwningCity->isRevealed(GC.getGame().getActiveTeam(), false, false))
		{
			iYield += pkYieldInfo->getCityChange();
			if (pkYieldInfo->getPopulationChangeDivisor() != 0)
			{
				iYield += (pOwningCity->getPopulation() + pkYieldInfo->getPopulationChangeOffset()) / pkYieldInfo->getPopulationChangeDivisor();
			}
		}

		if (ePlayer != NO_PLAYER)
		{
			CvPlayer& kPlayer = GET_PLAYER(ePlayer);

			// GetYieldFromXMilitaryUnits (France UA)
			iYield += kPlayer.GetYieldFromMilitaryUnits(eYield);

			// Mod for Player; used for Policies and such
			iYield += kPlayer.GetPlayerTraits()->GetCityYieldChanges(eYield);

			// Coastal City Mod
			if (pOwningCity->isCoastal())
			{
				iYield += kPlayer.GetCoastalCityYieldChange(eYield);
				iYield += kPlayer.GetPlayerTraits()->GetCoastalCityYieldChanges(eYield);
			}

			// Yields from city strength
			if (pOwningCity->GetYieldChangesPerCityStrengthTimes100(eYield) > 0)
			{
				iYield += pOwningCity->GetYieldChangesPerCityStrengthTimes100(eYield) * pOwningCity->getStrengthValue() / 10000;
			}
			if (pOwningCity->getStrengthValue() >= GD_INT_GET(CITY_STRENGTH_THRESHOLD_FOR_BONUSES) * 100)
			{
				iYield += kPlayer.getYieldPerCityOverStrengthThreshold(eYield);
			}

			int iTemp = kPlayer.GetCityYieldChangeTimes100(eYield); // In hundreds - will be added to capitalYieldChange below

			// Capital Mod
			if (pOwningCity->isCapital())
			{
				iTemp += kPlayer.GetCapitalYieldChangeTimes100(eYield);

				iYield += kPlayer.GetPlayerTraits()->GetCapitalYieldChanges(eYield);

				// Unfortunately these need to be rounded down individually until we rework all yields to have 2 decimals
				int iPerPopYield = pOwningCity->getPopulation() * kPlayer.GetCapitalYieldPerPopChange(eYield);
				iPerPopYield /= 100;
				iYield += iPerPopYield;

				int iPerPopYieldEmpire = kPlayer.getTotalPopulation() * kPlayer.GetCapitalYieldPerPopChangeEmpire(eYield);
				iPerPopYieldEmpire /= 100;
				iYield += iPerPopYieldEmpire;
			}

			iYield += iTemp / 100;
		}
	}

	// Yields from units on fortifications
	if (eImprovement != NO_IMPROVEMENT)
	{
		CvImprovementEntry* pkImprovementEntry = GC.getImprovementInfo(eImprovement);
		ASSERT_DEBUG(pkImprovementEntry);

		if (pkImprovementEntry->IsNoFollowUp() && !IsImprovementPillaged())
		{
			for (int iUnitLoop = 0; iUnitLoop < getNumUnits(); iUnitLoop++)
			{
				CvUnit* pUnit = getUnitByIndex(iUnitLoop);
				if (!pUnit)
					continue;

				iYield += pUnit->GetFortificationYieldChange(eYield) * pUnit->GetBaseCombatStrength() / 8;
			}
		}
	}

	return std::max(0, iYield);
}

int CvPlot::calculateReligionNatureYield(YieldTypes eYield, PlayerTypes ePlayer, ImprovementTypes eImprovement, FeatureTypes eFeature, ResourceTypes eResource, const CvCity* pOwningCity, const CvReligion* pMajorityReligion, const CvBeliefEntry* pSecondaryPantheon) const
{
	if (!pMajorityReligion)
		return 0;

	if (ePlayer == NO_PLAYER)
		return 0;

	int iYield = 0;

	//Change for improvement/resource
	int iReligionChange = 0;
	bool bRequiresImprovement = pMajorityReligion->m_Beliefs.RequiresImprovement(ePlayer);
	bool bRequiresNoImprovement = pMajorityReligion->m_Beliefs.RequiresNoImprovement(ePlayer);
	bool bRequiresResource = pMajorityReligion->m_Beliefs.RequiresResource(ePlayer);
	bool bRequiresNoFeature = pMajorityReligion->m_Beliefs.RequiresNoFeature(ePlayer);
	bool bRequiresEmptyTile = (bRequiresResource && bRequiresNoFeature);
	bool bRequiresBoth = (bRequiresImprovement && bRequiresResource);
	int iValue = pMajorityReligion->m_Beliefs.GetTerrainYieldChange(getTerrainType(), eYield, ePlayer, pOwningCity);
	if (MOD_BALANCE_CORE_BELIEFS_RESOURCE && (bRequiresImprovement || bRequiresResource || bRequiresNoImprovement))
	{
		if (bRequiresBoth)
		{
			if (eImprovement != NO_IMPROVEMENT && eResource != NO_RESOURCE)
			{
				if (GC.getImprovementInfo(eImprovement)->IsConnectsResource(eResource))
				{
					iReligionChange += iValue;
				}
			}
		}
		else if (bRequiresImprovement)
		{
			if (eImprovement != NO_IMPROVEMENT)
			{
				iReligionChange += iValue;
			}
		}
		else if (bRequiresResource)
		{
			if (eResource != NO_RESOURCE)
			{
				iReligionChange += iValue;
			}
		}
		else if (bRequiresEmptyTile)
		{
			if (eImprovement == NO_IMPROVEMENT && eFeature == NO_FEATURE)
			{
				iReligionChange += iValue;
			}
		}
		else if (bRequiresNoImprovement)
		{
			if (eImprovement == NO_IMPROVEMENT)
			{
				iReligionChange += iValue;
			}
		}
		else if (bRequiresNoFeature)
		{
			if (eFeature == NO_FEATURE)
			{
				iReligionChange += iValue;
			}
		}
		if (iReligionChange > iValue)
		{
			iReligionChange = iValue;
		}
	}
	else
	{
		iReligionChange = iValue;
	}

	iYield += iReligionChange;

	if (pSecondaryPantheon)
	{
		//Change for improvement/resource
		iReligionChange = 0;
		bool bRequiresImprovement = pSecondaryPantheon->RequiresImprovement();
		bool bRequiresNoImprovement = pSecondaryPantheon->RequiresNoImprovement();
		bool bRequiresResource = pSecondaryPantheon->RequiresResource();
		bool bRequiresNoFeature = pSecondaryPantheon->RequiresNoFeature();
		bool bRequiresEmptyTile = (bRequiresResource && bRequiresNoFeature);
		bool bRequiresBoth = (bRequiresImprovement && bRequiresResource);
		int iValue = pSecondaryPantheon->GetTerrainYieldChange(getTerrainType(), eYield);
		if (MOD_BALANCE_CORE_BELIEFS_RESOURCE && (bRequiresImprovement || bRequiresResource || bRequiresNoImprovement))
		{
			if (bRequiresBoth)
			{
				if (eImprovement != NO_IMPROVEMENT && eResource != NO_RESOURCE)
				{
					if (GC.getImprovementInfo(eImprovement)->IsConnectsResource(eResource))
					{
						iReligionChange += iValue;
					}
				}
			}
			else if (bRequiresImprovement)
			{
				if (eImprovement != NO_IMPROVEMENT)
				{
					if (GC.getImprovementInfo(eImprovement)->IsConnectsResource(eResource))
					{
						iReligionChange += iValue;
					}
				}
			}
			else if (bRequiresResource)
			{
				if (eResource != NO_RESOURCE)
				{
					iReligionChange += iValue;
				}
			}
			else if (bRequiresEmptyTile)
			{
				if (eImprovement == NO_IMPROVEMENT && getFeatureType() == NO_FEATURE)
				{
					iReligionChange += iValue;
				}
			}
			else if (bRequiresNoImprovement)
			{
				if (eImprovement == NO_IMPROVEMENT)
				{
					iReligionChange += iValue;
				}
			}
			else if (bRequiresNoFeature)
			{
				if (eFeature == NO_FEATURE)
				{
					iReligionChange += iValue;
				}
			}
			if (iReligionChange > iValue)
			{
				iReligionChange = iValue;
			}
		}
		else
		{
			iReligionChange = iValue;
		}

		iYield += iReligionChange;
	}

	iYield += pMajorityReligion->m_Beliefs.GetPlotYieldChange(getPlotType(), eYield, ePlayer, pOwningCity);
	if (pSecondaryPantheon)
	{
		iYield += pSecondaryPantheon->GetPlotYieldChange(getPlotType(), eYield);
	}

	if (isLake())
	{
		iYield += pMajorityReligion->m_Beliefs.GetLakePlotYieldChange(eYield, ePlayer, pOwningCity);
		if (pSecondaryPantheon)
		{
			iYield += pSecondaryPantheon->GetLakePlotYieldChange(eYield);
		}
	}

	if (eFeature != NO_FEATURE)
	{
		if (eImprovement == NO_IMPROVEMENT)
		{
			iYield += pMajorityReligion->m_Beliefs.GetUnimprovedFeatureYieldChange(eFeature, eYield, ePlayer, pOwningCity);
			if (pSecondaryPantheon)
			{
				iYield += pSecondaryPantheon->GetUnimprovedFeatureYieldChange(eFeature, eYield);
			}
		}

		//Change for improvement/resource
		iReligionChange = 0;
		bool bRequiresNoImprovement = pMajorityReligion->m_Beliefs.RequiresNoImprovement(ePlayer);
		bool bRequiresImprovement = pMajorityReligion->m_Beliefs.RequiresImprovement(ePlayer);
		if (MOD_BALANCE_CORE_BELIEFS_RESOURCE && bRequiresNoImprovement && eImprovement == NO_IMPROVEMENT)
		{
			iReligionChange += pMajorityReligion->m_Beliefs.GetFeatureYieldChange(eFeature, eYield, ePlayer, pOwningCity);
		}
		else if (MOD_BALANCE_CORE_BELIEFS_RESOURCE && bRequiresImprovement && eImprovement != NO_IMPROVEMENT)
		{
			iReligionChange += pMajorityReligion->m_Beliefs.GetFeatureYieldChange(eFeature, eYield, ePlayer, pOwningCity);
		}
		else
		{
			iReligionChange += pMajorityReligion->m_Beliefs.GetFeatureYieldChange(eFeature, eYield, ePlayer, pOwningCity);
		}

		iYield += iReligionChange;

		if (pSecondaryPantheon)
		{
			//Change for improvement/resource
			iReligionChange = 0;
			bool bRequiresNoImprovement = pSecondaryPantheon->RequiresNoImprovement();
			bool bRequiresImprovement = pSecondaryPantheon->RequiresImprovement();
			if (MOD_BALANCE_CORE_BELIEFS_RESOURCE && bRequiresNoImprovement && eImprovement == NO_IMPROVEMENT)
			{
				iReligionChange += pSecondaryPantheon->GetFeatureYieldChange(eFeature, eYield);
			}
			else if (MOD_BALANCE_CORE_BELIEFS_RESOURCE && bRequiresImprovement && eImprovement != NO_IMPROVEMENT)
			{
				iReligionChange += pSecondaryPantheon->GetFeatureYieldChange(eFeature, eYield);
			}
			else
			{
				iReligionChange += pSecondaryPantheon->GetFeatureYieldChange(eFeature, eYield);
			}

			iYield += iReligionChange;
		}

		if (IsNaturalWonder())
		{
			iYield += pMajorityReligion->m_Beliefs.GetYieldChangeNaturalWonder(eYield, ePlayer, pOwningCity);
			if (pSecondaryPantheon)
			{
				iYield += pSecondaryPantheon->GetYieldChangeNaturalWonder(eYield);
			}
		}
	}

	if (eResource != NO_RESOURCE)
	{
		iYield += pMajorityReligion->m_Beliefs.GetResourceYieldChange(eResource, eYield, ePlayer, pOwningCity);
		if (pSecondaryPantheon)
		{
			iYield += pSecondaryPantheon->GetResourceYieldChange(eResource, eYield);
		}
	}

	return iYield;
}

//	--------------------------------------------------------------------------------
int CvPlot::calculateBestNatureYield(YieldTypes eYield, PlayerTypes ePlayer) const
{
	return std::max(calculateNatureYield(eYield, ePlayer, getFeatureType(), getResourceType(ePlayer != NO_PLAYER ? GET_PLAYER(ePlayer).getTeam() : NO_TEAM), NULL), calculateNatureYield(eYield, ePlayer, NO_FEATURE, getResourceType(ePlayer != NO_PLAYER ? GET_PLAYER(ePlayer).getTeam() : NO_TEAM), NULL));
}

//	--------------------------------------------------------------------------------
int CvPlot::calculateTotalBestNatureYield(PlayerTypes ePlayer) const
{
	return (calculateBestNatureYield(YIELD_FOOD, ePlayer) + calculateBestNatureYield(YIELD_PRODUCTION, ePlayer) + calculateBestNatureYield(YIELD_GOLD, ePlayer));
}

int CvPlot::calculateReligionImprovementYield(YieldTypes eYield, PlayerTypes ePlayer, ImprovementTypes eImprovement, ResourceTypes eResource, const CvCity* pOwningCity, const CvReligion* pMajorityReligion, const CvBeliefEntry* pSecondaryPantheon) const
{
	if (!pOwningCity || !pMajorityReligion || ePlayer == NO_PLAYER)
		return 0;

	if (eImprovement == NO_IMPROVEMENT)
		return 0;

	CvImprovementEntry* pkImprovementInfo = GC.getImprovementInfo(eImprovement);
	if (!pkImprovementInfo)
		return 0;

	int iReligionChange = 0;
	bool bRequiresResource = pMajorityReligion->m_Beliefs.RequiresResource(pOwningCity->getOwner());
	if (pkImprovementInfo->IsCreatedByGreatPerson() || pkImprovementInfo->IsAdjacentCity() || pkImprovementInfo->IsIgnoreOwnership())
	{
		bRequiresResource = false;
	}
	if (MOD_BALANCE_CORE_BELIEFS_RESOURCE && bRequiresResource)
	{
		if (bRequiresResource && eResource != NO_RESOURCE && (pkImprovementInfo->IsImprovementResourceMakesValid(eResource) || pkImprovementInfo->GetResourceFromImprovement() == eResource))
		{
			iReligionChange += pMajorityReligion->m_Beliefs.GetImprovementYieldChange(eImprovement, eYield, pOwningCity->getOwner(), pOwningCity);
		}
	}
	else
	{
		iReligionChange += pMajorityReligion->m_Beliefs.GetImprovementYieldChange(eImprovement, eYield, pOwningCity->getOwner(), pOwningCity);
	}
	
	if (!pSecondaryPantheon)
		return iReligionChange;

	bRequiresResource = pSecondaryPantheon->RequiresResource();
	if (pkImprovementInfo->IsCreatedByGreatPerson() || pkImprovementInfo->IsAdjacentCity())
	{
		bRequiresResource = false;
	}
	if (MOD_BALANCE_CORE_BELIEFS_RESOURCE && bRequiresResource)
	{
		if (eResource != NO_RESOURCE && (pkImprovementInfo->IsImprovementResourceMakesValid(eResource) || pkImprovementInfo->GetResourceFromImprovement() == eResource))
		{
			iReligionChange += pSecondaryPantheon->GetImprovementYieldChange(eImprovement, eYield);
		}
	}
	else
	{
		iReligionChange += pSecondaryPantheon->GetImprovementYieldChange(eImprovement, eYield);
	}

	return iReligionChange;
}
//	--------------------------------------------------------------------------------
int CvPlot::calculateImprovementYield(YieldTypes eYield, PlayerTypes ePlayer, ImprovementTypes eImprovement, RouteTypes eRoute, FeatureTypes eFeature, ResourceTypes eResource, RouteTypes eForceCityConnection, const CvCity* pOwningCity, bool bOptimal) const
{
	int iBestYield = 0;
	int iYield = 0;
	fraction fFractionalYield = 0;
	int iI = 0;

	if (eImprovement == NO_IMPROVEMENT)
		return 0;

	CvImprovementEntry* pkImprovementInfo = GC.getImprovementInfo(eImprovement);
	if (!pkImprovementInfo)
		return 0;

	bool bIsFreshWater = isFreshWater();

	iYield = pkImprovementInfo->GetYieldChange(eYield);

	if (ePlayer != NO_PLAYER)
	{
		CvPlayerAI& kPlayer = GET_PLAYER(ePlayer);

		fFractionalYield += ComputeFractionalYieldFromAdjacentImprovement(*pkImprovementInfo, eYield);
		fFractionalYield += ComputeFractionalYieldFromAdjacentTerrain(*pkImprovementInfo, eYield);

		int iYieldChangePerEra = pkImprovementInfo->GetYieldChangePerEra(eYield);
		if (iYieldChangePerEra > 0)
		{
			int iPlotEra = GetArchaeologicalRecord().m_eEra;
			int iNumEras = kPlayer.GetCurrentEra() - iPlotEra;

			if (iPlotEra != NO_ERA && iNumEras > 0)
			{
				iYield += (iNumEras * iYieldChangePerEra);
			}
		}

		if (getOwner() == ePlayer)
		{
			for (iI = 0; iI < NUM_DIRECTION_TYPES; ++iI)
			{
				CvPlot* pAdjacentPlot = plotDirection(getX(), getY(), ((DirectionTypes)iI));

				if (pAdjacentPlot == NULL)
					continue;

				if (pAdjacentPlot->getFeatureType() != NO_FEATURE)
				{
					iYield += pkImprovementInfo->GetAdjacentFeatureYieldChanges(pAdjacentPlot->getFeatureType(), eYield);
				}

				if (pAdjacentPlot->getResourceType(kPlayer.getTeam()) != NO_RESOURCE)
				{
					iYield += pkImprovementInfo->GetAdjacentResourceYieldChanges(pAdjacentPlot->getResourceType(), eYield);
				}
			}
		}

		if (eResource != NO_RESOURCE)
		{
			iYield += pkImprovementInfo->GetImprovementResourceYield(eResource, eYield);
		}

		if (kPlayer.getGoldenAgeTurns() > 0)
		{
			iYield += pkImprovementInfo->GetGoldenAgeYieldChange(eYield);
		}
	}

	if (pOwningCity != NULL)
	{
		if (pOwningCity->GetWeLoveTheKingDayCounter() > 0)
		{
			iYield += pkImprovementInfo->GetWLTKDYieldChange(eYield);
		}
	}

	if(isRiver())
	{
		iYield += pkImprovementInfo->GetRiverSideYieldChange(eYield);
	}

	if(isCoastalLand())
	{
		iYield += pkImprovementInfo->GetCoastalLandYieldChange(eYield);
	}

	if(isHills())
	{
		iYield += pkImprovementInfo->GetHillsYieldChange(eYield);
	}

	if (eFeature != NO_FEATURE)
	{
		iYield += pkImprovementInfo->GetFeatureYieldChanges(eFeature, eYield);
	}

	// Check to see if there's a bonus to apply before doing any looping
	if	(	pkImprovementInfo->GetAdjacentCityYieldChange(eYield) > 0
		||	pkImprovementInfo->GetAdjacentMountainYieldChange(eYield) > 0
		)
	{
		for(iI = 0; iI < NUM_DIRECTION_TYPES; ++iI)
		{
			CvPlot* pAdjacentPlot = plotDirection(getX(), getY(), ((DirectionTypes)iI));

			if(pAdjacentPlot != NULL)
			{
				if(pAdjacentPlot->isCity())
				{
					// Is the owner of this Plot (with the Improvement) also the owner of an adjacent City?
					if(pAdjacentPlot->getPlotCity()->getOwner() == getOwner())
					{
						iYield += pkImprovementInfo->GetAdjacentCityYieldChange(eYield);
					}
				}
				if(pAdjacentPlot->isMountain())
				{
					iYield += pkImprovementInfo->GetAdjacentMountainYieldChange(eYield);
				}
			}
		}
	}

	if (bIsFreshWater || bOptimal)
	{
		iYield += pkImprovementInfo->GetFreshWaterYieldChange(eYield);
	}

	if(bOptimal)
	{
		iBestYield = 0;

		for(iI = 0; iI < GC.getNumRouteInfos(); ++iI)
		{
			iBestYield = std::max(iBestYield, pkImprovementInfo->GetRouteYieldChanges(iI, eYield));
		}

		iYield += iBestYield;
	}
	else
	{
		if(ePlayer != NO_PLAYER)
		{
			// GetRouteYieldChanges gives extra yieds if a trade route passes over the tile
			if(IsTradeUnitRoute() && eForceCityConnection == NUM_ROUTE_TYPES)
			{
				if( GET_PLAYER(ePlayer).GetCurrentEra() >= 4 )
				{
					iYield += GC.getImprovementInfo(eImprovement)->GetRouteYieldChanges(ROUTE_RAILROAD, eYield);
				}
				else
				{
					iYield += GC.getImprovementInfo(eImprovement)->GetRouteYieldChanges(ROUTE_ROAD, eYield);
				}
			}

			if (eRoute != NO_ROUTE || (eForceCityConnection != NUM_ROUTE_TYPES && eForceCityConnection != NO_ROUTE))
			{
				if ((eForceCityConnection == ROUTE_RAILROAD || (eForceCityConnection == NUM_ROUTE_TYPES && IsCityConnection(ePlayer, true /*bIndustrial*/))) && MOD_BALANCE_YIELD_SCALE_ERA)
				{
					iYield += pkImprovementInfo->GetRouteYieldChanges(ROUTE_RAILROAD, eYield);
				}
				else if (eForceCityConnection == ROUTE_ROAD || eForceCityConnection == ROUTE_RAILROAD || (eForceCityConnection == NUM_ROUTE_TYPES && IsCityConnection(ePlayer, false /*bIndustrial*/)))
				{
					iYield += pkImprovementInfo->GetRouteYieldChanges(ROUTE_ROAD, eYield);
				}

				if (eRoute != NO_ROUTE)
				{
					CvRouteInfo* pkRouteInfo = GC.getRouteInfo(eRoute);
					if (pkRouteInfo)
					{
						iYield += pkRouteInfo->getYieldChange(eYield);
					}
				}
			}
		}
	}

	if(bOptimal || ePlayer == NO_PLAYER)
	{
		for(iI = 0; iI < GC.getNumTechInfos(); ++iI)
		{
			iYield += pkImprovementInfo->GetTechYieldChanges(iI, eYield);

			if(bIsFreshWater)
			{
				iYield += pkImprovementInfo->GetTechFreshWaterYieldChanges(iI, eYield);
			}
			else
			{
				iYield += pkImprovementInfo->GetTechNoFreshWaterYieldChanges(iI, eYield);
			}
		}

		for(iI = 0; iI < GC.getNumPolicyInfos(); ++iI)
		{
			const PolicyTypes ePolicy = static_cast<PolicyTypes>(iI);
			CvPolicyEntry* pkPolicyEntry = GC.getPolicyInfo(ePolicy);
			if(pkPolicyEntry)
			{
				iYield += pkPolicyEntry->GetImprovementYieldChanges(eImprovement, eYield);
			}
		}
	}

	// add fractional yields
	iYield += fFractionalYield.Truncate();

	return iYield;
}

int CvPlot::calculatePlayerYield(YieldTypes eYield, int iCurrentYield, PlayerTypes ePlayer, ImprovementTypes eImprovement, FeatureTypes eFeature, ResourceTypes eResource, RouteTypes eForceCityConnection, const CvCity* pOwningCity, const CvReligion* pMajorityReligion, const CvBeliefEntry* pSecondaryPantheon, const CvReligion* pPlayerPantheon, bool bDisplay) const
{
	if (ePlayer == NO_PLAYER)
		return 0;

	int iYield = 0;

	const CvYieldInfo& kYield = *GC.getYieldInfo(eYield);
	CvPlayer& kPlayer = GET_PLAYER(ePlayer);
	CvTeam& kTeam = GET_TEAM(kPlayer.getTeam());
	CvPlayerTraits* pTraits = kPlayer.GetPlayerTraits();
	TerrainTypes eTerrain = getTerrainType();

	//plot yields
	iYield += pTraits->GetPlotYieldChange(getPlotType(), eYield);
	iYield += kPlayer.getPlotYieldChange(getPlotType(), eYield);

	if (isMountain())
	{
		if (!IsNaturalWonder(true))
		{
			int iRangeYield = pTraits->GetMountainRangeYield(eYield);
			int iEra = kPlayer.GetCurrentEra();
			if (iEra <= 0)
			{
				iEra = 1;
			}
			iRangeYield *= iEra;
			iYield += iRangeYield;
		}
		else
		{
			iYield += pTraits->GetYieldChangeNaturalWonder(eYield);
			iYield += kPlayer.GetYieldChangesNaturalWonder(eYield);

			if (eFeature != NO_FEATURE)
			{
				CvFeatureInfo* pFeatureInfo = GC.getFeatureInfo(eFeature);
				if (pFeatureInfo)
				{
					int iNWYieldChange = pFeatureInfo->getYieldChange(eYield);

					int iMod = 0;
					iMod += pTraits->GetNaturalWonderYieldModifier();

					//we only do this here because we need the natural yield right away.
					if (pMajorityReligion)
					{
						iMod += pMajorityReligion->m_Beliefs.GetYieldModifierNaturalWonder(eYield, ePlayer, pOwningCity);
						if (pSecondaryPantheon)
						{
							iMod += pSecondaryPantheon->GetYieldModifierNaturalWonder(eYield);
						}
					}

					if (MOD_RELIGION_PERMANENT_PANTHEON && pPlayerPantheon != NULL)
					{
						iMod += pPlayerPantheon->m_Beliefs.GetYieldModifierNaturalWonder(eYield, ePlayer, pOwningCity);
					}

					if (iMod != 0)
					{
						iNWYieldChange *= iMod;
						iNWYieldChange /= 100;

						iYield += iNWYieldChange;
					}
				}
			}
		}
	}
	else if (IsNaturalWonder(true))
	{

		iYield += pTraits->GetYieldChangeNaturalWonder(eYield);
		iYield += kPlayer.GetYieldChangesNaturalWonder(eYield);

		if (eFeature != NO_FEATURE)
		{
			CvFeatureInfo* pFeatureInfo = GC.getFeatureInfo(eFeature);
			if (pFeatureInfo)
			{
				int iNWYieldChange = pFeatureInfo->getYieldChange(eYield);

				int iMod = 0;
				iMod += pTraits->GetNaturalWonderYieldModifier();

				//we only do this here because we need the natural yield right away.
				if (pMajorityReligion)
				{
					iMod += pMajorityReligion->m_Beliefs.GetYieldModifierNaturalWonder(eYield, ePlayer, pOwningCity);
					if (pSecondaryPantheon)
					{
						iMod += pSecondaryPantheon->GetYieldModifierNaturalWonder(eYield);
					}
				}

				if (iMod != 0)
				{
					iNWYieldChange *= iMod;
					iNWYieldChange /= 100;

					iYield += iNWYieldChange;
				}
			}
		}
	}
	if (eImprovement != NO_IMPROVEMENT)
	{
		CvImprovementEntry* pImprovement = GC.getImprovementInfo(eImprovement);
		// Policy improvement yield changes
		iYield += kPlayer.getImprovementYieldChange(eImprovement, eYield);

		if (!pTraits->IsTradeRouteOnly())
		{
			// Trait player improvement yield changes that don't require a trade route connection
			iYield += pTraits->GetImprovementYieldChange(eImprovement, eYield);
		}
		// Team Tech Yield Changes
		iYield += kTeam.getImprovementYieldChange(eImprovement, eYield);

		if (isFreshWater())
		{
			// Team Tech Yield Changes
			iYield += kTeam.getImprovementFreshWaterYieldChange(eImprovement, eYield);
		}
		else
		{
			// Team Tech Yield Changes
			iYield += kTeam.getImprovementNoFreshWaterYieldChange(eImprovement, eYield);
		}

		// Player Accomplishment Yield Changes
		for (int iI = 0; iI < NUM_ACCOMPLISHMENTS_TYPES; iI++)
		{
			AccomplishmentTypes eAccomplishment = (AccomplishmentTypes)iI;
			int iCompletions = kPlayer.GetNumTimesAccomplishmentCompleted(eAccomplishment);

			if (iCompletions == 0)
				continue;

			iYield += pImprovement->GetAccomplishmentYieldChanges(eAccomplishment, eYield) * iCompletions;
		}
	}

	bool bIsCityConnection = IsCityConnection(ePlayer);
	if (eForceCityConnection != NUM_ROUTE_TYPES)
		bIsCityConnection = eForceCityConnection != NO_ROUTE;
	
	// Trait player terrain/improvement (for features handled below) yield changes that don't require a trade route connection
	if (pTraits->IsTradeRouteOnly() && getOwner() == ePlayer)
	{
		if (eFeature == NO_FEATURE && !MOD_USE_TRADE_FEATURES)
		{
			int iBonus = pTraits->GetTerrainYieldChange(eTerrain, eYield);
			if (iBonus > 0)
			{
				if (bIsCityConnection || IsTradeUnitRoute())
				{
					int iScale = 0;
					int iEra = (kPlayer.GetCurrentEra() + 1);

					iScale = ((iBonus * iEra) / 4);

					if (iScale <= 0)
					{
						iScale = 1;
					}
					iYield += iScale;
				}
			}
		}
		else
		{
			int iBonus = pTraits->GetTerrainYieldChange(eTerrain, eYield);
			if (iBonus > 0)
			{
				if (bIsCityConnection || IsTradeUnitRoute())
				{
					int iScale = 0;
					int iEra = (kPlayer.GetCurrentEra() + 1);

					iScale = ((iBonus * iEra) / 4);

					if (iScale <= 0)
					{
						iScale = 1;
					}
					iYield += iScale;
				}
			}
		}
		if (eImprovement != NO_IMPROVEMENT)
		{
			int iBonus2 = pTraits->GetImprovementYieldChange(eImprovement, eYield);
			if (iBonus2 > 0)
			{
				if (bIsCityConnection || IsTradeUnitRoute() || IsAdjacentToTradeRoute())
				{
					int iScale = 0;
					int iEra = (kPlayer.GetCurrentEra() + 1);

					iScale = ((iBonus2 * iEra) / 2);

					if (iScale <= 0)
					{
						iScale = 1;
					}
					iYield += iScale;
				}
			}
		}
	}
	else
	{
		iYield += pTraits->GetTerrainYieldChange(eTerrain, eYield);
	}

	iYield += kPlayer.getTerrainYieldChange(eTerrain, eYield);

	if (kPlayer.getExtraYieldThreshold(eYield) > 0)
	{
		if (iYield >= kPlayer.getExtraYieldThreshold(eYield))
		{
			iYield += /*1*/ GD_INT_GET(EXTRA_YIELD);
		}
	}

	int iBonusYield = eFeature == NO_FEATURE ? kTeam.getTerrainYieldChange(eTerrain, eYield) : kTeam.getFeatureYieldChange(eFeature, eYield);
	if (IsNaturalWonder())
	{
		int iMod = pTraits->GetNaturalWonderYieldModifier();
		if (iMod > 0)
		{
			iBonusYield *= (100 + iMod);
			iBonusYield /= 100;
		}
	}
	iYield += iBonusYield;

	if (pOwningCity != NULL)
	{
		if (isValidMovePlot(ePlayer))
		{
			if (isWater())
			{
				if (isLake())
				{ 
					iYield += kPlayer.getLakePlotYield(eYield);
				}
				else
				{
					iYield += kPlayer.getSeaPlotYield(eYield);
					iYield += pTraits->GetSeaPlotYieldChanges(eYield);
				}

				if (!bDisplay || pOwningCity->isRevealed(GC.getGame().getActiveTeam(), false, false))
				{
					int iCityYield = 0;
					if (isLake())
					{
						if (pOwningCity->getLakePlotYield(eYield) > 0)
						{
							iCityYield = pOwningCity->getLakePlotYield(eYield);
						}
					}
					else
					{
						iCityYield = pOwningCity->getSeaPlotYield(eYield);
					}
					iYield += iCityYield;
				}

				if (eResource != NO_RESOURCE)
				{
					if (!bDisplay || pOwningCity->isRevealed(GC.getGame().getActiveTeam(), false, false))
					{
						iYield += pOwningCity->getSeaResourceYield(eYield);
					}
				}
			}
			else if (isRiver())
			{
				if (!bDisplay || pOwningCity->isRevealed(GC.getGame().getActiveTeam(), false, false))
				{
					iYield += pOwningCity->getRiverPlotYield(eYield);
				}
			}

			// Extra yield for terrain
			if (eTerrain != NO_TERRAIN)
			{
				iYield += pOwningCity->GetTerrainExtraYield(eTerrain, eYield);
			}
		}

		if (eImprovement != NO_IMPROVEMENT)
		{
			CvImprovementEntry* pImprovement = GC.getImprovementInfo(eImprovement);
			if (pImprovement->IsCreatedByGreatPerson())
			{
				if (pOwningCity->GetWeLoveTheKingDayCounter() > 0)
				{
					if (pTraits->GetWLTKDGPImprovementModifier() > 0)
					{
						int iBoon = pTraits->GetWLTKDGPImprovementModifier();
						iYield *= (100 + iBoon);
						iYield /= 100;
					}
				}
			}

			// Extra yield for improvements
			iYield += pOwningCity->GetImprovementExtraYield(eImprovement, eYield);
			iYield += kPlayer.GetImprovementExtraYield(eImprovement, eYield);
		}


		// Extra yield for features
		if (eFeature != NO_FEATURE)
		{
			iYield += pOwningCity->GetFeatureExtraYield(eFeature, eYield);
		}
		
		iYield += pOwningCity->GetPlotExtraYield(getPlotType(), eYield);

		if (eResource != NO_RESOURCE)
		{
			const CvResourceInfo* pkResourceInfo = GC.getResourceInfo(eResource);
			if (pkResourceInfo)
			{
				if (kTeam.IsResourceRevealed(eResource))
				{
					// Extra yield from resources
					iYield += pOwningCity->GetResourceExtraYield(eResource, eYield);

					if (pkResourceInfo->getResourceUsage() == RESOURCEUSAGE_LUXURY)
					{
						iYield += pOwningCity->GetLuxuryExtraYield(eYield);
					}

					// Extra yield from Trait
					if (pkResourceInfo->getResourceUsage() == RESOURCEUSAGE_STRATEGIC)
					{
						iYield += pTraits->GetYieldChangeStrategicResources(eYield);
					}
				}

				iYield += pOwningCity->GetEventResourceYield(eResource, eYield);
				iYield += kPlayer.getResourceYieldChange(eResource, eYield);
				iYield += pTraits->GetResourceYieldChange(eResource, eYield);

				if (MOD_BALANCE_CORE_RESOURCE_MONOPOLIES && kPlayer.HasGlobalMonopoly(eResource))
				{
					int iTemp = pkResourceInfo->getYieldChangeFromMonopoly(eYield);
					if (iTemp > 0)
					{
						iTemp += GET_PLAYER(ePlayer).GetMonopolyModFlat();
						iYield += iTemp;
					}
				}
			}
		}

		if (eFeature != NO_FEATURE)
		{
			iYield += pOwningCity->GetEventFeatureYield(eFeature, eYield);
		}
		if (eTerrain != NO_TERRAIN)
		{
			iYield += pOwningCity->GetEventTerrainYield(eTerrain, eYield);

			for (int iI = 0; iI < NUM_DIRECTION_TYPES; ++iI)
			{
				CvPlot* pAdjacentPlot = plotDirection(getX(), getY(), ((DirectionTypes)iI));

				if (pAdjacentPlot != NULL && pAdjacentPlot->getImprovementType() != NO_IMPROVEMENT && pAdjacentPlot->getOwner() == ePlayer)
				{
					CvImprovementEntry* pkImprovementInfo2 = GC.getImprovementInfo(pAdjacentPlot->getImprovementType());
					if (pkImprovementInfo2 && pkImprovementInfo2->GetAdjacentTerrainYieldChanges(eTerrain, eYield) > 0)
					{
						iYield += pkImprovementInfo2->GetAdjacentTerrainYieldChanges(eTerrain, eYield);
					}
				}
			}
		}
		if (eImprovement != NO_IMPROVEMENT)
		{
			iYield += pOwningCity->GetEventImprovementYield(eImprovement, eYield);
		}
	}

	if (eFeature != NO_FEATURE)
	{
		// Player Trait
		if (eImprovement == NO_IMPROVEMENT && getImprovementType() == NO_IMPROVEMENT)
		{
			iYield += pTraits->GetUnimprovedFeatureYieldChange(eFeature, eYield);
			iYield += kPlayer.getUnimprovedFeatureYieldChange(eFeature, eYield);
		}

		// Leagues
		iYield += GC.getGame().GetGameLeagues()->GetFeatureYieldChange(ePlayer, eFeature, eYield);

		iYield += kPlayer.getFeatureYieldChange(eFeature, eYield);
		iYield += pTraits->GetFeatureYieldChange(eFeature, eYield);
	}

	if (eImprovement != NO_IMPROVEMENT && eYield == YIELD_CULTURE)
	{
		iYield += kPlayer.GetPlayerPolicies()->GetNumericModifier(POLICYMOD_EXTRA_CULTURE_FROM_IMPROVEMENTS);
		iYield += kPlayer.GetPlayerPolicies()->GetImprovementCultureChange(eImprovement);
	}

	if (kPlayer.isGoldenAge())
	{
		if ((iYield + iCurrentYield) >= kYield.getGoldenAgeYieldThreshold())
		{
			iYield += kYield.getGoldenAgeYield();
		}
	}

	return iYield;
}

int CvPlot::calculateYield(YieldTypes eYield, bool bDisplay, const CvCity* pOwningCity)
{
	if (!pOwningCity)
	{
		pOwningCity = getEffectiveOwningCity();
	}
	if(pOwningCity)
	{
		ReligionTypes eMajority = pOwningCity->GetCityReligions()->GetReligiousMajority();
		BeliefTypes eSecondaryPantheon = pOwningCity->GetCityReligions()->GetSecondaryReligionPantheonBelief();

		const CvReligion* pReligion = (eMajority != NO_RELIGION) ? GC.getGame().GetGameReligions()->GetReligion(eMajority, pOwningCity->getOwner()) : 0;
		const CvBeliefEntry* pBelief = (eSecondaryPantheon != NO_BELIEF) ? GC.GetGameBeliefs()->GetEntry(eSecondaryPantheon) : 0;

#if defined(MOD_RELIGION_PERMANENT_PANTHEON)
		// Mod for civs keeping their pantheon belief forever
		if (MOD_RELIGION_PERMANENT_PANTHEON)
		{
			if (GC.getGame().GetGameReligions()->HasCreatedPantheon(pOwningCity->getOwner()))
			{
				const CvReligion* pPantheon = GC.getGame().GetGameReligions()->GetReligion(RELIGION_PANTHEON, pOwningCity->getOwner());
				BeliefTypes ePantheonBelief = GC.getGame().GetGameReligions()->GetBeliefInPantheon(pOwningCity->getOwner());
				if (pPantheon != NULL && ePantheonBelief != NO_BELIEF && ePantheonBelief != eSecondaryPantheon)
				{
					if (pReligion == NULL || (pReligion != NULL && !pReligion->m_Beliefs.IsPantheonBeliefInReligion(ePantheonBelief, eMajority, pOwningCity->getOwner()))) // check that the our religion does not have our belief, to prevent double counting
					{
						return calculateYieldFast(eYield, bDisplay, pOwningCity, pReligion, pBelief, pPantheon);
					}
				}
			}
		}
#endif
		return calculateYieldFast(eYield, bDisplay, pOwningCity, pReligion, pBelief);
	}

	return calculateYieldFast(eYield, bDisplay, NULL, NULL, NULL);
}

//	--------------------------------------------------------------------------------
int CvPlot::calculateYieldFast(YieldTypes eYield, bool bDisplay, const CvCity* pOwningCity, const CvReligion* pMajorityReligion, const CvBeliefEntry* pSecondaryPantheon, const CvReligion* pPlayerPantheon)
{
	ImprovementTypes eImprovement = NO_IMPROVEMENT;
	RouteTypes eRoute = NO_ROUTE;
	PlayerTypes ePlayer = NO_PLAYER;

	if(bDisplay && GC.getGame().isDebugMode())
	{
		return getYield(eYield);
	}

	if(getTerrainType() == NO_TERRAIN)
	{
		return 0;
	}
#if defined(MOD_NO_YIELD_ICE)
	if(MOD_NO_YIELD_ICE)
	{
		if(isIce())
		{
			return 0;
		}
	}
#endif

	if(!isPotentialCityWork())
	{
		return 0;
	}

	if(bDisplay)
	{
		ePlayer = getRevealedOwner(GC.getGame().getActiveTeam());
		eImprovement = getRevealedImprovementType(GC.getGame().getActiveTeam());
		eRoute = getRevealedRouteType(GC.getGame().getActiveTeam());

		if(ePlayer == NO_PLAYER)
		{
			ePlayer = GC.getGame().getActivePlayer();
		}
	}
	else
	{
		ePlayer = getOwner();
		eImprovement = getImprovementType();
		eRoute = getRouteType();

		// For tile picker
		if (ePlayer == NO_PLAYER && pOwningCity)
		{
			ePlayer = pOwningCity->getOwner();
		}
	}

	if (IsImprovementPillaged())
		eImprovement = NO_IMPROVEMENT;
	if (IsRoutePillaged())
		eRoute = NO_ROUTE;

	FeatureTypes eFeature = getFeatureType();
	ResourceTypes eResource = bDisplay ? getResourceType(GC.getGame().getActiveTeam())  : getResourceType((ePlayer != NO_PLAYER) ? GET_PLAYER(ePlayer).getTeam() : NO_TEAM);

	int iYield = calculateNatureYield(eYield, ePlayer, eFeature, eResource, pOwningCity, bDisplay);
	iYield += calculateReligionImprovementYield(eYield, ePlayer, eImprovement, eResource, pOwningCity, pMajorityReligion, pSecondaryPantheon);
	iYield += calculateReligionNatureYield(eYield, ePlayer, eImprovement, eFeature, eResource, pOwningCity, pMajorityReligion, pSecondaryPantheon);
	iYield += calculateImprovementYield(eYield, ePlayer, eImprovement, eRoute, eFeature, eResource, NUM_ROUTE_TYPES, pOwningCity, false);
	iYield += calculatePlayerYield(eYield, iYield, ePlayer, eImprovement, eFeature, eResource, NUM_ROUTE_TYPES, pOwningCity, pMajorityReligion, pSecondaryPantheon, pPlayerPantheon, bDisplay);
#if defined(MOD_RELIGION_PERMANENT_PANTHEON)
	if (MOD_RELIGION_PERMANENT_PANTHEON && pPlayerPantheon != NULL)
	{
		iYield += calculateReligionImprovementYield(eYield, ePlayer, eImprovement, eResource, pOwningCity, pPlayerPantheon, NULL);
		iYield += calculateReligionNatureYield(eYield, ePlayer, eImprovement, eFeature, eResource, pOwningCity, pPlayerPantheon, NULL);
	}
#endif

	//no overhead if empty
	for (size_t i=0; i<m_vExtraYields.size(); i++)
    {
		if (m_vExtraYields[i].first == eYield)
		{
			iYield += m_vExtraYields[i].second;
			break;
		}
    }

	return std::max(0, iYield);
}


//	--------------------------------------------------------------------------------
bool CvPlot::hasYield() const
{
	for(int iI = 0; iI < NUM_YIELD_TYPES; ++iI)
	{
		//Simplification - errata yields not worth considering.
		if ((YieldTypes)iI > YIELD_CULTURE_LOCAL && !MOD_BALANCE_CORE_JFD)
			return false;

		if(getYield((YieldTypes)iI) > 0)
			return true;
	}

	return false;
}


//	--------------------------------------------------------------------------------
void CvPlot::updateYield()
{
	CvCity*	pOwningCity = getEffectiveOwningCity();
	if (pOwningCity)
	{
		ReligionTypes eMajority = pOwningCity->GetCityReligions()->GetReligiousMajority();
		BeliefTypes eSecondaryPantheon = pOwningCity->GetCityReligions()->GetSecondaryReligionPantheonBelief();

		const CvReligion* pReligion = (eMajority != NO_RELIGION) ? GC.getGame().GetGameReligions()->GetReligion(eMajority, pOwningCity->getOwner()) : 0;
		const CvBeliefEntry* pBelief = (eSecondaryPantheon != NO_BELIEF) ? GC.GetGameBeliefs()->GetEntry(eSecondaryPantheon) : 0;

#if defined(MOD_RELIGION_PERMANENT_PANTHEON)
		// Mod for civs keeping their pantheon belief forever
		if (MOD_RELIGION_PERMANENT_PANTHEON)
		{
			if (GC.getGame().GetGameReligions()->HasCreatedPantheon(getOwner()))
			{
				const CvReligion* pPantheon = GC.getGame().GetGameReligions()->GetReligion(RELIGION_PANTHEON, getOwner());
				BeliefTypes ePantheonBelief = GC.getGame().GetGameReligions()->GetBeliefInPantheon(getOwner());
				if (pPantheon != NULL && ePantheonBelief != NO_BELIEF && ePantheonBelief != eSecondaryPantheon)
				{
					updateYieldFast(pOwningCity, pReligion, pBelief, pPantheon);
					return;
				}
			}
		}
#endif
		updateYieldFast(pOwningCity, pReligion, pBelief);
		return;
	}

	updateYieldFast(NULL, NULL, NULL);
}

void CvPlot::updateYieldFast(CvCity* pWorkingCity, const CvReligion* pMajorityReligion, const CvBeliefEntry* pSecondaryPantheon, const CvReligion* pPlayerPantheon)
{
	bool bChange = false;
	if(getArea() == -1)
		return;

	for(int iI = 0; iI < NUM_YIELD_TYPES; ++iI)
	{
		YieldTypes eYield = (YieldTypes)iI;
		//Simplification - errata yields not worth considering.
		if (eYield > YIELD_CULTURE_LOCAL && !MOD_BALANCE_CORE_JFD)
			continue;

		int iNewYield = calculateYieldFast(eYield,false,pWorkingCity,pMajorityReligion,pSecondaryPantheon,pPlayerPantheon);

		if(getYield(eYield) != iNewYield)
		{
			int iOldYield = getYield(eYield);
			m_aiYield[iI] = max(0, iNewYield);

			if(pWorkingCity != NULL && pWorkingCity->GetCityCitizens()->IsWorkingPlot(this))
			{
				int iDelta = iNewYield - iOldYield;
				pWorkingCity->ChangeBaseYieldRateFromTerrain(eYield, iDelta);
				pWorkingCity->UpdateCityYields(eYield);
			}

			bChange = true;
		}
	}

	if(bChange)
		updateSymbols();
}

//	--------------------------------------------------------------------------------
int CvPlot::GetExplorationBonus(const CvPlayer* pPlayer, const CvUnit* pUnit)
{
	if (!pPlayer || !pUnit || pPlayer->getNumCities()==0)
		return 0;

	CvPlot* pRefPlot = pUnit->plot();

	//land based exploration - give a bonus to fertile tiles that are close to our own territory
	if (pUnit->getDomainType() == DOMAIN_LAND)
	{
		int iBonus = 0;
		if (pPlayer->getCapitalCity())
		{
			//do not use the founding values here, they are expensive to compute
			int iFertility = GC.getGame().GetSettlerSiteEvaluator()->PlotFertilityValue(this, pPlayer, true);
			int iRefFertility = GC.getGame().GetSettlerSiteEvaluator()->PlotFertilityValue(pPlayer->getCapitalCity()->plot(), pPlayer, true);
			iBonus = range((iFertility * 100) / MAX(1, iRefFertility), 0, 100);
		}

		if (getOwner() == NO_PLAYER)
			iBonus += 20;

		int iDistToOwnCities = pPlayer->GetCityDistancePathLength(this);
		int iDistRef = pPlayer->GetCityDistancePathLength(pRefPlot);
		if (iDistToOwnCities < iDistRef)
			iBonus += 20;

		return iBonus;
	}

	//naval exploration - the further away, the better
	return pPlayer->GetCityDistanceInPlots(this) - pPlayer->GetCityDistanceInPlots(pRefPlot);
}

//	--------------------------------------------------------------------------------
int CvPlot::getFoundValue(PlayerTypes eIndex)
{
	//this is just an indirection - the found values are stored in the player class now
	return GET_PLAYER(eIndex).getPlotFoundValue(getX(), getY() );
}


//	--------------------------------------------------------------------------------
bool CvPlot::isBestAdjacentFoundValue(PlayerTypes eIndex)
{
	CvPlayer& thisPlayer = GET_PLAYER(eIndex);
	int iPlotValue = getFoundValue(eIndex);

	if(iPlotValue == 0)
	{
		return false;
	}

	for(int iI = 0; iI < NUM_DIRECTION_TYPES; ++iI)
	{
		CvPlot* pAdjacentPlot = plotDirection(getX(), getY(), ((DirectionTypes)iI));

		if((pAdjacentPlot != NULL) && pAdjacentPlot->isRevealed(thisPlayer.getTeam()))
		{
			if(pAdjacentPlot->getFoundValue(eIndex) > iPlotValue)
			{
				return false;
			}
		}
	}

	return true;
}


//	--------------------------------------------------------------------------------
void CvPlot::setFoundValue(PlayerTypes eIndex, int iNewValue)
{
	//this is just an indirection - the found values are stored in the player class now
	GET_PLAYER(eIndex).setPlotFoundValue(getX(), getY(), iNewValue);
}


//	--------------------------------------------------------------------------------
int CvPlot::getPlayerCityRadiusCount(PlayerTypes eIndex) const
{
	ASSERT_DEBUG(eIndex >= 0, "eIndex is expected to be non-negative (invalid Index)");
	ASSERT_DEBUG(eIndex < MAX_PLAYERS, "eIndex is expected to be within maximum bounds (invalid Index)");

	return m_aiPlayerCityRadiusCount[eIndex];
}


//	--------------------------------------------------------------------------------
bool CvPlot::isPlayerCityRadius(PlayerTypes eIndex) const
{
	return (getPlayerCityRadiusCount(eIndex) > 0);
}


//	--------------------------------------------------------------------------------
void CvPlot::changePlayerCityRadiusCount(PlayerTypes eIndex, int iChange)
{
	ASSERT_DEBUG(eIndex >= 0, "eIndex is expected to be non-negative (invalid Index)");
	ASSERT_DEBUG(eIndex < MAX_PLAYERS, "eIndex is expected to be within maximum bounds (invalid Index)");

	if(0 != iChange)
	{
		m_aiPlayerCityRadiusCount[eIndex] += iChange;
		ASSERT_DEBUG(getPlayerCityRadiusCount(eIndex) >= 0);
	}
}

void CvPlot::flipVisibility(TeamTypes eTeam)
{
	if (eTeam == NO_TEAM)
		return;

	//flip visibility
	if (m_aiVisibilityCountThisTurnMax[eTeam] != m_aiVisibilityCount[eTeam])
	{
		m_aiVisibilityCountThisTurnMax[eTeam] = m_aiVisibilityCount[eTeam];
		if (m_aiVisibilityCount[eTeam] == 0) //in case it's now invisible
			updateFog();
	}
}

int CvPlot::getVisiblityCount(TeamTypes eTeam)
{
	return m_aiVisibilityCount[eTeam];
}
//	--------------------------------------------------------------------------------
PlotVisibilityChangeResult CvPlot::changeVisibilityCount(TeamTypes eTeam, int iChange, InvisibleTypes eSeeInvisible, bool bInformExplorationTracking, bool bAlwaysSeeInvisible, CvUnit* pUnit)
{
	ASSERT_DEBUG(eTeam >= 0, "eTeam is expected to be non-negative (invalid Index)");
	ASSERT_DEBUG(eTeam < MAX_TEAMS, "eTeam is expected to be within maximum bounds (invalid Index)");

	PlotVisibilityChangeResult eResult = VISIBILTY_CHANGE_NONE;
	if (iChange == 0)
		return eResult;

	bool bOldVisibility = (m_aiVisibilityCount[eTeam] > 0);
	bool bOldMaxVisibility = MOD_CORE_DELAYED_VISIBILITY ? (m_aiVisibilityCountThisTurnMax[eTeam] > 0) : bOldVisibility;

	// Apparently it's legal to decrease sight below zero - so catch that
	m_aiVisibilityCount[eTeam] = max(0, m_aiVisibilityCount[eTeam] + iChange);

	// Remember the maximum
	m_aiVisibilityCountThisTurnMax[eTeam] = max(m_aiVisibilityCountThisTurnMax[eTeam], m_aiVisibilityCount[eTeam]);

	if (eSeeInvisible != NO_INVISIBLE)
	{
		changeInvisibleVisibilityCount(eTeam, eSeeInvisible, iChange);
	}

	if (bAlwaysSeeInvisible)
	{
		changeInvisibleVisibilityCountUnit(eTeam, iChange);
		for(int iI = 0; iI < NUM_INVISIBLE_TYPES; iI++)
		{
			changeInvisibleVisibilityCount(eTeam, (InvisibleTypes) iI, iChange);
		}
	}

	// We couldn't see the Plot before but we can now
	// note: the isVisible check works even when MOD_CORE_DELAYED_VISIBILITY is active
	if (!bOldVisibility && isVisible(eTeam))
	{
		eResult = VISIBILITY_CHANGE_TO_VISIBLE;

		if (setRevealed(eTeam, true, pUnit)) // Change to revealed, returns true if the visibility was changed
		{
			// We are seeing this plot for the first time
			if (bInformExplorationTracking)
			{
				vector<PlayerTypes> vPlayers = GET_TEAM(eTeam).getPlayers();
				for (size_t i = 0; i < vPlayers.size(); i++)
				{
					GET_PLAYER(vPlayers[i]).GetEconomicAI()->UpdateExplorePlotsLocally(this);
				}
			}
		}
		else
		{
			// The visibility was not changed because it was already revealed, but we are changing to a visible state as well, so we must update.
			// Just trying to avoid redundant messages.
			if (eTeam == GC.getGame().getActiveTeam())
			{
				updateSymbols();
				updateFog(true);
				updateVisibility();
			}
		}

		// Update tactical AI, let it know that the tile was made visible
		if (!bOldMaxVisibility)
		{
			PlayerTypes eCurrentPlayer = GC.getGame().GetCurrentVisibilityPlayer();
			if (eCurrentPlayer != NO_PLAYER && GET_PLAYER(eCurrentPlayer).getTeam() == eTeam)
				GET_PLAYER(eCurrentPlayer).GetTacticalAI()->UpdateVisibilityFromUnits(this);
		}

		for (int iI = 0; iI < NUM_DIRECTION_TYPES; ++iI)
		{
			CvPlot* pAdjacentPlot = plotDirection(getX(), getY(), ((DirectionTypes)iI));

			if (pAdjacentPlot != NULL)
			{
				pAdjacentPlot->updateRevealedOwner(eTeam);
			}
		}

		// If there's a City here, meet its owner
		if (isCity())
		{
			// If the AI spots a human City, don't meet - wait for the human to find the AI
			if (GET_TEAM(eTeam).isHuman() || !getPlotCity()->isHuman())
			{
				GET_TEAM(eTeam).meet(getTeam(), false);	// If there's a City here, we can assume its owner is the same as the plot owner
			}

			GC.getGame().GetGameTrade()->InvalidateTradePathTeamCache(getTeam());
			GC.getGame().GetGameTrade()->InvalidateTradePathTeamCache(eTeam);
		}

		if (eTeam == GC.getGame().getActiveTeam())
		{
			updateCenterUnit();
		}
	}

	// Check units. Does not matter if we could see the plot before, invisible units might have turned visible!
	if (iChange > 0)
	{
		for (int iUnitLoop = 0; iUnitLoop < getNumUnits(); iUnitLoop++)
		{
			CvUnit* loopUnit = getUnitByIndex(iUnitLoop);
			if (!loopUnit || loopUnit->isInvisible(eTeam, false, false))
				continue;

			// If it is an enemy unit, update the danger plots! 
			if (GET_TEAM(eTeam).isAtWar(loopUnit->getTeam()))
			{
				const std::vector<PlayerTypes>& aePlayers = GET_TEAM(eTeam).getPlayers();
				for (size_t iI = 0; iI < aePlayers.size(); iI++)
				{
					PlayerTypes ePlayer = (PlayerTypes)aePlayers[iI];
					if (ePlayer != NO_PLAYER)
					{
						//if the attacker was not known before, it's new
						//if we could not see the plot before, it's sure to be new
						//need both conditions because AI sometimes infers the presence of units in invisible plots
						//so they are already known attackers!
						if (GET_PLAYER(ePlayer).AddKnownAttacker(loopUnit) || !bOldMaxVisibility)
							//todo: create a new tactical target as well? but might be too late
							//anyway might want to interrupt move mission ... depends on pathing flags
							if (pUnit)
								pUnit->SetSpottedEnemy(true);
					}
				}
			}

			// If the AI spots a human Unit, don't meet - wait for the human to find the AI
			if (GET_TEAM(eTeam).isHuman())
				GET_TEAM(eTeam).meet(loopUnit->getTeam(), false);
		}
	}

	// We could se the plot before but not anymore
	// With delayed visibility we do this in setTurnActive()
	if (!MOD_CORE_DELAYED_VISIBILITY && bOldVisibility && !isVisible(eTeam))
	{
		eResult = VISIBILITY_CHANGE_TO_INVISIBLE;
		if (eTeam == GC.getGame().getActiveTeam())
		{
			updateFog(true);
			updateCenterUnit();
		}
	}

	PlayerTypes eObserverUIPlayer = GC.getGame().getObserverUIOverridePlayer();
	if (eObserverUIPlayer != NO_PLAYER && GET_PLAYER(eObserverUIPlayer).getTeam() == eTeam && GET_PLAYER(GC.getGame().getActivePlayer()).isObserver())
	{
		// observer UI following eTeam? update visibility also for observer
		changeVisibilityCount(GET_PLAYER(GC.getGame().getActivePlayer()).getTeam(), iChange, eSeeInvisible, bInformExplorationTracking, bAlwaysSeeInvisible);
	}

	return eResult;
}

//	--------------------------------------------------------------------------------
PlayerTypes CvPlot::getRevealedOwner(TeamTypes eTeam, bool bDebug) const
{
	if(bDebug && GC.getGame().isDebugMode())
	{
		return getOwner();
	}
	else
	{
		ASSERT_DEBUG(eTeam >= 0, "eTeam is expected to be non-negative (invalid Index)");
		ASSERT_DEBUG(eTeam < MAX_TEAMS, "eTeam is expected to be within maximum bounds (invalid Index)");

		return (PlayerTypes)m_aiRevealedOwner[eTeam];
	}
}


//	--------------------------------------------------------------------------------
TeamTypes CvPlot::getRevealedTeam(TeamTypes eTeam, bool bDebug) const
{
	ASSERT_DEBUG(eTeam >= 0, "eTeam is expected to be non-negative (invalid Index)");
	ASSERT_DEBUG(eTeam < MAX_TEAMS, "eTeam is expected to be within maximum bounds (invalid Index)");

	PlayerTypes eRevealedOwner = getRevealedOwner(eTeam, bDebug);

	if(eRevealedOwner != NO_PLAYER)
	{
		return GET_PLAYER(eRevealedOwner).getTeam();
	}
	else
	{
		return NO_TEAM;
	}
}

//	--------------------------------------------------------------------------------
PlayerTypes CvPlot::getRevealedOwner(TeamTypes eTeam) const
{
	ASSERT_DEBUG(eTeam >= 0, "eTeam is expected to be non-negative (invalid Index)");
	ASSERT_DEBUG(eTeam < MAX_TEAMS, "eTeam is expected to be within maximum bounds (invalid Index)");

	return (PlayerTypes)m_aiRevealedOwner[eTeam];
}


//	--------------------------------------------------------------------------------
TeamTypes CvPlot::getRevealedTeam(TeamTypes eTeam) const
{
	ASSERT_DEBUG(eTeam >= 0, "eTeam is expected to be non-negative (invalid Index)");
	ASSERT_DEBUG(eTeam < MAX_TEAMS, "eTeam is expected to be within maximum bounds (invalid Index)");

	PlayerTypes eRevealedOwner = getRevealedOwner(eTeam);

	if(eRevealedOwner != NO_PLAYER)
	{
		return GET_PLAYER(eRevealedOwner).getTeam();
	}
	else
	{
		return NO_TEAM;
	}
}


//	--------------------------------------------------------------------------------
bool CvPlot::setRevealedOwner(TeamTypes eTeam, PlayerTypes eNewValue)
{
	ASSERT_DEBUG(eTeam >= 0, "eTeam is expected to be non-negative (invalid Index)");
	ASSERT_DEBUG(eTeam < MAX_TEAMS, "eTeam is expected to be within maximum bounds (invalid Index)");

	if(getRevealedOwner(eTeam, false) != eNewValue)
	{
		m_aiRevealedOwner[eTeam] = eNewValue;

		// Let the team know we've found someone (if eNewValue IS someone)
		if(eNewValue != NO_PLAYER)
		{
			GET_TEAM(eTeam).SetHasFoundPlayersTerritory(eNewValue, true);
		}
		return true;
	}

	ASSERT_DEBUG(m_aiRevealedOwner[eTeam] == eNewValue);
	return false;
}


//	--------------------------------------------------------------------------------
void CvPlot::updateRevealedOwner(TeamTypes eTeam)
{
	ASSERT_DEBUG(eTeam >= 0, "eTeam is expected to be non-negative (invalid Index)");
	ASSERT_DEBUG(eTeam < MAX_TEAMS, "eTeam is expected to be within maximum bounds (invalid Index)");

	//we change the revealed owner when the plot or a neighbor is visible (not revealed)
	bool bRevealed = isVisible(eTeam);

	if(!bRevealed)
	{
		for(int iI = 0; iI < NUM_DIRECTION_TYPES; ++iI)
		{
			CvPlot* pAdjacentPlot = plotDirection(getX(), getY(), ((DirectionTypes)iI));

			if(pAdjacentPlot != NULL)
			{
				if(pAdjacentPlot->isVisible(eTeam))
				{
					bRevealed = true;
					break;
				}
			}
		}
	}

	//only change the status if we consider the plot ownership revealed
	if (bRevealed)
		setRevealedOwner(eTeam, getOwner());
}


//	--------------------------------------------------------------------------------
bool CvPlot::isRiverCrossing(DirectionTypes eIndex) const
{
	ASSERT_DEBUG(eIndex < NUM_DIRECTION_TYPES, "eIndex is expected to be within maximum bounds (invalid Index)");

	if(eIndex == NO_DIRECTION)
	{
		return false;
	}

	int iFlowMask = 1 << eIndex;

	return ((m_cRiverCrossing & iFlowMask) != 0);
}


//	--------------------------------------------------------------------------------
void CvPlot::updateRiverCrossing(DirectionTypes eIndex)
{
	ASSERT_DEBUG(eIndex >= 0, "eIndex is expected to be non-negative (invalid Index)");
	ASSERT_DEBUG(eIndex < NUM_DIRECTION_TYPES, "eIndex is expected to be within maximum bounds (invalid Index)");

	bool bValid = false;
	CvPlot* pPlot = plotDirection(getX(), getY(), eIndex);

	if (!pPlot || (!pPlot->isWater() && !isWater())) // if there is no plot in that direction or both plots are land
	{
		switch (eIndex)
		{
		case DIRECTION_NORTHEAST:
			bValid = pPlot && pPlot->isNEOfRiver();
			break;

		case DIRECTION_EAST:
			bValid = isWOfRiver();
			break;

		case DIRECTION_SOUTHEAST:
			bValid = isNWOfRiver();
			break;

		case DIRECTION_SOUTHWEST:
			bValid = isNEOfRiver();
			break;

		case DIRECTION_WEST:
			bValid = pPlot && pPlot->isWOfRiver();
			break;

		case DIRECTION_NORTHWEST:
			bValid = pPlot && pPlot->isNWOfRiver();
			break;

		default:
			UNREACHABLE();
			break;
		}
	}

	if (isRiverCrossing(eIndex) != bValid)
	{
		char iFlowMask = 1 << eIndex;
		if (bValid)
		{
			m_cRiverCrossing |= iFlowMask;
			changeRiverCrossingCount(1);
		}
		else
		{
			m_cRiverCrossing &= ~iFlowMask;
			changeRiverCrossingCount(-1);
		}
	}
}


//	--------------------------------------------------------------------------------
void CvPlot::updateRiverCrossing()
{
	for (int iI = 0; iI < NUM_DIRECTION_TYPES; ++iI)
	{
		updateRiverCrossing(static_cast<DirectionTypes>(iI));
	}
}

//	--------------------------------------------------------------------------------
/// Force reveal a Resource for a team (for Goody Huts)
bool CvPlot::IsResourceForceReveal(TeamTypes eTeam) const
{
	return m_abResourceForceReveal[eTeam];
}

//	--------------------------------------------------------------------------------
/// Set force reveal a Resource for a team (for Goody Huts)
void CvPlot::SetResourceForceReveal(TeamTypes eTeam, bool bValue)
{
	ASSERT_DEBUG(eTeam >= 0, "eTeam is expected to be non-negative (invalid Index)");
	ASSERT_DEBUG(eTeam < MAX_TEAMS, "eTeam is expected to be within maximum bounds (invalid Index)");
	m_abResourceForceReveal[eTeam] = bValue;
}

RoutePlanTypes CvPlot::GetPlannedRouteState(PlayerTypes ePlayer) const
{
	return (RoutePlanTypes)m_aeHumanPlannedRouteState[ePlayer];
}

void CvPlot::SetPlannedRouteState(PlayerTypes ePlayer, RoutePlanTypes eRoutePlanType)
{
	ASSERT_DEBUG(ePlayer >= 0, "ePlayer is expected to be non-negative (invalid Index)");
	ASSERT_DEBUG(ePlayer < MAX_PLAYERS, "ePlayer is expected to be within maximum bounds (invalid Index)");
	m_aeHumanPlannedRouteState[ePlayer] = eRoutePlanType;
}

//	--------------------------------------------------------------------------------
/// Current player's knowledge of other players' visibility count
/// If the current player team is the same as eTeam, return the actual visibility count
int CvPlot::GetKnownVisibilityCount(TeamTypes eTeam) const
{
	ASSERT_DEBUG(eTeam >= 0, "eTeam is expected to be non-negative (invalid Index)");
	ASSERT_DEBUG(eTeam < MAX_TEAMS, "eTeam is expected to be within maximum bounds (invalid Index)");

	PlayerTypes eCurrentPlayer = GC.getGame().GetCurrentVisibilityPlayer();
	if (eCurrentPlayer != NO_PLAYER)
	{
		if (GET_PLAYER(eCurrentPlayer).getTeam() == eTeam)
		{
			return getVisibilityCount(eTeam);
		}
	}

	return m_aiKnownVisibilityCount[eTeam];
}

//	--------------------------------------------------------------------------------
/// Is this plot visible to eTeam according to current player knowledge
bool CvPlot::IsKnownVisibleToTeam(TeamTypes eTeam) const
{
	return GetKnownVisibilityCount(eTeam) > 0;
}

//	--------------------------------------------------------------------------------
/// Is this plot visible to an enemy to ePlayer according to current player knowledge
bool CvPlot::IsKnownVisibleToEnemy(PlayerTypes ePlayer) const
{
	const std::vector<PlayerTypes>& vEnemies = GET_PLAYER(ePlayer).GetPlayersAtWarWith();

	for (std::vector<PlayerTypes>::const_iterator it = vEnemies.begin(); it != vEnemies.end(); ++it)
	{
		CvPlayer& kEnemy = GET_PLAYER(*it);
		if (kEnemy.isAlive() && kEnemy.IsAtWarWith(ePlayer))
		{
			if (IsKnownVisibleToTeam(kEnemy.getTeam()))
			{
				return true;
			}
		}
	}

	return false;
}

void CvPlot::IncreaseKnownVisibilityCount(TeamTypes eTeam, TeamTypes eTeam2)
{
	ASSERT_DEBUG(eTeam >= 0, "eTeam is expected to be non-negative (invalid Index)");
	ASSERT_DEBUG(eTeam < MAX_TEAMS, "eTeam is expected to be within maximum bounds (invalid Index)");
	m_aiKnownVisibilityCount[eTeam]++;
	if (eTeam2 != NO_TEAM)
		m_aiKnownVisibilityCount[eTeam2]++;
}

void CvPlot::ResetKnownVisibility()
{
	for (int iI = 0; iI < MAX_PLAYERS; iI++)
	{
		m_aiKnownVisibilityCount[iI] = 0;
	}
}

#if defined(MOD_BALANCE_CORE)
//	--------------------------------------------------------------------------------
bool CvPlot::IsTeamImpassable(TeamTypes eTeam) const
{
	ASSERT_DEBUG(eTeam >= 0, "eTeam is expected to be non-negative (invalid Index)");
	ASSERT_DEBUG(eTeam < REALLY_MAX_TEAMS, "eTeam is expected to be within maximum bounds (invalid Index)");
	return m_abIsImpassable[eTeam];
}
//	--------------------------------------------------------------------------------
/// Set force reveal a Resource for a team (for Goody Huts)
void CvPlot::SetTeamImpassable(TeamTypes eTeam, bool bValue)
{
	ASSERT_DEBUG(eTeam >= 0, "eTeam is expected to be non-negative (invalid Index)");
	ASSERT_DEBUG(eTeam < REALLY_MAX_TEAMS, "eTeam is expected to be within maximum bounds (invalid Index)");
	m_abIsImpassable[eTeam] = bValue;
}
#endif
//	--------------------------------------------------------------------------------
bool CvPlot::setRevealed(TeamTypes eTeam, bool bNewValue, CvUnit* pUnit, bool bTerrainOnly, TeamTypes eFromTeam)
{
	int iI = 0;
	
#if defined(MOD_EVENTS_TILE_REVEALED)
	// We need to capture this value here, as a Natural Wonder may update it before we need it
	int iRevealedMajors = getNumMajorCivsRevealed();
#endif

	ASSERT_DEBUG(eTeam >= 0, "eTeam is expected to be non-negative (invalid Index)");
	ASSERT_DEBUG(eTeam < MAX_TEAMS, "eTeam is expected to be within maximum bounds (invalid Index)");

	TeamTypes eActiveTeam = GC.getGame().getActiveTeam();
	ICvUserInterface2* pInterface =  GC.GetEngineUserInterface();

	bool bVisibilityUpdated = false;
	bool bRevealed = isRevealed(eTeam) != bNewValue;

	if(bRevealed)
	{
		bVisibilityUpdated = true;
		m_bfRevealed.ToggleBit(eTeam);

		bool bEligibleForAchievement = MOD_API_ACHIEVEMENTS ? GET_PLAYER(GC.getGame().getActivePlayer()).isHuman() && !GC.getGame().isGameMultiPlayer() : false;

		if(area())
		{
			area()->changeNumRevealedTiles(eTeam, (bNewValue ? 1 : -1));
		}

		// Update tactical AI, let it know that the tile was revealed
		PlayerTypes eCurrentPlayer = GC.getGame().GetCurrentVisibilityPlayer();
		if (eCurrentPlayer != NO_PLAYER)
			GET_PLAYER(eCurrentPlayer).GetTacticalAI()->UpdateVisibilityFromBorders(this);

		// Natural Wonder
		if(eTeam != BARBARIAN_TEAM && bNewValue && !GET_TEAM(eTeam).isObserver())
		{
			if(getFeatureType() != NO_FEATURE)
			{
				if(GC.getFeatureInfo(getFeatureType())->IsNaturalWonder(true))
				{
					GET_TEAM(eTeam).ChangeNumNaturalWondersDiscovered(1);

					int iNumNaturalWondersLeft = GC.getMap().GetNumNaturalWonders() - GET_TEAM(eTeam).GetNumNaturalWondersDiscovered();

					if (MOD_EVENTS_NW_DISCOVERY)
					{
						GAMEEVENTINVOKE_HOOK(GAMEEVENT_NaturalWonderDiscovered, eTeam, getFeatureType(), getX(), getY(), (getNumMajorCivsRevealed() == 0), (pUnit?pUnit->getOwner():NO_PLAYER), (pUnit?pUnit->GetID():-1));
					}
					else
					{
						ICvEngineScriptSystem1* pkScriptSystem = gDLL->GetScriptSystem();
						if (pkScriptSystem) 
						{
							CvLuaArgsHandle args;
							args->Push(eTeam);
							args->Push(getFeatureType());
							args->Push(getX());
							args->Push(getY());
							args->Push((getNumMajorCivsRevealed() == 0)); // bFirst

							bool bResult = false;
							LuaSupport::CallHook(pkScriptSystem, "NaturalWonderDiscovered", args.get(), bResult);
						}
					}
					
					Localization::String strText = Localization::Lookup("TXT_KEY_NOTIFICATION_FOUND_NATURAL_WONDER");
					strText << iNumNaturalWondersLeft;
					strText << GC.getFeatureInfo(getFeatureType())->GetTextKey();
					Localization::String strSummary = Localization::Lookup("TXT_KEY_NOTIFICATION_SUMMARY_FOUND_NATURAL_WONDER");

					// Loop through all players to give them a Notification
					for(iI = 0; iI < MAX_MAJOR_CIVS; ++iI)
					{
						CvPlayerAI& playerI = GET_PLAYER((PlayerTypes)iI);
						if(playerI.isAlive())
						{
							if(playerI.getTeam() == eTeam)
							{
								// Num Natural Wonders found in a player's area
								if(playerI.getStartingPlot() != NULL)
								{
									if(getArea() == playerI.getStartingPlot()->getArea())
									{
										playerI.ChangeNumNaturalWondersDiscoveredInArea(1);
									}
								}
								playerI.CalculateNetHappiness();

								// Add World Anchor
								if(eTeam == eActiveTeam)
									SetWorldAnchor(WORLD_ANCHOR_NATURAL_WONDER, getFeatureType());

								// Notification
								CvNotifications* pNotifications = GET_PLAYER((PlayerTypes)iI).GetNotifications();
								if(pNotifications)
								{
									pNotifications->Add(NOTIFICATION_EXPLORATION_RACE, strText.toUTF8(), strSummary.toUTF8(), getX(), getY(), getFeatureType());
								}
							}
						}
					}

					// FIRST (MAJOR CIV) FINDER?
					int iFinderGold = 0;
					bool bFirstFinder = false;
					CvTeam& kTeam = GET_TEAM(eTeam);
					if(!kTeam.isMinorCiv() && !kTeam.isBarbarian() && !kTeam.isObserver())
					{
						if(getNumMajorCivsRevealed() == 0)
						{
							changeNumMajorCivsRevealed(1);
							bFirstFinder = true;

							// Does it yields gold to the first major civ finder?
							iFinderGold = GC.getFeatureInfo(getFeatureType())->getFirstFinderGold();

							// Does a player on this team have a trait that gives first finder gold?
							for(iI = 0; iI < MAX_MAJOR_CIVS; ++iI)
							{
								CvPlayerAI& playerI = GET_PLAYER((PlayerTypes)iI);
								if(playerI.isAlive())
								{
									if(playerI.getTeam() == eTeam)
									{
										iFinderGold += playerI.GetPlayerTraits()->GetNaturalWonderFirstFinderGold();
									}
								}
							}
						}
						else
						{
							// Does a player on this team have a trait that gives subsequent finder gold?
							for(iI = 0; iI < MAX_MAJOR_CIVS; ++iI)
							{
								CvPlayerAI& playerI = GET_PLAYER((PlayerTypes)iI);
								if(playerI.isAlive())
								{
									if(playerI.getTeam() == eTeam)
									{
										iFinderGold += playerI.GetPlayerTraits()->GetNaturalWonderSubsequentFinderGold();
									}
								}
							}
						}

						// Scale up or down based on difficulty
						if(iFinderGold > 0)
						{
							const int iStandardHandicap = GC.getInfoTypeForString("HANDICAP_PRINCE");
							if(iStandardHandicap >= 0)
							{
								const CvHandicapInfo* pkHandicapInfo = GC.getHandicapInfo((HandicapTypes)iStandardHandicap);
								if(pkHandicapInfo)
								{
									iFinderGold *= pkHandicapInfo->getBarbarianCampGold();
									iFinderGold /= GC.getGame().getHandicapInfo().getBarbarianCampGold();
								}
							}
						}

						if(iFinderGold > 0)
						{
							// Compute gold per team member
							int iGoldPerTeamMember = iFinderGold;

							if(kTeam.getNumMembers() > 0)
							{
								iGoldPerTeamMember = iFinderGold / kTeam.getNumMembers();
							}

							for(iI = 0; iI < MAX_MAJOR_CIVS; ++iI)
							{
								CvPlayerAI& playerI = GET_PLAYER((PlayerTypes)iI);
								if(playerI.isAlive())
								{
									if(playerI.getTeam() == eTeam)
									{
										playerI.GetTreasury()->ChangeGold(iGoldPerTeamMember);
									}
								}
							}

							if(eTeam == eActiveTeam)
							{
								char text[256] = {0};
								sprintf_s(text, "[COLOR_YELLOW]+%d[ENDCOLOR][ICON_GOLD]", iFinderGold);
								SHOW_PLOT_POPUP(this, NO_PLAYER, text);
							}
						}
					}

					// If it's the active team then tell them they found something
					if(eTeam == eActiveTeam)
					{
						bool bDontShowRewardPopup = GC.GetEngineUserInterface()->IsOptionNoRewardPopups();

						if ( GET_TEAM((TeamTypes)iI).isObserver() )
							bDontShowRewardPopup = true;

						// Popup (no MP)
						if(!GC.getGame().isReallyNetworkMultiPlayer() && !bDontShowRewardPopup)
						{
							CvPopupInfo kPopupInfo(BUTTONPOPUP_NATURAL_WONDER_REWARD, getX(), getY(), iFinderGold, 0 /*iFlags */, bFirstFinder);
							pInterface->AddPopup(kPopupInfo);
							CvPlayer& kActivePlayer = GET_PLAYER(GC.getGame().getActivePlayer());
							if (kActivePlayer.getTeam() == eActiveTeam)
							{
								// We are adding a popup that the player must make a choice in, make sure they are not in the end-turn phase.
								CancelActivePlayerEndTurn();
							}

							//Add Stat and check for Achievement
							if (bEligibleForAchievement && !GC.getGame().isGameMultiPlayer())
							{
								gDLL->IncrementSteamStatAndUnlock(ESTEAMSTAT_NATURALWONDERS, 100, ACHIEVEMENT_ALL_NATURALWONDER);
							}
						}

						//DLC2 Natural Wonder Achievements
						if (MOD_API_ACHIEVEMENTS)
						{
							CvFeatureInfo* pkFeatureInfo = GC.getFeatureInfo(getFeatureType());
							if(pkFeatureInfo)
							{
								CvString strFeatureType = pkFeatureInfo->GetType();
								if(strFeatureType == "FEATURE_FOUNTAIN_YOUTH")
									gDLL->UnlockAchievement(ACHIEVEMENT_SCENARIO_02_DISCOVER_FOUNTAIN);
								else if(strFeatureType == "FEATURE_EL_DORADO")
									gDLL->UnlockAchievement(ACHIEVEMENT_SCENARIO_02_DISCOVER_EL_DORADO);
							}
						}

						CvInterfacePtr<ICvPlot1> pDllPlot(new CvDllPlot(this));
						gDLL->GameplayNaturalWonderRevealed(pDllPlot.get());
					}
				}
			}
		}

		if(eTeam == eActiveTeam)
		{
			updateSymbols();
			updateFog(true);
			updateVisibility();

			// Active player is seeing this Plot for the first time
			if(isRevealed(eTeam))
			{
				// update the resources
				if(getResourceType(eTeam) != NO_RESOURCE)
				{
					// If we've force revealed the resource for this team, then don't send another event
					if(!IsResourceForceReveal(eTeam))
					{
						setLayoutDirty(true);
					}
				}

				// Found a Goody Hut
				if(getImprovementType() != NO_IMPROVEMENT)
				{
					if(GC.getImprovementInfo(getImprovementType())->IsGoody())
					{
						CvString strBuffer = GetLocalizedText("TXT_KEY_NOTIFICATION_FOUND_GOODY_HUT");
						CvString strSummary = GetLocalizedText("TXT_KEY_NOTIFICATION_SUMMARY_FOUND_GOODY_HUT");

						if (bEligibleForAchievement)
						{
							gDLL->UnlockAchievement(ACHIEVEMENT_ANCIENT_RUIN);
							OutputDebugString("RUINS!");
						}

						for(iI = 0; iI < MAX_MAJOR_CIVS; ++iI)
						{
							CvPlayerAI& playerI = GET_PLAYER((PlayerTypes)iI);
							if(playerI.isAlive())
							{
								if(playerI.getTeam() == eTeam)
								{
									CvNotifications* pNotifications = playerI.GetNotifications();
									if(pNotifications)
									{
										pNotifications->Add(NOTIFICATION_GOODY, strBuffer, strSummary, getX(), getY(), -1);
									}
								}
							}
						}
					}
				}

				if (bEligibleForAchievement)
				{
					gDLL->IncrementSteamStatAndUnlock(ESTEAMSTAT_TILESDISCOVERED, 1000, ACHIEVEMENT_1000TILES);
				}
			}
		}

		if(bNewValue)
		{
			if (pUnit && (pUnit->IsGainsXPFromScouting() || pUnit->IsGainsYieldFromScouting()) && !GET_TEAM(eTeam).isBarbarian() && !GET_TEAM(eTeam).isMinorCiv())
			{
				if (IsNaturalWonder())
				{
					pUnit->ChangeNumTilesRevealedThisTurn(/*12*/ GD_INT_GET(BALANCE_SCOUT_XP_RANDOM_VALUE));
				}
				else if (isGoody())
				{
					pUnit->ChangeNumTilesRevealedThisTurn(/*4*/ GD_INT_GET(BALANCE_SCOUT_XP_RANDOM_VALUE) / 3);
				}
				else if (getResourceType(pUnit->getTeam()) != NO_RESOURCE)
				{
					pUnit->ChangeNumTilesRevealedThisTurn(/*3*/ GD_INT_GET(BALANCE_SCOUT_XP_RANDOM_VALUE) / 4);
				}
				else
				{
					pUnit->ChangeNumTilesRevealedThisTurn(1);
				}	
			}

			if (pInterface->GetHeadSelectedUnit() != NULL)
			{
				// This is what determines if the camera jumps quickly or slowly - if we're revealing new plots go slower.  The following function sets this flag
				pInterface->SetSelectedUnitRevealingNewPlots(true);
			}
		}
	}

	if(!bTerrainOnly)
	{
		bool bVisibilityChanged = false;
		bool bImprovementVisibilityChanged = false;
		if(isRevealed(eTeam))
		{
			// If this plot is owned by someone, let the team know
			if(getOwner() != NO_PLAYER)
			{
				bVisibilityChanged |= GET_TEAM(eTeam).SetHasFoundPlayersTerritory(getOwner(), true);
			}

			if(eFromTeam == NO_TEAM)
			{
				bVisibilityChanged |= setRevealedOwner(eTeam, getOwner());
				bImprovementVisibilityChanged |= setRevealedImprovementType(eTeam, getImprovementType());
				bVisibilityChanged |= setRevealedRouteType(eTeam, getRouteType());
			}
			else
			{
				if(getRevealedOwner(eFromTeam) == getOwner())
				{
					bVisibilityChanged |= setRevealedOwner(eTeam, getRevealedOwner(eFromTeam));
				}

				if(getRevealedImprovementType(eFromTeam) == getImprovementType())
				{
					bImprovementVisibilityChanged |= setRevealedImprovementType(eTeam, getRevealedImprovementType(eFromTeam));
				}

				if(getRevealedRouteType(eFromTeam) == getRouteType())
				{
					bVisibilityChanged |= setRevealedRouteType(eTeam, getRevealedRouteType(eFromTeam));
				}
			}
		}
		else
		{
			bVisibilityChanged |= setRevealedOwner(eTeam, NO_PLAYER);
			bImprovementVisibilityChanged |= setRevealedImprovementType(eTeam, NO_IMPROVEMENT);
			bVisibilityChanged |= setRevealedRouteType(eTeam, NO_ROUTE);
		}

		if (!bVisibilityUpdated && (bVisibilityChanged || bImprovementVisibilityChanged))
		{
			if(eTeam == eActiveTeam)
			{
				if (!bImprovementVisibilityChanged)	// Don't bother sending again if the improvement change already sent the message
					updateSymbols();

				updateFog(true);
				updateVisibility();
			}

			bVisibilityUpdated = true;
		}
	}
	
#if defined(MOD_EVENTS_TILE_REVEALED)
	CvTeam& kTeam = GET_TEAM(eTeam);
	
	if (MOD_EVENTS_TILE_REVEALED && bNewValue && bRevealed) {
		GAMEEVENTINVOKE_HOOK(GAMEEVENT_TileRevealed, getX(), getY(), eTeam, eFromTeam, (kTeam.isMajorCiv() && iRevealedMajors == 0), (pUnit ? pUnit->getOwner() : NO_PLAYER), (pUnit ? pUnit->GetID() : -1));
	}

	// This is badly named, as it's actually an on/off "revealed to any major" flag, not a counter
	if (getNumMajorCivsRevealed() == 0) {
		if (kTeam.isMajorCiv()) {
			changeNumMajorCivsRevealed(1);
		}
	}
#endif

	PlayerTypes eObserverUIPlayer = GC.getGame().getObserverUIOverridePlayer();
	if (eObserverUIPlayer != NO_PLAYER && GET_PLAYER(eObserverUIPlayer).getTeam() == eTeam && GET_PLAYER(GC.getGame().getActivePlayer()).isObserver() && !GET_TEAM(eTeam).isObserver())
	{
		// observer UI following eTeam? update revealed status also for observer
		setRevealed(GET_PLAYER(GC.getGame().getActivePlayer()).getTeam(), bNewValue, NULL, bTerrainOnly, eTeam);
	}

	return bVisibilityUpdated;
}

//	--------------------------------------------------------------------------------
bool CvPlot::isAdjacentRevealed(TeamTypes eTeam) const
{
	CvPlot** aPlotsToCheck = GC.getMap().getNeighborsUnchecked(this);
	for(int iI=0; iI<NUM_DIRECTION_TYPES; iI++)
	{
		CvPlot* pAdjacentPlot = aPlotsToCheck[iI];
		if(pAdjacentPlot != NULL)
		{
			if(pAdjacentPlot->isRevealed(eTeam))
			{
				return true;
			}
		}
	}

	return false;
}

//	--------------------------------------------------------------------------------
bool CvPlot::isAdjacentNonrevealed(TeamTypes eTeam) const
{
	CvPlot** aPlotsToCheck = GC.getMap().getNeighborsUnchecked(this);
	for(int iI=0; iI<NUM_DIRECTION_TYPES; iI++)
	{
		CvPlot* pAdjacentPlot = aPlotsToCheck[iI];
		if(pAdjacentPlot != NULL)
		{
			if(!pAdjacentPlot->isRevealed(eTeam))
			{
				return true;
			}
		}
	}

	return false;
}

//	--------------------------------------------------------------------------------
int CvPlot::getNumAdjacentNonrevealed(TeamTypes eTeam) const
{
	int iCount = 0;

	CvPlot** aPlotsToCheck = GC.getMap().getNeighborsUnchecked(this);
	for(int i=0; i<NUM_DIRECTION_TYPES; i++)
	{
		CvPlot* pAdjacentPlot = aPlotsToCheck[i];
		if(pAdjacentPlot != NULL)
		{
			if(!pAdjacentPlot->isRevealed(eTeam))
			{
				iCount++;
			}
		}
	}

	return iCount;
}


//	--------------------------------------------------------------------------------
ImprovementTypes CvPlot::getRevealedImprovementType(TeamTypes eTeam, bool bDebug) const
{
	if(bDebug && GC.getGame().isDebugMode())
	{
		return getImprovementType();
	}
	else
	{
		ASSERT_DEBUG(eTeam >= 0, "eTeam is expected to be non-negative (invalid Index)");
		ASSERT_DEBUG(eTeam < MAX_TEAMS, "eTeam is expected to be within maximum bounds (invalid Index)");

		return (ImprovementTypes)m_aeRevealedImprovementType[eTeam];
	}
}

//	--------------------------------------------------------------------------------
ImprovementTypes CvPlot::getRevealedImprovementType(TeamTypes eTeam) const
{
	ASSERT_DEBUG(eTeam >= 0, "eTeam is expected to be non-negative (invalid Index)");
	ASSERT_DEBUG(eTeam < MAX_TEAMS, "eTeam is expected to be within maximum bounds (invalid Index)");

	return (ImprovementTypes)m_aeRevealedImprovementType[eTeam];
}

//	--------------------------------------------------------------------------------
bool CvPlot::setRevealedImprovementType(TeamTypes eTeam, ImprovementTypes eNewValue)
{
	ASSERT_DEBUG(eTeam >= 0, "eTeam is expected to be non-negative (invalid Index)");
	ASSERT_DEBUG(eTeam < MAX_TEAMS, "eTeam is expected to be within maximum bounds (invalid Index)");

	if (eNewValue < NO_IMPROVEMENT) return false;
	if (eNewValue > NO_IMPROVEMENT && GC.getImprovementInfo(eNewValue) == NULL) return false;

	ImprovementTypes eOldImprovementType = getRevealedImprovementType(eTeam);
	if(eOldImprovementType != eNewValue)
	{
		m_aeRevealedImprovementType[eTeam] = eNewValue;
		if(eTeam == GC.getGame().getActiveTeam())
		{
			updateSymbols();
			setLayoutDirty(true);
		}
		// Found a Barbarian Camp
		if(eNewValue == GD_INT_GET(BARBARIAN_CAMP_IMPROVEMENT))
		{
			CvString strBuffer = GetLocalizedText("TXT_KEY_NOTIFICATION_FOUND_BARB_CAMP");
			CvString strSummary = GetLocalizedText("TXT_KEY_NOTIFICATION_SUMMARY_FOUND_BARB_CAMP");
			GET_TEAM(eTeam).AddNotification(NOTIFICATION_BARBARIAN, strBuffer, strSummary, getX(), getY());
		}
		return true;
	}
	return false;
}


//	--------------------------------------------------------------------------------
RouteTypes CvPlot::getRevealedRouteType(TeamTypes eTeam, bool bDebug) const
{
	if(bDebug && GC.getGame().isDebugMode())
	{
		return getRouteType();
	}
	else
	{
		ASSERT_DEBUG(eTeam >= 0, "eTeam is expected to be non-negative (invalid Index)");
		ASSERT_DEBUG(eTeam < MAX_TEAMS, "eTeam is expected to be within maximum bounds (invalid Index)");

		return (RouteTypes)m_aeRevealedRouteType[eTeam];
	}
}

//	--------------------------------------------------------------------------------
RouteTypes CvPlot::getRevealedRouteType(TeamTypes eTeam) const
{
	ASSERT_DEBUG(eTeam >= 0, "eTeam is expected to be non-negative (invalid Index)");
	ASSERT_DEBUG(eTeam < MAX_TEAMS, "eTeam is expected to be within maximum bounds (invalid Index)");

	return (RouteTypes)m_aeRevealedRouteType[eTeam];
}

//	--------------------------------------------------------------------------------
bool CvPlot::setRevealedRouteType(TeamTypes eTeam, RouteTypes eNewValue)
{
	ASSERT_DEBUG(eTeam >= 0, "eTeam is expected to be non-negative (invalid Index)");
	ASSERT_DEBUG(eTeam < MAX_TEAMS, "eTeam is expected to be within maximum bounds (invalid Index)");

	if (eNewValue < NO_ROUTE) return false;
	if (eNewValue > NO_ROUTE && GC.getRouteInfo(eNewValue) == NULL) return false;

	if(getRevealedRouteType(eTeam, false) != eNewValue)
	{
		if(eTeam == GC.getGame().getActiveTeam())
		{
			setLayoutDirty(true);
		}
		m_aeRevealedRouteType[eTeam] = eNewValue;
		return true;
	}
	return false;

}

//	--------------------------------------------------------------------------------
//	Reset all current builds related to route or improvement, based on eBuild.
void CvPlot::SilentlyResetAllBuildProgress(BuildTypes eBuild)
{
	if (m_buildProgress.size() == 0)
		return;

	if (eBuild == NO_BUILD)
	{
		m_buildProgress.clear();
		return;
	}

	CvBuildInfo* pkBuildInfo = GC.getBuildInfo(eBuild);
	if(pkBuildInfo == NULL)
		return;
	
	bool bBuildImprovement = pkBuildInfo->getImprovement() != NO_IMPROVEMENT;
	bool bBuildRoute = pkBuildInfo->getRoute() != NO_ROUTE;

	for (map<BuildTypes, int>::iterator it = m_buildProgress.begin(), next_it = it; it != m_buildProgress.end(); it = next_it)
	{
		++next_it;

		CvBuildInfo* pkIterInfo = GC.getBuildInfo(it->first);
		if(pkIterInfo == NULL)
			continue;

		bool bIterImprovement = pkIterInfo->getImprovement() != NO_IMPROVEMENT;
		bool bIterRoute = pkIterInfo->getRoute() != NO_ROUTE;

		// Two groupings: Improvement (build or repair), Route(build, repair or remove)
		if	(	( bBuildImprovement && ( bIterImprovement || ( pkIterInfo->isRepair() && !IsRoutePillaged() ) ) )
			||	( bBuildRoute && ( bIterRoute || ( pkIterInfo->isRepair() && !IsImprovementPillaged() ) || pkIterInfo->IsRemoveRoute() ) )
			||	( pkBuildInfo->IsRemoveRoute() && ( bIterRoute || ( pkIterInfo->isRepair() && !IsImprovementPillaged() ) ) )
			)
		{
			m_buildProgress.erase(it);
		}
	}
}

// used for diplomacy stuff and war score when stealing a tile using a citadel or America's UA (VP)
int CvPlot::GetStealPlotValue(PlayerTypes eStealingPlayer, bool& bStoleHighValueTile) const
{
	PlayerTypes ePlotOwner = getOwner();
	if (ePlotOwner == NO_PLAYER || GET_PLAYER(ePlotOwner).getTeam() == GET_PLAYER(eStealingPlayer).getTeam())
		return 0;

	bool bWorkable = isPlayerCityRadius(ePlotOwner);
	bool bHighValueTile = false;

	int iTileValue = /*80*/ GD_INT_GET(STOLEN_TILE_BASE_WAR_VALUE);
	int iValueMultiplier = 0;
	if (IsNaturalWonder())
	{
		iValueMultiplier += 200;
		bHighValueTile = true;
	}
	else
	{
		if (getResourceType(GET_PLAYER(ePlotOwner).getTeam()) != NO_RESOURCE)
		{
			CvResourceInfo* pInfo = GC.getResourceInfo(getResourceType(GET_PLAYER(ePlotOwner).getTeam()));
			if (pInfo)
			{
				switch (pInfo->getResourceUsage())
				{
				case RESOURCEUSAGE_STRATEGIC:
					iValueMultiplier += 100;
					bHighValueTile = true;
					break;
				case RESOURCEUSAGE_LUXURY:
					iValueMultiplier += 50;
					bHighValueTile = true;
					break;
				case RESOURCEUSAGE_BONUS:
					iValueMultiplier += 20;
					break;
				}
			}
		}

		bool bChokePoint = IsChokePoint();
		if (bChokePoint)
		{
			iValueMultiplier += 50;
			bHighValueTile = true;
		}

		ImprovementTypes eImprovement = getImprovementType();
		if (eImprovement != NO_IMPROVEMENT)
		{
			CvImprovementEntry* pkImprovementInfo = GC.getImprovementInfo(eImprovement);
			ASSERT_DEBUG(pkImprovementInfo);

			if (bChokePoint)
			{
				iValueMultiplier += pkImprovementInfo->GetDefenseModifier();
				if (pkImprovementInfo->IsNoFollowUp())
					iValueMultiplier += 20;
			}

			if (pkImprovementInfo->IsCreatedByGreatPerson())
			{
				iValueMultiplier += 100;
				bHighValueTile = bHighValueTile || bWorkable || (MOD_BALANCE_VP && GetPlayerThatBuiltImprovement() == ePlotOwner);
			}
		}
	}

	iTileValue *= 100 + iValueMultiplier;
	iTileValue /= 100;

	if (bHighValueTile)
		bStoleHighValueTile = true;
	else if (!bWorkable)
		iTileValue /= 2;

	return iTileValue;
}


//	--------------------------------------------------------------------------------
int CvPlot::getBuildProgress(BuildTypes eBuild) const
{
	map<BuildTypes,int>::const_iterator it = m_buildProgress.find(eBuild);

	if (it != m_buildProgress.end())
		return it->second;

	return 0;
}

//	--------------------------------------------------------------------------------
bool CvPlot::getAnyBuildProgress() const
{
	return !m_buildProgress.empty();
}


//	--------------------------------------------------------------------------------
// Returns true if build finished...
bool CvPlot::changeBuildProgress(BuildTypes eBuild, int iChange, PlayerTypes ePlayer, bool bNewBuild)
{
	CvCity* pCity = NULL;
	CvString strBuffer;
	int iProduction = 0;
	bool bFinished = false;
	CvPlayer &kPlayer = GET_PLAYER(ePlayer);

	CvBuildInfo* pkBuildInfo = GC.getBuildInfo(eBuild);
	ASSERT_DEBUG(pkBuildInfo);

	//This shouldn't happen.
	if(pkBuildInfo == NULL)
		return false;

	if (m_iLastTurnBuildChanged == GC.getGame().getGameTurn() && !bNewBuild)
		return false;

	if(iChange != 0)
	{
		// wipe out related build progress when starting a new build
		if (getBuildProgress(eBuild) == 0)
		{
			SilentlyResetAllBuildProgress(eBuild);
		}
		ImprovementTypes eImprovement = (ImprovementTypes)pkBuildInfo->getImprovement();

		m_iLastTurnBuildChanged = GC.getGame().getGameTurn();

		m_buildProgress[eBuild] += iChange;
		ASSERT_DEBUG(getBuildProgress(eBuild) >= 0);

		if(getBuildProgress(eBuild) >= getBuildTime(eBuild, ePlayer))
		{
			m_buildProgress.erase(eBuild);

			// Constructed Improvement
			if (eImprovement != NO_IMPROVEMENT)
			{
				setImprovementType(eImprovement, ePlayer);

				CvImprovementEntry& newImprovementEntry = *GC.getImprovementInfo(eImprovement);

				// If this improvement removes the underlying resource, do that
				if (newImprovementEntry.IsRemovesResource())
				{
					if (getResourceType() != NO_RESOURCE)
					{
						setResourceType(NO_RESOURCE, 0);
					}
				}

				// If we want to prompt the user about archaeology, let's record that
				if (newImprovementEntry.IsPromptWhenComplete())
				{
					if (GetArchaeologicalRecord().m_eArtifactType != NO_GREAT_WORK_ARTIFACT_CLASS)
					{
						kPlayer.SetNumArchaeologyChoices(kPlayer.GetNumArchaeologyChoices() + 1);
						kPlayer.GetCulture()->AddDigCompletePlot(this);

						if (kPlayer.isHuman())
						{
							CvNotifications* pNotifications = NULL;
							Localization::String locString;
							Localization::String locSummary;
							pNotifications = kPlayer.GetNotifications();
							if (pNotifications)
							{
								strBuffer = GetLocalizedText("TXT_KEY_NOTIFICATION_CHOOSE_ARCHAEOLOGY");
								CvString strSummary = GetLocalizedText("TXT_KEY_NOTIFICATION_SUMMARY_CHOOSE_ARCHAEOLOGY");
								pNotifications->Add(NOTIFICATION_CHOOSE_ARCHAEOLOGY, strBuffer, strSummary, getX(), getY(), kPlayer.GetID());
								CancelActivePlayerEndTurn();
							}

							// Raiders of the Lost Ark achievement
							if (MOD_API_ACHIEVEMENTS)
							{
								const char* szCivKey = kPlayer.getCivilizationTypeKey();
								if (getOwner() != NO_PLAYER && !GC.getGame().isNetworkMultiPlayer() && strcmp(szCivKey, "CIVILIZATION_AMERICA") == 0)
								{
									CvPlayer &kPlotOwner = GET_PLAYER(getOwner());
									szCivKey = kPlotOwner.getCivilizationTypeKey();
									if (strcmp(szCivKey, "CIVILIZATION_EGYPT") == 0)
									{
										for (int i = 0; i < MAX_MAJOR_CIVS; i++)
										{
											CvPlayer &kLoopPlayer = GET_PLAYER((PlayerTypes)i);
											if (kLoopPlayer.GetID() != NO_PLAYER && kLoopPlayer.isAlive())
											{
												szCivKey = kLoopPlayer.getCivilizationTypeKey();
												if (strcmp(szCivKey, "CIVILIZATION_GERMANY"))
												{
													CvUnit *pLoopUnit = NULL;
													int iUnitLoop = 0;
													for (pLoopUnit = kLoopPlayer.firstUnit(&iUnitLoop); pLoopUnit != NULL; pLoopUnit = kLoopPlayer.nextUnit(&iUnitLoop))
													{
														if (strcmp(pLoopUnit->getUnitInfo().GetType(), "UNIT_ARCHAEOLOGIST") == 0)
														{
															if (plotDistance(pLoopUnit->getX(), pLoopUnit->getY(), getX(), getY()) <= 2)
															{
																gDLL->UnlockAchievement(ACHIEVEMENT_XP2_33);
															}
														}
													}
												}
											}
										}
									}
								}
							}
						}
						else
						{
							ArchaeologyChoiceType eChoice = kPlayer.GetCulture()->GetArchaeologyChoice(this);
							kPlayer.GetCulture()->DoArchaeologyChoice(eChoice);
						}
					}
				}
			}

			// Constructed Route
			if(pkBuildInfo->getRoute() != NO_ROUTE)
			{
				const RouteTypes eRoute = (RouteTypes)pkBuildInfo->getRoute();
				CvRouteInfo* pkRouteInfo = GC.getRouteInfo(eRoute);
				if(pkRouteInfo)
				{
					setRouteType(eRoute, ePlayer);
				}
			}

			// Remove Feature
			if(getFeatureType() != NO_FEATURE)
			{
				if(pkBuildInfo->isFeatureRemove(getFeatureType()))
				{
					ASSERT_DEBUG(ePlayer != NO_PLAYER, "ePlayer should be valid");

					iProduction = getFeatureProduction(eBuild, ePlayer, &pCity);

					if(iProduction > 0)
					{
						pCity->changeFeatureProduction(iProduction);
						if(pCity->getOwner() == GC.getGame().getActivePlayer())
						{
							strBuffer = GetLocalizedText("TXT_KEY_MISC_CLEARING_FEATURE_RESOURCE", GC.getFeatureInfo(getFeatureType())->GetTextKey(), iProduction, pCity->getNameKey());
							GC.GetEngineUserInterface()->AddCityMessage(0, pCity->GetIDInfo(), pCity->getOwner(), false, /*10*/ GD_INT_GET(EVENT_MESSAGE_TIME), strBuffer);
						}
					}

					setFeatureType(NO_FEATURE);
				}
			}

			// Repairing a Pillaged Tile
			if(pkBuildInfo->isRepair())
			{
				if(IsImprovementPillaged())
				{
					SetImprovementPillaged(false);
				}
				else if(IsRoutePillaged())
				{
					SetRoutePillaged(false);
				}
			}

			if(pkBuildInfo->IsRemoveRoute())
			{
				setRouteType(NO_ROUTE, ePlayer);
			}

			bFinished = true;

			ICvEngineScriptSystem1* pkScriptSystem = gDLL->GetScriptSystem();
			if(pkScriptSystem)
			{
				CvLuaArgsHandle args;
				args->Push(ePlayer);
				args->Push(getX());
				args->Push(getY());
				args->Push(eImprovement);

				bool bResult = false;
				LuaSupport::CallHook(pkScriptSystem, "BuildFinished", args.get(), bResult);
			}
		}
	}

	return bFinished;
}


//	--------------------------------------------------------------------------------
CvUnit* CvPlot::getCenterUnit()
{
	return m_pCenterUnit;
}

//	--------------------------------------------------------------------------------
const CvUnit* CvPlot::getCenterUnit() const
{
	return m_pCenterUnit;
}


//	--------------------------------------------------------------------------------
const CvUnit* CvPlot::getDebugCenterUnit() const
{
	const CvUnit* pCenterUnit = getCenterUnit();

	if(pCenterUnit == NULL)
	{
		if(GC.getGame().isDebugMode())
		{
			const IDInfo* pUnitNode = headUnitNode();
			if(pUnitNode == NULL)
				pCenterUnit = NULL;
			else
				pCenterUnit = GetPlayerUnit(*pUnitNode);
		}
	}

	return pCenterUnit;
}


//	--------------------------------------------------------------------------------
void CvPlot::setCenterUnit(CvUnit* pNewValue)
{
	CvUnit* pOldValue = getCenterUnit();
	m_pCenterUnit = pNewValue;

	if(pOldValue != pNewValue)
	{
		CvUnit* newCenterUnit = getCenterUnit();
		if(newCenterUnit)
		{
			newCenterUnit->setInfoBarDirty(true);
		}
	}
}
int CvPlot::getInvisibleVisibilityCountUnit(TeamTypes eTeam) const
{
	ASSERT_DEBUG(eTeam >= 0, "eTeam is expected to be non-negative (invalid Index)");
	ASSERT_DEBUG(eTeam < MAX_TEAMS, "eTeam is expected to be within maximum bounds (invalid Index)");
	for (size_t i = 0; i < m_vInvisibleVisibilityUnitCount.size(); i++)
		if (m_vInvisibleVisibilityUnitCount[i].first == eTeam)
			return m_vInvisibleVisibilityUnitCount[i].second;

	return 0;
}

bool CvPlot::isInvisibleVisibleUnit(TeamTypes eTeam) const
{
	return (getInvisibleVisibilityCountUnit(eTeam) > 0);
}

void CvPlot::changeInvisibleVisibilityCountUnit(TeamTypes eTeam, int iChange)
{
	bool bOldInvisibleVisible = false;
	bool bNewInvisibleVisible = false;

	ASSERT_DEBUG(eTeam >= 0, "eTeam is expected to be non-negative (invalid Index)");
	ASSERT_DEBUG(eTeam < MAX_TEAMS, "eTeam is expected to be within maximum bounds (invalid Index)");
	if (eTeam < 0 || eTeam >= MAX_TEAMS) return;

	if (iChange != 0)
	{
		bOldInvisibleVisible = isInvisibleVisibleUnit(eTeam);

		//-------- rarely used, so use a sparse format
		bool bFound = false;
		for (size_t i = 0; i < m_vInvisibleVisibilityUnitCount.size(); i++)
		{
			if (m_vInvisibleVisibilityUnitCount[i].first == eTeam)
			{
				m_vInvisibleVisibilityUnitCount[i].second += iChange;
				bFound = true;

				if (m_vInvisibleVisibilityUnitCount[i].second == 0)
					m_vInvisibleVisibilityUnitCount.erase(m_vInvisibleVisibilityUnitCount.begin() + i);

				break;
			}
		}
		if (!bFound)
			m_vInvisibleVisibilityUnitCount.push_back( make_pair(eTeam,iChange) );
		//--------

		bNewInvisibleVisible = isInvisibleVisibleUnit(eTeam);
		if (bOldInvisibleVisible != bNewInvisibleVisible)
		{
			TeamTypes activeTeam = GC.getGame().getActiveTeam();
			if (eTeam == activeTeam)
			{
				// for all (nominally invisible) units in this plot
				// tell the engine to flip whether they are being drawn or not
				IDInfo* pUnitNode = NULL;
				CvUnit* pLoopUnit = NULL;
				pUnitNode = headUnitNode();
				while (pUnitNode != NULL)
				{
					pLoopUnit = GetPlayerUnit(*pUnitNode);
					pUnitNode = nextUnitNode(pUnitNode);

					if (NULL != pLoopUnit && pLoopUnit->getTeam() != activeTeam && pLoopUnit->IsHiddenByNearbyUnit(pLoopUnit->plot()))
					{
						CvInterfacePtr<ICvUnit1> pDllUnit(new CvDllUnit(pLoopUnit));
						gDLL->GameplayUnitVisibility(pDllUnit.get(), bNewInvisibleVisible, true);
					}
				}

				updateCenterUnit();

			}
		}
	}
}
//	--------------------------------------------------------------------------------
int CvPlot::getInvisibleVisibilityCount(TeamTypes eTeam, InvisibleTypes eInvisible) const
{
	ASSERT_DEBUG(eTeam >= 0, "eTeam is expected to be non-negative (invalid Index)");
	ASSERT_DEBUG(eTeam < MAX_TEAMS, "eTeam is expected to be within maximum bounds (invalid Index)");
	ASSERT_DEBUG(eInvisible >= 0, "eInvisible is expected to be non-negative (invalid Index)");
	ASSERT_DEBUG(eInvisible < NUM_INVISIBLE_TYPES, "eInvisible is expected to be within maximum bounds (invalid Index)");
	if (eTeam < 0 || eTeam >= MAX_TEAMS) return 0;
	if (eInvisible < 0 || eInvisible >= NUM_INVISIBLE_TYPES) return 0;

	for (size_t i = 0; i < m_vInvisibleVisibilityCount.size(); i++)
		if (m_vInvisibleVisibilityCount[i].first == eTeam)
			return m_vInvisibleVisibilityCount[i].second[eInvisible];

	return 0;
}


//	--------------------------------------------------------------------------------
bool CvPlot::isInvisibleVisible(TeamTypes eTeam, InvisibleTypes eInvisible)	const
{
	return (getInvisibleVisibilityCount(eTeam, eInvisible) > 0);
}


//	--------------------------------------------------------------------------------
void CvPlot::changeInvisibleVisibilityCount(TeamTypes eTeam, InvisibleTypes eInvisible, int iChange)
{
	bool bOldInvisibleVisible = false;
	bool bNewInvisibleVisible = false;

	ASSERT_DEBUG(eTeam >= 0, "eTeam is expected to be non-negative (invalid Index)");
	ASSERT_DEBUG(eTeam < MAX_TEAMS, "eTeam is expected to be within maximum bounds (invalid Index)");
	ASSERT_DEBUG(eInvisible >= 0, "eInvisible is expected to be non-negative (invalid Index)");
	ASSERT_DEBUG(eInvisible < NUM_INVISIBLE_TYPES, "eInvisible is expected to be within maximum bounds (invalid Index)");
	if (eTeam < 0 || eTeam >= MAX_TEAMS) return;
	if (eInvisible < 0 || eInvisible >= NUM_INVISIBLE_TYPES) return;

	if (iChange != 0)
	{
		bOldInvisibleVisible = isInvisibleVisible(eTeam, eInvisible);

		//-------- rarely used, so use a sparse format
		bool bFound = false;
		for (size_t i = 0; i < m_vInvisibleVisibilityCount.size(); i++)
		{
			if (m_vInvisibleVisibilityCount[i].first == eTeam)
			{
				m_vInvisibleVisibilityCount[i].second[eInvisible] += iChange;
				bFound = true;

				bool bAllZero = true;
				for (size_t j = 0; j < NUM_INVISIBLE_TYPES; j++)
					if (m_vInvisibleVisibilityCount[i].second[j] != 0)
						bAllZero = false;

				if (bAllZero)
					m_vInvisibleVisibilityCount.erase(m_vInvisibleVisibilityCount.begin() + i);

				break;
			}
		}
		if (!bFound)
		{
			vector<int> values(NUM_INVISIBLE_TYPES, 0);
			values[eInvisible] = iChange;
			m_vInvisibleVisibilityCount.push_back(make_pair(eTeam, values));
		}
		//--------

		bNewInvisibleVisible = isInvisibleVisible(eTeam, eInvisible);

		if (bOldInvisibleVisible != bNewInvisibleVisible)
		{
			TeamTypes activeTeam = GC.getGame().getActiveTeam();
			if (eTeam == activeTeam)
			{
				// for all (nominally invisible) units in this plot
				// tell the engine to flip whether they are being drawn or not
				IDInfo* pUnitNode = NULL;
				CvUnit* pLoopUnit = NULL;
				pUnitNode = headUnitNode();
				while (pUnitNode != NULL)
				{
					pLoopUnit = GetPlayerUnit(*pUnitNode);
					pUnitNode = nextUnitNode(pUnitNode);

					if (NULL != pLoopUnit && pLoopUnit->getTeam() != activeTeam && pLoopUnit->getInvisibleType() == eInvisible)
					{
						CvInterfacePtr<ICvUnit1> pDllUnit(new CvDllUnit(pLoopUnit));
						gDLL->GameplayUnitVisibility(pDllUnit.get(), bNewInvisibleVisible, true);
					}
				}

				updateCenterUnit();

			}
		}
	}
}

//	--------------------------------------------------------------------------------
int CvPlot::getNumLayerUnits(int iLayerID /*= -1*/) const
{
	if (iLayerID == DEFAULT_UNIT_MAP_LAYER)
		// Base layer
		return m_units.getLength();
	else
		if (iLayerID == -1)
		{
			// All layers
			int iCount = m_units.getLength();
			const CvPlotManager& kManager = GC.getMap().plotManager();
			int iNumLayers = kManager.GetNumLayers();
			for (int iIndex = 0; iIndex < iNumLayers; ++iIndex)
			{		
				iCount += kManager.GetNumUnitsByIndex(m_iX, m_iY, iIndex);
			}
			return iCount;
		}
		else
		{
			// Specific layer
			return GC.getMap().plotManager().GetNumUnits(m_iX, m_iY, iLayerID);
		}
}

//	--------------------------------------------------------------------------------
CvUnit* CvPlot::getLayerUnit(int iIndex, int iLayerID /*= -1*/) const
{
	if (iIndex >= 0)
	{
		if (iLayerID == DEFAULT_UNIT_MAP_LAYER)
			// The base layer
			return getUnitByIndex(iIndex);
		else
		if (iLayerID == -1)
		{
			// All layers
			int iCount = m_units.getLength();
			if (iIndex < iCount)
				return getUnitByIndex(iIndex);
			else
			{
				const CvPlotManager& kManager = GC.getMap().plotManager();
				int iNumLayers = kManager.GetNumLayers();
				for (int iLayerIndex = 0; iLayerIndex < iNumLayers; ++iLayerIndex)
				{		
					const CvIDInfoFixedVector& kUnits = kManager.GetUnitsByIndex(m_iX, m_iY, iLayerIndex);
					if (iIndex < (iCount + (int)kUnits.size()))
						return ::GetPlayerUnit( kUnits[iIndex - iCount] );
					else
						iCount += kUnits.size();
				}				
			}
		}
		else
		{
			// Specific layer
			const CvIDInfoFixedVector& kUnits = GC.getMap().plotManager().GetUnits(m_iX, m_iY, iLayerID);
			if (iIndex < (int)kUnits.size())
				return ::GetPlayerUnit( kUnits[iIndex] );
		}
	}

	return NULL;
}

//	--------------------------------------------------------------------------------
int CvPlot::getNumUnits() const
{
	return m_units.getLength();
}
#if defined(MOD_BALANCE_CORE)
int CvPlot::GetUnitPlotExperience() const
{
	VALIDATE_OBJECT();
	return m_iUnitPlotExperience;
}
void CvPlot::ChangeUnitPlotExperience(int iExperience)
{
	VALIDATE_OBJECT();
	m_iUnitPlotExperience += iExperience;
}
int CvPlot::GetUnitPlotGAExperience() const
{
	VALIDATE_OBJECT();
	return m_iUnitPlotGAExperience;
}
void CvPlot::ChangeUnitPlotGAExperience(int iExperience)
{
	VALIDATE_OBJECT();
	m_iUnitPlotGAExperience += iExperience;
}
bool CvPlot::IsUnitPlotExperience() const
{
	return GetUnitPlotExperience() > 0 || GetUnitPlotGAExperience() > 0;
}
int CvPlot::GetPlotMovesChange() const
{
	VALIDATE_OBJECT();
	return m_iPlotChangeMoves;
}
void CvPlot::ChangePlotMovesChange(int iValue)
{
	VALIDATE_OBJECT();
	m_iPlotChangeMoves += iValue;
}
bool CvPlot::IsRestoreMoves() const
{
	VALIDATE_OBJECT();
	return m_iRestoreMoves > 0;
}
void CvPlot::ChangeRestoreMovesCount(int iValue)
{
	VALIDATE_OBJECT();
	m_iRestoreMoves += iValue;
}
#endif
//	--------------------------------------------------------------------------------
int CvPlot::GetNumCombatUnits()
{
	int iCount = 0;

	IDInfo* pUnitNode = NULL;
	CvUnit* pLoopUnit = NULL;

	pUnitNode = headUnitNode();

	while(pUnitNode != NULL)
	{
		pLoopUnit = GetPlayerUnit(*pUnitNode);

		if(pLoopUnit && pLoopUnit->IsCombatUnit())
		{
			iCount++;
		}

		pUnitNode = nextUnitNode(pUnitNode);
	}

	return iCount;
}


//	--------------------------------------------------------------------------------
CvUnit* CvPlot::getUnitByIndex(int iIndex) const
{
	const IDInfo* pUnitNode = m_units.getAt(iIndex);

	if(pUnitNode != NULL)
	{
		return GetPlayerUnit(*pUnitNode);
	}
	else
	{
		return NULL;
	}
}

//	---------------------------------------------------------------------------
//	Return the index of the unit in the plot's list.  -1 if it is not in the list.
int CvPlot::getUnitIndex(CvUnit* pUnit) const
{
	int iIndex = 0;
	if(pUnit)
	{
		IDInfo kUnitInfo = pUnit->GetIDInfo();

		const IDInfo* pUnitNode = headUnitNode();

		while(pUnitNode != NULL)
		{
			if(*pUnitNode == kUnitInfo)
				return iIndex;

			++iIndex;
			pUnitNode = nextUnitNode(pUnitNode);
		}
	}
	return -1;
}

//	---------------------------------------------------------------------------
void CvPlot::addUnit(CvUnit* pUnit, bool bUpdate)
{
	ASSERT_DEBUG(pUnit, "pUnit is expected to be non-NULL");
	if(pUnit == NULL)
		return;

	ASSERT_DEBUG(pUnit->at(getX(), getY()), "pUnit is expected to be at getX_INLINE and getY_INLINE");

	IDInfo* pUnitNode = headUnitNode();
	while(pUnitNode != NULL)
	{
		CvUnit* pLoopUnit = GetPlayerUnit(*pUnitNode);
		if(!isBeforeUnitCycle(pLoopUnit, pUnit))
		{
			break;
		}

		pUnitNode = nextUnitNode(pUnitNode);
	}

	if(pUnitNode != NULL)
	{
		IDInfo unitIDInfo = pUnit->GetIDInfo();
		m_units.insertBefore(&unitIDInfo, pUnitNode);
	}
	else
	{
		IDInfo unitIDInfo = pUnit->GetIDInfo();
		m_units.insertAtEnd(&unitIDInfo);
	}

	if(bUpdate)
	{
		updateCenterUnit();
	}
}


//	--------------------------------------------------------------------------------
void CvPlot::removeUnit(CvUnit* pUnit, bool bUpdate)
{
	IDInfo* pUnitNode = headUnitNode();

	while(pUnitNode != NULL)
	{
		if(GetPlayerUnit(*pUnitNode) == pUnit)
		{
			ASSERT_DEBUG(GetPlayerUnit(*pUnitNode)->at(getX(), getY()), "The current unit instance is expected to be at getX_INLINE and getY_INLINE");
			m_units.deleteNode(pUnitNode);
			break;
		}
		else
		{
			pUnitNode = nextUnitNode(pUnitNode);
		}
	}

	GC.getMap().plotManager().RemoveUnit(pUnit->GetIDInfo(), m_iX, m_iY, -1);

	if(bUpdate)
	{
		updateCenterUnit();
	}
}

//	--------------------------------------------------------------------------------
const IDInfo* CvPlot::nextUnitNode(const IDInfo* pNode) const
{
	return m_units.next(pNode);
}

//	--------------------------------------------------------------------------------
IDInfo* CvPlot::nextUnitNode(IDInfo* pNode)
{
	return m_units.next(pNode);
}

//	--------------------------------------------------------------------------------
const IDInfo* CvPlot::prevUnitNode(const IDInfo* pNode) const
{
	return m_units.prev(pNode);
}

//	--------------------------------------------------------------------------------
IDInfo* CvPlot::prevUnitNode(IDInfo* pNode)
{
	return m_units.prev(pNode);
}

//	--------------------------------------------------------------------------------
const IDInfo* CvPlot::headUnitNode() const
{
	return m_units.head();
}

//	--------------------------------------------------------------------------------
IDInfo* CvPlot::headUnitNode()
{
	return m_units.head();
}

//	--------------------------------------------------------------------------------
const IDInfo* CvPlot::tailUnitNode() const
{
	return m_units.tail();
}

//	--------------------------------------------------------------------------------
IDInfo* CvPlot::tailUnitNode()
{
	return m_units.tail();
}

//	--------------------------------------------------------------------------------
uint CvPlot::getUnits(IDInfoVector* pkInfoVector) const
{
	uint uiCount = 0;
	if (pkInfoVector)
	{
		pkInfoVector->clear();
	
		const IDInfo* pUnitNode = headUnitNode();

		while (pUnitNode != NULL)
		{
			pkInfoVector->push_back(*pUnitNode);
			pUnitNode = nextUnitNode(pUnitNode);
			++uiCount;
		}
	}
	return uiCount;
}

//	--------------------------------------------------------------------------------
CvString CvPlot::getScriptData() const
{
	CvString scriptData = (m_szScriptData != NULL) ? m_szScriptData : "";
	return scriptData;
}

//	--------------------------------------------------------------------------------
void CvPlot::setScriptData(const char* szNewValue)
{
	SAFE_DELETE_ARRAY(m_szScriptData);
	m_szScriptData = _strdup(szNewValue);
}

void CvPlot::showPopupText(PlayerTypes ePlayer, const char* szMessage)
{
	if (ePlayer == NO_PLAYER || isVisible(GET_PLAYER(ePlayer).getTeam()))
	{
		//show the popup only if we're not on autoplay - too many stored popups seems to lead to crashes
		if (!GET_PLAYER( GC.getGame().getActivePlayer() ).isObserver())
		{
			DLLUI->AddPopupText(getX(), getY(), szMessage, GC.getMap().GetPopupCount(m_iPlotIndex)*0.5f);
			GC.getMap().IncreasePopupCount(m_iPlotIndex);
		}
	}
}

// Protected Functions...

//	--------------------------------------------------------------------------------
void CvPlot::processArea(CvArea* pArea, int iChange)
{
	CvCity* pCity = NULL;
	int iI = 0;
	int iJ = 0;

	pArea->changeNumTiles(iChange);

	if(isOwned())
	{
		pArea->changeNumOwnedTiles(iChange);
	}

	if(isNEOfRiver())
	{
		pArea->changeNumRiverEdges(iChange);
	}
	if(isNWOfRiver())
	{
		pArea->changeNumRiverEdges(iChange);
	}
	if(isWOfRiver())
	{
		pArea->changeNumRiverEdges(iChange);
	}

	if(getResourceType() != NO_RESOURCE)
	{
		pArea->changeNumResources(getResourceType(), iChange);
	}

	if(getImprovementType() != NO_IMPROVEMENT)
	{
		pArea->changeNumImprovements(getImprovementType(), iChange);
	}

	for(iI = 0; iI < MAX_PLAYERS; ++iI)
	{
		if(GET_PLAYER((PlayerTypes)iI).getStartingPlot() == this)
		{
			pArea->changeNumStartingPlots(iChange);
		}

		pArea->changeUnitsPerPlayer(((PlayerTypes)iI), (plotCount(PUF_isPlayer, iI) * iChange));
	}

	for(iI = 0; iI < MAX_TEAMS; ++iI)
	{
		if(isRevealed(((TeamTypes)iI)))
		{
			pArea->changeNumRevealedTiles(((TeamTypes)iI), iChange);
		}
	}

	pCity = getPlotCity();

	if(pCity != NULL)
	{
		// XXX make sure all of this syncs up...
		pArea->changeCitiesPerPlayer(pCity->getOwner(), iChange);
		pArea->changePopulationPerPlayer(pCity->getOwner(), (pCity->getPopulation() * iChange));

		for(iI = 0; iI < GC.getNumBuildingInfos(); ++iI)
		{
			const BuildingTypes eBuilding = static_cast<BuildingTypes>(iI);
			CvBuildingEntry* pkBuildingInfo = GC.getBuildingInfo(eBuilding);
			if(pkBuildingInfo)
			{
				if(pCity->GetCityBuildings()->GetNumActiveBuilding(eBuilding) > 0)
				{
					for(iJ = 0; iJ < NUM_YIELD_TYPES; iJ++)
					{
						//Simplification - errata yields not worth considering.
						if ((YieldTypes)iJ > YIELD_CULTURE_LOCAL && !MOD_BALANCE_CORE_JFD)
							break;

						pArea->changeYieldRateModifier(pCity->getOwner(), ((YieldTypes)iJ), (pkBuildingInfo->GetAreaYieldModifier(iJ) * iChange * pCity->GetCityBuildings()->GetNumActiveBuilding(eBuilding)));
					}
				}
			}
		}

		for(iI = 0; iI < MAX_PLAYERS; ++iI)
		{
			if(pArea->getTargetCity((PlayerTypes)iI) == pCity)
			{
				pArea->setTargetCity(((PlayerTypes)iI), NULL);
			}
		}
	}
}

//	--------------------------------------------------------------------------------
template<typename Plot, typename Visitor>
void CvPlot::Serialize(Plot& plot, Visitor& visitor)
{
	const bool bLoading = visitor.isLoading();
	const bool bSaving = visitor.isSaving();

	// Hack for where we need to mutate the plot directly
	// Necessary because C++03 has no if constexpr =(
	// Don't use this in save branches and things will be okay
	CvPlot& mutPlot = const_cast<CvPlot&>(plot);

	visitor(plot.m_iArea);
	visitor(plot.m_iOwnershipDuration);
	visitor(plot.m_iImprovementDuration);
	visitor(plot.m_iUpgradeProgress);
	visitor(plot.m_iNumMajorCivsRevealed);
	visitor(plot.m_iCityRadiusCount);
	visitor(plot.m_iReconCount);
	visitor(plot.m_iRiverCrossingCount);
	visitor(plot.m_iResourceNum);
	visitor(plot.m_iLandmass);
	visitor(plot.m_iContinent);
	visitor(plot.m_vRivers);

	// Bit fields
	{
		enum PlotSaveStructBitFlags
		{
			PLOT_BIT_FLAG_IMPROVEMENT_EMBASSY			= (1 << 0),
			PLOT_BIT_FLAG_IMPROVEMENT_PASSABLE			= (1 << 1),
			PLOT_BIT_FLAG_IMPROVEMENT_PILLAGED			= (1 << 2),
			PLOT_BIT_FLAG_ROUTE_PILLAGED				= (1 << 3),
			PLOT_BIT_FLAG_STARTING_PLOT					= (1 << 4),
			PLOT_BIT_FLAG_HILLS							= (1 << 5),
			PLOT_BIT_FLAG_NE_OF_RIVER					= (1 << 6),
			PLOT_BIT_FLAG_W_OF_RIVER					= (1 << 7),
			PLOT_BIT_FLAG_NW_OF_RIVER					= (1 << 8),
			PLOT_BIT_FLAG_POTENTIAL_CITY_WORK			= (1 << 9),
			PLOT_BIT_FLAG_BARB_CAMP_NOT_CONVERTING		= (1 << 10),
			PLOT_BIT_FLAG_ROUGH_PLOT					= (1 << 11),
			PLOT_BIT_FLAG_RESOURCE_LINKED_CITY_ACTIVE	= (1 << 12),
			PLOT_BIT_FLAG_IMPROVED_BY_GIFT_FROM_MAYOR	= (1 << 13),
			PLOT_BIT_FLAG_IS_IMPASSABLE					= (1 << 14),
		};

		uint16 plotBits = 0;
		if (bLoading)
		{
			visitor >> plotBits;
			mutPlot.m_bImprovementEmbassy		= !!(plotBits & PLOT_BIT_FLAG_IMPROVEMENT_EMBASSY);
			mutPlot.m_bImprovementPassable		= !!(plotBits & PLOT_BIT_FLAG_IMPROVEMENT_PASSABLE);
			mutPlot.m_bImprovementPillaged		= !!(plotBits & PLOT_BIT_FLAG_IMPROVEMENT_PILLAGED);
			mutPlot.m_bRoutePillaged			= !!(plotBits & PLOT_BIT_FLAG_ROUTE_PILLAGED);
			mutPlot.m_bStartingPlot				= !!(plotBits & PLOT_BIT_FLAG_STARTING_PLOT);
			mutPlot.m_bHills					= !!(plotBits & PLOT_BIT_FLAG_HILLS);
			mutPlot.m_bNEOfRiver				= !!(plotBits & PLOT_BIT_FLAG_NE_OF_RIVER);
			mutPlot.m_bWOfRiver					= !!(plotBits & PLOT_BIT_FLAG_W_OF_RIVER);
			mutPlot.m_bNWOfRiver				= !!(plotBits & PLOT_BIT_FLAG_NW_OF_RIVER);
			mutPlot.m_bPotentialCityWork		= !!(plotBits & PLOT_BIT_FLAG_POTENTIAL_CITY_WORK);
			mutPlot.m_bBarbCampNotConverting	= !!(plotBits & PLOT_BIT_FLAG_BARB_CAMP_NOT_CONVERTING);
			mutPlot.m_bRoughPlot				= !!(plotBits & PLOT_BIT_FLAG_ROUGH_PLOT);
			mutPlot.m_bResourceLinkedCityActive	= !!(plotBits & PLOT_BIT_FLAG_RESOURCE_LINKED_CITY_ACTIVE);
			mutPlot.m_bImprovedByGiftFromMajor	= !!(plotBits & PLOT_BIT_FLAG_IMPROVED_BY_GIFT_FROM_MAYOR);
			mutPlot.m_bIsImpassable				= !!(plotBits & PLOT_BIT_FLAG_IS_IMPASSABLE);
		}
		if (bSaving)
		{
			plotBits = 0;
			if (plot.m_bImprovementEmbassy)			{ plotBits |= PLOT_BIT_FLAG_IMPROVEMENT_EMBASSY; }
			if (plot.m_bImprovementPassable)		{ plotBits |= PLOT_BIT_FLAG_IMPROVEMENT_PASSABLE; }
			if (plot.m_bImprovementPillaged)		{ plotBits |= PLOT_BIT_FLAG_IMPROVEMENT_PILLAGED; }
			if (plot.m_bRoutePillaged)				{ plotBits |= PLOT_BIT_FLAG_ROUTE_PILLAGED; }
			if (plot.m_bStartingPlot)				{ plotBits |= PLOT_BIT_FLAG_STARTING_PLOT; }
			if (plot.m_bHills)						{ plotBits |= PLOT_BIT_FLAG_HILLS; }
			if (plot.m_bNEOfRiver)					{ plotBits |= PLOT_BIT_FLAG_NE_OF_RIVER; }
			if (plot.m_bWOfRiver)					{ plotBits |= PLOT_BIT_FLAG_W_OF_RIVER; }
			if (plot.m_bNWOfRiver)					{ plotBits |= PLOT_BIT_FLAG_NW_OF_RIVER; }
			if (plot.m_bPotentialCityWork)			{ plotBits |= PLOT_BIT_FLAG_POTENTIAL_CITY_WORK; }
			if (plot.m_bBarbCampNotConverting)		{ plotBits |= PLOT_BIT_FLAG_BARB_CAMP_NOT_CONVERTING; }
			if (plot.m_bRoughPlot)					{ plotBits |= PLOT_BIT_FLAG_ROUGH_PLOT; }
			if (plot.m_bResourceLinkedCityActive)	{ plotBits |= PLOT_BIT_FLAG_RESOURCE_LINKED_CITY_ACTIVE; }
			if (plot.m_bImprovedByGiftFromMajor)	{ plotBits |= PLOT_BIT_FLAG_IMPROVED_BY_GIFT_FROM_MAYOR; }
			if (plot.m_bIsImpassable)				{ plotBits |= PLOT_BIT_FLAG_IS_IMPASSABLE; }
			visitor << plotBits;
		}
	}

	visitor(plot.m_iUnitPlotExperience);
	visitor(plot.m_iUnitPlotGAExperience);
	visitor(plot.m_iPlotChangeMoves);
	visitor(plot.m_iRestoreMoves);

	visitor(plot.m_eOwner);
	visitor(plot.m_ePlotType);
	visitor(plot.m_eTerrainType);

	visitor.template as<FeatureTypes>(plot.m_eFeatureType);
	visitor.template as<ResourceTypes>(plot.m_eResourceType);
	visitor.template as<ImprovementTypes>(plot.m_eImprovementType);

	visitor(plot.m_ePlayerBuiltImprovement);
	visitor(plot.m_ePlayerResponsibleForImprovement);
	visitor(plot.m_ePlayerBuiltRoute);
	visitor(plot.m_ePlayerResponsibleForRoute);
	visitor(plot.m_ePlayerThatClearedBarbCampHere);
	visitor(plot.m_eLandmarkCreditMinor);
	visitor(plot.m_ePlayerThatClearedDigHere);
	visitor(plot.m_ePlayerThatDestroyedCityHere);
	visitor(plot.m_eRouteType);
	visitor(plot.m_eUnitIncrement);
	visitor(plot.m_eWorldAnchor);
	visitor(plot.m_cWorldAnchorData);

	visitor(plot.m_eRiverEFlowDirection);
	visitor(plot.m_eRiverSEFlowDirection);
	visitor(plot.m_eRiverSWFlowDirection);

	visitor(plot.m_bIsCity);
	
	visitor(plot.m_owningCity.eOwner);
	visitor(plot.m_owningCity.iID);
	visitor(plot.m_owningCityOverride.eOwner);
	visitor(plot.m_owningCityOverride.iID);

	for (uint i = 0; i < NUM_YIELD_TYPES; i++)
	{
		visitor(plot.m_aiYield[i]);
	}

	for (int i = 0; i < MAX_TEAMS; i++)
	{
		visitor(plot.m_aiPlayerCityRadiusCount[i]);
		visitor(plot.m_aiVisibilityCount[i]);
		visitor(plot.m_aiVisibilityCountThisTurnMax[i]);
		visitor(plot.m_aiRevealedOwner[i]);
		visitor(plot.m_abResourceForceReveal[i]);
		visitor.template as<ImprovementTypes>(plot.m_aeRevealedImprovementType[i]);
		visitor.template as<RouteTypes>(plot.m_aeRevealedRouteType[i]);
		visitor(plot.m_abIsImpassable[i]);
		visitor(plot.m_abStrategicRoute[i]);
		visitor(plot.m_aeHumanPlannedRouteState[i]);
	}

	visitor(plot.m_bfRevealed.m_bits);
	visitor(plot.m_cRiverCrossing);

	// Script data
	{
		// FIXME - This is simply dreadful.
		bool hasScriptData = false;
		if (bSaving)
			hasScriptData = (plot.m_szScriptData != NULL);
		visitor(hasScriptData);
		if (hasScriptData)
		{
			std::string scriptData;
			if (bSaving)
				scriptData.assign(plot.m_szScriptData);
			visitor(scriptData);
			if (bLoading)
				mutPlot.setScriptData(scriptData.c_str());
		}
	}

	visitor(plot.m_buildProgress);
	visitor(plot.m_vInvisibleVisibilityUnitCount);
	visitor(plot.m_vInvisibleVisibilityCount);

	// m_units
	{
		uint32 uLength = 0;
		if (bSaving)
			uLength = uint32(plot.m_units.getLength());
		visitor(uLength);

		for (uint32 uIdx = 0; uIdx < uLength; ++uIdx)
		{
			IDInfo data;
			if (bSaving)
				data = *plot.m_units.getAt(uIdx);

			visitor(data);

			if (bLoading)
				mutPlot.m_units.insertAtEnd(&data);
		}
	}

	visitor(plot.m_cContinentType);
	visitor(plot.m_kArchaeologyData);
	visitor(plot.m_iNumTradeUnitRoute);
	visitor(plot.m_iLastTurnBuildChanged);

	visitor(plot.m_sSpawnedResourceX);
	visitor(plot.m_sSpawnedResourceY);
}

//	--------------------------------------------------------------------------------
//
// read object from a stream
// used during load
//
void CvPlot::read(FDataStream& kStream, int iX, int iY)
{
	init(iX, iY);

	// Perform shared serialize
	CvStreamLoadVisitor serialVisitor(kStream);
	Serialize(*this, serialVisitor);
}

//	--------------------------------------------------------------------------------
//
// write object to a stream
// used during save
//
void CvPlot::write(FDataStream& kStream) const
{
	// Perform shared serialize
	CvStreamSaveVisitor serialVisitor(kStream);
	Serialize(*this, serialVisitor);
}

//	--------------------------------------------------------------------------------
void CvPlot::setLayoutDirty(bool bDirty)
{
	if(bDirty)
	{
		GC.GetEngineUserInterface()->setDirty(PlotData_DIRTY_BIT,true);
	}
	m_bPlotLayoutDirty = bDirty;
}

//	--------------------------------------------------------------------------------
bool CvPlot::isLayoutDirty() const
{
	return m_bPlotLayoutDirty;
}

enum eRoadTypes
{
    ROAD_REGULAR = 0,
    RR_REGULAR,

    ROAD_PILLAGED,
    RR_PILLAGED,

    ROAD_UNDER_CONSTRUCTION,
    RR_UNDER_CONSTRUCTION,

    ROAD_TRADE_ROUTE,
    RR_TRADE_ROUTE,

    NUM_ROAD_RR_TYPES,
};

//	--------------------------------------------------------------------------------
void CvPlot::updateLayout(bool bDebug)
{
	TeamTypes eActiveTeam = GC.getGame().getActiveTeam();

	FogOfWarModeTypes eFOWMode = GetActiveFogOfWarMode();

	ResourceTypes eThisResource = (isCity() || eFOWMode == FOGOFWARMODE_UNEXPLORED) ? NO_RESOURCE : getResourceType(eActiveTeam);

	ImprovementTypes eThisImprovement = getRevealedImprovementType(eActiveTeam, bDebug);
	bool bShowHalfBuilt = false;
	if(eThisImprovement == NO_IMPROVEMENT && getAnyBuildProgress() && eFOWMode == FOGOFWARMODE_OFF)
	{
		// see if we are improving the tile
		for(int iBuildIndex = 0; iBuildIndex < GC.getNumBuildInfos(); iBuildIndex++)
		{
			BuildTypes eBuild = (BuildTypes)iBuildIndex;
			CvBuildInfo* build = GC.getBuildInfo(eBuild);
			if(build)
			{
				ImprovementTypes eInnerImprovement = (ImprovementTypes)build->getImprovement();
				if(eInnerImprovement == NO_IMPROVEMENT)
				{
					continue;
				}

				int iProgress = getBuildProgress(eBuild);
				if(iProgress > 0)
				{
					eThisImprovement = eInnerImprovement;
					bShowHalfBuilt = true;
					break;
				}
			}

		}
	}

	int improvementState = 2; // half-built = 1, full built = 2, pillaged = 3
	if(IsImprovementPillaged() && eFOWMode == FOGOFWARMODE_OFF)
	{
		improvementState = 3;
	}
	else if(bShowHalfBuilt)
	{
		improvementState = 1;
	}

	RouteTypes eRoute = getRevealedRouteType(eActiveTeam, bDebug);
	byte eRoadTypeValue = NUM_ROAD_RR_TYPES;
	if(eRoute != NO_ROUTE)
	{
		switch(eRoute)
		{
		case NO_ROUTE:
		case ROUTE_ANY:
			UNREACHABLE();
		case ROUTE_ROAD:
			if(IsRoutePillaged())
			{
				eRoadTypeValue = ROAD_PILLAGED;
			}
			else if(IsCityConnection())
			{
				eRoadTypeValue = ROAD_TRADE_ROUTE;
			}
			else
			{
				eRoadTypeValue = ROAD_REGULAR;
			}
			break;
		case ROUTE_RAILROAD:
			if(IsRoutePillaged())
			{
				eRoadTypeValue = RR_PILLAGED;
			}
			else if(IsCityConnection())
			{
				eRoadTypeValue = RR_TRADE_ROUTE;
			}
			else
			{
				eRoadTypeValue = RR_REGULAR;
			}
			break;
		}
	}
	else
	{
		bShowHalfBuilt = false;
		if(eRoute == NO_ROUTE && getAnyBuildProgress() && eFOWMode == FOGOFWARMODE_OFF)
		{
			// see if we are improving the tile
			for(int iBuildIndex = 0; iBuildIndex < GC.getNumBuildInfos(); iBuildIndex++)
			{
				BuildTypes eBuild = (BuildTypes)iBuildIndex;
				CvBuildInfo* build = GC.getBuildInfo(eBuild);
				if(build)
				{
					RouteTypes eInnerRoute = (RouteTypes)build->getRoute();
					if(eInnerRoute == NO_ROUTE)
					{
						continue;
					}

					int iProgress = getBuildProgress(eBuild);
					if(iProgress > 0)
					{
						eRoute = eInnerRoute;
						bShowHalfBuilt = true;
						break;
					}
				}

			}
		}
		if(bShowHalfBuilt)
		{
			switch(eRoute)
			{
			case NO_ROUTE:
			case ROUTE_ANY:
				UNREACHABLE();
			case ROUTE_ROAD:
				eRoadTypeValue = ROAD_UNDER_CONSTRUCTION;
				break;
			case ROUTE_RAILROAD:
				eRoadTypeValue = RR_UNDER_CONSTRUCTION;
				break;
			}
		}
	}

	CvInterfacePtr<ICvPlot1> pDllPlot(new CvDllPlot(this));
	gDLL->GameplayPlotStateChange
	(
	    pDllPlot.get(),
	    eThisResource,
	    eThisImprovement,
	    improvementState,
	    eRoute,
	    eRoadTypeValue
	);
}

//	--------------------------------------------------------------------------------
bool CvPlot::isLayoutStateDifferent() const
{
	return (m_bLayoutStateWorked != isBeingWorked());
}

//	--------------------------------------------------------------------------------
void CvPlot::setLayoutStateToCurrent()
{
	m_bLayoutStateWorked = isBeingWorked();
}

//------------------------------------------------------------------------------------------------

void CvPlot::getVisibleImprovementState(ImprovementTypes& eType, bool& bWorked)
{
	eType = NO_IMPROVEMENT;
	bWorked = false;

	if(GC.getGame().getActiveTeam() == NO_TEAM)
	{
		return;
	}

	eType = getRevealedImprovementType(GC.getGame().getActiveTeam(), true);

	// worked state
	if(isActiveVisible() && isBeingWorked())
	{
		bWorked = true;
	}
}

//	--------------------------------------------------------------------------------
void CvPlot::getVisibleResourceState(ResourceTypes& eType, bool& bImproved, bool& bWorked)
{
	eType = NO_RESOURCE;
	bImproved = false;
	bWorked = false;

	if(GC.getGame().getActiveTeam() == NO_TEAM)
	{
		return;
	}

	if(GC.getGame().isDebugMode())
	{
		eType = getResourceType();
	}
	else if(isRevealed(GC.getGame().getActiveTeam()))
	{
		eType = getResourceType(GC.getGame().getActiveTeam());
	}

	// improved and worked states ...
	if(eType != NO_RESOURCE)
	{
		ImprovementTypes eRevealedImprovement = getRevealedImprovementType(GC.getGame().getActiveTeam(), true);
		if ((eRevealedImprovement != NO_IMPROVEMENT) && GC.getImprovementInfo(eRevealedImprovement)->IsConnectsResource(eType))
		{
			bImproved = true;
			bWorked = isBeingWorked();
		}
	}
}

//	--------------------------------------------------------------------------------
int CvPlot::getYieldWithBuild(BuildTypes eBuild, YieldTypes eYield, bool bWithUpgrade, RouteTypes eForceCityConnection, PlayerTypes ePlayer, const CvCity* pOwningCity, const CvReligion* pMajorityReligion, const CvBeliefEntry* pSecondaryPantheon, const CvReligion* pPlayerPantheon) const
{
	int iYield = 0;

	if(getTerrainType() == NO_TERRAIN)
		return 0;

	if(!isPotentialCityWork())
		return 0;

	FeatureTypes eFeature = getFeatureType();
	ResourceTypes eResource = getResourceType(GET_PLAYER(ePlayer).getTeam());
	CvBuildInfo* pkBuildInfo = GC.getBuildInfo(eBuild);
	ImprovementTypes eNewImprovement = (ImprovementTypes)pkBuildInfo->getImprovement();
	ImprovementTypes eOldImprovement = getImprovementType();

	// If we're not changing the improvement that's here, use the improvement that's here already
	if (eNewImprovement == NO_IMPROVEMENT)
	{
		if (!IsImprovementPillaged() || GC.getBuildInfo(eBuild)->isRepair())
		{
			eNewImprovement = eOldImprovement;
		}
	}

	// If we're not changing the route that's here, and we are not removing the route, use the improvement that's here already
	RouteTypes eNewRoute = (RouteTypes)pkBuildInfo->getRoute();
	if (eNewRoute == NO_ROUTE)
	{
		if (!GC.getBuildInfo(eBuild)->IsRemoveRoute() && (!IsRoutePillaged() || (GC.getBuildInfo(eBuild)->isRepair() && !IsImprovementPillaged())))
		{
			eNewRoute = getRouteType();
		}
	}

	// Will the build remove the feature?
	if (getFeatureType() != NO_FEATURE)
	{
		if (GC.getBuildInfo(eBuild)->isFeatureRemove(getFeatureType()))
		{
			if (GET_PLAYER(ePlayer).GetPlayerTraits()->IsWoodlandMovementBonus() && (eFeature == FEATURE_FOREST || eFeature == FEATURE_JUNGLE) && eNewRoute == NO_ROUTE)
				eForceCityConnection = NO_ROUTE;

			eFeature = NO_FEATURE;
		}
	}

	if (eNewImprovement != eOldImprovement)
	{
		// Was the previous improvement granting a feature/resource?
		if (eOldImprovement != NO_IMPROVEMENT)
		{
			CvImprovementEntry* pkOldImprovementInfo = GC.getImprovementInfo(eOldImprovement);
			FeatureTypes eOldCreatedFeature = pkOldImprovementInfo->GetCreatedFeature();
			if (eOldCreatedFeature != NO_FEATURE)
			{
				eFeature = NO_FEATURE;
			}

			ResourceTypes eOldCreatedResource = (ResourceTypes)pkOldImprovementInfo->GetResourceFromImprovement();
			if (eOldCreatedResource != NO_RESOURCE)
			{
				eResource = NO_RESOURCE;
			}
		}

		// Will the new improvement grant a feature/resource?
		if (eNewImprovement != NO_IMPROVEMENT)
		{
			CvImprovementEntry* pkImprovement = GC.getImprovementInfo(eNewImprovement);
			FeatureTypes eCreatedFeature = pkImprovement->GetCreatedFeature();
			if (eCreatedFeature != NO_FEATURE)
			{
				eFeature = eCreatedFeature;
			}

			ResourceTypes eCreatedResource = (ResourceTypes)pkImprovement->GetResourceFromImprovement();
			if (eCreatedResource != NO_RESOURCE)
			{
				eResource = eCreatedResource;
			}
		}
	}

	// Nature yield
	iYield = calculateNatureYield(eYield, ePlayer, eFeature, eResource, pOwningCity);
	iYield += calculateReligionNatureYield(eYield, ePlayer, eNewImprovement, eFeature, eResource, pOwningCity, pMajorityReligion, pSecondaryPantheon);

	if(eNewImprovement != NO_IMPROVEMENT)
	{
		if(bWithUpgrade)
		{
			//in the case that improvements upgrade, use 2 upgrade levels higher for the
			//yield calculations.
			ImprovementTypes eUpgradeImprovement = (ImprovementTypes)GC.getImprovementInfo(eNewImprovement)->GetImprovementUpgrade();
			if(eUpgradeImprovement != NO_IMPROVEMENT)
			{
				//unless it's trade on a low food tile, in which case only use 1 level higher
				if((eYield != YIELD_GOLD) || (getYield(YIELD_FOOD) >= /*2*/ GD_INT_GET(FOOD_CONSUMPTION_PER_POPULATION)))
				{
					ImprovementTypes eUpgradeImprovement2 = (ImprovementTypes)GC.getImprovementInfo(eUpgradeImprovement)->GetImprovementUpgrade();
					if(eUpgradeImprovement2 != NO_IMPROVEMENT)
					{
						eUpgradeImprovement = eUpgradeImprovement2;
					}
				}
			}

			if((eUpgradeImprovement != NO_IMPROVEMENT) && (eUpgradeImprovement != eNewImprovement))
			{
				eNewImprovement = eUpgradeImprovement;
			}
		}

		iYield += calculateImprovementYield(eYield, ePlayer, eNewImprovement, eNewRoute, eFeature, eResource, eForceCityConnection, pOwningCity, false) + calculateReligionImprovementYield(eYield, ePlayer, eNewImprovement, eResource, pOwningCity, pMajorityReligion, pSecondaryPantheon);
		if (MOD_RELIGION_PERMANENT_PANTHEON && pPlayerPantheon != NULL)
		{
			iYield += calculateReligionImprovementYield(eYield, ePlayer, eNewImprovement, eResource, pOwningCity, pPlayerPantheon, NULL);
		}
	}

	iYield += calculatePlayerYield(eYield, iYield, ePlayer, eNewImprovement, eFeature, eResource, eForceCityConnection, pOwningCity, pMajorityReligion, pSecondaryPantheon, pPlayerPantheon, false);

	//no overhead if empty
	for (size_t i=0; i<m_vExtraYields.size(); i++)
	{
		if (m_vExtraYields[i].first == eYield)
		{
			iYield += m_vExtraYields[i].second;
			break;
		}
	}

	return std::max(0, iYield);
}

//	--------------------------------------------------------------------------------
bool CvPlot::canTrain(UnitTypes eUnit) const
{
	CvUnitEntry* pkUnitInfo = GC.getUnitInfo(eUnit);
	if(pkUnitInfo == NULL)
	{
		return false;
	}

	CvUnitEntry& thisUnitEntry = *pkUnitInfo;
	DomainTypes thisUnitDomain = thisUnitEntry.GetDomainType();

	if(thisUnitEntry.IsPrereqResources())
	{
		if(thisUnitDomain == DOMAIN_SEA)
		{
			bool bValid = false;

			for(int iI = 0; iI < NUM_DIRECTION_TYPES; ++iI)
			{
				CvPlot* pLoopPlot = plotDirection(getX(), getY(), ((DirectionTypes)iI));

				if(pLoopPlot != NULL)
				{
					if(pLoopPlot->isWater())
					{
						if(pLoopPlot->area()->getNumTotalResources() > 0)
						{
							bValid = true;
							break;
						}
					}
				}
			}

			if(!bValid)
			{
				return false;
			}
		}
		else if (thisUnitDomain == DOMAIN_LAND)
		{
			if(area()->getNumTotalResources() == 0)
			{
				return false;
			}
		}
	}

	if (isCity())
	{
		if (thisUnitDomain == DOMAIN_SEA)
		{
			//fast check for ocean (-1)
			//check for canals only in Vox Populi
			if (!isCoastalLand(-1, true, MOD_BALANCE_VP) || !isCoastalLand(thisUnitEntry.GetMinAreaSize(), true, MOD_BALANCE_VP))
			{
				return false;
			}
		}
		else if (thisUnitDomain == DOMAIN_LAND)
		{
			if(landmass()->getNumTiles() < thisUnitEntry.GetMinAreaSize())
			{
				return false;
			}
		}
	}
	else
	{
		if(landmass()->getNumTiles() < thisUnitEntry.GetMinAreaSize())
		{
			return false;
		}

		if(thisUnitDomain == DOMAIN_SEA)
		{
			if(!isWater())
			{
				return false;
			}
		}
		else if(thisUnitDomain == DOMAIN_LAND)
		{
			if(isWater())
			{
				return false;
			}
		}
		else
		{
			return false;
		}
	}

	return true;
}

//	--------------------------------------------------------------------------------
int CvPlot::countNumAirUnits(TeamTypes eTeam, bool bNoSuicide) const
{
	int iCount = 0;

	const IDInfo* pUnitNode = headUnitNode();
	while(pUnitNode != NULL)
	{
		const CvUnit* pLoopUnit = GetPlayerUnit(*pUnitNode);
		pUnitNode = nextUnitNode(pUnitNode);

		if (!pLoopUnit || pLoopUnit->isDelayedDeath())
			continue;

		if (pLoopUnit->getDomainType() != DOMAIN_AIR)
			continue;

		if (pLoopUnit->isCargo())
			continue;

		if(eTeam == NO_TEAM || pLoopUnit->getTeam() == eTeam)
		{
			if (bNoSuicide && !pLoopUnit->isSuicide())
				iCount += pLoopUnit->getUnitInfo().GetAirUnitCap();
			else
				iCount += pLoopUnit->getUnitInfo().GetAirUnitCap();
		}
	}

	return iCount;
}

//	--------------------------------------------------------------------------------
int CvPlot::GetPlotIndex() const
{
	return m_iPlotIndex;
}

//	--------------------------------------------------------------------------------
char CvPlot::GetContinentType() const
{
	return m_cContinentType;
}

//	--------------------------------------------------------------------------------
void CvPlot::SetContinentType(const char cContinent)
{
#if defined(MOD_EVENTS_TERRAFORMING)
	if (MOD_EVENTS_TERRAFORMING) {
		GAMEEVENTINVOKE_HOOK(GAMEEVENT_TerraformingPlot, TERRAFORMINGEVENT_CONTINENT, m_iX, m_iY, 0, cContinent, m_cContinentType, -1, -1);
	}
#endif

	m_cContinentType = cContinent;
}

//	--------------------------------------------------------------------------------
CvSyncArchive<CvPlot>& CvPlot::getSyncArchive()
{
	return m_syncArchive;
}

//	--------------------------------------------------------------------------------
const CvSyncArchive<CvPlot>& CvPlot::getSyncArchive() const
{
	return m_syncArchive;
}

//	--------------------------------------------------------------------------------
std::string CvPlot::debugDump(const FAutoVariableBase&) const
{
	std::string result = "Game Turn : ";
	char gameTurnBuffer[8] = {0};
	int gameTurn = GC.getGame().getGameTurn();
	sprintf_s(gameTurnBuffer, "%d\0", gameTurn);
	result += gameTurnBuffer;
	return result;
}

//	--------------------------------------------------------------------------------
std::string CvPlot::stackTraceRemark(const FAutoVariableBase& var) const
{
	return debugDump(var);
}

//	---------------------------------------------------------------------------
//	Validate the contents of the plot.  This will attempt to clean up inconsistencies.
//	Returns 0 if no errors or:
//		bit 0 = true, an error was found in the plot's data
//		bit 1 = true, the error was un-recoverable
int CvPlot::Validate(CvMap& kParentMap)
{
	//------------------------------
	// force mountain terrain on mountain plots for correct impassability & yields - map scripts are sloppy here
	if (m_eTerrainType == TERRAIN_MOUNTAIN && m_ePlotType != PLOT_MOUNTAIN)
	{
		if (!IsNaturalWonder())
			setFeatureType(NO_FEATURE);
		setPlotType(PLOT_MOUNTAIN);
		setResourceType(NO_RESOURCE, 0);
	}

	if (m_ePlotType == PLOT_MOUNTAIN && m_eTerrainType != TERRAIN_MOUNTAIN)
	{
		if (!IsNaturalWonder())
			setFeatureType(NO_FEATURE);
		setTerrainType(TERRAIN_MOUNTAIN);
		setResourceType(NO_RESOURCE, 0);
	}

	// force correct terrain for ocean plots - just a failsafe
	if (m_ePlotType == PLOT_OCEAN && m_eTerrainType != TERRAIN_COAST && m_eTerrainType != TERRAIN_OCEAN)
	{ 
		setTerrainType(TERRAIN_COAST);
	}

	if ( (m_eTerrainType == TERRAIN_COAST || m_eTerrainType == TERRAIN_OCEAN) && m_ePlotType != PLOT_OCEAN)
	{
		setPlotType(PLOT_OCEAN);
	}
	//------------------------------

	int iError = 0;
	IDInfo* pUnitNode = headUnitNode();

	while(pUnitNode != NULL)
	{
		CvUnit* pkUnit = GetPlayerUnit(*pUnitNode);
		if(pkUnit == NULL)
		{
			m_units.deleteNode(pUnitNode);
			pUnitNode = headUnitNode();
			iError |= 1;
		}
		else
		{
			if(pkUnit->getX() != m_iX || pkUnit->getY() != m_iY)
			{
				iError |= 1;
				// Unit thinks it is in another plot.  Unit wins.
				m_units.deleteNode(pUnitNode);

				CvPlot* pkCorrectPlot = kParentMap.plot(pkUnit->getX(), pkUnit->getY());
				if(pkCorrectPlot)
				{
					if(pkCorrectPlot->getUnitIndex(pkUnit) < 0)	// Already in there?
						pkCorrectPlot->addUnit(pkUnit, false);		// Nope, add it.
				}
				else
				{
					// Drat, that plot does not exist.
					iError |= 2;
				}

				pUnitNode = headUnitNode();
			}
			else
				pUnitNode = nextUnitNode(pUnitNode);
		}
	}

	return iError;
}

//	--------------------------------------------------------------------------------
/// Some reason we don't need to pay maintenance here?
bool CvPlot::MustPayMaintenanceHere(PlayerTypes ePlayer) const
{
	return !static_cast<bool>(isHills() && GET_PLAYER(ePlayer).GetPlayerTraits()->IsNoHillsImprovementMaintenance());
}

//	---------------------------------------------------------------------------
void CvPlot::SetArchaeologicalRecord(GreatWorkArtifactClass eType, PlayerTypes ePlayer1, PlayerTypes ePlayer2)
{
	if (ePlayer1 != NO_PLAYER)
	{
		m_kArchaeologyData.m_eArtifactType = eType;
		m_kArchaeologyData.m_ePlayer1 = ePlayer1;
		m_kArchaeologyData.m_ePlayer2 = ePlayer2;
		m_kArchaeologyData.m_eEra = GET_PLAYER(ePlayer1).GetCurrentEra();
	}
}

//	---------------------------------------------------------------------------
void CvPlot::SetArchaeologicalRecord(GreatWorkArtifactClass eType, EraTypes eEra, PlayerTypes ePlayer1, PlayerTypes ePlayer2)
{
	if (ePlayer1 != NO_PLAYER)
	{
		m_kArchaeologyData.m_eArtifactType = eType;
		m_kArchaeologyData.m_ePlayer1 = ePlayer1;
		m_kArchaeologyData.m_ePlayer2 = ePlayer2;
		m_kArchaeologyData.m_eEra = eEra;
	}
}

//	---------------------------------------------------------------------------
void CvPlot::AddArchaeologicalRecord(GreatWorkArtifactClass eType, PlayerTypes ePlayer1, PlayerTypes ePlayer2)
{
	ImprovementTypes eImprovement = getImprovementType();
	if (eImprovement != NO_IMPROVEMENT && GC.getImprovementInfo(eImprovement))
	{
		if (GC.getImprovementInfo(eImprovement)->IsPermanent() || GC.getImprovementInfo(eImprovement)->IsCreatedByGreatPerson())
			return;
	}
	// Make sure the new record is more significant
	if (!GC.getGame().IsArchaeologyTriggered() && eType > m_kArchaeologyData.m_eArtifactType)
	{
		if (ePlayer1 != NO_PLAYER)
		{
			m_kArchaeologyData.m_eArtifactType = eType;
			m_kArchaeologyData.m_ePlayer1 = ePlayer1;
			m_kArchaeologyData.m_ePlayer2 = ePlayer2;
			m_kArchaeologyData.m_eEra = GET_PLAYER(ePlayer1).GetCurrentEra();
		}
	}
}

//	---------------------------------------------------------------------------
void CvPlot::AddArchaeologicalRecord(GreatWorkArtifactClass eType, EraTypes eEra, PlayerTypes ePlayer1, PlayerTypes ePlayer2)
{
	ImprovementTypes eImprovement = getImprovementType();
	if (eImprovement != NO_IMPROVEMENT && GC.getImprovementInfo(eImprovement))
	{
		if (GC.getImprovementInfo(eImprovement)->IsPermanent() || GC.getImprovementInfo(eImprovement)->IsCreatedByGreatPerson())
			return;
	}

	// Make sure the new record is more significant
	if (!GC.getGame().IsArchaeologyTriggered() && eType > m_kArchaeologyData.m_eArtifactType)
	{
		if (ePlayer1 != NO_PLAYER)
		{
			m_kArchaeologyData.m_eArtifactType = eType;
			m_kArchaeologyData.m_ePlayer1 = ePlayer1;
			m_kArchaeologyData.m_ePlayer2 = ePlayer2;
			m_kArchaeologyData.m_eEra = eEra;
		}
	}
}

//	---------------------------------------------------------------------------
void CvPlot::ClearArchaeologicalRecord()
{
	m_kArchaeologyData.m_eArtifactType = NO_GREAT_WORK_ARTIFACT_CLASS;
	m_kArchaeologyData.m_ePlayer1 = NO_PLAYER;
	m_kArchaeologyData.m_ePlayer2 = NO_PLAYER;
	m_kArchaeologyData.m_eEra = NO_ERA;
#if defined(MOD_BALANCE_CORE)
	ResourceTypes eArtifactResourceType = static_cast<ResourceTypes>(GD_INT_GET(ARTIFACT_RESOURCE));
	ResourceTypes eHiddenArtifactResourceType = static_cast<ResourceTypes>(GD_INT_GET(HIDDEN_ARTIFACT_RESOURCE));
	if (getResourceType() == eArtifactResourceType || getResourceType() == eHiddenArtifactResourceType)
	{
		setResourceType(NO_RESOURCE, 0);
	}
#endif
}

//	---------------------------------------------------------------------------
CvArchaeologyData CvPlot::GetArchaeologicalRecord() const
{
	return m_kArchaeologyData; // return a copy of the record
}

//	---------------------------------------------------------------------------
void CvPlot::SetArtifactType(GreatWorkArtifactClass eType)
{
	m_kArchaeologyData.m_eArtifactType = eType;
}

//	---------------------------------------------------------------------------
void CvPlot::SetArtifactGreatWork(GreatWorkType eWork)
{
	m_kArchaeologyData.m_eWork = eWork;
}

//	---------------------------------------------------------------------------
bool CvPlot::HasWrittenArtifact() const
{
	bool bRtnValue = false;
	GreatWorkArtifactClass eArtifactClass = m_kArchaeologyData.m_eArtifactType;
	if (eArtifactClass == CvTypes::getARTIFACT_WRITING())
	{
		bRtnValue = true;
	}
	return bRtnValue;
}

//	--------------------------------------------------------------------------------
// Citadel
int CvPlot::GetDamageFromAdjacentPlots(PlayerTypes ePlayer) const
{
	VALIDATE_OBJECT();
	int iDamage = 0;

	// Look around this unit to see if there's an adjacent citadel
	// But exclude this plot!
	for (int i=RING0_PLOTS; i<RING1_PLOTS; i++)
	{
		CvPlot* pLoopPlot = iterateRingPlots(this,i);
		if(pLoopPlot != NULL)
		{
			ImprovementTypes eImprovement = pLoopPlot->getImprovementType();

			// Citadel here?
			if(eImprovement != NO_IMPROVEMENT && !pLoopPlot->IsImprovementPillaged() && GC.getImprovementInfo(eImprovement)->GetNearbyEnemyDamage() != 0)
			{
				if (pLoopPlot->getOwner() != NO_PLAYER && GET_TEAM(GET_PLAYER(ePlayer).getTeam()).isAtWar(pLoopPlot->getTeam()))
					iDamage = max(iDamage, GC.getImprovementInfo(eImprovement)->GetNearbyEnemyDamage());
			}

			// Unit here that acts like a citadel?
			for (int iZ = 0; iZ < pLoopPlot->getNumUnits(); iZ++)
			{
				CvUnit* pLoopUnit = pLoopPlot->getUnitByIndex(iZ);
				if (pLoopUnit && GET_TEAM(GET_PLAYER(ePlayer).getTeam()).isAtWar(pLoopUnit->getTeam()))
					iDamage = max(iDamage, pLoopUnit->getNearbyEnemyDamage());
			}
		}
	}

	return iDamage;
}

//	---------------------------------------------------------------------------
void CvPlot::updateImpassable(TeamTypes eTeam)
{
	const TerrainTypes eTerrain = getTerrainType();
	const FeatureTypes eFeature = getFeatureType();

	//not really related but a good place to update this
	//if we have any visibility limiting features here, consider the plot as rough
	// in VP, the elevation of all land tiles is increased by 1
	m_bRoughPlot = (seeThroughLevel() > (MOD_BALANCE_VP ? 2 : 1));
	//only land is is passable by default
	m_bIsImpassable = isMountain();
	if (eTeam != NO_TEAM)
		SetTeamImpassable(eTeam, m_bIsImpassable);
	else
		for (size_t i=0; i<MAX_TEAMS; i++)
			SetTeamImpassable((TeamTypes)i, m_bIsImpassable);

	//if it's passable, check for blocking terrain/features
	if(eTerrain != NO_TERRAIN)
	{
		if(eFeature == NO_FEATURE)
		{
			CvTerrainInfo* pkTerrainInfo = GC.getTerrainInfo(eTerrain);
			if(pkTerrainInfo)
			{
				m_bIsImpassable = pkTerrainInfo->isImpassable();
				if (eTeam != NO_TEAM)
				{
					SetTeamImpassable(eTeam, m_bIsImpassable);

					if(m_bIsImpassable && (TechTypes)pkTerrainInfo->GetPrereqPassable() != NO_TECH)
					{
						if (GET_TEAM(eTeam).GetTeamTechs()->HasTech((TechTypes)pkTerrainInfo->GetPrereqPassable()))
							SetTeamImpassable(eTeam, false);
					}
				}
				else
				{
					for (size_t i=0; i<MAX_TEAMS; i++)
					{
						SetTeamImpassable((TeamTypes)i, m_bIsImpassable);

						if(m_bIsImpassable && (TechTypes)pkTerrainInfo->GetPrereqPassable() != NO_TECH)
						{
							if (GET_TEAM((TeamTypes)i).GetTeamTechs()->HasTech((TechTypes)pkTerrainInfo->GetPrereqPassable()))
								SetTeamImpassable((TeamTypes)i, false);
						}
					}
				}
			}
		}
		else
		{
			CvFeatureInfo* pkFeatureInfo = GC.getFeatureInfo(eFeature);
			if(pkFeatureInfo)
			{
				m_bIsImpassable = pkFeatureInfo->isImpassable();
				if (eTeam != NO_TEAM)
				{
					SetTeamImpassable(eTeam, m_bIsImpassable);

					if(m_bIsImpassable && (TechTypes)pkFeatureInfo->GetPrereqPassable() != NO_TECH)
					{
						if (GET_TEAM(eTeam).GetTeamTechs()->HasTech((TechTypes)pkFeatureInfo->GetPrereqPassable()))
							SetTeamImpassable(eTeam, false);
					}
				}
				else
				{
					for (size_t i=0; i<MAX_TEAMS; i++)
					{
						SetTeamImpassable((TeamTypes)i, m_bIsImpassable);

						if(m_bIsImpassable && (TechTypes)pkFeatureInfo->GetPrereqPassable() != NO_TECH)
						{
							if (GET_TEAM((TeamTypes)i).GetTeamTechs()->HasTech((TechTypes)pkFeatureInfo->GetPrereqPassable()))
								SetTeamImpassable((TeamTypes)i, false);
						}
					}
				}	
			}
		}
	}
}

int CvPlot::getTurnDamage(bool bIgnoreTerrainDamage, bool bIgnoreFeatureDamage, bool bExtraTerrainDamage, bool bExtraFeatureDamage) const
{
	int damage = 0;

	const TerrainTypes eTerrain = getTerrainType();
	const FeatureTypes eFeature = getFeatureType();
		
	// Make an exception for the volcano
	if (eFeature != NO_FEATURE)
	{
		CvFeatureInfo* pkFeatureInfo = GC.getFeatureInfo(eFeature);
		if (pkFeatureInfo)
		{
			if (pkFeatureInfo->GetType() == CvString("FEATURE_VOLCANO"))
			{
				bIgnoreTerrainDamage = false;
				bIgnoreFeatureDamage = false;
			}
			if (!bIgnoreFeatureDamage)
			{
				damage += pkFeatureInfo->getTurnDamage();
			}
			if (bExtraFeatureDamage)
			{
				damage += pkFeatureInfo->getExtraTurnDamage();
			}
		}
	}

	if (eTerrain != NO_TERRAIN) 
	{
		CvTerrainInfo* pkTerrainInfo = GC.getTerrainInfo(eTerrain);
		if (pkTerrainInfo) 
		{
			// no damage for units on montain cities
			if (!bIgnoreTerrainDamage && !isCity())
			{						
				damage += pkTerrainInfo->getTurnDamage();
			}
			if (bExtraTerrainDamage)
			{
				damage += pkTerrainInfo->getExtraTurnDamage();
			}
		}
	}

	return damage;
}

bool CvPlot::isImpassable(TeamTypes eTeam) const
{
	if(eTeam != NO_TEAM)
	{
		return IsTeamImpassable(eTeam);
	}

	//this is only set for mountains, no ice!
	return m_bIsImpassable;
}

//--------------------------------------------------------------------
// in updateImpassable we check terrain and features (per plot), combined with technologies (per team). here we additionally look at traits (per player). 
// result is a simplified version of canMoveInto. since we don't know the particular of the unit, we are more restrictive here
bool CvPlot::isValidMovePlot(PlayerTypes ePlayer, bool bCheckTerritory) const
{
	if ( getRouteType()!=NO_ROUTE && !IsRoutePillaged() && (!isCity() || getOwner()==ePlayer) ) //if it's a city, it needs to be our city
		return true;

	if (ePlayer==NO_PLAYER)
		return !m_bIsImpassable;
	else
	{
		TeamTypes eTeam = GET_PLAYER(ePlayer).getTeam();
		if ( IsTeamImpassable(eTeam) )
		{
			bool bCanPassBecauseOfPlayerTrait = false;

			//check some special traits
			if (isIce() && GET_PLAYER(ePlayer).CanCrossIce() )
				bCanPassBecauseOfPlayerTrait = true;
			//inca
			if (isMountain() && GET_PLAYER(ePlayer).CanCrossMountain() )
				bCanPassBecauseOfPlayerTrait = true;
			//deep water...
			if (isDeepWater() && GET_PLAYER(ePlayer).CanCrossOcean())
				bCanPassBecauseOfPlayerTrait = true;
			//and shallow water... (this is necessary because of scenarios and tech situations where units can embark before techs, and vice-versa.
			if (isShallowWater() && GET_PLAYER(ePlayer).CanEmbark())
				bCanPassBecauseOfPlayerTrait = true;

			if (!bCanPassBecauseOfPlayerTrait)
				return false;
		}

		//now check territory also - majors only (minors are always open)
		if ( bCheckTerritory && getTeam()!=NO_TEAM && getTeam()!=eTeam && !GET_TEAM(getTeam()).isMinorCiv())
		{
			if (!GET_TEAM(getTeam()).IsAllowsOpenBordersToTeam(eTeam) && !GET_TEAM(eTeam).isAtWar(getTeam()))
			{
				return false;
			}
		}

		//seems we're good
		return true;
	}
}

//----------------------------------------------------------
//conservative estimate whether we can put a combat unit here. does not check different domains etc
bool CvPlot::canPlaceCombatUnit(PlayerTypes ePlayer) const
{
	if (!isValidMovePlot(ePlayer))
		return false;

	if (getOwner()!=NO_PLAYER && ePlayer!=NO_PLAYER)
	{
		TeamTypes ePlotTeam = GET_PLAYER(getOwner()).getTeam();
		TeamTypes eTestTeam = GET_PLAYER(ePlayer).getTeam();
		if (ePlotTeam!=eTestTeam && !GET_TEAM(ePlotTeam).isMinorCiv() && !GET_TEAM(ePlotTeam).IsAllowsOpenBordersToTeam(eTestTeam) && !GET_TEAM(eTestTeam).isAtWar(ePlotTeam))
			return false;
	}

	//can't place into a plot with another combat unit (owner does not matter)
	if(getNumDefenders(NO_PLAYER) >= getUnitLimit())
		return false;
			
	//can't place into a plot with a foreign city
	if(isCity() && getPlotCity()->getOwner()!=ePlayer)
		return false;

	return true;
}

bool CvPlot::IsCivilization(CivilizationTypes iCivilizationType) const
{
	return (GET_PLAYER(getOwner()).getCivilizationType() == iCivilizationType);
}

bool CvPlot::HasFeature(FeatureTypes iFeatureType) const
{
	if (iFeatureType == (FeatureTypes)GC.getInfoTypeForString("FEATURE_LAKE")) {
		return IsFeatureLake();
	} else if (iFeatureType == (FeatureTypes)GC.getInfoTypeForString("FEATURE_RIVER")) {
		return IsFeatureRiver();
	}

	return (getFeatureType() == iFeatureType);
}

bool CvPlot::IsFeatureLake() const
{
	return isLake();
}

bool CvPlot::IsFeatureRiver() const
{
	return isRiver();
}

bool CvPlot::HasAnyNaturalWonder() const
{
	return IsNaturalWonder();
}

bool CvPlot::HasNaturalWonder(FeatureTypes iFeatureType) const
{
	return HasFeature(iFeatureType);
}

bool CvPlot::HasImprovement(ImprovementTypes iImprovementType) const
{
	return (getImprovementType() == iImprovementType);
}

bool CvPlot::HasPlotType(PlotTypes iPlotType) const
{
	return (getPlotType() == iPlotType);
}

bool CvPlot::HasResource(ResourceTypes iResourceType) const
{
	return (getResourceType() == iResourceType);
}

bool CvPlot::HasRoute(RouteTypes iRouteType) const
{
	return (getRouteType() == iRouteType);
}

bool CvPlot::HasTerrain(TerrainTypes iTerrainType) const
{
	return (getTerrainType() == iTerrainType);
}

bool CvPlot::IsAdjacentToFeature(FeatureTypes iFeatureType) const
{
	int iX = getX(); int iY = getY();

	for (int iDirection = 0; iDirection < NUM_DIRECTION_TYPES; iDirection++) {
		CvPlot* pLoopPlot = plotDirection(iX, iY, (DirectionTypes) iDirection);

		if (pLoopPlot != NULL && pLoopPlot->HasFeature(iFeatureType)) {
			return true;
		}
	}

	return false;
}

bool CvPlot::IsWithinDistanceOfFeature(FeatureTypes iFeatureType, int iDistance) const
{
	int iX = getX(); int iY = getY();

	for (int i = -iDistance; i <= iDistance; ++i) {
		for (int j = -iDistance; j <= iDistance; ++j) {
			CvPlot* pLoopPlot = ::plotXYWithRangeCheck(iX, iY, i, j, iDistance);
		
			if (pLoopPlot != NULL && pLoopPlot->HasFeature(iFeatureType)) {
				return true;
			}
		}
	}

	return false;
}

#if defined(MOD_BALANCE_CORE)
bool CvPlot::IsWithinDistanceOfUnit(PlayerTypes ePlayer, UnitTypes eOtherUnit, int iDistance, bool bIsFriendly, bool bIsEnemy) const
{
	int iX = getX(); int iY = getY();
	CvUnit* pLoopUnit = NULL;
	if(iDistance >= 0)
	{
		for(int iI = 0; iI < this->getNumUnits(); iI++)
		{
			pLoopUnit = this->getUnitByIndex(iI);
			if(pLoopUnit != NULL)
			{
				if(pLoopUnit->getUnitType() == eOtherUnit)
				{
					if(bIsFriendly && GET_PLAYER(pLoopUnit->getOwner()).getTeam() == GET_PLAYER(ePlayer).getTeam())
					{
						return true;
					}
					else if(bIsEnemy && GET_TEAM(GET_PLAYER(pLoopUnit->getOwner()).getTeam()).isAtWar(GET_PLAYER(ePlayer).getTeam()))
					{
						return true;
					}
					else if(!bIsFriendly && !bIsEnemy)
					{
						return true;
					}
				}
			}
		}
	}
	for (int i = -iDistance; i <= iDistance; ++i) 
	{
		for (int j = -iDistance; j <= iDistance; ++j) 
		{
			CvPlot* pLoopPlot = ::plotXYWithRangeCheck(iX, iY, i, j, iDistance);
		
			if (pLoopPlot != NULL && pLoopPlot->getNumUnits() != 0 && pLoopPlot != this)
			{
				for(int iI = 0; iI < pLoopPlot->getNumUnits(); iI++)
				{
					pLoopUnit = pLoopPlot->getUnitByIndex(iI);
					if(pLoopUnit != NULL)
					{
						if(pLoopUnit->getUnitType() == eOtherUnit)
						{
							if(bIsFriendly && GET_PLAYER(pLoopUnit->getOwner()).getTeam() == GET_PLAYER(ePlayer).getTeam())
							{
								return true;
							}
							else if(bIsEnemy && GET_TEAM(GET_PLAYER(pLoopUnit->getOwner()).getTeam()).isAtWar(GET_PLAYER(ePlayer).getTeam()))
							{
								return true;
							}
							else if(!bIsFriendly && !bIsEnemy)
							{
								return true;
							}
						}
					}
				}
			}
		}
	}
	return false;
}

bool CvPlot::IsWithinDistanceOfCity(const CvUnit* eThisUnit, int iDistance, bool bIsFriendly, bool bIsEnemy) const
{
	if(eThisUnit != NULL)
	{
		for (int i = -iDistance; i <= iDistance; ++i) 
		{
			for (int j = -iDistance; j <= iDistance; ++j) 
			{
				int iX = eThisUnit->getX(); int iY = eThisUnit->getY();
				CvPlot* pLoopPlot = ::plotXYWithRangeCheck(iX, iY, i, j, iDistance);		
				if (pLoopPlot != NULL)
				{
					if(pLoopPlot->isCity())
					{
						if(bIsFriendly && pLoopPlot->isFriendlyCity(*eThisUnit))
						{
							return true;
						}
						else if(bIsEnemy && pLoopPlot->isEnemyCity(*eThisUnit))
						{
							return true;
						}
						else if(!bIsFriendly && !bIsEnemy)
						{
							return true;
						}
					}
				}
			}
		}
	}
	return false;
}

bool CvPlot::IsWithinDistanceOfUnitCombatType(PlayerTypes ePlayer, UnitCombatTypes eUnitCombat, int iDistance, bool bIsFriendly, bool bIsEnemy) const
{
	int iX = getX(); int iY = getY();
	CvUnit* pLoopUnit = NULL;
	if(iDistance >= 0)
	{
		for(int iI = 0; iI < this->getNumUnits(); iI++)
		{
			pLoopUnit = this->getUnitByIndex(iI);
			if(pLoopUnit != NULL)
			{
				if(pLoopUnit->getUnitCombatType() == eUnitCombat)
				{
					if(bIsFriendly && GET_PLAYER(pLoopUnit->getOwner()).getTeam() == GET_PLAYER(ePlayer).getTeam())
					{
						return true;
					}
					else if(bIsEnemy && GET_TEAM(GET_PLAYER(pLoopUnit->getOwner()).getTeam()).isAtWar(GET_PLAYER(ePlayer).getTeam()))
					{
						return true;
					}
					else if(!bIsFriendly && !bIsEnemy)
					{
						return true;
					}
				}
			}
		}
	}
	for (int i = -iDistance; i <= iDistance; ++i) 
	{
		for (int j = -iDistance; j <= iDistance; ++j) 
		{
			CvPlot* pLoopPlot = ::plotXYWithRangeCheck(iX, iY, i, j, iDistance);
		
			if (pLoopPlot != NULL && pLoopPlot->getNumUnits() != 0 && pLoopPlot != this)
			{
				for(int iI = 0; iI < pLoopPlot->getNumUnits(); iI++)
				{
					pLoopUnit = pLoopPlot->getUnitByIndex(iI);
					if(pLoopUnit != NULL)
					{
						if(pLoopUnit->getUnitCombatType() == eUnitCombat)
						{
							if(bIsFriendly && GET_PLAYER(pLoopUnit->getOwner()).getTeam() == GET_PLAYER(ePlayer).getTeam())
							{
								return true;
							}
							else if(bIsEnemy && GET_TEAM(GET_PLAYER(pLoopUnit->getOwner()).getTeam()).isAtWar(GET_PLAYER(ePlayer).getTeam()))
							{
								return true;
							}
							else if(!bIsFriendly && !bIsEnemy)
							{
								return true;
							}
						}
					}
				}
			}
		}
	}
	return false;
}

bool CvPlot::IsWithinDistanceOfUnitClass(PlayerTypes ePlayer, UnitClassTypes eUnitClass, int iDistance, bool bIsFriendly, bool bIsEnemy) const
{
	int iX = getX(); int iY = getY();
	CvUnit* pLoopUnit = NULL;
	if(iDistance >= 0)
	{
		for(int iI = 0; iI < this->getNumUnits(); iI++)
		{
			pLoopUnit = this->getUnitByIndex(iI);
			if(pLoopUnit != NULL)
			{
				if(pLoopUnit->getUnitClassType() == eUnitClass)
				{
					if(bIsFriendly && GET_PLAYER(pLoopUnit->getOwner()).getTeam() == GET_PLAYER(ePlayer).getTeam())
					{
						return true;
					}
					else if(bIsEnemy && GET_TEAM(GET_PLAYER(pLoopUnit->getOwner()).getTeam()).isAtWar(GET_PLAYER(ePlayer).getTeam()))
					{
						return true;
					}
					else if(!bIsFriendly && !bIsEnemy)
					{
						return true;
					}
				}
			}
		}
	}
	for (int i = -iDistance; i <= iDistance; ++i) 
	{
		for (int j = -iDistance; j <= iDistance; ++j) 
		{
			CvPlot* pLoopPlot = ::plotXYWithRangeCheck(iX, iY, i, j, iDistance);
		
			if (pLoopPlot != NULL && pLoopPlot->getNumUnits() != 0 && pLoopPlot != this)
			{
				for(int iI = 0; iI < pLoopPlot->getNumUnits(); iI++)
				{
					pLoopUnit = pLoopPlot->getUnitByIndex(iI);
					if(pLoopUnit != NULL)
					{
						if(pLoopUnit->getUnitClassType() == eUnitClass)
						{
							if(bIsFriendly && GET_PLAYER(pLoopUnit->getOwner()).getTeam() == GET_PLAYER(ePlayer).getTeam())
							{
								return true;
							}
							else if(bIsEnemy && GET_TEAM(GET_PLAYER(pLoopUnit->getOwner()).getTeam()).isAtWar(GET_PLAYER(ePlayer).getTeam()))
							{
								return true;
							}
							else if(!bIsFriendly && !bIsEnemy)
							{
								return true;
							}
						}
					}
				}
			}
		}
	}
	return false;
}

bool CvPlot::IsWithinDistanceOfUnitPromotion(PlayerTypes ePlayer, PromotionTypes eUnitPromotion, int iDistance, bool bIsFriendly, bool bIsEnemy) const
{
	int iX = getX(); int iY = getY();
	CvUnit* pLoopUnit = NULL;
	if(iDistance >= 0)
	{
		for(int iI = 0; iI < this->getNumUnits(); iI++)
		{
			pLoopUnit = this->getUnitByIndex(iI);
			if(pLoopUnit != NULL)
			{
				if (pLoopUnit->isHasPromotion(eUnitPromotion))
				{
					if(bIsFriendly && GET_PLAYER(pLoopUnit->getOwner()).getTeam() == GET_PLAYER(ePlayer).getTeam())
					{
						return true;
					}
					else if(bIsEnemy && GET_TEAM(GET_PLAYER(pLoopUnit->getOwner()).getTeam()).isAtWar(GET_PLAYER(ePlayer).getTeam()))
					{
						return true;
					}
					else if(!bIsFriendly && !bIsEnemy)
					{
						return true;
					}
				}
			}
		}
	}
	for (int i = -iDistance; i <= iDistance; ++i) 
	{
		for (int j = -iDistance; j <= iDistance; ++j) 
		{
			CvPlot* pLoopPlot = ::plotXYWithRangeCheck(iX, iY, i, j, iDistance);
		
			if (pLoopPlot != NULL && pLoopPlot->getNumUnits() != 0 && pLoopPlot != this)
			{
				for(int iI = 0; iI < pLoopPlot->getNumUnits(); iI++)
				{
					pLoopUnit = pLoopPlot->getUnitByIndex(iI);
					if(pLoopUnit != NULL)
					{
						if(pLoopUnit->isHasPromotion(eUnitPromotion))
						{
							if(bIsFriendly && GET_PLAYER(pLoopUnit->getOwner()).getTeam() == GET_PLAYER(ePlayer).getTeam())
							{
								return true;
							}
							else if(bIsEnemy && GET_TEAM(GET_PLAYER(pLoopUnit->getOwner()).getTeam()).isAtWar(GET_PLAYER(ePlayer).getTeam()))
							{
								return true;
							}
							else if(!bIsFriendly && !bIsEnemy)
							{
								return true;
							}
						}
					}
				}
			}
		}
	}
	return false;
}

bool CvPlot::IsAdjacentToUnit(PlayerTypes ePlayer, UnitTypes eOtherUnit, bool bIsFriendly, bool bIsEnemy) const
{
	int iX = getX(); int iY = getY();
	CvUnit* pLoopUnit = NULL;
	for (int iDirection = 0; iDirection < NUM_DIRECTION_TYPES; iDirection++) 
	{
		CvPlot* pLoopPlot = plotDirection(iX, iY, (DirectionTypes) iDirection);
		
		if (pLoopPlot != NULL && pLoopPlot->getNumUnits() != 0 && pLoopPlot != this)
		{
			for(int iI = 0; iI < pLoopPlot->getNumUnits(); iI++)
			{
				pLoopUnit = pLoopPlot->getUnitByIndex(iI);
				if(pLoopUnit != NULL)
				{
					if(pLoopUnit->getUnitType() == eOtherUnit)
					{
						if(bIsFriendly && GET_PLAYER(pLoopUnit->getOwner()).getTeam() == GET_PLAYER(ePlayer).getTeam())
						{
							return true;
						}
						else if(bIsEnemy && GET_TEAM(GET_PLAYER(pLoopUnit->getOwner()).getTeam()).isAtWar(GET_PLAYER(ePlayer).getTeam()))
						{
							return true;
						}
						else if(!bIsFriendly && !bIsEnemy)
						{
							return true;
						}
					}
				}
			}
		}
	}
	return false;
}

bool CvPlot::IsAdjacentToUnitCombatType(PlayerTypes ePlayer, UnitCombatTypes eUnitCombat, bool bIsFriendly, bool bIsEnemy) const
{
	int iX = getX(); int iY = getY();
	CvUnit* pLoopUnit = NULL;
	for (int iDirection = 0; iDirection < NUM_DIRECTION_TYPES; iDirection++) 
	{
		CvPlot* pLoopPlot = plotDirection(iX, iY, (DirectionTypes) iDirection);
		
		if (pLoopPlot != NULL && pLoopPlot->getNumUnits() != 0 && pLoopPlot != this)
		{
			for(int iI = 0; iI < pLoopPlot->getNumUnits(); iI++)
			{
				pLoopUnit = pLoopPlot->getUnitByIndex(iI);
				if(pLoopUnit != NULL)
				{
					if(pLoopUnit->getUnitCombatType() == eUnitCombat)
					{
						if(bIsFriendly && GET_PLAYER(pLoopUnit->getOwner()).getTeam() == GET_PLAYER(ePlayer).getTeam())
						{
							return true;
						}
						else if(bIsEnemy && GET_TEAM(GET_PLAYER(pLoopUnit->getOwner()).getTeam()).isAtWar(GET_PLAYER(ePlayer).getTeam()))
						{
							return true;
						}
						else if(!bIsFriendly && !bIsEnemy)
						{
							return true;
						}
					}
				}
			}
		}
	}
	return false;
}

bool CvPlot::IsAdjacentToUnitClass(PlayerTypes ePlayer, UnitClassTypes eUnitClass, bool bIsFriendly, bool bIsEnemy) const
{
	int iX = getX(); int iY = getY();
	CvUnit* pLoopUnit = NULL;
	for (int iDirection = 0; iDirection < NUM_DIRECTION_TYPES; iDirection++) 
	{
		CvPlot* pLoopPlot = plotDirection(iX, iY, (DirectionTypes) iDirection);
		
		if (pLoopPlot != NULL && pLoopPlot->getNumUnits() != 0 && pLoopPlot != this)
		{
			for(int iI = 0; iI < pLoopPlot->getNumUnits(); iI++)
			{
				pLoopUnit = pLoopPlot->getUnitByIndex(iI);
				if(pLoopUnit != NULL)
				{
					if(pLoopUnit->getUnitClassType() == eUnitClass)
					{
						if(bIsFriendly && GET_PLAYER(pLoopUnit->getOwner()).getTeam() == GET_PLAYER(ePlayer).getTeam())
						{
							return true;
						}
						else if(bIsEnemy && GET_TEAM(GET_PLAYER(pLoopUnit->getOwner()).getTeam()).isAtWar(GET_PLAYER(ePlayer).getTeam()))
						{
							return true;
						}
						else if(!bIsFriendly && !bIsEnemy)
						{
							return true;
						}
					}
				}
			}
		}
	}
	return false;
}

bool CvPlot::IsAdjacentToUnitPromotion(PlayerTypes ePlayer, PromotionTypes eUnitPromotion, bool bIsFriendly, bool bIsEnemy) const
{
	int iX = getX(); int iY = getY();
	CvUnit* pLoopUnit = NULL;
	for (int iDirection = 0; iDirection < NUM_DIRECTION_TYPES; iDirection++) 
	{
		CvPlot* pLoopPlot = plotDirection(iX, iY, (DirectionTypes) iDirection);
		
		if (pLoopPlot != NULL && pLoopPlot->getNumUnits() != 0 && pLoopPlot != this)
		{
			for(int iI = 0; iI < pLoopPlot->getNumUnits(); iI++)
			{
				pLoopUnit = pLoopPlot->getUnitByIndex(iI);
				if(pLoopUnit != NULL)
				{
					if(pLoopUnit->isHasPromotion(eUnitPromotion))
					{
						if(bIsFriendly && GET_PLAYER(pLoopUnit->getOwner()).getTeam() == GET_PLAYER(ePlayer).getTeam())
						{
							return true;
						}
						else if(bIsEnemy && GET_TEAM(GET_PLAYER(pLoopUnit->getOwner()).getTeam()).isAtWar(GET_PLAYER(ePlayer).getTeam()))
						{
							return true;
						}
						else if(!bIsFriendly && !bIsEnemy)
						{
							return true;
						}
					}
				}
			}
		}
	}
	return false;
}

bool CvPlot::IsAdjacentToTradeRoute() const
{
	int iX = getX(); int iY = getY();
	for (int iDirection = 0; iDirection < NUM_DIRECTION_TYPES; iDirection++) 
	{
		CvPlot* pLoopPlot = plotDirection(iX, iY, (DirectionTypes) iDirection);		
		if (pLoopPlot != NULL && pLoopPlot->IsTradeUnitRoute())
		{
			return true;
		}
	}
	return false;
}

bool CvPlot::IsAdjacentToRoute(RouteTypes eType) const
{
	int iX = getX(); int iY = getY();
	for (int iDirection = 0; iDirection < NUM_DIRECTION_TYPES; iDirection++) 
	{
		CvPlot* pLoopPlot = plotDirection(iX, iY, (DirectionTypes) iDirection);		
		if (pLoopPlot != NULL && pLoopPlot->isRoute())
		{
			return eType==ROUTE_ANY || pLoopPlot->getRouteType()==eType;
		}
	}
	return false;
}
#endif

bool CvPlot::IsAdjacentToImprovement(ImprovementTypes iImprovementType) const
{
	int iX = getX(); int iY = getY();

	for (int iDirection = 0; iDirection < NUM_DIRECTION_TYPES; iDirection++) {
		CvPlot* pLoopPlot = plotDirection(iX, iY, (DirectionTypes) iDirection);

		if (pLoopPlot != NULL && pLoopPlot->HasImprovement(iImprovementType)) {
			return true;
		}
	}

	return false;
}

bool CvPlot::IsWithinDistanceOfImprovement(ImprovementTypes iImprovementType, int iDistance) const
{
	int iX = getX(); int iY = getY();

	for (int i = -iDistance; i <= iDistance; ++i) {
		for (int j = -iDistance; j <= iDistance; ++j) {
			CvPlot* pLoopPlot = ::plotXYWithRangeCheck(iX, iY, i, j, iDistance);
		
			if (pLoopPlot != NULL && pLoopPlot->HasImprovement(iImprovementType)) {
				return true;
			}
		}
	}

	return false;
}

bool CvPlot::IsAdjacentToPlotType(PlotTypes iPlotType) const
{
	int iX = getX(); int iY = getY();

	for (int iDirection = 0; iDirection < NUM_DIRECTION_TYPES; iDirection++) {
		CvPlot* pLoopPlot = plotDirection(iX, iY, (DirectionTypes) iDirection);

		if (pLoopPlot != NULL && pLoopPlot->HasPlotType(iPlotType)) {
			return true;
		}
	}

	return false;
}

bool CvPlot::IsWithinDistanceOfPlotType(PlotTypes iPlotType, int iDistance) const
{
	int iX = getX(); int iY = getY();

	for (int i = -iDistance; i <= iDistance; ++i) {
		for (int j = -iDistance; j <= iDistance; ++j) {
			CvPlot* pLoopPlot = ::plotXYWithRangeCheck(iX, iY, i, j, iDistance);
		
			if (pLoopPlot != NULL && pLoopPlot->HasPlotType(iPlotType)) {
				return true;
			}
		}
	}

	return false;
}

bool CvPlot::IsAdjacentToResource(ResourceTypes iResourceType) const
{
	int iX = getX(); int iY = getY();

	for (int iDirection = 0; iDirection < NUM_DIRECTION_TYPES; iDirection++) {
		CvPlot* pLoopPlot = plotDirection(iX, iY, (DirectionTypes) iDirection);

		if (pLoopPlot != NULL && pLoopPlot->HasResource(iResourceType)) {
			return true;
		}
	}

	return false;
}

bool CvPlot::IsWithinDistanceOfResource(ResourceTypes iResourceType, int iDistance) const
{
	int iX = getX(); int iY = getY();

	for (int i = -iDistance; i <= iDistance; ++i) {
		for (int j = -iDistance; j <= iDistance; ++j) {
			CvPlot* pLoopPlot = ::plotXYWithRangeCheck(iX, iY, i, j, iDistance);
		
			if (pLoopPlot != NULL && pLoopPlot->HasResource(iResourceType)) {
				return true;
			}
		}
	}

	return false;
}

bool CvPlot::IsAdjacentToTerrain(TerrainTypes iTerrainType) const
{
	int iX = getX(); int iY = getY();

	for (int iDirection = 0; iDirection < NUM_DIRECTION_TYPES; iDirection++) {
		CvPlot* pLoopPlot = plotDirection(iX, iY, (DirectionTypes) iDirection);

		if (pLoopPlot != NULL && pLoopPlot->HasTerrain(iTerrainType)) {
			return true;
		}
	}

	return false;
}

bool CvPlot::IsWithinDistanceOfTerrain(TerrainTypes iTerrainType, int iDistance) const
{
	int iX = getX(); int iY = getY();

	for (int i = -iDistance; i <= iDistance; ++i) {
		for (int j = -iDistance; j <= iDistance; ++j) {
			CvPlot* pLoopPlot = ::plotXYWithRangeCheck(iX, iY, i, j, iDistance);
		
			if (pLoopPlot != NULL && pLoopPlot->HasTerrain(iTerrainType)) {
				return true;
			}
		}
	}

	return false;
}

// Is tile stealing blocked by the presence of an improvement
bool CvPlot::IsStealBlockedByImprovement() const
{
	if (getOwner() == NO_PLAYER)
		return false;

	if (getImprovementType() == NO_IMPROVEMENT)
		return false;

	if (IsImprovementPillaged())
		return false;

	return GC.getImprovementInfo(getImprovementType())->IsBlockTileSteal();
}

///-------------------------------------
/// Is an enemy city next to us?
bool CvPlot::IsEnemyCityAdjacent(TeamTypes eMyTeam, const CvCity* pSpecifyCity) const
{
	for(int iI = 0; iI < NUM_DIRECTION_TYPES; iI++)
	{
		CvPlot* pLoopPlot = plotDirection(getX(), getY(), ((DirectionTypes)iI));
		if(!pLoopPlot)
		{
			continue;
		}

		CvCity* pCity = pLoopPlot->getPlotCity();
		if(!pCity)
		{
			continue;
		}

		if (!pSpecifyCity || (pCity->getX() == pSpecifyCity->getX() && pCity->getY() == pSpecifyCity->getY()))
		{
			if(GET_TEAM(eMyTeam).isAtWar(pCity->getTeam()))
			{
				return true;
			}
		}
	}

	return false;
}

///-------------------------------------
/// Is an enemy unit next to us?
bool CvPlot::IsEnemyUnitAdjacent(TeamTypes eMyTeam) const
{
	for (int iI = 0; iI < NUM_DIRECTION_TYPES; iI++)
	{
		CvPlot* pLoopPlot = plotDirection(getX(), getY(), ((DirectionTypes)iI));
		if (!pLoopPlot)
			continue;

		IDInfo* pUnitNode = pLoopPlot->headUnitNode();

		// Loop through all units on this plot
		while (pUnitNode != NULL)
		{
			CvUnit* pLoopUnit = ::GetPlayerUnit(*pUnitNode);
			pUnitNode = pLoopPlot->nextUnitNode(pUnitNode);

			if (pLoopUnit && !pLoopUnit->isDelayedDeath())
			{
				TeamTypes eTheirTeam = pLoopUnit->getTeam();

				// This team which this unit belongs to must be at war with us
				if (GET_TEAM(eTheirTeam).isAtWar(eMyTeam))
					return true;
			}
		}
	}

	return false;
}

vector<CvUnit*> CvPlot::GetAdjacentFriendlyCombatUnits(TeamTypes eMyTeam, int iRange, DomainTypes eDomain) const
{
	vector<CvUnit*> result;
	int iMaxDistance = min(5,iRange);
	for (int iJ = RING1_PLOTS; iJ < RING_PLOTS[iMaxDistance]; iJ++)
	{
		CvPlot* pLoopPlot = iterateRingPlots(this, iJ);
		if(pLoopPlot)
		{
			const IDInfo* pUnitNode = pLoopPlot->headUnitNode();
			while (pUnitNode != NULL)
			{
				CvUnit* pLoopUnit = ::GetPlayerUnit(*pUnitNode);
				pUnitNode = pLoopPlot->nextUnitNode(pUnitNode);

				if (pLoopUnit && pLoopUnit->IsCanAttack() && pLoopUnit->getTeam()==eMyTeam)
				{
					//this check skips air units ...
					if (eDomain==NO_DOMAIN || pLoopUnit->getDomainType() == eDomain)
						result.push_back(pLoopUnit);
				}
			}

		}
	}

	return result;
}

//friendly, enemy power
pair<int,int> CvPlot::GetLocalUnitPower(PlayerTypes ePlayer, int iRange, bool bSameDomain) const
{
	int iFriendlyPower = 0;
	int iEnemyPower = 0;
	CvPlayer& kPlayer = GET_PLAYER(ePlayer);

	iRange = range(iRange, 1, 5);
	for (int i = 0; i < RING_PLOTS[iRange]; i++)
	{
		CvPlot* pTestPlot = iterateRingPlots(this, i);
		if (!pTestPlot || !pTestPlot->isVisible(kPlayer.getTeam()))
			continue;

		//also take into account carriers!
		const IDInfo* pUnitNode = pTestPlot->headUnitNode();
		while(pUnitNode != NULL)
		{
			CvUnit* pLoopUnit = ::GetPlayerUnit(*pUnitNode);
			pUnitNode = pTestPlot->nextUnitNode(pUnitNode);

			if (pLoopUnit && pLoopUnit->IsCanAttack())
			{
				//this check skips air units ...
				if (bSameDomain && pLoopUnit->getDomainType() != getDomain())
					continue;

				if (kPlayer.IsAtWarWith(pLoopUnit->getOwner()) && !pLoopUnit->isInvisible(kPlayer.getTeam(), false))
				{
					iEnemyPower += pLoopUnit->GetPower();
				}
				else if (kPlayer.getTeam() == pLoopUnit->getTeam())
				{
					iFriendlyPower += pLoopUnit->GetPower();
				}
			}
		}
	}

	return make_pair(iFriendlyPower,iEnemyPower);
}

int CvPlot::GetNumEnemyUnitsAdjacent(TeamTypes eMyTeam, DomainTypes eDomain, const CvUnit* pUnitToExclude, bool bConsiderFlanking, TeamTypes eSpecificTeam, bool bIncludeEmbarked) const
{
	int iNumEnemiesAdjacent = 0;

	CvPlot** aPlotsToCheck = GC.getMap().getNeighborsUnchecked(this);
	for(int iCount=0; iCount<NUM_DIRECTION_TYPES; iCount++)
	{
		CvPlot* pLoopPlot = aPlotsToCheck[iCount];
		if(pLoopPlot != NULL && pLoopPlot->isVisible(eMyTeam))
		{
			IDInfo* pUnitNode = pLoopPlot->headUnitNode();

			// Loop through all units on this plot
			while(pUnitNode != NULL)
			{
				CvUnit* pLoopUnit = ::GetPlayerUnit(*pUnitNode);
				pUnitNode = pLoopPlot->nextUnitNode(pUnitNode);

				// No NULL, and no unit we want to exclude
				if(pLoopUnit && pLoopUnit != pUnitToExclude)
				{
					// Must be a combat Unit
					if(pLoopUnit->IsCombatUnit() && (!pLoopUnit->isEmbarked() || bIncludeEmbarked))
					{
						TeamTypes eTheirTeam = pLoopUnit->getTeam();

						// This team which this unit belongs to must be at war with us
						if(GET_TEAM(eTheirTeam).isAtWar(eMyTeam) || eTheirTeam == eSpecificTeam)
						{
							// Must be same domain
							if (pLoopUnit->getDomainType() == eDomain || pLoopUnit->getDomainType() == DOMAIN_HOVER || eDomain == NO_DOMAIN)
							{
								iNumEnemiesAdjacent += bConsiderFlanking ? pLoopUnit->GetFlankPower() : 1;
							}
						}
					}
				}
			}
		}
	}

	return iNumEnemiesAdjacent;
}

int CvPlot::GetNumFriendlyUnitsAdjacent(TeamTypes eMyTeam, DomainTypes eDomain, bool bCountRanged, const CvUnit* pUnitToExclude) const
{
	int iNumFriendliesAdjacent = 0;

	CvPlot** aPlotsToCheck = GC.getMap().getNeighborsUnchecked(this);
	for (int iCount = 0; iCount < NUM_DIRECTION_TYPES; iCount++)
	{
		CvPlot* pLoopPlot = aPlotsToCheck[iCount];
		if (pLoopPlot)
		{
			IDInfo* pUnitNode = pLoopPlot->headUnitNode();

			// Loop through all units on this plot
			while (pUnitNode != NULL)
			{
				CvUnit* pLoopUnit = ::GetPlayerUnit(*pUnitNode);
				pUnitNode = pLoopPlot->nextUnitNode(pUnitNode);

				// Not NULL, not excluded, and must be a non-embarked combat unit on our team
				if (pLoopUnit && pLoopUnit != pUnitToExclude && pLoopUnit->IsCombatUnit() && !pLoopUnit->isEmbarked() && pLoopUnit->getTeam() == eMyTeam)
				{
					// Excluding ranged units?
					if (pLoopUnit->IsCanAttackRanged() && !bCountRanged)
						continue;

					// Domain must match
					if (eDomain == NO_DOMAIN || pLoopUnit->getDomainType() == eDomain || pLoopUnit->getDomainType() == DOMAIN_HOVER)
						iNumFriendliesAdjacent++;
				}
			}
		}
	}

	return iNumFriendliesAdjacent;
}

#if defined(MOD_BALANCE_CORE)
int CvPlot::GetNumSpecificFriendlyUnitCombatsAdjacent(TeamTypes eMyTeam, UnitCombatTypes eUnitCombat, const CvUnit* pUnitToExclude) const
{
	int iNumber = 0;

	CvPlot** aPlotsToCheck = GC.getMap().getNeighborsUnchecked(this);
	for(int iCount=0; iCount<NUM_DIRECTION_TYPES; iCount++)
	{
		CvPlot* pLoopPlot = aPlotsToCheck[iCount];
		if(pLoopPlot != NULL)
		{
			IDInfo* pUnitNode = pLoopPlot->headUnitNode();

			// Loop through all units on this plot
			while(pUnitNode != NULL)
			{
				CvUnit* pLoopUnit = ::GetPlayerUnit(*pUnitNode);
				pUnitNode = pLoopPlot->nextUnitNode(pUnitNode);

				// No NULL, and no unit we want to exclude
				if(pLoopUnit && pLoopUnit != pUnitToExclude)
				{
					// Must be a combat Unit
					if(pLoopUnit->IsCombatUnit() && !pLoopUnit->isEmbarked())
					{
						// Same team?
						if(pLoopUnit->getTeam() == eMyTeam)
						{
							// Must be same unit combat type
							if (pLoopUnit->getUnitCombatType() == eUnitCombat)
							{
								iNumber++;
							}
						}
					}
				}
			}
		}
	}

	return iNumber;
}
#endif

bool CvPlot::IsFriendlyUnitAdjacent(TeamTypes eMyTeam, bool bCombatUnit) const
{
	CvPlot** aPlotsToCheck = GC.getMap().getNeighborsUnchecked(this);
	for(int iCount=0; iCount<NUM_DIRECTION_TYPES; iCount++)
	{
		CvPlot* pLoopPlot = aPlotsToCheck[iCount];
		if(pLoopPlot != NULL)
		{
			IDInfo* pUnitNode = pLoopPlot->headUnitNode();

			while(pUnitNode != NULL)
			{
				CvUnit* pLoopUnit = ::GetPlayerUnit(*pUnitNode);
				pUnitNode = pLoopPlot->nextUnitNode(pUnitNode);

				if(pLoopUnit && pLoopUnit->getTeam() == eMyTeam)
				{
					// Combat Unit?
					if(!bCombatUnit || pLoopUnit->IsCombatUnit())
					{
						return true;
					}
				}
			}
		}
	}

	return false;
}

int CvPlot::GetNumSpecificPlayerUnitsAdjacent(PlayerTypes ePlayer, const CvUnit* pUnitToExclude, const CvUnit* pExampleUnitType, bool bCombatOnly) const
{
	int iNumUnitsAdjacent = 0;
	for(int iI = 0; iI < NUM_DIRECTION_TYPES; iI++)
	{
		CvPlot* pLoopPlot = plotDirection(getX(), getY(), ((DirectionTypes)iI));
		if(pLoopPlot != NULL)
		{
			IDInfo* pUnitNode = pLoopPlot->headUnitNode();

			// Loop through all units on this plot
			while(pUnitNode != NULL)
			{
				CvUnit* pLoopUnit = ::GetPlayerUnit(*pUnitNode);
				pUnitNode = pLoopPlot->nextUnitNode(pUnitNode);

				// No NULL, and no unit we want to exclude
				if(pLoopUnit && pLoopUnit != pUnitToExclude && pLoopUnit->getOwner()==ePlayer)
				{
					// Must be a combat Unit
					if(!bCombatOnly || pLoopUnit->IsCombatUnit())
					{
						if(!pExampleUnitType || pLoopUnit->getUnitType() == pExampleUnitType->getUnitType())
						{
							iNumUnitsAdjacent++;
						}
					}
				}
			}
		}
	}

	return iNumUnitsAdjacent;
}

///-------------------------------------

#if defined(MOD_BALANCE_CORE)
static int GetDefensiveApproachMultiplierTimes100(CivApproachTypes eApproach)
{
	switch (eApproach)
	{
	case CIV_APPROACH_FRIENDLY:
		return 0;
	case CIV_APPROACH_NEUTRAL:
		return 100;
	case CIV_APPROACH_AFRAID:
	case CIV_APPROACH_GUARDED:
		return 200;
	case CIV_APPROACH_DECEPTIVE:
	case CIV_APPROACH_HOSTILE:
	case CIV_APPROACH_WAR:
		return 150;
	default:
		return 0;
	}
}

static int GetDefensiveStrengthMultiplierTimes100(StrengthTypes eStrength)
{
	switch (eStrength)
	{
	case STRENGTH_PATHETIC:
		return 10;
	case STRENGTH_WEAK:
		return 25;
	case STRENGTH_POOR:
		return 50;
	case STRENGTH_AVERAGE:
		return 100;
	case STRENGTH_STRONG:
		return 125;
	case STRENGTH_POWERFUL:
		return 150;
	case STRENGTH_IMMENSE:
		return 200;
	default:
		return 0;
	}
}

static int GetOffensiveApproachMultiplierTimes100(CivApproachTypes eApproach)
{
	switch (eApproach)
	{
	case CIV_APPROACH_FRIENDLY:
	case CIV_APPROACH_AFRAID:
		return 0;
	case CIV_APPROACH_NEUTRAL:
		return 50;
	case CIV_APPROACH_GUARDED:
		return 100;
	case CIV_APPROACH_DECEPTIVE:
		return 150;
	case CIV_APPROACH_HOSTILE:
	case CIV_APPROACH_WAR:
		return 200;
	default:
		return 0;
	}
}

static int GetOffensiveStrengthMultiplierTimes100(StrengthTypes eStrength)
{
	switch (eStrength)
	{
	case STRENGTH_PATHETIC:
		return 200;
	case STRENGTH_WEAK:
		return 150;
	case STRENGTH_POOR:
		return 125;
	case STRENGTH_AVERAGE:
		return 100;
	case STRENGTH_STRONG:
		return 50;
	case STRENGTH_POWERFUL:
		return 25;
	case STRENGTH_IMMENSE:
		return 10;
	default:
		return 0;
	}
}

int CvPlot::GetStrategicValue(PlayerTypes ePlayer) const
{
	TeamTypes eTeam = GET_PLAYER(ePlayer).getTeam();
	if (eTeam == NO_TEAM)
		return 0;

	CvDiplomacyAI* pDiplomacyAI = GET_PLAYER(ePlayer).GetDiplomacyAI();

	// Evaluate based on surrounding plots
	int iNearbyThreat = 0;
	int iNearbyTarget = 0;

	for (int i = RING0_PLOTS; i < RING2_PLOTS; i++)
	{
		CvPlot* pLoopNearbyPlot = iterateRingPlots(this, i);

		if (pLoopNearbyPlot == NULL)
			continue;

		if (!pLoopNearbyPlot->isRevealed(eTeam))
			continue;

		//impassable is uninteresting
		if (!pLoopNearbyPlot->isValidMovePlot(pLoopNearbyPlot->getOwner()))
			continue;

		if (pLoopNearbyPlot->isWater())
			continue;

		if (pLoopNearbyPlot->isOwned() && pLoopNearbyPlot->getOwner() != ePlayer && GET_PLAYER(pLoopNearbyPlot->getOwner()).isMajorCiv())
		{
			CivApproachTypes eApproach = pDiplomacyAI->GetCivApproach(pLoopNearbyPlot->getOwner());
			StrengthTypes eStrength = pDiplomacyAI->GetMilitaryStrengthComparedToUs(pLoopNearbyPlot->getOwner());

			iNearbyThreat = max(iNearbyThreat, GetDefensiveApproachMultiplierTimes100(eApproach) * GetDefensiveStrengthMultiplierTimes100(eStrength) / 100);
			iNearbyTarget = max(iNearbyTarget, GetOffensiveApproachMultiplierTimes100(eApproach) * GetOffensiveStrengthMultiplierTimes100(eStrength) / 100);
		}
	}
	
	// Threatening civs (unfriendly and/or powerful)
	int iStrategicValue = max(iNearbyThreat, iNearbyTarget);

	// range: [0,400]
	return iStrategicValue;
}

int CvPlot::GetDefenseBuildValue(PlayerTypes eOwner, BuildTypes eBuild, ImprovementTypes eImprovement, const SBuilderState& sState) const
{
	CvPlayer& kPlayer = GET_PLAYER(eOwner);
	TeamTypes eTeam = kPlayer.getTeam();
	if(eTeam == NO_TEAM)
		return 0;

	if (eImprovement == NO_IMPROVEMENT)
		return 0;

	CvImprovementEntry* pkImprovementInfo = GC.getImprovementInfo(eImprovement);
	int iImprovementDefenseModifier = pkImprovementInfo->GetDefenseModifier();
	int iImprovementDamage = pkImprovementInfo->GetNearbyEnemyDamage();
	int iCultureBombRadius = pkImprovementInfo->GetCultureBombRadius();

	CvDiplomacyAI* pDiplomacyAI = kPlayer.GetDiplomacyAI();

	// Evaluate based on surrounding plots
	int iMaxAdjacentThreat = 0;

	int iNearestOwnedCityDistance = 0;
	int iAdjacentOtherPlayerLand = 0;
	int iAdjacentOwnedLand = 0;
	int iNearbyDefensiveStructures = 0;

	int iTotalAdjacentDamage = 0;

	// Adjacent tiles
	for (int iRingID = 1; iRingID < 3; iRingID++)
	{
		for (int iI = RING_PLOTS[iRingID - 1]; iI < RING_PLOTS[iRingID]; iI++)
		{
			CvPlot* pAdjacentPlot = iterateRingPlots(getX(), getY(), iI);

			if (pAdjacentPlot == NULL)
				continue;

			//impassable is uninteresting
			if (!pAdjacentPlot->isValidMovePlot(pAdjacentPlot->getOwner(), false))
				continue;

			//coasts are easy avenues of attack
			if (pAdjacentPlot->isWater())
				continue;

			if (pAdjacentPlot->getOwner() == eOwner)
			{
				if (iRingID == 1)
					iAdjacentOwnedLand++;

				// Avoid building next to cities
				if (pAdjacentPlot->isCity())
					iNearestOwnedCityDistance = iRingID;

				if (iImprovementDamage > 0 && iRingID == 1)
				{
					int iMaxDamageAlreadyInPlot = 0;
					for (int iJ = 0; iJ < NUM_DIRECTION_TYPES; iJ++)
					{
						CvPlot* pLoopAdjacentAdjacentPlot = plotDirection(pAdjacentPlot->getX(), pAdjacentPlot->getY(), ((DirectionTypes)iJ));

						if (!pLoopAdjacentAdjacentPlot)
							continue;

						if (pLoopAdjacentAdjacentPlot == this)
							continue;

						if (pLoopAdjacentAdjacentPlot->getOwner() != eOwner)
							continue;

						map<int, pair<BuildTypes, ImprovementTypes>>::const_iterator it = sState.mChangedPlotImprovements.find(pLoopAdjacentAdjacentPlot->GetPlotIndex());
						ImprovementTypes eAdjacentImprovement = it != sState.mChangedPlotImprovements.end() ? it->second.second : NO_IMPROVEMENT;

						// If we are not planning on building an improvement here, use the one that exists already
						if (eAdjacentImprovement == NO_IMPROVEMENT && !pLoopAdjacentAdjacentPlot->IsImprovementPillaged())
							eAdjacentImprovement = pLoopAdjacentAdjacentPlot->getImprovementType();

						if (eAdjacentImprovement == NO_IMPROVEMENT)
							continue;

						CvImprovementEntry* pkAdjacentImprovementInfo = GC.getImprovementInfo(eAdjacentImprovement);
						if (pkAdjacentImprovementInfo)
						{
							int iImprovementDamage = pkAdjacentImprovementInfo->GetNearbyEnemyDamage();
							if (iImprovementDamage > iMaxDamageAlreadyInPlot)
								iMaxDamageAlreadyInPlot = iImprovementDamage;
						}
					}

					if (iImprovementDamage > iMaxDamageAlreadyInPlot)
						iTotalAdjacentDamage += iImprovementDamage - iMaxDamageAlreadyInPlot;
				}

				map<int, pair<BuildTypes, ImprovementTypes>>::const_iterator it = sState.mChangedPlotImprovements.find(pAdjacentPlot->GetPlotIndex());
				ImprovementTypes eAdjacentImprovement = it != sState.mChangedPlotImprovements.end() ? it->second.second : NO_IMPROVEMENT;

				// If we are not planning on building an improvement here, use the one that exists already
				if (eAdjacentImprovement == NO_IMPROVEMENT && !pAdjacentPlot->IsImprovementPillaged())
					eAdjacentImprovement = pAdjacentPlot->getImprovementType();

				if (eAdjacentImprovement != NO_IMPROVEMENT)
				{
					CvImprovementEntry* pkAdjacentImprovementInfo = GC.getImprovementInfo(eAdjacentImprovement);
					if (pkAdjacentImprovementInfo->GetDefenseModifier() > 0)
						iNearbyDefensiveStructures++;

				}
			}
			else if (pAdjacentPlot->isOwned() && pAdjacentPlot->getTeam() != eTeam && GET_PLAYER(pAdjacentPlot->getOwner()).isMajorCiv())
			{
				iAdjacentOtherPlayerLand++;

				CivApproachTypes eApproach = pDiplomacyAI->GetCivApproach(pAdjacentPlot->getOwner());
				StrengthTypes eStrength = pDiplomacyAI->GetMilitaryStrengthComparedToUs(pAdjacentPlot->getOwner());

				iMaxAdjacentThreat = max(iMaxAdjacentThreat, GetDefensiveApproachMultiplierTimes100(eApproach) * GetDefensiveStrengthMultiplierTimes100(eStrength) / 100);

				if (iCultureBombRadius >= iRingID && eBuild != NO_BUILD && !pAdjacentPlot->isCity() && !pAdjacentPlot->IsStealBlockedByImprovement())
					iAdjacentOwnedLand++;
			}
		}
	}

	// No defensive utility from building here
	if (iMaxAdjacentThreat == 0 || iAdjacentOtherPlayerLand<3)
		return 0;

	bool bIgnoreFeature = eBuild != NO_BUILD && getFeatureType() != NO_FEATURE && GC.getBuildInfo(eBuild)->isFeatureRemove(getFeatureType());

	int iDefenseModifier = defenseModifier(eTeam, true, bIgnoreFeature) + iImprovementDefenseModifier;

	// range of iMaxAdjacentThreat is [0,400]
	// max value for a fort (50 defense, 0 damage): 400
	// max value for a citadel (100 defense, 30 damage): 1520
	int iDefensiveValueTimes100 = iMaxAdjacentThreat * (iDefenseModifier * 2 + iTotalAdjacentDamage / 2) / 100;

	//Avoid fort spam
	iDefensiveValueTimes100 -= 50 * iNearbyDefensiveStructures;

	//Avoid building next to cities
	if (iNearestOwnedCityDistance > 0)
		iDefensiveValueTimes100 -= 150 * (3 - iNearestOwnedCityDistance);

	if (iDefensiveValueTimes100 <= 0)
		return 0;

	// Bonus for plots that are not too exposed (range=[0,420])
	int iDefensibilityTimes100 = iAdjacentOwnedLand * 70;

	// max value for a fort (50 defense, 0 damage): 1680
	// max value for a citadel (100 defense, 30 damage): 4872
	return (iDefensiveValueTimes100 * iDefensibilityTimes100) / 100;
}

#endif

FDataStream& operator<<(FDataStream& saveTo, const CvPlot* const& readFrom)
{
	int idx = -1;
	if (readFrom != NULL)
	{
		idx = readFrom->GetPlotIndex();
		ASSERT_DEBUG(GC.getMap().plotByIndex(idx) == readFrom, "Saving plot pointer that is not member of map");
	}
	saveTo << idx;
	return saveTo;
}
FDataStream& operator<<(FDataStream& saveTo, CvPlot* const& readFrom)
{
	return saveTo << const_cast<const CvPlot* const&>(readFrom);
}
FDataStream& operator>>(FDataStream& loadFrom, const CvPlot*& writeTo)
{
	int idx = 0;
	loadFrom >> idx;
	if (idx != -1)
	{
		writeTo = GC.getMap().plotByIndex(idx);
		ASSERT_DEBUG(writeTo != NULL, "Read plot pointer index that is out of bounds");
	}
	else
	{
		writeTo = NULL;
	}
	return loadFrom;
}
FDataStream& operator>>(FDataStream& loadFrom, CvPlot*& writeTo)
{
	return loadFrom >> const_cast<const CvPlot*&>(writeTo);
}

template<typename PlotWithScore, typename Visitor>
void SPlotWithScore::Serialize(PlotWithScore& plotWithScore, Visitor& visitor)
{
	visitor(plotWithScore.pPlot);
	visitor(plotWithScore.score);
}

FDataStream& operator<<(FDataStream& saveTo, const SPlotWithScore& readFrom)
{
	CvStreamSaveVisitor serialVisitor(saveTo);
	SPlotWithScore::Serialize(readFrom, serialVisitor);
	return saveTo;
}
FDataStream& operator>>(FDataStream& loadFrom, SPlotWithScore& writeTo)
{
	CvStreamLoadVisitor serialVisitor(loadFrom);
	SPlotWithScore::Serialize(writeTo, serialVisitor);
	return loadFrom;
}

template<typename PlotWithTwoScoresL2, typename Visitor>
void SPlotWithTwoScoresL2::Serialize(PlotWithTwoScoresL2& plotWithTwoScoresL2, Visitor& visitor)
{
	visitor(plotWithTwoScoresL2.pPlot);
	visitor(plotWithTwoScoresL2.score1);
	visitor(plotWithTwoScoresL2.score2);
}

FDataStream& operator<<(FDataStream& saveTo, const SPlotWithTwoScoresL2& readFrom)
{
	CvStreamSaveVisitor serialVisitor(saveTo);
	SPlotWithTwoScoresL2::Serialize(readFrom, serialVisitor);
	return saveTo;
}
FDataStream& operator>>(FDataStream& loadFrom, SPlotWithTwoScoresL2& writeTo)
{
	CvStreamLoadVisitor serialVisitor(loadFrom);
	SPlotWithTwoScoresL2::Serialize(writeTo, serialVisitor);
	return loadFrom;
}

CvSeeder CvPlot::GetPseudoRandomSeed() const
{
	return CvSeeder(static_cast<uint>(getX()) * 17 + static_cast<uint>(getY()) * 23);
}
