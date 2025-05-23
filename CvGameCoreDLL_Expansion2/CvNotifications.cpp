/*	-------------------------------------------------------------------------------------------------------
	© 1991-2012 Take-Two Interactive Software and its subsidiaries.  Developed by Firaxis Games.  
	Sid Meier's Civilization V, Civ, Civilization, 2K Games, Firaxis Games, Take-Two Interactive Software 
	and their respective logos are all trademarks of Take-Two interactive Software, Inc.  
	All other marks and trademarks are the property of their respective owners.  
	All rights reserved. 
	------------------------------------------------------------------------------------------------------- */
#include "CvGameCoreDLLPCH.h"
#include "CvNotifications.h"
#include "CvPlayer.h"
#include "CvCity.h"
#include "CvDiplomacyAI.h"
#include "FStlContainerSerialization.h"
#include "ICvDLLUserInterface.h"
#include "CvEnumSerialization.h"
#include "CvDllPlot.h"
#if defined(MOD_ACTIVE_DIPLOMACY)
#include "CvDiplomacyRequests.h"
#endif

// Include this after all other headers.
#include "LintFree.h"

///
template<typename NotificationT, typename Visitor>
void CvNotifications::Notification::Serialize(NotificationT& notification, Visitor& visitor)
{
	const bool bLoading = visitor.isLoading();

	visitor(notification.m_eNotificationType);
	visitor(notification.m_strMessage);
	visitor(notification.m_strSummary);
	visitor(notification.m_iX);
	visitor(notification.m_iY);
	visitor(notification.m_iGameDataIndex);
	visitor(notification.m_iExtraGameData);
	visitor(notification.m_iTurn);
	visitor(notification.m_iLookupIndex);
	visitor(notification.m_bDismissed);
	visitor(notification.m_ePlayerID);
	if (bLoading)
	{
		visitor.loadAssign(notification.m_bNeedsBroadcast, true); // all loads should re-broadcast their events
		visitor.loadAssign(notification.m_bWaitExtraTurn, false); // not saving this
	}
}

/// Serialization read
FDataStream& operator>>(FDataStream& loadFrom, CvNotifications::Notification& writeTo)
{
	CvStreamLoadVisitor serialVisitor(loadFrom);
	CvNotifications::Notification::Serialize(writeTo, serialVisitor);
	return loadFrom;
}

/// Serialization write
FDataStream& operator<<(FDataStream& saveTo, const CvNotifications::Notification& readFrom)
{
	CvStreamSaveVisitor serialVisitor(saveTo);
	CvNotifications::Notification::Serialize(readFrom, serialVisitor);
	return saveTo;
}

void CvNotifications::Notification::Clear()
{
	m_eNotificationType = NO_NOTIFICATION_TYPE;
	m_strMessage = "";
	m_strSummary = "";
	m_iX = -1;
	m_iY = -1;
	m_iGameDataIndex = -1;
	m_iTurn = -1;
	m_iLookupIndex = -1;
	m_bNeedsBroadcast = false;
	m_bDismissed = false;
	m_bWaitExtraTurn = false;
}

/// Constructor
CvNotifications::CvNotifications(void)
{
	Uninit();
}

/// Destructor
CvNotifications::~CvNotifications(void)
{
	Uninit();
}

/// Init
void CvNotifications::Init(PlayerTypes ePlayer)
{
	Uninit();
	m_ePlayer = ePlayer;

	for(uint ui = 0; ui < MaxNotifications; ui++)
	{
		m_aNotifications[ui].Clear();
	}
	m_iNotificationsBeginIndex = 0;
	m_iNotificationsEndIndex = 0;
}

/// Uninit
void CvNotifications::Uninit(void)
{
	m_ePlayer = NO_PLAYER;
	m_iCurrentLookupIndex = 0;

	m_iNotificationsBeginIndex = -1;
	m_iNotificationsEndIndex = -1;
}

///
template<typename Notifications, typename Visitor>
void CvNotifications::Serialize(Notifications& notifications, Visitor& visitor)
{
	visitor(notifications.m_ePlayer);
	visitor(notifications.m_iCurrentLookupIndex);
	visitor(notifications.m_iNotificationsBeginIndex);
	visitor(notifications.m_iNotificationsEndIndex);
	visitor(notifications.m_aNotifications);
}

/// Serialization read
void CvNotifications::Read(FDataStream& kStream)
{
	CvStreamLoadVisitor serialVisitor(kStream);
	Serialize(*this, serialVisitor);
}

/// Serialization write
void CvNotifications::Write(FDataStream& kStream) const
{
	CvStreamSaveVisitor serialVisitor(kStream);
	Serialize(*this, serialVisitor);
}

FDataStream& operator>>(FDataStream& stream, CvNotifications& notifications)
{
	notifications.Read(stream);
	return stream;
}
FDataStream& operator<<(FDataStream& stream, const CvNotifications& notifications)
{
	notifications.Write(stream);
	return stream;
}

/// Update - called from within CvPlayer
void CvNotifications::Update(void)
{
	int iIndex = m_iNotificationsBeginIndex;
	while(iIndex != m_iNotificationsEndIndex)
	{
		Notification& kNotification = m_aNotifications[iIndex];

		if(!kNotification.m_bDismissed)
		{
			if(IsNotificationExpired(iIndex))
			{
				Dismiss(kNotification.m_iLookupIndex, /*bUserInvoked*/ false);
				//GC.GetEngineUserInterface()->RemoveNotification(kNotification.m_iLookupIndex);
				//kNotification.m_bDismissed = true;
			}
			else
			{
				if(kNotification.m_bNeedsBroadcast)
				{
					// If the notification is for the 'active' player and that active player actually has his turn active or its not hotseat, then show the notification, else wait.
					// The 'active' player is only set to a human and during the AI turn, the 'active' player is the last human to do their turn.
					if(kNotification.m_ePlayerID == GC.getGame().getActivePlayer())
					{
						if(!CvPreGame::isHotSeatGame() || GET_PLAYER(GC.getGame().getActivePlayer()).isTurnActive())
						{
							GC.GetEngineUserInterface()->AddNotification(kNotification.m_iLookupIndex, kNotification.m_eNotificationType, kNotification.m_strMessage.c_str(), kNotification.m_strSummary.c_str(), kNotification.m_iGameDataIndex, kNotification.m_iExtraGameData, m_ePlayer, kNotification.m_iX, kNotification.m_iY);
							kNotification.m_bNeedsBroadcast = false;
						}
					}
					else if(gDLL->IsPlayerConnected(kNotification.m_ePlayerID))
					{//We consider a notification to have been broadcast if the notification 
						//is for a remote player who is network connected to the game.
						kNotification.m_bNeedsBroadcast = false;
					}
				}
			}
		}

		iIndex++;

		if(iIndex >= int(MaxNotifications))
		{
			iIndex = 0;
		}
	}
}

// EndOfTurnCleanup - called from within CvPlayer at the end of turn
void CvNotifications::EndOfTurnCleanup(void)
{
	int iIndex = m_iNotificationsBeginIndex;
	while(iIndex != m_iNotificationsEndIndex)
	{
		if(!m_aNotifications[iIndex].m_bDismissed)
		{
			if(IsNotificationEndOfTurnExpired(iIndex))
			{
				if (m_aNotifications[iIndex].m_bWaitExtraTurn)
					m_aNotifications[iIndex].m_bWaitExtraTurn = false;
				else
					Dismiss(m_aNotifications[iIndex].m_iLookupIndex, /*bUserInvoked*/ false);
			}
		}

		iIndex++;

		if(iIndex >= int(MaxNotifications))
		{
			iIndex = 0;
		}
	}
}

/// Adds a new notification to the list
int CvNotifications::AddByName(const char* pszNotificationName, const char* strMessage, const char* strSummary, int iX, int iY, int iGameDataIndex, int iExtraGameData)
{
	if (pszNotificationName && pszNotificationName[0] != 0)
	{
		return Add((NotificationTypes) FStringHash(pszNotificationName), strMessage, strSummary, iX, iY, iGameDataIndex, iExtraGameData);
	}
	return -1;
}

/// Adds a new notification to the list
int CvNotifications::Add(NotificationTypes eNotificationType, const char* strMessage, const char* strSummary, int iX, int iY, int iGameDataIndex, int iExtraGameData)
{
	// if the player is not human, do not record
	if(!GET_PLAYER(m_ePlayer).isHuman())
	{
		return -1;
	}

	// If we're in debug mode, don't do anything
	if(GC.getGame().isDebugMode())
		return -1;

	Notification newNotification;
	newNotification.Clear();
	newNotification.m_ePlayerID = m_ePlayer;
	newNotification.m_eNotificationType = eNotificationType;
	newNotification.m_strMessage = strMessage;
	newNotification.m_strSummary = strSummary;
	newNotification.m_iX = iX;
	newNotification.m_iY = iY;
	newNotification.m_iGameDataIndex = iGameDataIndex;
	newNotification.m_iExtraGameData = iExtraGameData;
	newNotification.m_iTurn = GC.getGame().getGameTurn();
	newNotification.m_iLookupIndex = m_iCurrentLookupIndex;
	newNotification.m_bNeedsBroadcast = true;
	newNotification.m_bDismissed = false;
	newNotification.m_bWaitExtraTurn = false;

#if defined(MOD_ACTIVE_DIPLOMACY)
	// Hack to get human-human deals showing with the diplomacy icon with proper civ background - otherwise they have the question mark background.
	// Deals from AI are coming through with the sending player ID in m_iGameDataIndex, but humans have it in iX (and m_iGameDataIndex is -1). I can't see iX myself where I need it in the Lua so I am hacking it across here.
	// There is probably something else afoot but this seems to work for the minute and appears safe.
	if (MOD_ACTIVE_DIPLOMACY && GC.getGame().isReallyNetworkMultiPlayer() && newNotification.m_eNotificationType == NOTIFICATION_PLAYER_DEAL_RECEIVED) {
		if (newNotification.m_iGameDataIndex == -1) {
			newNotification.m_iGameDataIndex = iX;
		}
	}
#endif

	// Is this notification being added during the player's auto-moves and will it expire at the end of the turn?
	// If so, set a flag so the notification will stick around for an extra turn.
	if (GET_PLAYER(m_ePlayer).isTurnActive() && GET_PLAYER(m_ePlayer).isAutoMoves() && IsNotificationTypeEndOfTurnExpired(eNotificationType))
		newNotification.m_bWaitExtraTurn = true;

	if(IsNotificationRedundant(newNotification))
	{
		// redundant notification
		return -1;
	}

	if(IsArrayFull())
	{
		RemoveOldestNotification();
	}

	m_aNotifications[m_iNotificationsEndIndex] = newNotification;

	if(GC.getGame().isFinalInitialized())
	{
		// If the notification is for the 'active' player and that active player actually has his turn active or its not hotseat, then show the notification, else wait
		// The 'active' player is only set to a human and during the AI turn, the 'active' player is the last human to do their turn.
		if(newNotification.m_ePlayerID == GC.getGame().getActivePlayer() && (!CvPreGame::isHotSeatGame() || GET_PLAYER(GC.getGame().getActivePlayer()).isTurnActive()))
		{
			GC.GetEngineUserInterface()->AddNotification(newNotification.m_iLookupIndex, newNotification.m_eNotificationType, newNotification.m_strMessage.c_str(), newNotification.m_strSummary.c_str(), newNotification.m_iGameDataIndex, newNotification.m_iExtraGameData, m_ePlayer, iX, iY);

#if defined(MOD_UI_CITY_EXPANSION)
			// Don't show effect with production notification or city tile acquisition
			bool bShow = (eNotificationType != NOTIFICATION_PRODUCTION);
			
			if (MOD_UI_CITY_EXPANSION) {
				bShow = bShow && (eNotificationType != NOTIFICATION_CITY_TILE);
			}
			
			if(bShow)
#else
			// Don't show effect with production notification
			if(eNotificationType != NOTIFICATION_PRODUCTION)
#endif
			{
				CvPlot* pPlot = GC.getMap().plot(iX, iY);
				if(pPlot != NULL)
				{
					CvInterfacePtr<ICvPlot1> pDllPlot(new CvDllPlot(pPlot));
					gDLL->GameplayDoFX(pDllPlot.get());
				}
				else
				{
					gDLL->GameplayDoFX(NULL);
				}


			}
			m_aNotifications[m_iNotificationsEndIndex].m_bNeedsBroadcast = false;
		}

		gDLL->GameplayMinimapNotification(iX, iY, m_iCurrentLookupIndex+1);	// The index is used to uniquely identify each flashing dot on the minimap. We're adding 1 since the selected unit is always 0. It ain't pretty, but it'll work
	}

	IncrementEndIndex();

	m_iCurrentLookupIndex++;

	return newNotification.m_iLookupIndex;
}

//	---------------------------------------------------------------------------
void CvNotifications::Activate(int iLookupIndex)
{
	int iIndex = m_iNotificationsBeginIndex;
	while(iIndex != m_iNotificationsEndIndex)
	{
		if(m_aNotifications[iIndex].m_iLookupIndex == iLookupIndex)
		{
			Activate(m_aNotifications[iIndex]);
			break;
		}
		iIndex++;
		if(iIndex >= int(MaxNotifications))
		{
			iIndex = 0;
		}
	}
}

//	---------------------------------------------------------------------------
void CvNotifications::Dismiss(int iLookupIndex, bool bUserInvoked)
{
	int iIndex = m_iNotificationsBeginIndex;
	while(iIndex != m_iNotificationsEndIndex)
	{
		if(m_aNotifications[iIndex].m_iLookupIndex == iLookupIndex)
		{
			m_aNotifications[iIndex].m_bDismissed = true;
			GC.GetEngineUserInterface()->RemoveNotification(m_aNotifications[iIndex].m_iLookupIndex, m_ePlayer);

			switch(m_aNotifications[iIndex].m_eNotificationType)
			{
			case NOTIFICATION_POLICY:
			{
				if(m_ePlayer == GC.getGame().getActivePlayer() && bUserInvoked)
				{
					GC.GetEngineUserInterface()->SetPolicyNotificationSeen(true);
				}
			}
			default:
				break;
			}

			break;
		}

		iIndex++;
		if(iIndex >= int(MaxNotifications))
		{
			iIndex = 0;
		}
	}
}

//	---------------------------------------------------------------------------
bool CvNotifications::MayUserDismiss(int iLookupIndex)
{
	int iIndex = m_iNotificationsBeginIndex;
	while(iIndex != m_iNotificationsEndIndex)
	{
		if(m_aNotifications[iIndex].m_iLookupIndex == iLookupIndex)
		{
			switch(m_aNotifications[iIndex].m_eNotificationType)
			{
			case NOTIFICATION_DIPLO_VOTE:
			case NOTIFICATION_PRODUCTION:
			case NOTIFICATION_TECH:
			case NOTIFICATION_FREE_TECH:
			case NOTIFICATION_FREE_POLICY:
			case NOTIFICATION_FREE_GREAT_PERSON:
			case NOTIFICATION_FOUND_PANTHEON:
			case NOTIFICATION_FOUND_RELIGION:
			case NOTIFICATION_ENHANCE_RELIGION:
			case NOTIFICATION_SPY_STOLE_TECH:
			case NOTIFICATION_MAYA_LONG_COUNT:
			case NOTIFICATION_FAITH_GREAT_PERSON:
			case NOTIFICATION_ADD_REFORMATION_BELIEF:
			case NOTIFICATION_LEAGUE_CALL_FOR_PROPOSALS:
			case NOTIFICATION_CHOOSE_ARCHAEOLOGY:
			case NOTIFICATION_LEAGUE_CALL_FOR_VOTES:
			case NOTIFICATION_CHOOSE_IDEOLOGY:
			case NOTIFICATION_PLAYER_DEAL_RECEIVED:
				return false;
				break;

			// We'll let the user right click the End Turn button to ignore this, as the notification will be sent again next turn 
			case NOTIFICATION_CITY_TILE:
				// We could just let this drop through as the default is true anyway
				return true;
				break;

			case -364200720:
			case 826076831:
			case 419811917:
			case -1608954742:
				return false;
				break;

			case NOTIFICATION_POLICY:
				return GC.getGame().isOption(GAMEOPTION_POLICY_SAVING);

			default:
				return true;
				break;
			}
		}

		iIndex++;
		if(iIndex >= int(MaxNotifications))
		{
			iIndex = 0;
		}
	}

	return false;
}

//	---------------------------------------------------------------------------
void CvNotifications::Rebroadcast(void)
{
	int iIndex = m_iNotificationsBeginIndex;
	while(iIndex != m_iNotificationsEndIndex)
	{
		if(!m_aNotifications[iIndex].m_bDismissed)
		{
			m_aNotifications[iIndex].m_bNeedsBroadcast = true;
		}

		iIndex++;
		if(iIndex >= int(MaxNotifications))
		{
			iIndex = 0;
		}
	}
}
//	---------------------------------------------------------------------------
bool CvNotifications::GetEndTurnBlockedType(EndTurnBlockingTypes& eBlockingType, int& iNotificationIndex)
{
	eBlockingType = NO_ENDTURN_BLOCKING_TYPE;
	iNotificationIndex = -1;
	
	int iIndex = m_iNotificationsBeginIndex;
	while(iIndex != m_iNotificationsEndIndex)
	{
		if(!m_aNotifications[iIndex].m_bDismissed)
		{
			switch(m_aNotifications[iIndex].m_eNotificationType)
			{
			case NOTIFICATION_CITY_RANGE_ATTACK:
			{
				bool automaticallyEndTurns = GC.getGame().isGameMultiPlayer() ? GC.GetEngineUserInterface()->IsMPAutoEndTurnEnabled() : GC.GetEngineUserInterface()->IsSPAutoEndTurnEnabled();
				if(automaticallyEndTurns)
				{//City range attacks only block turns if the player is using auto end turn.
					eBlockingType = ENDTURN_BLOCKING_CITY_RANGE_ATTACK;
					iNotificationIndex = m_aNotifications[iIndex].m_iLookupIndex;
					return true;
				}
				break;
			}

			case NOTIFICATION_DIPLO_VOTE:
				eBlockingType = ENDTURN_BLOCKING_DIPLO_VOTE;
				iNotificationIndex = m_aNotifications[iIndex].m_iLookupIndex;
				return true;
				break;

			case NOTIFICATION_PRODUCTION:
				eBlockingType = ENDTURN_BLOCKING_PRODUCTION;
				iNotificationIndex = m_aNotifications[iIndex].m_iLookupIndex;
				return true;
				break;

#if defined(MOD_UI_CITY_EXPANSION)
			case NOTIFICATION_CITY_TILE:
				if (MOD_UI_CITY_EXPANSION) 
				{
					eBlockingType = ENDTURN_BLOCKING_CITY_TILE;
					iNotificationIndex = m_aNotifications[iIndex].m_iLookupIndex;
					return true;
				}
				break;
#endif

			case NOTIFICATION_POLICY:
				eBlockingType = ENDTURN_BLOCKING_POLICY;
				iNotificationIndex = m_aNotifications[iIndex].m_iLookupIndex;
				return true;
				break;

			case NOTIFICATION_FREE_POLICY:
				eBlockingType = ENDTURN_BLOCKING_FREE_POLICY;
				iNotificationIndex = m_aNotifications[iIndex].m_iLookupIndex;
				return true;
				break;

			case NOTIFICATION_TECH:
				eBlockingType = ENDTURN_BLOCKING_RESEARCH;
				iNotificationIndex = m_aNotifications[iIndex].m_iLookupIndex;
				return true;
				break;

			case NOTIFICATION_FREE_TECH:
				eBlockingType = ENDTURN_BLOCKING_FREE_TECH;
				iNotificationIndex = m_aNotifications[iIndex].m_iLookupIndex;
				return true;
				break;

			case NOTIFICATION_FREE_GREAT_PERSON:
				eBlockingType = ENDTURN_BLOCKING_FREE_ITEMS;
				iNotificationIndex = m_aNotifications[iIndex].m_iLookupIndex;
				return true;
				break;

			case NOTIFICATION_FOUND_PANTHEON:
				eBlockingType = ENDTURN_BLOCKING_FOUND_PANTHEON;
				iNotificationIndex = m_aNotifications[iIndex].m_iLookupIndex;
				return true;
				break;

			case NOTIFICATION_FOUND_RELIGION:
				eBlockingType = ENDTURN_BLOCKING_FOUND_RELIGION;
				iNotificationIndex = m_aNotifications[iIndex].m_iLookupIndex;
				return true;
				break;

			case NOTIFICATION_ENHANCE_RELIGION:
				eBlockingType = ENDTURN_BLOCKING_ENHANCE_RELIGION;
				iNotificationIndex = m_aNotifications[iIndex].m_iLookupIndex;
				return true;
				break;

			case NOTIFICATION_SPY_STOLE_TECH:
				eBlockingType = ENDTURN_BLOCKING_STEAL_TECH;
				iNotificationIndex = m_aNotifications[iIndex].m_iLookupIndex;
				return true;
				break;

			case NOTIFICATION_MAYA_LONG_COUNT:
				eBlockingType = ENDTURN_BLOCKING_MAYA_LONG_COUNT;
				iNotificationIndex = m_aNotifications[iIndex].m_iLookupIndex;
				return true;
				break;

			case NOTIFICATION_FAITH_GREAT_PERSON:
				eBlockingType = ENDTURN_BLOCKING_FAITH_GREAT_PERSON;
				iNotificationIndex = m_aNotifications[iIndex].m_iLookupIndex;
				return true;
				break;

			case NOTIFICATION_ADD_REFORMATION_BELIEF:
				eBlockingType = ENDTURN_BLOCKING_ADD_REFORMATION_BELIEF;
				iNotificationIndex = m_aNotifications[iIndex].m_iLookupIndex;
				return true;
				break;

			case NOTIFICATION_LEAGUE_CALL_FOR_PROPOSALS:
				eBlockingType = ENDTURN_BLOCKING_LEAGUE_CALL_FOR_PROPOSALS;
				iNotificationIndex = m_aNotifications[iIndex].m_iLookupIndex;
				return true;
				break;

			case NOTIFICATION_CHOOSE_ARCHAEOLOGY:
				eBlockingType = ENDTURN_BLOCKING_CHOOSE_ARCHAEOLOGY;
				iNotificationIndex = m_aNotifications[iIndex].m_iLookupIndex;
				return true;
				break;

			case NOTIFICATION_LEAGUE_CALL_FOR_VOTES:
				eBlockingType = ENDTURN_BLOCKING_LEAGUE_CALL_FOR_VOTES;
				iNotificationIndex = m_aNotifications[iIndex].m_iLookupIndex;
				return true;
				break;

			case NOTIFICATION_CHOOSE_IDEOLOGY:
				eBlockingType = ENDTURN_BLOCKING_CHOOSE_IDEOLOGY;
				iNotificationIndex = m_aNotifications[iIndex].m_iLookupIndex;
				return true;
				break;

#if defined(MOD_BALANCE_CORE)
			case NOTIFICATION_PLAYER_DEAL_RECEIVED:
				eBlockingType = ENDTURN_BLOCKING_PENDING_DEAL;
				iNotificationIndex = m_aNotifications[iIndex].m_iLookupIndex;
				return true;
				break;

			case 826076831:
			case 419811917:
			case -1608954742:
				eBlockingType = ENDTURN_BLOCKING_EVENT_CHOICE;
				iNotificationIndex = m_aNotifications[iIndex].m_iLookupIndex;
				return true;
				break;

			case -364200720:
				eBlockingType = ENDTURN_BLOCKING_CHOOSE_CITY_FATE;
				iNotificationIndex = m_aNotifications[iIndex].m_iLookupIndex;
				return true;
				break;
#endif

			default:
				// these notifications don't block, so don't return a blocking type
				break;
			}
		}

		iIndex++;
		if(iIndex >= int(MaxNotifications))
		{
			iIndex = 0;
		}
	}

	return false;
}

//	---------------------------------------------------------------------------
int CvNotifications::GetNumNotifications(void) const
{
	if(m_iNotificationsEndIndex >= m_iNotificationsBeginIndex)
	{
		int iValue = m_iNotificationsEndIndex - m_iNotificationsBeginIndex;
		return iValue;
	}

	int iValue = (int(MaxNotifications) - m_iNotificationsBeginIndex) + m_iNotificationsEndIndex;
	return iValue;
}

CvString CvNotifications::GetNotificationStr(int iZeroBasedIndex)  // ignores the begin/end values
{
	int iRealIndex = (m_iNotificationsBeginIndex + iZeroBasedIndex) % int(MaxNotifications);
	return m_aNotifications[iRealIndex].m_strMessage;
}

CvString CvNotifications::GetNotificationSummary(int iZeroBasedIndex)
{
	int iRealIndex = (m_iNotificationsBeginIndex + iZeroBasedIndex) % int(MaxNotifications);
	return m_aNotifications[iRealIndex].m_strSummary;
}


int CvNotifications::GetNotificationID(int iZeroBasedIndex)  // ignores begin/end values
{
	int iRealIndex = (m_iNotificationsBeginIndex + iZeroBasedIndex) % int(MaxNotifications);
	return m_aNotifications[iRealIndex].m_iLookupIndex;
}

int CvNotifications::GetNotificationTurn(int iZeroBasedIndex)
{
	int iRealIndex = (m_iNotificationsBeginIndex + iZeroBasedIndex) % int(MaxNotifications);
	return m_aNotifications[iRealIndex].m_iTurn;
}

bool CvNotifications::IsNotificationDismissed(int iZeroBasedIndex)
{
	int iRealIndex = (m_iNotificationsBeginIndex + iZeroBasedIndex) % int(MaxNotifications);
	return m_aNotifications[iRealIndex].m_bDismissed;
}


void CvNotifications::Activate(Notification& notification)
{
	GC.GetEngineUserInterface()->ActivateNotification(notification.m_iLookupIndex, notification.m_eNotificationType, notification.m_strMessage, notification.m_iX, notification.m_iY, notification.m_iGameDataIndex, notification.m_iExtraGameData, m_ePlayer);

	gDLL->GameplayMinimapNotification(notification.m_iX, notification.m_iY, notification.m_iLookupIndex+1);	// The index is used to uniquely identify each flashing dot on the minimap. We're adding 1 since the selected unit is always 0. It ain't pretty, but it'll work

	switch(notification.m_eNotificationType)
	{
	case NOTIFICATION_WONDER_COMPLETED_ACTIVE_PLAYER:
	{
		CvPopupInfo kPopup(BUTTONPOPUP_WONDER_COMPLETED_ACTIVE_PLAYER, notification.m_iGameDataIndex, notification.m_iExtraGameData, notification.m_iX, notification.m_iY);
		GC.GetEngineUserInterface()->AddPopup(kPopup);
	}
	break;
	case NOTIFICATION_GREAT_WORK_COMPLETED_ACTIVE_PLAYER:
		{
			CvPopupInfo kPopup(BUTTONPOPUP_GREAT_WORK_COMPLETED_ACTIVE_PLAYER, notification.m_iGameDataIndex, notification.m_iExtraGameData, notification.m_iX, notification.m_iY);
			GC.GetEngineUserInterface()->AddPopup(kPopup);
		}
		break;
	case NOTIFICATION_BUY_TILE:
	{
		// Jon say - do like Sid would!
		CvCity* pCity = GET_PLAYER(m_ePlayer).getCapitalCity();
		if(pCity)
		{
			CvInterfacePtr<ICvPlot1> pDllPlot = GC.WrapPlotPointer(pCity->plot());
			GC.GetEngineUserInterface()->lookAt(pDllPlot.get(), CAMERALOOKAT_NORMAL);
		}
	}
	break;
	case NOTIFICATION_TECH:
	case NOTIFICATION_FREE_TECH:
	{
		CvPopupInfo kPopup(BUTTONPOPUP_CHOOSETECH, m_ePlayer, notification.m_iGameDataIndex, notification.m_iExtraGameData);
		strcpy_s(kPopup.szText, notification.m_strMessage);
		GC.GetEngineUserInterface()->AddPopup(kPopup);
	}
	break;
	case NOTIFICATION_TECH_AWARD:
	{
		if(notification.m_iExtraGameData != -1)
		{
			CvPopupInfo kPopup(BUTTONPOPUP_TECH_AWARD, m_ePlayer, notification.m_iGameDataIndex, notification.m_iExtraGameData);
			strcpy_s(kPopup.szText, notification.m_strMessage);
			GC.GetEngineUserInterface()->AddPopup(kPopup);
		}
	}
	break;
	case NOTIFICATION_POLICY:
	case NOTIFICATION_FREE_POLICY:
	{
		CvPopupInfo kPopup(BUTTONPOPUP_CHOOSEPOLICY, m_ePlayer);
		GC.GetEngineUserInterface()->AddPopup(kPopup);
	}
	break;
	case NOTIFICATION_DIPLO_VOTE:
	{
		CvPopupInfo kPopup(BUTTONPOPUP_DIPLO_VOTE, m_ePlayer);
		GC.GetEngineUserInterface()->AddPopup(kPopup);
	}
	break;
	case NOTIFICATION_MINOR_QUEST:
	{
		int iQuestFlags = notification.m_iExtraGameData;
		CvPlot* pPlot = GC.getMap().plot(notification.m_iX, notification.m_iY);
		if(pPlot)
		{
			CvInterfacePtr<ICvPlot1> pDllPlot = GC.WrapPlotPointer(pPlot);
			GC.GetEngineUserInterface()->lookAt(pDllPlot.get(), CAMERALOOKAT_NORMAL);
			gDLL->GameplayDoFX(pDllPlot.get());
		}

		PlayerTypes ePlayer = (PlayerTypes)notification.m_iGameDataIndex;
		if (GET_PLAYER(ePlayer).isAlive())
		{
			GC.GetEngineUserInterface()->SetTempString(notification.m_strMessage);
			CvPopupInfo kPopup(BUTTONPOPUP_CITY_STATE_MESSAGE, notification.m_iGameDataIndex, iQuestFlags);
			GC.GetEngineUserInterface()->AddPopup(kPopup);
		}
	}
	break;
	case NOTIFICATION_PRODUCTION:
	{
		CvCity* pCity = GC.getMap().plot(notification.m_iX, notification.m_iY)->getPlotCity();//GET_PLAYER(m_ePlayer).getCity(notification.m_iGameDataIndex);
		if(!pCity || (pCity->getOwner() != m_ePlayer))
		{
			return;
		}

		CvPopupInfo kPopupInfo(BUTTONPOPUP_CHOOSEPRODUCTION);

		kPopupInfo.iData1 = pCity->GetID();

		kPopupInfo.bOption2 = false;	// Not in purchase mode

		// slewis - do we need the stuff below?
		//kPopupInfo.setOption1(false);

		OrderTypes eOrder = (OrderTypes) notification.m_iGameDataIndex;
		int iItemID = notification.m_iExtraGameData;
		kPopupInfo.iData2 = eOrder;
		kPopupInfo.iData3 = iItemID;

		GC.GetEngineUserInterface()->AddPopup(kPopupInfo);
	}
	break;
#if defined(MOD_UI_CITY_EXPANSION)
	case NOTIFICATION_CITY_TILE:
	{
		if (MOD_UI_CITY_EXPANSION) {
			CvCity* pCity = GC.getMap().plot(notification.m_iX, notification.m_iY)->getPlotCity();
			if (!pCity) {
				return;
			}

			// CUSTOMLOG("Activate NOTIFICATION_CITY_TILE for city %s at (%i, %i)", pCity->getName().c_str(), notification.m_iX, notification.m_iY);
			// We want the C++ equivalent of UI.SetInterfaceMode(INTERFACEMODE_PURCHASE_PLOT) followed by
			// UI.DoSelectCityAtPlot(pPlot) (which itself happens to be bugged!)
			// The following is identical to the "Select Next City" then "Open City View" keyboard short-cuts code,
			// but it always opens the capital's city view screen!
			// DLLUI->selectCity(GC.WrapCityPointer(pCity).get());
			// DLLUI->setInterfaceMode(INTERFACEMODE_PURCHASE_PLOT);
			// DLLUI->selectLookAtCity();
			// DLLUI->lookAtSelectionPlot();
		
			// As the City View screen isn't a pop-up, we'll have to call a pop-up to open that screen and then immediately dismiss itself
			int iModderOffset = gCustomMods.getOption("UI_CITY_EXPANSION_BUTTONPOPUP_MODDER_OFFSET", 0);
			CvPopupInfo kPopupInfoOpen((ButtonPopupTypes) (BUTTONPOPUP_MODDER_0 + iModderOffset));
			kPopupInfoOpen.iData1 = pCity->GetID();
			kPopupInfoOpen.iData2 = notification.m_iLookupIndex;
			GC.GetEngineUserInterface()->AddPopup(kPopupInfoOpen);
		} else {
			// Default behavior is to move the camera to the X,Y passed in
			CvPlot* pPlot = GC.getMap().plot(notification.m_iX, notification.m_iY);
			if (pPlot) {
				CvInterfacePtr<ICvPlot1> pDllPlot = GC.WrapPlotPointer(pPlot);

				GC.GetEngineUserInterface()->lookAt(pDllPlot.get(), CAMERALOOKAT_NORMAL);
				gDLL->GameplayDoFX(pDllPlot.get());
			}
		}
	}
	break;
#endif
	case NOTIFICATION_UNIT_PROMOTION:
	{
		CvUnit* pUnit = GET_PLAYER(m_ePlayer).getUnit(notification.m_iExtraGameData);
		if(pUnit)
		{
			CvPlot* pPlot = pUnit->plot();
			if(pPlot)
			{
				CvInterfacePtr<ICvPlot1> pDllPlot = GC.WrapPlotPointer(pPlot);
				CvInterfacePtr<ICvUnit1> pDllUnit = GC.WrapUnitPointer(pUnit);

				GC.GetEngineUserInterface()->lookAt(pDllPlot.get(), CAMERALOOKAT_NORMAL);
				GC.GetEngineUserInterface()->selectUnit(pDllUnit.get(), false);
				gDLL->GameplayDoFX(pDllPlot.get());
			}
		}
	}
	break;
	case NOTIFICATION_PLAYER_DEAL:
	{
		GC.GetEngineUserInterface()->OpenPlayerDealScreen((PlayerTypes) notification.m_iX);
	}
	break;
	case NOTIFICATION_PLAYER_DEAL_RECEIVED:
	{
#if defined(MOD_ACTIVE_DIPLOMACY)
		if(GC.getGame().isReallyNetworkMultiPlayer() && MOD_ACTIVE_DIPLOMACY)
		{
			// JdH => we need to switch behaviour for AI vs Human players.
			PlayerTypes eFrom = static_cast<PlayerTypes>(notification.m_iX);
			CvPlayer& kFrom = GET_PLAYER(eFrom);
			if (kFrom.isHuman() && notification.m_iY != -2 /* request hack */)
			{
			// Keep old PvP notification behaviour
				GC.GetEngineUserInterface()->OpenPlayerDealScreen(eFrom);
			}
			else
			{
				// This request was sent by an AI.
				PlayerTypes eTo = notification.m_ePlayerID;
				CvPlayer& kTo = GET_PLAYER(eTo);
				kTo.GetDiplomacyRequests()->ActivateAllFrom(eFrom);
			}
			// JdH <=
		}
		else
		{
			GC.GetEngineUserInterface()->OpenPlayerDealScreen((PlayerTypes) notification.m_iX);
		}
#else
		GC.GetEngineUserInterface()->OpenPlayerDealScreen((PlayerTypes) notification.m_iX);
#endif
	}
	break;
	case NOTIFICATION_FREE_GREAT_PERSON:
	{
		if(GET_PLAYER(m_ePlayer).GetNumFreeGreatPeople() > 0)
		{
			CvPopupInfo kPopup(BUTTONPOPUP_CHOOSE_FREE_GREAT_PERSON, m_ePlayer, notification.m_iGameDataIndex, notification.m_iExtraGameData);
			GC.GetEngineUserInterface()->AddPopup(kPopup);
		}
	}
	break;
	case NOTIFICATION_FOUND_PANTHEON:
	{
		CvPopupInfo kPopup(BUTTONPOPUP_FOUND_PANTHEON, m_ePlayer, true /*bPantheonBelief*/);
		GC.GetEngineUserInterface()->AddPopup(kPopup);
	}
	break;

	case NOTIFICATION_ADD_REFORMATION_BELIEF:
	{
		CvPopupInfo kPopup(BUTTONPOPUP_FOUND_PANTHEON, m_ePlayer, false /*bPantheonBelief*/);
		GC.GetEngineUserInterface()->AddPopup(kPopup);
	}
	break;

	case NOTIFICATION_FOUND_RELIGION:
	case NOTIFICATION_ENHANCE_RELIGION:
	{
		CvPopupInfo kPopup(BUTTONPOPUP_FOUND_RELIGION, m_ePlayer);
		kPopup.iData1 = notification.m_iX;
		kPopup.iData2 = notification.m_iY;

		kPopup.bOption1 = notification.m_eNotificationType == NOTIFICATION_FOUND_RELIGION;
		GC.GetEngineUserInterface()->AddPopup(kPopup);
	}
	break;

	case NOTIFICATION_SPY_CREATED_ACTIVE_PLAYER:
	case NOTIFICATION_SPY_EVICTED:
	case NOTIFICATION_SPY_PROMOTION:
	{
		CvPopupInfo kPopup(BUTTONPOPUP_ESPIONAGE_OVERVIEW, m_ePlayer);
		GC.GetEngineUserInterface()->AddPopup(kPopup);
	}
	break;

	case NOTIFICATION_SPY_STOLE_TECH:
	{
		CvPopupInfo kPopup(BUTTONPOPUP_CHOOSE_TECH_TO_STEAL, m_ePlayer, notification.m_iGameDataIndex, notification.m_iExtraGameData);
		strcpy(kPopup.szText, notification.m_strMessage);
		GC.GetEngineUserInterface()->AddPopup(kPopup);
	}
	break;

	case NOTIFICATION_MAYA_LONG_COUNT:
	{
		if(GET_PLAYER(m_ePlayer).GetNumMayaBoosts() > 0)
		{
			CvPopupInfo kPopup(BUTTONPOPUP_CHOOSE_MAYA_BONUS, m_ePlayer, notification.m_iGameDataIndex, notification.m_iExtraGameData);
			GC.GetEngineUserInterface()->AddPopup(kPopup);
		}
	}
	break;

	case NOTIFICATION_FAITH_GREAT_PERSON:
	{
		if(GET_PLAYER(m_ePlayer).GetNumFaithGreatPeople() > 0)
		{
			CvPopupInfo kPopup(BUTTONPOPUP_CHOOSE_FAITH_GREAT_PERSON, m_ePlayer, notification.m_iGameDataIndex, notification.m_iExtraGameData);
			GC.GetEngineUserInterface()->AddPopup(kPopup);
		}
	}
	break;

	case NOTIFICATION_TECH_STOLEN_SPY_IDENTIFIED:
	case NOTIFICATION_SPY_KILLED_A_SPY:
	{
		ASSERT_DEBUG(notification.m_iGameDataIndex >= -1, "notification.m_iGameDataIndex is out of bounds");
		if(notification.m_iGameDataIndex >= 0 && notification.m_iExtraGameData == -1)
		{
			PlayerTypes eTargetPlayer = (PlayerTypes)notification.m_iGameDataIndex;
			if (!GET_PLAYER(eTargetPlayer).isHuman())
			{
				GET_PLAYER((PlayerTypes)notification.m_iGameDataIndex).GetDiplomacyAI()->DoBeginDiploWithHumanEspionageResult();
			}

			GC.GetEngineUserInterface()->RemoveNotification(notification.m_iLookupIndex, m_ePlayer);
			notification.m_iExtraGameData = 1; // slewis hack to mark notification as seen so we don't re-enter diplomacy
		}
	}
	break;

	case NOTIFICATION_INTRIGUE_DECEPTION:
	case NOTIFICATION_INTRIGUE_SNEAK_ATTACK_ARMY_AGAINST_KNOWN_CITY_KNOWN:
	case NOTIFICATION_INTRIGUE_SNEAK_ATTACK_ARMY_AGAINST_KNOWN_CITY_UNKNOWN:
	case NOTIFICATION_INTRIGUE_SNEAK_ATTACK_AMPHIB_AGAINST_KNOWN_CITY_UNKNOWN:
	case NOTIFICATION_INTRIGUE_SNEAK_ATTACK_AMPHIB_AGAINST_KNOWN_CITY_KNOWN:
		ASSERT_DEBUG(notification.m_iGameDataIndex >= 0, "notification.m_iGameDataIndex is out of bounds");
		if(notification.m_iGameDataIndex >= 0)
		{
			PlayerTypes ePlayerToContact = (PlayerTypes)notification.m_iGameDataIndex;
			if (!GET_PLAYER(ePlayerToContact).isHuman() && ePlayerToContact != m_ePlayer && GET_PLAYER(m_ePlayer).GetEspionage()->HasRecentIntrigueAbout(ePlayerToContact) && !GET_TEAM(GET_PLAYER(m_ePlayer).getTeam()).isAtWar(GET_PLAYER(ePlayerToContact).getTeam()))
			{
				GET_PLAYER(ePlayerToContact).GetDiplomacyAI()->DoBeginDiploWithHumanInDiscuss();
			}
		}
		break;

	case NOTIFICATION_LEAGUE_CALL_FOR_PROPOSALS:
	case NOTIFICATION_LEAGUE_CALL_FOR_VOTES:
	case NOTIFICATION_LEAGUE_VOTING_DONE:
	case NOTIFICATION_LEAGUE_VOTING_SOON:
		ASSERT_DEBUG(notification.m_iGameDataIndex >= 0, "notification.m_iGameDataIndex is out of bounds");
		if (notification.m_iGameDataIndex >= 0)
		{
			LeagueTypes eLeague = (LeagueTypes) notification.m_iGameDataIndex;
			CvPopupInfo kPopup(BUTTONPOPUP_LEAGUE_OVERVIEW, eLeague);
			GC.GetEngineUserInterface()->AddPopup(kPopup);
		}
		break;

	case NOTIFICATION_IDEOLOGY_CHOSEN:
#if !defined(MOD_BALANCE_CORE)
	case NOTIFICATION_CULTURE_VICTORY_SOMEONE_INFLUENTIAL:
#endif
	case NOTIFICATION_CULTURE_VICTORY_WITHIN_TWO:
	case NOTIFICATION_CULTURE_VICTORY_WITHIN_TWO_ACTIVE_PLAYER:
	case NOTIFICATION_CULTURE_VICTORY_WITHIN_ONE:
	case NOTIFICATION_CULTURE_VICTORY_WITHIN_ONE_ACTIVE_PLAYER:
	case NOTIFICATION_CULTURE_VICTORY_NO_LONGER_INFLUENTIAL:
		ASSERT_DEBUG(notification.m_iGameDataIndex >= 0, "notification.m_iGameDataIndex is out of bounds");
		if (notification.m_iGameDataIndex >= 0)
		{
			CvPopupInfo kPopup(BUTTONPOPUP_CULTURE_OVERVIEW);
			kPopup.iData2 = 3; // Tab to select
			GC.GetEngineUserInterface()->AddPopup(kPopup);
		}
		break;

	case NOTIFICATION_CHOOSE_ARCHAEOLOGY:
		ASSERT_DEBUG(notification.m_iGameDataIndex >= 0, "notification.m_iGameDataIndex is out of bounds");
		if (notification.m_iGameDataIndex >= 0)
		{
			CvPopupInfo kPopup(BUTTONPOPUP_CHOOSE_ARCHAEOLOGY, m_ePlayer);
			GC.GetEngineUserInterface()->AddPopup(kPopup);
		}
		break;

	case NOTIFICATION_CHOOSE_IDEOLOGY:
		ASSERT_DEBUG(notification.m_iGameDataIndex >= 0, "notification.m_iGameDataIndex is out of bounds");
		if (notification.m_iGameDataIndex >= 0)
		{
			CvPopupInfo kPopup(BUTTONPOPUP_CHOOSE_IDEOLOGY, m_ePlayer);
			GC.GetEngineUserInterface()->AddPopup(kPopup);
		}
		break;

	case NOTIFICATION_LEAGUE_PROJECT_COMPLETE:
		ASSERT_DEBUG(notification.m_iGameDataIndex >= 0, "notification.m_iGameDataIndex is out of bounds");
		if (notification.m_iGameDataIndex >= 0)
		{
			LeagueTypes eLeague = (LeagueTypes) notification.m_iGameDataIndex;
			LeagueProjectTypes eProject = (LeagueProjectTypes) notification.m_iExtraGameData;
			CvPopupInfo kPopup(BUTTONPOPUP_LEAGUE_PROJECT_COMPLETED, eLeague, eProject);
			GC.GetEngineUserInterface()->AddPopup(kPopup);
		}
		break;
#if defined(MOD_BALANCE_CORE)
	case 419811917:
		ASSERT_DEBUG(notification.m_iGameDataIndex >= 0, "notification.m_iGameDataIndex is out of bounds");
		if (notification.m_iGameDataIndex >= 0)
		{
			EventTypes eEvent = (EventTypes)notification.m_iGameDataIndex;
			CvPopupInfo kPopup(BUTTONPOPUP_MODDER_10, m_ePlayer, eEvent);
			GC.GetEngineUserInterface()->AddPopup(kPopup);
		}
		break;
	case 826076831:
		ASSERT_DEBUG(notification.m_iGameDataIndex >= 0, "notification.m_iGameDataIndex is out of bounds");
		if (notification.m_iGameDataIndex >= 0)
		{
			CityEventTypes eEvent = (CityEventTypes)notification.m_iGameDataIndex;
			int iCityID = notification.m_iExtraGameData;
			CvPopupInfo kPopup(BUTTONPOPUP_MODDER_8, m_ePlayer, eEvent, iCityID);
			GC.GetEngineUserInterface()->AddPopup(kPopup);
		}
		break;
	case -1608954742:
		ASSERT_DEBUG(notification.m_iGameDataIndex >= 0, "notification.m_iGameDataIndex is out of bounds");
		if (notification.m_iGameDataIndex >= 0)
		{
			CityEventTypes eEvent = (CityEventTypes)notification.m_iGameDataIndex;
			int iSpyID = notification.m_iExtraGameData;
			CvPlot* pPlot = GC.getMap().plot(notification.m_iX, notification.m_iY);
			if (pPlot)
			{
				CvPopupInfo kPopup(BUTTONPOPUP_MODDER_12, m_ePlayer, eEvent, pPlot->GetPlotIndex(), iSpyID);
				GC.GetEngineUserInterface()->AddPopup(kPopup);
			}
		}
		break;
	case -364200720:
		if (GC.getMap().plot(notification.m_iX, notification.m_iY) != NULL)
		{
			CvPlot* pPlot = GC.getMap().plot(notification.m_iX, notification.m_iY);
			CvCity* pCity = pPlot->getPlotCity();
			if (pCity)
			{
				std::vector<int> tempInt = pCity->getCaptureDataInt();
				std::vector<bool> tempBool = pCity->getCaptureDataBool();

				int iTemp[5] = { tempInt[0], tempInt[1], tempInt[2], tempInt[3], tempInt[4] };
				bool bTemp[2] = { tempBool[0], tempBool[1] };

				GC.GetEngineUserInterface()->AddPopup(BUTTONPOPUP_CITY_CAPTURED, POPUP_PARAM_INT_ARRAY(iTemp), POPUP_PARAM_BOOL_ARRAY(bTemp));
			}
		}
		break;
#endif

	default:	// Default behavior is to move the camera to the X,Y passed in
	{
		CvPlot* pPlot = GC.getMap().plot(notification.m_iX, notification.m_iY);
		if(pPlot)
		{
			CvInterfacePtr<ICvPlot1> pDllPlot = GC.WrapPlotPointer(pPlot);

			GC.GetEngineUserInterface()->lookAt(pDllPlot.get(), CAMERALOOKAT_NORMAL);
			gDLL->GameplayDoFX(pDllPlot.get());
		}
	}
	break;
	}
}

//	---------------------------------------------------------------------------
bool CvNotifications::IsNotificationRedundant(Notification& notification)
{
	switch(notification.m_eNotificationType)
	{
	case NOTIFICATION_TECH:
	{
		int iIndex = m_iNotificationsBeginIndex;
		while(iIndex != m_iNotificationsEndIndex)
		{
			if(notification.m_eNotificationType == m_aNotifications[iIndex].m_eNotificationType)
			{
				if(!notification.m_bDismissed && !m_aNotifications[iIndex].m_bDismissed)
				{
					if (IsNotificationTypeEndOfTurnExpired(notification.m_eNotificationType) && notification.m_bWaitExtraTurn)
					{
						if (m_aNotifications[iIndex].m_bWaitExtraTurn)
							return true;
					}
					else
						// We already added this kind of notification so we don't need another
						return true;
				}
			}


			iIndex++;
			if(iIndex >= int(MaxNotifications))
			{
				iIndex = 0;
			}
		}

		return false;
	}
	break;

	case NOTIFICATION_FREE_TECH:
	{
		int iIndex = m_iNotificationsBeginIndex;
		while(iIndex != m_iNotificationsEndIndex)
		{
			if(notification.m_eNotificationType == m_aNotifications[iIndex].m_eNotificationType)
			{
				if(!notification.m_bDismissed && !m_aNotifications[iIndex].m_bDismissed)
				{
					// we've already added a free tech notification, don't need another
					return true;
				}
			}


			iIndex++;
			if(iIndex >= int(MaxNotifications))
			{
				iIndex = 0;
			}
		}

		return false;
	}
	break;

	case NOTIFICATION_POLICY:
	{
		int iIndex = m_iNotificationsBeginIndex;
		while(iIndex != m_iNotificationsEndIndex)
		{
			if(notification.m_eNotificationType == m_aNotifications[iIndex].m_eNotificationType)
			{
				if(!notification.m_bDismissed && !m_aNotifications[iIndex].m_bDismissed)
				{
					// we've already added a policy notification, don't need another
					return true;
				}
			}


			iIndex++;
			if(iIndex >= int(MaxNotifications))
			{
				iIndex = 0;
			}
		}

		return false;
	}
	break;

	case NOTIFICATION_FREE_POLICY:
	{
		int iIndex = m_iNotificationsBeginIndex;
		while(iIndex != m_iNotificationsEndIndex)
		{
			if(notification.m_eNotificationType == m_aNotifications[iIndex].m_eNotificationType)
			{
				if(!notification.m_bDismissed && !m_aNotifications[iIndex].m_bDismissed)
				{
					// we've already added a tech notification, don't need another
					return true;
				}
			}


			iIndex++;
			if(iIndex >= int(MaxNotifications))
			{
				iIndex = 0;
			}
		}

		return false;
	}
	break;

	case NOTIFICATION_PRODUCTION:
	{
		int iIndex = m_iNotificationsBeginIndex;
		while(iIndex != m_iNotificationsEndIndex)
		{
			if(notification.m_eNotificationType == m_aNotifications[iIndex].m_eNotificationType &&
			        notification.m_iX == m_aNotifications[iIndex].m_iX &&
			        notification.m_iY == m_aNotifications[iIndex].m_iY)
			{
				if(!notification.m_bDismissed && !m_aNotifications[iIndex].m_bDismissed)
				{
					// we've already added a notification for this city to the notification system, so don't add another one
					return true;
				}
			}

			iIndex++;
			if(iIndex >= int(MaxNotifications))
			{
				iIndex = 0;
			}
		}

		return false;
	}
	break;

#if defined(MOD_UI_CITY_EXPANSION)
	case NOTIFICATION_CITY_TILE:
	{
		if (MOD_UI_CITY_EXPANSION) {
			int iIndex = m_iNotificationsBeginIndex;
			while(iIndex != m_iNotificationsEndIndex)
			{
				if(notification.m_eNotificationType == m_aNotifications[iIndex].m_eNotificationType &&
				   notification.m_iX == m_aNotifications[iIndex].m_iX && notification.m_iY == m_aNotifications[iIndex].m_iY)
				{
					if(!notification.m_bDismissed && !m_aNotifications[iIndex].m_bDismissed)
					{
						// we've already added a notification for this city to the notification system, so don't add another one
						return true;
					}
				}

				iIndex++;
				if(iIndex >= int(MaxNotifications))
				{
					iIndex = 0;
				}
			}
		}

		return false;
	}
	break;
#endif

	case NOTIFICATION_ENEMY_IN_TERRITORY:
	{
		int iIndex = m_iNotificationsBeginIndex;
		while(iIndex != m_iNotificationsEndIndex)
		{
			// Only one "enemy in territory" notification at a time
			if(notification.m_eNotificationType == m_aNotifications[iIndex].m_eNotificationType)
			{
				if(!notification.m_bDismissed && !m_aNotifications[iIndex].m_bDismissed)
				{
					return true;
				}
			}

			iIndex++;
			if(iIndex >= int(MaxNotifications))
			{
				iIndex = 0;
			}
		}

		return false;
	}
	break;

	case NOTIFICATION_UNIT_PROMOTION:
	{
		int iIndex = m_iNotificationsBeginIndex;
		while(iIndex != m_iNotificationsEndIndex)
		{
			if(notification.m_eNotificationType == m_aNotifications[iIndex].m_eNotificationType)
			{
				if(notification.m_iExtraGameData == m_aNotifications[iIndex].m_iExtraGameData)
				{
					if(!notification.m_bDismissed && !m_aNotifications[iIndex].m_bDismissed)
					{
						// we've already added a notification for this unit to the notification system, so don't add another one
						return true;
					}
				}
			}

			iIndex++;
			if(iIndex >= int(MaxNotifications))
			{
				iIndex = 0;
			}
		}

		return false;
	}
	break;

	case NOTIFICATION_DIPLOMACY_DECLARATION:
	{
		PlayerTypes eOurPlayer1 = (PlayerTypes) notification.m_iGameDataIndex;
		PlayerTypes eOurPlayer2 = (PlayerTypes) notification.m_iExtraGameData;

		// Notification is NOT being used to inform of a DoF or Denouncement (otherwise there would be valid players in these slots)
		if(eOurPlayer1 == -1 || eOurPlayer2 == -1)
			return false;

		PlayerTypes eCheckingPlayer1;
		PlayerTypes eCheckingPlayer2;

		int iIndex = m_iNotificationsBeginIndex;
		while(iIndex != m_iNotificationsEndIndex)
		{
			if(notification.m_eNotificationType == m_aNotifications[iIndex].m_eNotificationType)
			{
				eCheckingPlayer1 = (PlayerTypes) m_aNotifications[iIndex].m_iGameDataIndex;
				eCheckingPlayer2 = (PlayerTypes) m_aNotifications[iIndex].m_iExtraGameData;

				// Players match - we already have a notification with this player combo
				if((eOurPlayer1 == eCheckingPlayer1 && eOurPlayer2 == eCheckingPlayer2) ||
				        (eOurPlayer1 == eCheckingPlayer2 && eOurPlayer2 == eCheckingPlayer1))
				{
					return true;
				}
			}

			iIndex++;
			if(iIndex >= int(MaxNotifications))
				iIndex = 0;
		}

		return false;
	}
	break;

	case NOTIFICATION_FOUND_PANTHEON:
	case NOTIFICATION_FOUND_RELIGION:
	case NOTIFICATION_ENHANCE_RELIGION:
	case NOTIFICATION_ADD_REFORMATION_BELIEF:
	case NOTIFICATION_CHOOSE_ARCHAEOLOGY:
	case NOTIFICATION_CHOOSE_IDEOLOGY:
	{
		int iIndex = m_iNotificationsBeginIndex;
		while(iIndex != m_iNotificationsEndIndex)
		{
			if(notification.m_eNotificationType == m_aNotifications[iIndex].m_eNotificationType)
			{
				if(!notification.m_bDismissed && !m_aNotifications[iIndex].m_bDismissed)
				{
					// we've already added a pantheon notification, don't need another
					return true;
				}
			}


			iIndex++;
			if(iIndex >= int(MaxNotifications))
			{
				iIndex = 0;
			}
		}

		return false;
	}
	break;

	case NOTIFICATION_LEAGUE_CALL_FOR_PROPOSALS:
	case NOTIFICATION_LEAGUE_CALL_FOR_VOTES:
	case NOTIFICATION_LEAGUE_VOTING_SOON:
	{
		int iIndex = m_iNotificationsBeginIndex;
		while(iIndex != m_iNotificationsEndIndex)
		{
			if(notification.m_eNotificationType == m_aNotifications[iIndex].m_eNotificationType)
			{
				if(!notification.m_bDismissed && !m_aNotifications[iIndex].m_bDismissed)
				{
					// we've already added one of this notification type, don't need another
					return true;
				}
			}


			iIndex++;
			if(iIndex >= int(MaxNotifications))
			{
				iIndex = 0;
			}
		}
		return false;
	}
	break;

	case NOTIFICATION_LEAGUE_PROJECT_COMPLETE:
	case NOTIFICATION_LEAGUE_PROJECT_PROGRESS:
		{
			int iIndex = m_iNotificationsBeginIndex;
			while(iIndex != m_iNotificationsEndIndex)
			{
				if(notification.m_eNotificationType == m_aNotifications[iIndex].m_eNotificationType)
				{
					if(!notification.m_bDismissed && !m_aNotifications[iIndex].m_bDismissed)
					{
						// Same League ID
						if (notification.m_iGameDataIndex == m_aNotifications[iIndex].m_iGameDataIndex)
						{
							// Same Project type
							if (notification.m_iExtraGameData == m_aNotifications[iIndex].m_iExtraGameData)
							{
								return true;
							}
						}
					}
				}


				iIndex++;
				if(iIndex >= int(MaxNotifications))
				{
					iIndex = 0;
				}
			}
			return false;
		}
		break;

	default:
		return false;
		break;
	}
}

bool CvNotifications::IsNotificationExpired(int iIndex)
{
	switch(m_aNotifications[iIndex].m_eNotificationType)
	{
	case NOTIFICATION_BUY_TILE:
	{
		if(GET_PLAYER(m_ePlayer).GetTreasury()->GetGold() < GET_PLAYER(m_ePlayer).GetBuyPlotCost())
		{
			return true;
		}
	}
	break;

	case NOTIFICATION_BARBARIAN:
	{
		CvPlot* pPlot = GC.getMap().plot(m_aNotifications[iIndex].m_iX, m_aNotifications[iIndex].m_iY);
		if(!pPlot->HasBarbarianCamp())
		{
			return true;
		}
	}
	break;

	case NOTIFICATION_CITY_RANGE_ATTACK:
	{
		int iCityID = m_aNotifications[iIndex].m_iGameDataIndex;
		CvCity* pCity = GET_PLAYER(m_ePlayer).getCity(iCityID);

		if(pCity == NULL)
			return true;

		else if(!pCity->CanRangeStrikeNow())
			return true;
	}
	break;

	case NOTIFICATION_GOODY:
	{
		CvPlot* pPlot = GC.getMap().plot(m_aNotifications[iIndex].m_iX, m_aNotifications[iIndex].m_iY);
		if(!pPlot->isGoody(GET_PLAYER(m_ePlayer).getTeam()))
		{
			return true;
		}
	}
	break;

	case NOTIFICATION_TECH:
	{
		CvPlayerAI& kPlayer = GET_PLAYER(m_ePlayer);
		CvPlayerTechs* pkPlayerTechs = kPlayer.GetPlayerTechs();
		if(pkPlayerTechs->GetCurrentResearch() != NO_TECH)
		{
			return true;
		}

		int iNotificationIndex = m_iNotificationsBeginIndex;
		while(iNotificationIndex != m_iNotificationsEndIndex)
		{
			if(NOTIFICATION_FREE_TECH == m_aNotifications[iNotificationIndex].m_eNotificationType)
			{
				if(!m_aNotifications[iNotificationIndex].m_bDismissed)
				{
					return true;
				}
			}

			iNotificationIndex++;
			if(iNotificationIndex >= int(MaxNotifications))
			{
				iNotificationIndex = 0;
			}
		}

		//Expire this notification if there are no more techs that can be researched at this time.
		return pkPlayerTechs->GetNumTechsCanBeResearched() == 0;
	}
	break;
	case NOTIFICATION_FREE_TECH:
	{
		CvPlayerAI& kPlayer = GET_PLAYER(m_ePlayer);
		if(kPlayer.GetNumFreeTechs() == 0)
		{
			return true;
		}
		else
		{
			//Expire this notification if there are no more techs that can be researched at this time.
			return kPlayer.GetPlayerTechs()->GetNumTechsCanBeResearched() == 0;
		}
	}
	break;

	case NOTIFICATION_FREE_POLICY:
	{
		if(GC.getGame().isOption(GAMEOPTION_POLICY_SAVING))
		{
			if(GET_PLAYER(m_ePlayer).GetNumFreePolicies() == 0 && GET_PLAYER(m_ePlayer).GetNumFreeTenets() == 0)
				return true;
		}
		else
		{
			if((GET_PLAYER(m_ePlayer).getJONSCultureTimes100() < GET_PLAYER(m_ePlayer).getNextPolicyCost() * 100 && GET_PLAYER(m_ePlayer).GetNumFreePolicies() == 0 && GET_PLAYER(m_ePlayer).GetNumFreeTenets() == 0))
				return true;
		}
	}
	break;

	case NOTIFICATION_FREE_GREAT_PERSON:
	{
		if(GET_PLAYER(m_ePlayer).GetNumFreeGreatPeople() == 0)
		{
			return true;
		}
	}
	break;

	case NOTIFICATION_MAYA_LONG_COUNT:
	{
		if(GET_PLAYER(m_ePlayer).GetNumMayaBoosts() == 0)
		{
			return true;
		}
	}
	break;

	case NOTIFICATION_FAITH_GREAT_PERSON:
	{
		if(GET_PLAYER(m_ePlayer).GetNumFaithGreatPeople() == 0)
		{
			return true;
		}
	}
	break;

	case NOTIFICATION_POLICY:
	{
		if(GET_PLAYER(m_ePlayer).getJONSCultureTimes100() < GET_PLAYER(m_ePlayer).getNextPolicyCost() * 100 && GET_PLAYER(m_ePlayer).GetNumFreePolicies() == 0 && GET_PLAYER(m_ePlayer).GetNumFreeTenets() == 0)
		{
			return true;
		}
	}
	break;
	case NOTIFICATION_PRODUCTION:
	{
		//CvCity* pCity = GET_PLAYER(m_ePlayer).getCity(m_aNotifications[iIndex].m_iGameDataIndex);
		CvCity* pCity = GC.getMap().plot(m_aNotifications[iIndex].m_iX, m_aNotifications[iIndex].m_iY)->getPlotCity();//GET_PLAYER(m_ePlayer).getCity(notification.m_iGameDataIndex);

		// if the city no longer exists
		if(!pCity || (pCity->getOwner() != m_ePlayer))
		{
			return true;
		}

		// if the city is a puppet
		if(pCity->IsPuppet())
		{
			return true;
		}

		// City has chosen something
		if(pCity->getOrderQueueLength() > 0)
		{
			return true;
		}
	}
	break;
#if defined(MOD_UI_CITY_EXPANSION)
	case NOTIFICATION_CITY_TILE:
	{
		if (MOD_UI_CITY_EXPANSION) {
			CvCity* pCity = GC.getMap().plot(m_aNotifications[iIndex].m_iX, m_aNotifications[iIndex].m_iY)->getPlotCity();

			// if the city no longer exists, is a puppet, or doesn't belong to the active player
			if (!pCity || pCity->IsPuppet() || (pCity->getOwner() != GC.getGame().getActivePlayer())) {
				// we no longer need the notification
				return true;
			}

			// if the city has choosen a tile (probably by cycling the cities after the first notification)
			if (pCity->GetJONSCultureStoredTimes100() < pCity->GetJONSCultureThreshold() * 100) {
				// we no longer need the notification
				return true;
			}
		}
	}
	break;
#endif
	case NOTIFICATION_DIPLO_VOTE:
	{
		TeamTypes eTeam = GET_PLAYER(m_ePlayer).getTeam();

		// Vote from this team registered
		if(GC.getGame().GetVoteCast(eTeam) != NO_TEAM)
		{
			return true;
		}

		// Votes from ALL teams registered. This is necessary in addition to the above if block, because if this player is the last to vote
		// then everything gets reset immediately, and it'll be NO_TEAM by the time this function is tested again
		if(GC.getGame().GetNumVictoryVotesExpected() == 0)
		{
			return true;
		}
	}
	break;
	case NOTIFICATION_UNIT_PROMOTION:
	{
		CvUnit* pUnit = GET_PLAYER(m_ePlayer).getUnit(m_aNotifications[iIndex].m_iExtraGameData);
		if(!pUnit || !pUnit->isPromotionReady())
		{
			return true;
		}
	}
	break;
	case NOTIFICATION_PLAYER_DEAL:
	{
		CvGame& game = GC.getGame();

#if defined(MOD_ACTIVE_DIPLOMACY)
		PlayerTypes eFrom = static_cast<PlayerTypes>(m_aNotifications[iIndex].m_iX);
		if((!GET_PLAYER(m_ePlayer).isHuman() || !GET_PLAYER(eFrom).isHuman()) && GC.getGame().isReallyNetworkMultiPlayer() && MOD_ACTIVE_DIPLOMACY)
		{
			if (game.GetGameDeals().GetProposedMPDeal(m_ePlayer, eFrom, true) == NULL)
			{
				return true;
			}
		}
		else
		{
			if(!game.GetGameDeals().ProposedDealExists(m_ePlayer, eFrom))
			{
				return true;
			}
		}
#else
		if(!game.GetGameDeals().ProposedDealExists(m_ePlayer, (PlayerTypes)(m_aNotifications[iIndex].m_iX)))

		{
			return true;
		}
#endif
	}
	break;
	case NOTIFICATION_PLAYER_DEAL_RECEIVED:
	{
		CvGame& game = GC.getGame();
#if defined(MOD_ACTIVE_DIPLOMACY)
		PlayerTypes eFrom = static_cast<PlayerTypes>(m_aNotifications[iIndex].m_iX);
		// DN: Not understanding the rationale behind this code and it seems there is more to it but skipping it for human-human deals fixes not getting notifications for those deals and doesn't *seem* to break anything (although some code may be redundant now)
		if((!GET_PLAYER(m_ePlayer).isHuman() || !GET_PLAYER(eFrom).isHuman()) && GC.getGame().isReallyNetworkMultiPlayer() && MOD_ACTIVE_DIPLOMACY)
		{
			// JdH =>			
			if (!GET_PLAYER(m_ePlayer).GetDiplomacyRequests()->HasActiveRequestFrom(eFrom))
			{
				return true;
			}
			if (m_aNotifications[iIndex].m_iY != -1 /* no deal request */) // TODO: check if pvp deals really use m_iY == -1
			{
				return false;
			}
			else if (game.GetGameDeals().GetProposedMPDeal(eFrom, m_ePlayer, true) == NULL)
			{
				return true;
			}
			// JdH <=
		}
		else
		{
			if(!game.GetGameDeals().ProposedDealExists(eFrom,  m_ePlayer))
			{
				return true;
			}
		}
#else
		if(!game.GetGameDeals().ProposedDealExists((PlayerTypes)(m_aNotifications[iIndex].m_iX),  m_ePlayer))
		{
			return true;
		}
#endif
	}
	break;
	case NOTIFICATION_DEMAND_RESOURCE:
	{
		// if this is a "you ran out of this resource" demand resource.
		// I did this so not to break the save format
		if(m_aNotifications[iIndex].m_iX == -1 && m_aNotifications[iIndex].m_iY == -1)
		{
			if(GET_PLAYER(m_ePlayer).getNumResourceAvailable((ResourceTypes)m_aNotifications[iIndex].m_iGameDataIndex, true) >= 0)
			{
				return true;
			}
		}
	}

	case NOTIFICATION_FOUND_PANTHEON:
	{
		CvGame& kGame(GC.getGame());
		CvGameReligions* pkReligions(kGame.GetGameReligions());
		return pkReligions->CanCreatePantheon(m_ePlayer, true) != CvGameReligions::FOUNDING_OK;
	}
	break;

	case NOTIFICATION_ADD_REFORMATION_BELIEF:
	{
		CvGame& kGame(GC.getGame());
		CvGameReligions* pkReligions(kGame.GetGameReligions());
		return pkReligions->HasAddedReformationBelief(m_ePlayer);
	}

	case NOTIFICATION_FOUND_RELIGION:
	{
		CvGame& kGame(GC.getGame());
		CvGameReligions* pkReligions(kGame.GetGameReligions());

		if (pkReligions->GetNumReligionsStillToFound() <= 0 && !GET_PLAYER(m_ePlayer).GetPlayerTraits()->IsAlwaysReligion())
			return true;	// None left, dismiss the notification

		return pkReligions->HasCreatedReligion(m_ePlayer);
	}
	break;

	case NOTIFICATION_ENHANCE_RELIGION:
	{
		CvGame& kGame(GC.getGame());
		CvGameReligions* pkReligions(kGame.GetGameReligions());

		ReligionTypes eReligion = GET_PLAYER(m_ePlayer).GetReligions()->GetOwnedReligion();
		if (pkReligions->GetAvailableEnhancerBeliefs(m_ePlayer, eReligion).size() == 0)
			return true;	// None left, dismiss the notification.
		if (pkReligions->GetAvailableFollowerBeliefs(m_ePlayer, eReligion).size() == 0)
			return true;	// None left, dismiss the notification.		

		const CvReligion* pReligion = pkReligions->GetReligion(eReligion, m_ePlayer);
		return (NULL != pReligion && pReligion->m_bEnhanced);
	}
	break;

	case NOTIFICATION_SPY_STOLE_TECH:
	{
		CvPlayerEspionage* pEspionage = GET_PLAYER(m_ePlayer).GetEspionage();
		return pEspionage->m_aiNumTechsToStealList[m_aNotifications[iIndex].m_iGameDataIndex] <= 0;
	}
	break;

	case NOTIFICATION_LEAGUE_CALL_FOR_PROPOSALS:
	{
		LeagueTypes eLeague = (LeagueTypes) m_aNotifications[iIndex].m_iGameDataIndex;
		CvLeague* pLeague = GC.getGame().GetGameLeagues()->GetLeague(eLeague);
		return !pLeague->CanPropose(m_ePlayer);
	}
	break;

	case NOTIFICATION_CHOOSE_ARCHAEOLOGY:
	{
		if (GET_PLAYER(m_ePlayer).GetNumArchaeologyChoices() == 0)
		{
			return true;
		}
	}
	break;

	case NOTIFICATION_CHOOSE_IDEOLOGY:
		{
			if (GET_PLAYER(m_ePlayer).GetPlayerPolicies()->GetLateGamePolicyTree() != NO_POLICY_BRANCH_TYPE)
			{
				return true;
			}
		}
		break;

	case NOTIFICATION_LEAGUE_CALL_FOR_VOTES:
	{
		LeagueTypes eLeague = (LeagueTypes) m_aNotifications[iIndex].m_iGameDataIndex;
		CvLeague* pLeague = GC.getGame().GetGameLeagues()->GetLeague(eLeague);
		return !pLeague->CanVote(m_ePlayer);
	}
	break;

	case NOTIFICATION_PLAYER_CONNECTING:
	{
		if(!gDLL->IsPlayerHotJoining(m_aNotifications[iIndex].m_iGameDataIndex)){
			//Player has finished hot joining.  Remove this notification, we'll add a NOTIFICATION_PLAYER_RECONNECTED in NetProxy::OnHotJoinComplete().
			return true;
		}
	}
	break;
#if defined(MOD_BALANCE_CORE)
	case 826076831: // City Event Notification
	{
		CityEventTypes eCityEvent = (CityEventTypes)m_aNotifications[iIndex].m_iGameDataIndex;
		if(eCityEvent != NO_EVENT_CITY)
		{
			int iCityID = m_aNotifications[iIndex].m_iExtraGameData;
			CvCity* pCity = GET_PLAYER(m_ePlayer).getCity(iCityID);
			if (!pCity)
			{
				return true;
			}

			if(pCity->IsEventActive(eCityEvent))
			{
				int iNumEvent = 0;
				CityEventChoiceTypes eEventChoice = NO_EVENT_CHOICE_CITY;
				for(int iLoop = 0; iLoop < GC.getNumCityEventChoiceInfos(); iLoop++)
				{
					eEventChoice = (CityEventChoiceTypes)iLoop;
					if(eEventChoice != NO_EVENT_CHOICE_CITY)
					{
						CvModEventCityChoiceInfo* pkEventChoiceInfo = GC.getCityEventChoiceInfo(eEventChoice);
						if(pkEventChoiceInfo != NULL)
						{
							if(pCity->IsCityEventChoiceValid(eEventChoice, eCityEvent))
							{
								iNumEvent++;
								break;
							}
						}
					}
				}
				if(iNumEvent <= 0)
				{
					pCity->SetEventActive(eCityEvent, false);
					return true;
				}
				else
				{
					return false;
				}
			}
			else
			{
				pCity->SetEventActive(eCityEvent, false);
				return true;
			}
		}
		return true;
	}
	break;
	case -1608954742:
	{
		CityEventTypes eCityEvent = (CityEventTypes)m_aNotifications[iIndex].m_iGameDataIndex;
		if (eCityEvent != NO_EVENT_CITY)
		{
			CvCity* pCity = GC.getMap().plot(m_aNotifications[iIndex].m_iX, m_aNotifications[iIndex].m_iY)->getPlotCity();
			if (!pCity)
				return true;
			
			if (pCity->GetCityEspionage()->GetPendingEvents(m_ePlayer) > 0)
				return false;
		}
		return true;
	}
	break;
	case 419811917: // Player Event Notification
	{
		EventTypes eEvent = (EventTypes)m_aNotifications[iIndex].m_iGameDataIndex;
		if(eEvent != NO_EVENT)
		{
			if (GET_PLAYER(m_ePlayer).IsEventActive(eEvent))
			{
				int iNumEvent = 0;
				EventChoiceTypes eEventChoice = NO_EVENT_CHOICE;
				for(int iLoop = 0; iLoop < GC.getNumEventChoiceInfos(); iLoop++)
				{
					eEventChoice = (EventChoiceTypes)iLoop;
					if(eEventChoice != NO_EVENT_CHOICE)
					{
						CvModEventChoiceInfo* pkEventChoiceInfo = GC.getEventChoiceInfo(eEventChoice);
						if(pkEventChoiceInfo != NULL)
						{
							if(GET_PLAYER(m_ePlayer).IsEventChoiceValid(eEventChoice, eEvent))
							{
								iNumEvent++;
								break;
							}
						}
					}
				}
				if(iNumEvent <= 0)
				{
					GET_PLAYER(m_ePlayer).SetEventActive(eEvent, false);
					return true;
				}
				else
				{
					return false;
				}
			}
			else
			{
				GET_PLAYER(m_ePlayer).SetEventActive(eEvent, false);
				return true;
			}
		}
		return true;
	}
	break;
	case -364200720:
	{
		CvCity* pCity = GC.getMap().plot(m_aNotifications[iIndex].m_iX, m_aNotifications[iIndex].m_iY)->getPlotCity();
		if (!pCity)
			return true;

		if (!pCity->isPendingCapture())
			return true;
	}
	break;
#endif

	default:	// don't expire
	{
		return false;
	}
	break;
	}

	return false;
}

//	---------------------------------------------------------------------------
bool CvNotifications::IsNotificationTypeEndOfTurnExpired(NotificationTypes eNotificationType, int iForSpecificEntry /*= -1*/)
{
	switch(eNotificationType)
	{
	case NOTIFICATION_POLICY:
	case NOTIFICATION_FREE_POLICY:
	case NOTIFICATION_TECH:
	case NOTIFICATION_FREE_TECH:
	case NOTIFICATION_PRODUCTION:
	case NOTIFICATION_DIPLO_VOTE:
	case NOTIFICATION_PLAYER_DEAL:
	case NOTIFICATION_PLAYER_DEAL_RECEIVED:
	case NOTIFICATION_FREE_GREAT_PERSON:
	case NOTIFICATION_FOUND_PANTHEON:
	case NOTIFICATION_FOUND_RELIGION:
	case NOTIFICATION_ENHANCE_RELIGION:
	case NOTIFICATION_SPY_STOLE_TECH:
	case NOTIFICATION_MAYA_LONG_COUNT:
	case NOTIFICATION_FAITH_GREAT_PERSON:
	case NOTIFICATION_ADD_REFORMATION_BELIEF:
	case NOTIFICATION_LEAGUE_CALL_FOR_PROPOSALS:
	case NOTIFICATION_CHOOSE_ARCHAEOLOGY:
	case NOTIFICATION_LEAGUE_CALL_FOR_VOTES:
	case NOTIFICATION_CHOOSE_IDEOLOGY:
		return false;
		break;

	// These multiplayer notifications expire at the end of the next turn.
	case NOTIFICATION_PLAYER_RECONNECTED:
	case NOTIFICATION_PLAYER_DISCONNECTED:
	case NOTIFICATION_HOST_MIGRATION:
	case NOTIFICATION_PLAYER_CONNECTING:
		if(iForSpecificEntry != -1 && m_aNotifications[iForSpecificEntry].m_iTurn == GC.getGame().getGameTurn()) //same turn as creation.
		{
			return false;
		}
		break;

	// In multiplayer, these notifications expire once they've been broadcast for the player and
	// it is at least the end of the next turn.
	// These are notifications that can occur mid-turn and are important enough that they shouldn't
	// expire until the player has seen them.
	case NOTIFICATION_UNIT_PROMOTION:
	case NOTIFICATION_CAPITAL_LOST_ACTIVE_PLAYER:
	case NOTIFICATION_CAPITAL_LOST:
	case NOTIFICATION_WAR_ACTIVE_PLAYER:
	case NOTIFICATION_WAR:
	case NOTIFICATION_PEACE_ACTIVE_PLAYER:
	case NOTIFICATION_PEACE:
	case NOTIFICATION_VICTORY:
	case NOTIFICATION_UNIT_DIED:
	case NOTIFICATION_CITY_LOST:
	case NOTIFICATION_PLAYER_KILLED:
	case NOTIFICATION_DIPLOMACY_DECLARATION:
	case NOTIFICATION_OTHER_PLAYER_NEW_ERA:
	case NOTIFICATION_MINOR_BUYOUT:
	case NOTIFICATION_LIBERATED_MAJOR_CITY:
	case NOTIFICATION_RESURRECTED_MAJOR_CIV:
	case NOTIFICATION_TURN_MODE_SEQUENTIAL:
	case NOTIFICATION_TURN_MODE_SIMULTANEOUS:
	case NOTIFICATION_PLAYER_KICKED:

	//XP1
	case NOTIFICATION_RELIGION_FOUNDED_ACTIVE_PLAYER:
	case NOTIFICATION_RELIGION_FOUNDED:
	case NOTIFICATION_PANTHEON_FOUNDED_ACTIVE_PLAYER:
	case NOTIFICATION_PANTHEON_FOUNDED:

	//XP2
	case NOTIFICATION_TRADE_ROUTE_BROKEN:
	case NOTIFICATION_REFORMATION_BELIEF_ADDED_ACTIVE_PLAYER:
	case NOTIFICATION_REFORMATION_BELIEF_ADDED:
		if(iForSpecificEntry != -1 && GC.getGame().isGameMultiPlayer() 
			&& (m_aNotifications[iForSpecificEntry].m_bNeedsBroadcast //not broadcast yet.
				|| m_aNotifications[iForSpecificEntry].m_iTurn == GC.getGame().getGameTurn())) //same turn as creation.
		{
			return false;
		}
		break;

#if defined(MOD_UI_CITY_EXPANSION)
	case NOTIFICATION_CITY_TILE:
		return !MOD_UI_CITY_EXPANSION;
		break;
#endif

	default:
		return true;
		break;
	}

	return true;
}

//	---------------------------------------------------------------------------
bool CvNotifications::IsNotificationEndOfTurnExpired(int iIndex)
{
	return IsNotificationTypeEndOfTurnExpired( m_aNotifications[iIndex].m_eNotificationType, iIndex );
}

//	---------------------------------------------------------------------------
bool CvNotifications::IsArrayFull() const
{
	int iAdjustedEndIndex = m_iNotificationsEndIndex + 1;
	if(iAdjustedEndIndex >= int(MaxNotifications))
	{
		iAdjustedEndIndex = 0;
	}

	return iAdjustedEndIndex == m_iNotificationsBeginIndex;
}

//	---------------------------------------------------------------------------
void CvNotifications::RemoveOldestNotification()
{
	// if the notification is somehow active, dismiss it
	if(!m_aNotifications[m_iNotificationsBeginIndex].m_bDismissed)
	{
		Dismiss(m_aNotifications[m_iNotificationsBeginIndex].m_iLookupIndex, /*bUserInvoked*/ false);
	}
	m_aNotifications[m_iNotificationsBeginIndex].Clear();
	IncrementBeginIndex();
}

void CvNotifications::IncrementBeginIndex()
{
	m_iNotificationsBeginIndex++;
	if(m_iNotificationsBeginIndex >= int(MaxNotifications))
	{
		m_iNotificationsBeginIndex = 0;
	}
}

void CvNotifications::IncrementEndIndex()
{
	m_iNotificationsEndIndex++;
	if(m_iNotificationsEndIndex >= int(MaxNotifications))
	{
		m_iNotificationsEndIndex = 0;
	}
}

//	---------------------------------------------------------------------------
// static
void CvNotifications::AddToPlayer(PlayerTypes ePlayer, NotificationTypes eNotificationType, const char* strMessage, const char* strSummary, int iX/*=-1*/, int iY/*=-1*/, int iGameDataIndex/*=-1*/, int iExtraGameData/*=-1*/)
{
	if(ePlayer != NO_PLAYER)
	{
		CvPlayer& kPlayer = GET_PLAYER(ePlayer);

		if(kPlayer.isLocalPlayer())
		{
			CvNotifications* pNotifications = kPlayer.GetNotifications();
			if(pNotifications)
			{
				pNotifications->Add(eNotificationType, strMessage, strSummary, iX, iY, iGameDataIndex, iExtraGameData);
			}
		}
	}
}
