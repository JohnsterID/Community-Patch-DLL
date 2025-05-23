print("This is the modded TopPanel from CBP- CSD")
-------------------------------
-- TopPanel.lua
-------------------------------

local g_activePlayerObserver = Game.GetActivePlayer();

function UpdateData()

	local iPlayerID = g_activePlayerObserver;
	if( iPlayerID >= 0 ) then
		local pPlayer = Players[iPlayerID];
		local pTeam = Teams[pPlayer:GetTeam()];
		local pCity = UI.GetHeadSelectedCity();
		
		if (pPlayer:GetNumCities() > 0) then
			
			Controls.TopPanelInfoStack:SetHide(false);
			
			if (pCity ~= nil and UI.IsCityScreenUp()) then		
				Controls.MenuButton:SetText(Locale.ToUpper(Locale.ConvertTextKey("TXT_KEY_RETURN")));
				Controls.MenuButton:SetToolTipString(Locale.ConvertTextKey("TXT_KEY_CITY_SCREEN_EXIT_TOOLTIP"));
			else
				Controls.MenuButton:SetText(Locale.ToUpper(Locale.ConvertTextKey("TXT_KEY_MENU")));
				Controls.MenuButton:SetToolTipString(Locale.ConvertTextKey("TXT_KEY_MENU_TOOLTIP"));
			end

			local strInstantYields = "[ICON_CAPITAL]";

			Controls.InstantYields:SetText(strInstantYields);
			-----------------------------
			-- Update science stats
			-----------------------------
			local strScienceText;
			
			if (Game.IsOption(GameOptionTypes.GAMEOPTION_NO_SCIENCE)) then
				strScienceText = Locale.ConvertTextKey("TXT_KEY_TOP_PANEL_SCIENCE_OFF");
			else
			
				local sciencePerTurn = pPlayer:GetScienceTimes100() / 100;
			
				-- No Science
				if (sciencePerTurn <= 0) then
					strScienceText = string.format("[COLOR:255:60:60:255]" .. Locale.ConvertTextKey("TXT_KEY_NO_SCIENCE") .. "[/COLOR]");
				-- We have science
				else
					strScienceText = string.format("+%s", sciencePerTurn);

					local iGoldPerTurn = pPlayer:CalculateGoldRateTimes100();
					
					-- Gold being deducted from our Science
					if (pPlayer:GetGoldTimes100() + iGoldPerTurn < 0) then
						strScienceText = "[COLOR:255:60:0:255]" .. strScienceText .. "[/COLOR]";
					-- Normal Science state
					else
						strScienceText = "[COLOR:33:190:247:255]" .. strScienceText .. "[/COLOR]";
					end
				end
			
				strScienceText = "[ICON_RESEARCH]" .. strScienceText;
			end
			
			Controls.SciencePerTurn:SetText(strScienceText);
			
			-----------------------------
			-- Update gold stats
			-----------------------------
			local iTotalGold = pPlayer:GetGoldTimes100() / 100;
			local iGoldPerTurn = pPlayer:CalculateGoldRateTimes100() / 100;
			
			-- Accounting for positive or negative GPT - there's obviously a better way to do this.  If you see this comment and know how, it's up to you ;)
			-- Text is White when you can buy a Plot
			--if (iTotalGold >= pPlayer:GetBuyPlotCost(-1,-1)) then
				--if (iGoldPerTurn >= 0) then
					--strGoldStr = string.format("[COLOR:255:255:255:255]%i (+%i)[/COLOR]", iTotalGold, iGoldPerTurn)
				--else
					--strGoldStr = string.format("[COLOR:255:255:255:255]%i (%i)[/COLOR]", iTotalGold, iGoldPerTurn)
				--end
			---- Text is Yellow or Red when you can't buy a Plot
			--else
			local strGoldStr = Locale.ConvertTextKey("TXT_KEY_TOP_PANEL_GOLD", iTotalGold, iGoldPerTurn);
			--end
			
			Controls.GoldPerTurn:SetText(strGoldStr);

			-----------------------------
			-- Update international trade routes
			-----------------------------
			local iUsedTradeRoutes = pPlayer:GetNumInternationalTradeRoutesUsed();
			local iAvailableTradeRoutes = pPlayer:GetNumInternationalTradeRoutesAvailable();
			local strInternationalTradeRoutes = Locale.ConvertTextKey("TXT_KEY_TOP_PANEL_INTERNATIONAL_TRADE_ROUTES", iUsedTradeRoutes, iAvailableTradeRoutes);
			Controls.InternationalTradeRoutes:SetText(strInternationalTradeRoutes);
			
			-----------------------------
			-- Update Happiness
			-----------------------------
			local strHappiness;
			
			if (Game.IsOption(GameOptionTypes.GAMEOPTION_NO_HAPPINESS) or not pPlayer:IsAlive()) then
				strHappiness = Locale.ConvertTextKey("TXT_KEY_TOP_PANEL_HAPPINESS_OFF");
			else
				local happypop = pPlayer:GetHappinessFromCitizenNeeds()
				local unhappypop = pPlayer:GetUnhappinessFromCitizenNeeds()
				local percent = pPlayer:GetExcessHappiness()
				
				local percentString = ""
				if percent >= 75 then --ecstatic
					percentString = "[ICON_CITIZEN] [COLOR_FONT_GREEN]"..percent.."%[ENDCOLOR]"
				elseif percent < 75 and percent > 60 then -- content
					percentString = "[ICON_CITIZEN] [COLOR_POSITIVE_TEXT]"..percent.."%[ENDCOLOR]"
				elseif percent <= 60 and percent >= 50 then -- swing vote
					percentString = "[ICON_CITIZEN] [COLOR_SELECTED_TEXT]"..percent.."%[ENDCOLOR]"
				elseif percent < 50 and percent >= 35 then  -- unhappy
					percentString = "[ICON_CITIZEN] [COLOR_FONT_RED]"..percent.."%[ENDCOLOR]"
				elseif percent < 35 and percent >= 20 then  -- very unhappy
					percentString = "[ICON_CITIZEN] [COLOR_RED]"..percent.."%[ENDCOLOR]"
				else -- 20<= winter palace vibes
					percentString = "[ICON_CITIZEN] [COLOR_RED]"..percent.."%[ENDCOLOR]"
				end

				strHappiness = Locale.ConvertTextKey("TXT_KEY_HAPPINESS_TOP_PANEL_VP", percentString, unhappypop, happypop);
			end
			
			Controls.HappinessString:SetText(strHappiness);
			
			-----------------------------
			-- Update Golden Age Info
			-----------------------------
			local strGoldenAgeStr;

			if (Game.IsOption(GameOptionTypes.GAMEOPTION_NO_HAPPINESS)) then
				strGoldenAgeStr = Locale.ConvertTextKey("TXT_KEY_TOP_PANEL_GOLDEN_AGES_OFF");
			else
				if (pPlayer:GetGoldenAgeTurns() > 0) then
				    if (pPlayer:GetGoldenAgeTourismModifier() > 0) then
						strGoldenAgeStr = string.format(Locale.ToUpper(Locale.ConvertTextKey("TXT_KEY_UNIQUE_GOLDEN_AGE_ANNOUNCE")) .. " (%i)", pPlayer:GetGoldenAgeTurns());
					else
						strGoldenAgeStr = string.format(Locale.ToUpper(Locale.ConvertTextKey("TXT_KEY_GOLDEN_AGE_ANNOUNCE")) .. " (%i)", pPlayer:GetGoldenAgeTurns());
					end
				else
					iChange = pPlayer:GetHappinessForGAP() + pPlayer:GetGAPFromReligion() + pPlayer:GetGAPFromTraits() + pPlayer:GetGAPFromCitiesTimes100() / 100;
					if(iChange > 0) then
						strGoldenAgeStr = string.format("%s/%i (+%s)", pPlayer:GetGoldenAgeProgressMeterTimes100() / 100, pPlayer:GetGoldenAgeProgressThreshold(), iChange);
					elseif(iChange < 0) then
						strGoldenAgeStr = string.format("%s/%i (%s)", pPlayer:GetGoldenAgeProgressMeterTimes100() / 100, pPlayer:GetGoldenAgeProgressThreshold(), iChange);
					else
						strGoldenAgeStr = string.format("%i/%i", pPlayer:GetGoldenAgeProgressMeter(), pPlayer:GetGoldenAgeProgressThreshold());
					end
					
				end
			
				strGoldenAgeStr = "[ICON_GOLDEN_AGE][COLOR:255:255:255:255]" .. strGoldenAgeStr .. "[/COLOR]";
			end
			
			Controls.GoldenAgeString:SetText(strGoldenAgeStr);
			
			-----------------------------
			-- Update Culture
			-----------------------------

			local strCultureStr;
			
			if (Game.IsOption(GameOptionTypes.GAMEOPTION_NO_POLICIES)) then
				strCultureStr = Locale.ConvertTextKey("TXT_KEY_TOP_PANEL_POLICIES_OFF");
			else
			
				if (pPlayer:GetNextPolicyCost() > 0) then
					strCultureStr = string.format("%s/%i (+%s)", pPlayer:GetJONSCultureTimes100() / 100, pPlayer:GetNextPolicyCost(), pPlayer:GetTotalJONSCulturePerTurnTimes100() / 100);
				else
					strCultureStr = string.format("%s (+%s)", pPlayer:GetJONSCultureTimes100() / 100, pPlayer:GetTotalJONSCulturePerTurnTimes100() / 100);
				end
			
				strCultureStr = "[ICON_CULTURE][COLOR:255:0:255:255]" .. strCultureStr .. "[/COLOR]";
			end
			
			Controls.CultureString:SetText(strCultureStr);
			
			-----------------------------
			-- Update Tourism
			-----------------------------
			local strTourism;
			strTourism = string.format("[ICON_TOURISM] +%s", pPlayer:GetTourism() / 100);
			Controls.TourismString:SetText(strTourism);
			
			-----------------------------
			-- Update Faith
			-----------------------------
			local strFaithStr;
			if (Game.IsOption(GameOptionTypes.GAMEOPTION_NO_RELIGION)) then
				strFaithStr = Locale.ConvertTextKey("TXT_KEY_TOP_PANEL_RELIGION_OFF");
			else
				strFaithStr = string.format("%s (+%s)", pPlayer:GetFaithTimes100() / 100, pPlayer:GetTotalFaithPerTurnTimes100() / 100);
				strFaithStr = "[ICON_PEACE]" .. strFaithStr;
			end
			Controls.FaithString:SetText(strFaithStr);
			
			-----------------------------
			-- Update Units Supplied
			-----------------------------
			local iUnitsSupplied = pPlayer:GetNumUnitsSupplied();
			local iUnitsTotal = pPlayer:GetNumUnitsToSupply();

			local strSupplyStr = "";
			if(iUnitsTotal > iUnitsSupplied) then
				strSupplyStr = string.format(" [COLOR_NEGATIVE_TEXT](%i/%i)[ENDCOLOR]", iUnitsTotal, iUnitsSupplied);
			else
				strSupplyStr = string.format(" (%i/%i)", iUnitsTotal, iUnitsSupplied);
			end
			strSupplyStr = "[ICON_WAR]" .. strSupplyStr;

			Controls.UnitSupplyString:SetText(strSupplyStr);
	
			-----------------------------
			-- Update Resources
			-----------------------------
			local pResource;
			local bShowResource;
			local iNumAvailable;
			local iNumUsed;
			local iNumTotal;
			
			local strResourceText = "";
			local strTempText = "";
			
			for pResource in GameInfo.Resources() do
				local iResourceLoop = pResource.ID;
				
				if (Game.GetResourceUsageType(iResourceLoop) == ResourceUsageTypes.RESOURCEUSAGE_STRATEGIC) then
					
					bShowResource = false;
					
					if (pPlayer:IsResourceRevealed(iResourceLoop)) then
						if (pPlayer:IsResourceCityTradeable(iResourceLoop)) then
							bShowResource = true;
						end
					end
					
					iNumAvailable = pPlayer:GetNumResourceAvailable(iResourceLoop, true);
					iNumUsed = pPlayer:GetNumResourceUsed(iResourceLoop);
					iNumTotal = pPlayer:GetNumResourceTotal(iResourceLoop, true);
					
					if (iNumTotal > 0 or iNumUsed > 0) then
						bShowResource = true;
					end
							
					if (bShowResource) then
						local text = Locale.ConvertTextKey(pResource.IconString);
						strTempText = string.format("%i %s   ", iNumAvailable, text);
						
						-- Colorize for amount available
						if (iNumAvailable > 0) then
							strTempText = "[COLOR_POSITIVE_TEXT]" .. strTempText .. "[ENDCOLOR]";
						elseif (iNumAvailable < 0) then
							strTempText = "[COLOR_WARNING_TEXT]" .. strTempText .. "[ENDCOLOR]";
						end
						
						strResourceText = strResourceText .. strTempText;
					end
				end
			end
			
			Controls.ResourceString:SetText(strResourceText);
			
			-----------------------------
			-- Update Spy Points
			-----------------------------
			--[[local strSpiesStr;
			if (Game.IsOption("GAMEOPTION_NO_ESPIONAGE") or Game.GetSpyThreshold() == 0) then
				strSpiesStr = "";
			else
				strSpiesStr = "[ICON_SPY]"; --.. string.format(" %i/%i", pPlayer:GetSpyPoints(), Game.GetSpyThreshold());
			end
			Controls.SpyPointsString:SetText(strSpiesStr);]]
			
		-- No Cities, so hide science
		else
			
			Controls.TopPanelInfoStack:SetHide(true);
			
		end
		
		-- Update turn counter
		local turn = Locale.ConvertTextKey("TXT_KEY_TP_TURN_COUNTER", Game.GetGameTurn());
		Controls.CurrentTurn:SetText(turn);
		
		-- Update date
		local date;
		local traditionalDate = Game.GetTurnString();
		
		if (pPlayer:IsUsingMayaCalendar()) then
			date = pPlayer:GetMayaCalendarString();
			local toolTipString = Locale.ConvertTextKey("TXT_KEY_MAYA_DATE_TOOLTIP", pPlayer:GetMayaCalendarLongString(), traditionalDate);
			Controls.CurrentDate:SetToolTipString(toolTipString);
		else
			date = traditionalDate;
		end
		
		Controls.CurrentDate:SetText(date);
	else
		Controls.TopPanelInfoStack:SetHide(true);
	end
end

function OnTopPanelDirty()
	UpdateData();
end

-------------------------------------------------
-------------------------------------------------
function OnCivilopedia()	
	-- In City View, return to main game
	--if (UI.GetHeadSelectedCity() ~= nil) then
		--Events.SerialEventExitCityScreen();
	--end
	--
	-- opens the Civilopedia without changing its current state
	Events.SearchForPediaEntry("");
end
Controls.CivilopediaButton:RegisterCallback( Mouse.eLClick, OnCivilopedia );


-------------------------------------------------
-------------------------------------------------
function OnMenu()
	
	-- In City View, return to main game
	if (UI.GetHeadSelectedCity() ~= nil) then
		Events.SerialEventExitCityScreen();
		--UI.SetInterfaceMode(InterfaceModeTypes.INTERFACEMODE_SELECTION);
	-- In Main View, open Menu Popup
	else
	    UIManager:QueuePopup( LookUpControl( "/InGame/GameMenu" ), PopupPriority.InGameMenu );
	end
end
Controls.MenuButton:RegisterCallback( Mouse.eLClick, OnMenu );


-------------------------------------------------
-------------------------------------------------
function OnCultureClicked()

	local isObserver = Players[Game.GetActivePlayer()]:IsObserver();
	Events.SerialEventGameMessagePopup( { Type = ButtonPopupTypes.BUTTONPOPUP_CHOOSEPOLICY, Data3 = isObserver and 1 or 0, Data4 = g_activePlayerObserver } );

end
Controls.CultureString:RegisterCallback( Mouse.eLClick, OnCultureClicked );


-------------------------------------------------
-------------------------------------------------
function OnTechClicked()

	local isObserver = Players[Game.GetActivePlayer()]:IsObserver();
	Events.SerialEventGameMessagePopup( { Type = ButtonPopupTypes.BUTTONPOPUP_TECH_TREE, Data2 = -1, Data4 = isObserver and 1 or 0, Data5 = g_activePlayerObserver} );

end
Controls.SciencePerTurn:RegisterCallback( Mouse.eLClick, OnTechClicked );

-------------------------------------------------
-------------------------------------------------
function OnTourismClicked()
	
	Events.SerialEventGameMessagePopup( { Type = ButtonPopupTypes.BUTTONPOPUP_CULTURE_OVERVIEW, Data2 = 4 } );

end
Controls.TourismString:RegisterCallback( Mouse.eLClick, OnTourismClicked );


-------------------------------------------------
-------------------------------------------------
function OnFaithClicked()
	
	Events.SerialEventGameMessagePopup( { Type = ButtonPopupTypes.BUTTONPOPUP_RELIGION_OVERVIEW } );

end
Controls.FaithString:RegisterCallback( Mouse.eLClick, OnFaithClicked );

-------------------------------------------------
-------------------------------------------------
function OnSupplyClicked()
	
	Events.SerialEventGameMessagePopup( { Type = ButtonPopupTypes.BUTTONPOPUP_MILITARY_OVERVIEW } );

end
Controls.UnitSupplyString:RegisterCallback( Mouse.eLClick, OnSupplyClicked );



-------------------------------------------------
-------------------------------------------------
function OnGoldClicked()
	
	Events.SerialEventGameMessagePopup( { Type = ButtonPopupTypes.BUTTONPOPUP_ECONOMIC_OVERVIEW } );

end
Controls.GoldPerTurn:RegisterCallback( Mouse.eLClick, OnGoldClicked );

-------------------------------------------------
-------------------------------------------------
function OnTradeRouteClicked()
	
	Events.SerialEventGameMessagePopup( { Type = ButtonPopupTypes.BUTTONPOPUP_TRADE_ROUTE_OVERVIEW } );

end
Controls.InternationalTradeRoutes:RegisterCallback( Mouse.eLClick, OnTradeRouteClicked );


-------------------------------------------------
-- TOOLTIPS
-------------------------------------------------


-- Tooltip init
function DoInitTooltips()
	Controls.SciencePerTurn:SetToolTipCallback( ScienceTipHandler );
	Controls.GoldPerTurn:SetToolTipCallback( GoldTipHandler );
	Controls.HappinessString:SetToolTipCallback( HappinessTipHandler );
	Controls.GoldenAgeString:SetToolTipCallback( GoldenAgeTipHandler );
	Controls.CultureString:SetToolTipCallback( CultureTipHandler );
	Controls.TourismString:SetToolTipCallback( TourismTipHandler );
	Controls.FaithString:SetToolTipCallback( FaithTipHandler );
	Controls.ResourceString:SetToolTipCallback( ResourcesTipHandler );
	Controls.InternationalTradeRoutes:SetToolTipCallback( InternationalTradeRoutesTipHandler );
	Controls.UnitSupplyString:SetToolTipCallback( UnitSupplyHandler );
	Controls.InstantYields:SetToolTipCallback( InstantYieldHandler );
	--Controls.SpyPointsString:SetToolTipCallback( SpyPointsTipHandler );
end

-- Science Tooltip
local tipControlTable = {};
TTManager:GetTypeControlTable( "TooltipTypeTopPanel", tipControlTable );
function ScienceTipHandler( control )

	local strText = "";
	
	if (Game.IsOption(GameOptionTypes.GAMEOPTION_NO_SCIENCE)) then
		strText = Locale.ConvertTextKey("TXT_KEY_TOP_PANEL_SCIENCE_OFF_TOOLTIP");
	else
	
		local iPlayerID = g_activePlayerObserver;
		local pPlayer = Players[iPlayerID];
		local pTeam = Teams[pPlayer:GetTeam()];
		local pCity = UI.GetHeadSelectedCity();
	
		local iSciencePerTurn = pPlayer:GetScienceTimes100() / 100;
	
		if (pPlayer:IsAnarchy()) then
			strText = strText .. Locale.ConvertTextKey("TXT_KEY_TP_ANARCHY", pPlayer:GetAnarchyNumTurns());
			strText = strText .. "[NEWLINE][NEWLINE]";
		end
	
		-- Science
		if (not OptionsManager.IsNoBasicHelp()) then
			strText = strText .. Locale.ConvertTextKey("TXT_KEY_TP_SCIENCE", iSciencePerTurn);
		
			if (pPlayer:GetNumCities() > 0) then
				strText = strText .. "[NEWLINE][NEWLINE]";
			end
		end
	
		local bFirstEntry = true;
	
		-- Science LOSS from Budget Deficits
		local iScienceFromBudgetDeficit = pPlayer:GetScienceFromBudgetDeficitTimes100();
		if (iScienceFromBudgetDeficit ~= 0) then
		
			-- Add separator for non-initial entries
			if (bFirstEntry) then
				bFirstEntry = false;
			else
				strText = strText .. "[NEWLINE]";
			end

			strText = strText .. Locale.ConvertTextKey("TXT_KEY_TP_SCIENCE_FROM_BUDGET_DEFICIT", iScienceFromBudgetDeficit / 100);
			strText = strText .. "[NEWLINE]";
		end
	
		-- Science from Cities
		local iScienceFromCities = pPlayer:GetScienceFromCitiesTimes100(true);
		if (iScienceFromCities ~= 0) then
		
			-- Add separator for non-initial entries
			if (bFirstEntry) then
				bFirstEntry = false;
			else
				strText = strText .. "[NEWLINE]";
			end

			strText = strText .. Locale.ConvertTextKey("TXT_KEY_TP_SCIENCE_FROM_CITIES", iScienceFromCities / 100);
		end
	
		-- Science from Trade Routes
		local iScienceFromTrade = pPlayer:GetScienceFromCitiesTimes100(false) - iScienceFromCities;
		if (iScienceFromTrade ~= 0) then
			if (bFirstEntry) then
				bFirstEntry = false;
			else
				strText = strText .. "[NEWLINE]";
			end
			
			strText = strText .. Locale.ConvertTextKey("TXT_KEY_TP_SCIENCE_FROM_ITR", iScienceFromTrade / 100);
		end
	
		-- Science from Other Players
		local iScienceFromOtherPlayers = pPlayer:GetScienceFromOtherPlayersTimes100();
		if (iScienceFromOtherPlayers ~= 0) then
		
			-- Add separator for non-initial entries
			if (bFirstEntry) then
				bFirstEntry = false;
			else
				strText = strText .. "[NEWLINE]";
			end

			strText = strText .. Locale.ConvertTextKey("TXT_KEY_TP_SCIENCE_FROM_MINORS", iScienceFromOtherPlayers / 100);
		end
	
		-- Science from Happiness
		local iScienceFromHappiness = pPlayer:GetScienceFromHappinessTimes100();
		if (iScienceFromHappiness ~= 0) then
			
			-- Add separator for non-initial entries
			if (bFirstEntry) then
				bFirstEntry = false;
			else
				strText = strText .. "[NEWLINE]";
			end
	
			strText = strText .. Locale.ConvertTextKey("TXT_KEY_TP_SCIENCE_FROM_HAPPINESS", iScienceFromHappiness / 100);
		end
	
		-- Science from Research Agreements
		local iScienceFromRAs = pPlayer:GetScienceFromResearchAgreementsTimes100();
		if (iScienceFromRAs ~= 0) then
		
			-- Add separator for non-initial entries
			if (bFirstEntry) then
				bFirstEntry = false;
			else
				strText = strText .. "[NEWLINE]";
			end
	
			strText = strText .. Locale.ConvertTextKey("TXT_KEY_TP_SCIENCE_FROM_RESEARCH_AGREEMENTS", iScienceFromRAs / 100);
		end
		
		-- C4DF
		-- Science from Vassals
		local iScienceFromVassals = pPlayer:GetYieldPerTurnFromVassalsTimes100(YieldTypes.YIELD_SCIENCE) / 100;
		if (iScienceFromVassals ~= 0) then
		
			-- Add separator for non-initial entries
			if (bFirstEntry) then
				bFirstEntry = false;
			else
				strText = strText .. "[NEWLINE]";
			end
	
			strText = strText .. Locale.ConvertTextKey("TXT_KEY_TP_SCIENCE_VASSALS", iScienceFromVassals);
		end

		-- Science from Allies (CSD MOD)
		local iScienceFromAllies = pPlayer:GetScienceRateFromMinorAllies();
		if (iScienceFromAllies ~= 0) then
		
			-- Add separator for non-initial entries
			if (bFirstEntry) then
				bFirstEntry = false;
			else
				strText = strText .. "[NEWLINE]";
			end
	
			strText = strText .. Locale.ConvertTextKey("TXT_KEY_MINOR_SCIENCE_FROM_LEAGUE_ALLIES", iScienceFromAllies);
		end
		
-- CBP
		-- Science from Annexed Minors
		local iScienceFromAnnexedMinors = pPlayer:GetSciencePerTurnFromAnnexedMinors();
		if (iScienceFromAnnexedMinors ~= 0) then
		
			-- Add separator for non-initial entries
			if (bFirstEntry) then
				bFirstEntry = false;
			else
				strText = strText .. "[NEWLINE]";
			end
	
			strText = strText .. Locale.ConvertTextKey("TXT_KEY_TP_SCIENCE_FROM_ANNEXED_MINORS", iScienceFromAnnexedMinors);
		end
-- END

-- CBP Science from Religion
		local iScienceFromReligion = pPlayer:GetYieldPerTurnFromReligion(YieldTypes.YIELD_SCIENCE);
		if (iScienceFromReligion ~= 0) then
		
			-- Add separator for non-initial entries
			if (bFirstEntry) then
				bFirstEntry = false;
			else
				strText = strText .. "[NEWLINE]";
			end
	
			strText = strText .. Locale.ConvertTextKey("TXT_KEY_SCIENCE_FROM_RELIGION", iScienceFromReligion);
		end
		local iScienceFromMinors = pPlayer:GetSciencePerTurnFromMinorCivs();
		if (iScienceFromMinors ~= 0) then
		
			-- Add separator for non-initial entries
			if (bFirstEntry) then
				bFirstEntry = false;
			else
				strText = strText .. "[NEWLINE]";
			end
	
			strText = strText .. Locale.ConvertTextKey("TXT_KEY_SCIENCE_FROM_MINORS", iScienceFromMinors);
		end
		
		-- Science from Espionage
		local iScienceFromEspionage = pPlayer:GetYieldPerTurnFromEspionageEvents(YieldTypes.YIELD_SCIENCE, true) - pPlayer:GetYieldPerTurnFromEspionageEvents(YieldTypes.YIELD_SCIENCE, false) + (pPlayer:GetSciencePerTurnFromPassiveSpyBonusesTimes100() / 100);
		if (iScienceFromEspionage > 0) then
		
			-- Add separator for non-initial entries
			if (bFirstEntry) then
				bFirstEntry = false;
			else
				strText = strText .. "[NEWLINE]";
			end
	
			strText = strText .. Locale.ConvertTextKey("TXT_KEY_TP_SCIENCE_FROM_ESPIONAGE_POSITIVE", iScienceFromEspionage);
		elseif (iScienceFromEspionage < 0) then
		
			-- Add separator for non-initial entries
			if (bFirstEntry) then
				bFirstEntry = false;
			else
				strText = strText .. "[NEWLINE]";
			end
	
			strText = strText .. Locale.ConvertTextKey("TXT_KEY_TP_SCIENCE_FROM_ESPIONAGE_NEGATIVE", iScienceFromEspionage);
		end
		
		-- Let people know that building more cities makes techs harder to get
		if (not OptionsManager.IsNoBasicHelp()) then
			strText = strText .. "[NEWLINE][NEWLINE]";
			strText = strText .. Locale.ConvertTextKey("TXT_KEY_TP_TECH_CITY_COST", Game.GetNumCitiesTechCostMod());
		end
	end
	
	tipControlTable.TooltipLabel:SetText( strText );
	tipControlTable.TopPanelMouseover:SetHide(false);
    
    -- Autosize tooltip
    tipControlTable.TopPanelMouseover:DoAutoSize();
	
end

-- Gold Tooltip
function GoldTipHandler( control )

	local strText = "";
	local iPlayerID = g_activePlayerObserver;
	local pPlayer = Players[iPlayerID];
	local pTeam = Teams[pPlayer:GetTeam()];
	local pCity = UI.GetHeadSelectedCity();
	
	local iTotalGold = pPlayer:GetGoldTimes100() / 100;

	local iGoldPerTurnFromOtherPlayers = pPlayer:GetGoldPerTurnFromDiplomacy();
	local iGoldPerTurnToOtherPlayers = 0;
	if (iGoldPerTurnFromOtherPlayers < 0) then
		iGoldPerTurnToOtherPlayers = -iGoldPerTurnFromOtherPlayers;
		iGoldPerTurnFromOtherPlayers = 0;
	end
	
	local iGoldPerTurnFromReligion = pPlayer:GetGoldPerTurnFromReligion();

	local fTradeRouteGold = (pPlayer:GetGoldFromCitiesTimes100() - pPlayer:GetGoldFromCitiesMinusTradeRoutesTimes100()) / 100;
	local fGoldPerTurnFromCities = pPlayer:GetGoldFromCitiesMinusTradeRoutesTimes100() / 100;
	local fCityConnectionGold = pPlayer:GetCityConnectionGoldTimes100() / 100;
	--local fInternationalTradeRouteGold = pPlayer:GetGoldPerTurnFromTradeRoutesTimes100() / 100;
	local fTraitGold = pPlayer:GetGoldPerTurnFromTraits();
	-- CBP
	local iInternalRouteGold = pPlayer:GetInternalTradeRouteGoldBonus();
	local iMinorGold = pPlayer:GetGoldPerTurnFromMinorCivs();
	local iAnnexedMinorsGold = pPlayer:GetGoldPerTurnFromAnnexedMinors();
	-- END
-- C4DF
	-- Gold from Vassals
	local iGoldFromVassals = pPlayer:GetYieldPerTurnFromVassalsTimes100(YieldTypes.YIELD_GOLD) / 100;
	local iGoldFromVassalTax = math.floor(pPlayer:GetMyShareOfVassalTaxes() / 100);
	-- Gold from Espionage
	local iGoldFromEspionageIncoming = pPlayer:GetYieldPerTurnFromEspionageEvents(YieldTypes.YIELD_GOLD, true);
	
-- END
-- C4DF CHANGE
	local fTotalIncome = fGoldPerTurnFromCities + iGoldPerTurnFromOtherPlayers + fCityConnectionGold + iGoldPerTurnFromReligion + fTradeRouteGold + fTraitGold + iMinorGold + iInternalRouteGold + iAnnexedMinorsGold;
	if (iGoldFromEspionageIncoming > 0) then
		fTotalIncome = fTotalIncome + iGoldFromEspionageIncoming;
	end
	if (iGoldFromVassals > 0) then
		fTotalIncome = fTotalIncome + iGoldFromVassals;
	end
	if (iGoldFromVassalTax > 0) then
		fTotalIncome = fTotalIncome + iGoldFromVassalTax;
	end
-- C4DF END CHANGE	
	if (pPlayer:IsAnarchy()) then
		strText = strText .. Locale.ConvertTextKey("TXT_KEY_TP_ANARCHY", pPlayer:GetAnarchyNumTurns());
		strText = strText .. "[NEWLINE][NEWLINE]";
	end
	
	if (not OptionsManager.IsNoBasicHelp()) then
		strText = strText .. Locale.ConvertTextKey("TXT_KEY_TP_AVAILABLE_GOLD", iTotalGold);
		strText = strText .. "[NEWLINE][NEWLINE]";
	end
	
	strText = strText .. "[COLOR:150:255:150:255]";
	strText = strText .. "+" .. Locale.ConvertTextKey("TXT_KEY_TP_TOTAL_INCOME", fTotalIncome);
	strText = strText .. "[NEWLINE]  [ICON_BULLET]" .. Locale.ConvertTextKey("TXT_KEY_TP_CITY_OUTPUT", fGoldPerTurnFromCities);
	strText = strText .. "[NEWLINE]  [ICON_BULLET]" .. Locale.ConvertTextKey("TXT_KEY_TP_GOLD_FROM_CITY_CONNECTIONS", math.floor(fCityConnectionGold));
	strText = strText .. "[NEWLINE]  [ICON_BULLET]" .. Locale.ConvertTextKey("TXT_KEY_TP_GOLD_FROM_ITR", math.floor(fTradeRouteGold));
	if (math.floor(fTraitGold) > 0) then
		strText = strText .. "[NEWLINE]  [ICON_BULLET]" .. Locale.ConvertTextKey("TXT_KEY_TP_GOLD_FROM_TRAITS", math.floor(fTraitGold));
	end
	--CBP
	if (iInternalRouteGold > 0) then
		strText = strText .. "[NEWLINE]  [ICON_BULLET]" .. Locale.ConvertTextKey("TXT_KEY_TP_GOLD_FROM_INTERNAL_TRADE", iInternalRouteGold);
	end
	--END
	if (iGoldPerTurnFromOtherPlayers > 0) then
		strText = strText .. "[NEWLINE]  [ICON_BULLET]" .. Locale.ConvertTextKey("TXT_KEY_TP_GOLD_FROM_OTHERS", iGoldPerTurnFromOtherPlayers);
	end
	if (iGoldPerTurnFromReligion > 0) then
		strText = strText .. "[NEWLINE]  [ICON_BULLET]" .. Locale.ConvertTextKey("TXT_KEY_TP_GOLD_FROM_RELIGION", iGoldPerTurnFromReligion);
	end
-- C4DF
	-- Gold from Vassals
	if (iGoldFromVassals > 0) then
		strText = strText .. "[NEWLINE]  [ICON_BULLET]" .. Locale.ConvertTextKey("TXT_KEY_TP_GOLD_VASSALS", iGoldFromVassals);
	end

	-- Gold from Vassal Tax
	if (iGoldFromVassalTax > 0) then
		strText = strText .. "[NEWLINE]  [ICON_BULLET]" .. Locale.ConvertTextKey("TXT_KEY_TP_GOLD_VASSAL_TAX", iGoldFromVassalTax);
	end
--  END
-- COMMUNITY PATCH CHANGE
	if (iMinorGold > 0) then
		strText = strText .. "[NEWLINE]  [ICON_BULLET]" .. Locale.ConvertTextKey("TXT_KEY_TP_GOLD_FROM_MINORS", iMinorGold);
	end
	if (iAnnexedMinorsGold > 0) then
		strText = strText .. "[NEWLINE]  [ICON_BULLET]" .. Locale.ConvertTextKey("TXT_KEY_TP_GOLD_FROM_ANNEXED_MINORS", iAnnexedMinorsGold);
	end
	if (iGoldFromEspionageIncoming > 0) then
		strText = strText .. "[NEWLINE]  [ICON_BULLET]" .. Locale.ConvertTextKey("TXT_KEY_TP_GOLD_FROM_ESPIONAGE_INCOMING", iGoldFromEspionageIncoming);
	end
--END
	strText = strText .. "[/COLOR]";
	
	local iUnitCost = pPlayer:CalculateUnitCost();
	local iBuildingMaintenance = pPlayer:GetBuildingGoldMaintenance();
	local iImprovementMaintenance = pPlayer:GetImprovementGoldMaintenance();
-- BEGIN C4DF
	local iExpenseFromVassalTaxes = pPlayer:GetExpensePerTurnFromVassalTaxes();
	local iVassalMaintenance = pPlayer:GetVassalGoldMaintenance();
	local iExpenseFromEspionage = pPlayer:GetYieldPerTurnFromEspionageEvents(YieldTypes.YIELD_GOLD, false);
-- END C4DF
	local iTotalExpenses = iUnitCost + iBuildingMaintenance + iImprovementMaintenance + iGoldPerTurnToOtherPlayers;
-- BEGIN C4DF
	if (iVassalMaintenance > 0) then
		iTotalExpenses = iTotalExpenses + iVassalMaintenance;
	end
	if (iExpenseFromVassalTaxes > 0) then
		iTotalExpenses = iTotalExpenses + iExpenseFromVassalTaxes;
	end
	if(iExpenseFromEspionage > 0) then
		iTotalExpenses = iTotalExpenses + iExpenseFromEspionage;
	end
-- END C4DF
--END
	strText = strText .. "[NEWLINE]";
	strText = strText .. "[COLOR:255:150:150:255]";
	strText = strText .. "[NEWLINE]-" .. Locale.ConvertTextKey("TXT_KEY_TP_TOTAL_EXPENSES", iTotalExpenses);
	if (iUnitCost ~= 0) then
		strText = strText .. "[NEWLINE]  [ICON_BULLET]" .. Locale.ConvertTextKey("TXT_KEY_TP_UNIT_MAINT", iUnitCost);
	end
	if (iBuildingMaintenance ~= 0) then
		strText = strText .. "[NEWLINE]  [ICON_BULLET]" .. Locale.ConvertTextKey("TXT_KEY_TP_GOLD_BUILDING_MAINT", iBuildingMaintenance);
	end
	if (iImprovementMaintenance ~= 0) then
		strText = strText .. "[NEWLINE]  [ICON_BULLET]" .. Locale.ConvertTextKey("TXT_KEY_TP_GOLD_TILE_MAINT", iImprovementMaintenance);
	end
	if (iGoldPerTurnToOtherPlayers > 0) then
		strText = strText .. "[NEWLINE]  [ICON_BULLET]" .. Locale.ConvertTextKey("TXT_KEY_TP_GOLD_TO_OTHERS", iGoldPerTurnToOtherPlayers);
	end
-- COMMUNITY PATCH CHANGE
--END
-- C4DF
	if (iVassalMaintenance > 0) then
		strText = strText .. "[NEWLINE]  [ICON_BULLET]" .. Locale.ConvertTextKey("TXT_KEY_TP_GOLD_VASSAL_MAINT", iVassalMaintenance);
	end

	-- Gold from Vassal Tax
	if (iExpenseFromVassalTaxes > 0) then
		strText = strText .. "[NEWLINE]  [ICON_BULLET]" .. Locale.ConvertTextKey("TXT_KEY_TP_GOLD_VASSAL_TAX", iExpenseFromVassalTaxes);
	end
	-- Gold From Espionage
	if (iExpenseFromEspionage > 0) then
		strText = strText .. "[NEWLINE]  [ICON_BULLET]" .. Locale.ConvertTextKey("TXT_KEY_TP_GOLD_FROM_ESPIONAGE_OUTGOING", iExpenseFromEspionage);
	end
--  END
	strText = strText .. "[/COLOR]";
	
	if (fTotalIncome + iTotalGold < 0) then
		strText = strText .. "[NEWLINE][COLOR:255:60:60:255]" .. Locale.ConvertTextKey("TXT_KEY_TP_LOSING_SCIENCE_FROM_DEFICIT") .. "[/COLOR]";
	end
	
	-- Basic explanation of Happiness
	if (not OptionsManager.IsNoBasicHelp()) then
		strText = strText .. "[NEWLINE][NEWLINE]";
		strText = strText ..  Locale.ConvertTextKey("TXT_KEY_TP_GOLD_EXPLANATION");
	end
	
	--Controls.GoldPerTurn:SetToolTipString(strText);
	
	tipControlTable.TooltipLabel:SetText( strText );
	tipControlTable.TopPanelMouseover:SetHide(false);
    
    -- Autosize tooltip
    tipControlTable.TopPanelMouseover:DoAutoSize();
	
end

-- Happiness Tooltip
function HappinessTipHandler( control )

	local strText = "";
	local iPlayerID = g_activePlayerObserver;
	local pPlayer = Players[iPlayerID];
	
	if (Game.IsOption(GameOptionTypes.GAMEOPTION_NO_HAPPINESS) or not pPlayer:IsAlive()) then
		strText = Locale.ConvertTextKey("TXT_KEY_TOP_PANEL_HAPPINESS_OFF_TOOLTIP");
	else
		if (pPlayer:IsEmpireSuperUnhappy() and not Game.IsOption(GameOptionTypes.GAMEOPTION_NO_BARBARIANS)) then
			strText = strText .. "[COLOR_RED]" .. Locale.ConvertTextKey("TXT_KEY_TP_EMPIRE_SUPER_UNHAPPY");
		elseif (pPlayer:IsEmpireSuperUnhappy() and Game.IsOption(GameOptionTypes.GAMEOPTION_NO_BARBARIANS)) then
			strText = strText .. "[COLOR_RED]" .. Locale.ConvertTextKey("TXT_KEY_TP_EMPIRE_SUPER_UNHAPPY_NO_REBELS");
		elseif (pPlayer:IsEmpireVeryUnhappy() and not Game.IsOption(GameOptionTypes.GAMEOPTION_NO_BARBARIANS)) then
			strText = strText .. "[COLOR_RED]" .. Locale.ConvertTextKey("TXT_KEY_TP_EMPIRE_VERY_UNHAPPY");
		elseif (pPlayer:IsEmpireVeryUnhappy() and Game.IsOption(GameOptionTypes.GAMEOPTION_NO_BARBARIANS)) then
			strText = strText .. "[COLOR_RED]" .. Locale.ConvertTextKey("TXT_KEY_TP_EMPIRE_VERY_UNHAPPY_NO_REBELS");
		elseif (pPlayer:IsEmpireUnhappy()) then
			strText = strText .. "[COLOR_FONT_RED]" .. Locale.ConvertTextKey("TXT_KEY_TP_EMPIRE_UNHAPPY");
		else
			strText = strText .. "[COLOR_POSITIVE_TEXT]" .. Locale.ConvertTextKey("TXT_KEY_TP_TOTAL_HAPPINESS");
		end

		if(pPlayer:GetUnhappinessGrowthPenalty() ~= 0) then
			strText = strText .. Locale.ConvertTextKey("TXT_KEY_TP_UNHAPPINESS_EMPIRE_PENALTIES",-pPlayer:GetUnhappinessGrowthPenalty(),
			-pPlayer:GetUnhappinessSettlerCostPenalty(),-pPlayer:GetUnhappinessCombatStrengthPenalty())
		end
		strText = strText .. "[/COLOR][NEWLINE][NEWLINE][COLOR:150:255:150:255]";
		
		-- First do Happiness
		local TotalHappiness = pPlayer:GetHappinessFromCitizenNeeds();
		strText = strText .. Locale.ConvertTextKey("TXT_KEY_TP_HAPPINESS_SOURCES", TotalHappiness);
		if (TotalHappiness ~= 0) then
			local ResourceHappiness = pPlayer:GetBonusHappinessFromLuxuriesFlat();
			if (ResourceHappiness ~= 0) then
				local AvgResourceHappiness  = pPlayer:GetBonusHappinessFromLuxuriesFlatForUI();
				strText = strText .. "[NEWLINE][ICON_BULLET]" .. Locale.ConvertTextKey("TXT_KEY_TP_HAPPINESS_RESOURCE_CITY", ResourceHappiness, AvgResourceHappiness);
			end

			local NaturalWonderAndLandmarkHappiness = pPlayer:GetHappinessFromNaturalWonders();
			if (NaturalWonderAndLandmarkHappiness ~= 0) then
				strText = strText .. "[NEWLINE][ICON_BULLET]" .. Locale.ConvertTextKey("TXT_KEY_TP_HAPPINESS_NATURAL_WONDERS", NaturalWonderAndLandmarkHappiness);
			end

			local ReligionHappiness = pPlayer:GetHappinessFromReligion();
			if (ReligionHappiness ~= 0) then
				strText = strText .. "[NEWLINE][ICON_BULLET]" .. Locale.ConvertTextKey("TXT_KEY_TP_HAPPINESS_STATE_RELIGION_VP", ReligionHappiness);
			end

			local LeagueHappiness = pPlayer:GetHappinessFromLeagues();
			if (LeagueHappiness ~= 0) then
				strText = strText .. "[NEWLINE][ICON_BULLET]" .. Locale.ConvertTextKey("TXT_KEY_TP_HAPPINESS_LEAGUES", LeagueHappiness);
			end

			local EventHappiness = pPlayer:GetEventHappiness();
			if (EventHappiness ~= 0) then
				strText = strText .. "[NEWLINE][ICON_BULLET]" .. Locale.ConvertTextKey("TXT_KEY_TP_HAPPINESS_EVENT", EventHappiness);
			end

			local MilitaryUnitHappiness = pPlayer:GetHappinessFromMilitaryUnits();
			if (MilitaryUnitHappiness ~= 0) then
				strText = strText .. "[NEWLINE][ICON_BULLET]" .. Locale.ConvertTextKey("TXT_KEY_TP_HAPPINESS_MILITARY_UNITS", MilitaryUnitHappiness);
			end

			local CityConnectionHappiness = pPlayer:GetHappinessFromTradeRoutes();
			if (CityConnectionHappiness ~= 0) then
				strText = strText .. "[NEWLINE][ICON_BULLET]" .. Locale.ConvertTextKey("TXT_KEY_TP_HAPPINESS_CONNECTED_CITIES", CityConnectionHappiness);
			end

			local CityStateHappiness = pPlayer:GetHappinessFromMinorCivs();
			if (CityStateHappiness ~= 0) then
				strText = strText .. "[NEWLINE][ICON_BULLET]" .. Locale.ConvertTextKey("TXT_KEY_TP_HAPPINESS_CITY_STATE_FRIENDSHIP", CityStateHappiness);
			end
			
			local AnnexedMinorsHappiness = pPlayer:GetHappinessFromAnnexedMinors();
			if (AnnexedMinorsHappiness ~= 0) then
				strText = strText .. "[NEWLINE][ICON_BULLET]" .. Locale.ConvertTextKey("TXT_KEY_TP_HAPPINESS_FROM_ANNEXED_MINORS", AnnexedMinorsHappiness);
			end

			local VassalHappiness = pPlayer:GetHappinessFromVassals();
			if (VassalHappiness ~= 0) then
				strText = strText .. "[NEWLINE][ICON_BULLET]" .. Locale.ConvertTextKey("TXT_KEY_TP_HAPPINESS_VASSALS", VassalHappiness);
			end
			
			local WarWithMajorsHappiness = pPlayer:GetHappinessFromWarsWithMajors();
			if (WarWithMajorsHappiness ~= 0) then
				strText = strText .. "[NEWLINE][ICON_BULLET]" .. Locale.ConvertTextKey("TXT_KEY_TP_HAPPINESS_WAR_WITH_MAJORS", WarWithMajorsHappiness);
			end

			local HandicapHappiness = pPlayer:GetHandicapHappiness();
			if (HandicapHappiness ~= 0) then
				strText = strText .. "[NEWLINE][ICON_BULLET]" .. Locale.ConvertTextKey("TXT_KEY_TP_HAPPINESS_DIFFICULTY_LEVEL", HandicapHappiness);
			end

			local LocalCityHappiness = pPlayer:GetEmpireHappinessFromCities();
			if (LocalCityHappiness ~= 0) then
				strText = strText .. "[NEWLINE][ICON_BULLET]" .. Locale.ConvertTextKey("TXT_KEY_TP_HAPPINESS_CITY_LOCAL", LocalCityHappiness);
			end
		end

		strText = strText .. "[/COLOR]";

		-- Now do Unhappiness
		local TotalUnhappiness = pPlayer:GetUnhappinessFromCitizenNeeds();
		strText = strText .. "[NEWLINE][NEWLINE][COLOR:255:150:150:255]";
		strText = strText .. Locale.ConvertTextKey("TXT_KEY_TP_UNHAPPINESS_TOTAL", TotalUnhappiness);
		if (TotalUnhappiness ~= 0) then
			local WarWearinessUnhappiness = pPlayer:GetUnhappinessFromWarWeariness();
			if (WarWearinessUnhappiness ~= 0) then
				strText = strText .. "[NEWLINE][ICON_BULLET]" .. Locale.ConvertTextKey("TXT_KEY_TP_UNHAPPINESS_WAR_WEARINESS", WarWearinessUnhappiness);
			end

			local PublicOpinionUnhappiness = pPlayer:GetUnhappinessFromPublicOpinion();
			if (PublicOpinionUnhappiness ~= 0) then
				strText = strText .. "[NEWLINE][ICON_BULLET]" .. Locale.ConvertTextKey("TXT_KEY_TP_UNHAPPINESS_PUBLIC_OPINION", PublicOpinionUnhappiness);
			end

			local OccupationUnhappiness = pPlayer:GetUnhappinessFromOccupiedCities();
			if (OccupationUnhappiness ~= 0) then
				strText = strText .. "[NEWLINE][ICON_BULLET]" .. Locale.ConvertTextKey("TXT_KEY_TP_UNHAPPINESS_OCCUPIED_POPULATION", OccupationUnhappiness);
			end

			local PuppetUnhappiness = pPlayer:GetUnhappinessFromPuppetCityPopulation();
			if (PuppetUnhappiness ~= 0) then
				strText = strText .. "[NEWLINE][ICON_BULLET]" .. Locale.ConvertTextKey("TXT_KEY_TP_UNHAPPINESS_PUPPET_CITIES", PuppetUnhappiness);
			end

			local FamineUnhappiness = pPlayer:GetUnhappinessFromFamine();
			if (FamineUnhappiness ~= 0) then
				strText = strText .. "[NEWLINE][ICON_BULLET]" .. Locale.ConvertTextKey("TXT_KEY_TP_UNHAPPINESS_FAMINE", FamineUnhappiness);
			end

			local PillagedTileUnhappiness = pPlayer:GetUnhappinessFromPillagedTiles();
			if (PillagedTileUnhappiness ~= 0) then
				strText = strText .. "[NEWLINE][ICON_BULLET]" .. Locale.ConvertTextKey("TXT_KEY_TP_UNHAPPINESS_PILLAGED", PillagedTileUnhappiness);
			end

			local IsolationUnhappiness = pPlayer:GetUnhappinessFromIsolation();
			if (IsolationUnhappiness ~= 0) then
				strText = strText .. "[NEWLINE][ICON_BULLET]" .. Locale.ConvertTextKey("TXT_KEY_TP_UNHAPPINESS_ISOLATION", IsolationUnhappiness);
			end

			local UnitUnhappiness = pPlayer:GetUnhappinessFromUnits();
			if (UnitUnhappiness ~= 0) then
				strText = strText .. "[NEWLINE][ICON_BULLET]" .. Locale.ConvertTextKey("TXT_KEY_TP_UNHAPPINESS_UNITS", UnitUnhappiness);
			end

			local DistressUnhappiness = pPlayer:GetUnhappinessFromDistress();
			if (DistressUnhappiness ~= 0) then
				strText = strText .. "[NEWLINE][ICON_BULLET]" .. Locale.ConvertTextKey("TXT_KEY_TP_UNHAPPINESS_DISTRESS", DistressUnhappiness);
			end

			local PovertyUnhappiness = pPlayer:GetUnhappinessFromPoverty();
			if (PovertyUnhappiness ~= 0) then
				strText = strText .. "[NEWLINE][ICON_BULLET]" .. Locale.ConvertTextKey("TXT_KEY_TP_UNHAPPINESS_POVERTY", PovertyUnhappiness);
			end

			local IlliteracyUnhappiness = pPlayer:GetUnhappinessFromIlliteracy();
			if (IlliteracyUnhappiness ~= 0) then
				strText = strText .. "[NEWLINE][ICON_BULLET]" .. Locale.ConvertTextKey("TXT_KEY_TP_UNHAPPINESS_ILLITERACY", IlliteracyUnhappiness);
			end

			local BoredomUnhappiness = pPlayer:GetUnhappinessFromBoredom();
			if (BoredomUnhappiness ~= 0) then
				strText = strText .. "[NEWLINE][ICON_BULLET]" .. Locale.ConvertTextKey("TXT_KEY_TP_UNHAPPINESS_BOREDOM", BoredomUnhappiness);
			end

			local ReligiousUnrestUnhappiness = pPlayer:GetUnhappinessFromReligiousUnrest();
			if (ReligiousUnrestUnhappiness ~= 0) then
				strText = strText .. "[NEWLINE][ICON_BULLET]" .. Locale.ConvertTextKey("TXT_KEY_TP_UNHAPPINESS_RELIGIOUS_UNREST", ReligiousUnrestUnhappiness);
			end

			local UrbanizationUnhappiness = pPlayer:GetUnhappinessFromCitySpecialists() / 100;
			if (UrbanizationUnhappiness ~= 0) then
				strText = strText .. "[NEWLINE][ICON_BULLET]" .. Locale.ConvertTextKey("TXT_KEY_TP_UNHAPPINESS_SPECIALISTS", UrbanizationUnhappiness);
			end

			local UrbanizationPuppetUnhappiness = pPlayer:GetUnhappinessFromPuppetCitySpecialists();
			if (UrbanizationPuppetUnhappiness ~= 0) then
				strText = strText .. "[NEWLINE][ICON_BULLET]" .. Locale.ConvertTextKey("TXT_KEY_TP_UNHAPPINESS_PUPPET_CITIES_SPECIALISTS", UrbanizationPuppetUnhappiness);
			end
		end

		strText = strText .. "[/COLOR]";

		-- Basic explanation of Happiness
		if (not OptionsManager.IsNoBasicHelp()) then
			strText = strText .. "[NEWLINE][NEWLINE]";
			strText = strText ..  Locale.ConvertTextKey("TXT_KEY_TP_HAPPINESS_EXPLANATION");
		end
	end

	tipControlTable.TooltipLabel:SetText( strText );
	tipControlTable.TopPanelMouseover:SetHide(false);

    -- Autosize tooltip
    tipControlTable.TopPanelMouseover:DoAutoSize();
end

-- Golden Age Tooltip
function GoldenAgeTipHandler( control )

	local strText = "";
	local iPlayerID = g_activePlayerObserver;
	local pPlayer = Players[iPlayerID];
	
	if (Game.IsOption(GameOptionTypes.GAMEOPTION_NO_HAPPINESS) or not pPlayer:IsAlive()) then
		strText = Locale.ConvertTextKey("TXT_KEY_TOP_PANEL_HAPPINESS_OFF_TOOLTIP");
	else
		local pTeam = Teams[pPlayer:GetTeam()];
		local pCity = UI.GetHeadSelectedCity();
		
		local iHappiness = pPlayer:GetHappinessForGAP();
		local iGAPReligion = pPlayer:GetGAPFromReligion();
		local iGAPTrait = pPlayer:GetGAPFromTraits();
		local iGAPCities = pPlayer:GetGAPFromCitiesTimes100() / 100;
		local iChange = iHappiness + iGAPReligion + iGAPTrait + iGAPCities;
	
		if (pPlayer:GetGoldenAgeTurns() > 0) then
			strText = Locale.ConvertTextKey("TXT_KEY_TP_GOLDEN_AGE_NOW", pPlayer:GetGoldenAgeTurns());
		else
			if (iChange > 0) then
				iTurnsUntilGoldenAge = (pPlayer:GetGoldenAgeProgressThreshold() - pPlayer:GetGoldenAgeProgressMeterTimes100() / 100) / iChange;
				iTurnsUntilGoldenAge = math.max(0, math.ceil(iTurnsUntilGoldenAge));
				strText = strText .. Locale.ConvertTextKey("TXT_KEY_TOP_PANEL_TURNS_UNTIL_GOLDEN_AGE", iTurnsUntilGoldenAge);
			end
		end
		

		if(strText ~= "") then
			strText = strText .. "[NEWLINE][NEWLINE]";
		end

		strText = strText .. Locale.ConvertTextKey("TXT_KEY_TP_GOLDEN_AGE_PROGRESS", pPlayer:GetGoldenAgeProgressMeterTimes100() / 100, pPlayer:GetGoldenAgeProgressThreshold());
		strText = strText .. "[NEWLINE]";
		
		if (iHappiness >= 0) then
			strText = strText .. Locale.ConvertTextKey("TXT_KEY_TP_GOLDEN_AGE_ADDITION", iHappiness);
		else
			strText = strText .. "[COLOR_WARNING_TEXT]" .. Locale.ConvertTextKey("TXT_KEY_TP_GOLDEN_AGE_LOSS", -iHappiness) .. "[ENDCOLOR]";
		end
		-- CBP
		if (iGAPReligion > 0) then
			strText = strText .. "[NEWLINE]";
			strText = strText .. Locale.ConvertTextKey("TXT_KEY_TP_GOLDEN_AGE_ADDITION_RELIGION", iGAPReligion);
		end
		if (iGAPTrait > 0) then
			strText = strText .. "[NEWLINE]";
			strText = strText .. Locale.ConvertTextKey("TXT_KEY_TP_GOLDEN_AGE_ADDITION_TRAIT", iGAPTrait);
		end
		if (iGAPCities > 0) then
			strText = strText .. "[NEWLINE]";
			strText = strText .. Locale.ConvertTextKey("TXT_KEY_TP_GOLDEN_AGE_ADDITION_CITIES", iGAPCities);
		end
		-- END
		
	
		strText = strText .. "[NEWLINE][NEWLINE]";
		if (pPlayer:IsGoldenAgeCultureBonusDisabled()) then
			strText = strText ..  Locale.ConvertTextKey("TXT_KEY_TP_GOLDEN_AGE_EFFECT_NO_CULTURE");		
		else
			strText = strText ..  Locale.ConvertTextKey("TXT_KEY_TP_GOLDEN_AGE_EFFECT");		
		end
		
		if (pPlayer:GetGoldenAgeTurns() > 0 and pPlayer:GetGoldenAgeTourismModifier() > 0) then
			strText = strText .. "[NEWLINE][NEWLINE]";
			strText = strText ..  Locale.ConvertTextKey("TXT_KEY_TP_CARNIVAL_EFFECT");			
		end
	end
	
	tipControlTable.TooltipLabel:SetText( strText );
	tipControlTable.TopPanelMouseover:SetHide(false);
    
    -- Autosize tooltip
    tipControlTable.TopPanelMouseover:DoAutoSize();
	
end

-- Culture Tooltip
function CultureTipHandler( control )

	local strText = "";
	
	if (Game.IsOption(GameOptionTypes.GAMEOPTION_NO_POLICIES)) then
		strText = Locale.ConvertTextKey("TXT_KEY_TOP_PANEL_POLICIES_OFF_TOOLTIP");
	else
	
		local iPlayerID = g_activePlayerObserver;
		local pPlayer = Players[iPlayerID];
    
	    local iTurns;
		local iCultureNeeded = pPlayer:GetNextPolicyCost() - (pPlayer:GetJONSCultureTimes100() / 100);
	    if (iCultureNeeded <= 0) then
			iTurns = 0;
		else
			if (pPlayer:GetTotalJONSCulturePerTurnTimes100() == 0) then
				iTurns = "?";
			else
				iTurns = iCultureNeeded / (pPlayer:GetTotalJONSCulturePerTurnTimes100() / 100);
				iTurns = math.ceil(iTurns);
			end
	    end
	    
	    if (pPlayer:IsAnarchy()) then
			strText = strText .. Locale.ConvertTextKey("TXT_KEY_TP_ANARCHY", pPlayer:GetAnarchyNumTurns());
		end
	    
		strText = strText .. Locale.ConvertTextKey("TXT_KEY_NEXT_POLICY_TURN_LABEL", iTurns);
	
		if (not OptionsManager.IsNoBasicHelp()) then
			strText = strText .. "[NEWLINE][NEWLINE]";
			strText = strText .. Locale.ConvertTextKey("TXT_KEY_TP_CULTURE_ACCUMULATED", pPlayer:GetJONSCultureTimes100() / 100);
			strText = strText .. "[NEWLINE]";
		
			if (pPlayer:GetNextPolicyCost() > 0) then
				strText = strText .. Locale.ConvertTextKey("TXT_KEY_TP_CULTURE_NEXT_POLICY", pPlayer:GetNextPolicyCost());
			end
		end

		if (pPlayer:IsAnarchy()) then
			tipControlTable.TooltipLabel:SetText( strText );
			tipControlTable.TopPanelMouseover:SetHide(false);
			tipControlTable.TopPanelMouseover:DoAutoSize();
			return;
		end

		strText = strText .. "[NEWLINE]";
		strText = strText .. pPlayer:GetTotalCulturePerTurnTooltip();
		
-- CBP
		if(pPlayer:GetTechsToFreePolicy() >= 0)then
			strText = strText .. "[NEWLINE][NEWLINE]";
			strText = strText .. Locale.ConvertTextKey("TXT_KEY_TP_TECHS_NEEDED_FOR_NEXT_FREE_POLICY", pPlayer:GetTechsToFreePolicy());
		end
--END 

		-- Let people know that building more cities makes policies harder to get
		if (not OptionsManager.IsNoBasicHelp()) then
			strText = strText .. "[NEWLINE][NEWLINE]";
			strText = strText .. Locale.ConvertTextKey("TXT_KEY_TP_CULTURE_CITY_COST", Game.GetNumCitiesPolicyCostMod());
		end
	end
	
	tipControlTable.TooltipLabel:SetText( strText );
	tipControlTable.TopPanelMouseover:SetHide(false);
    
    -- Autosize tooltip
    tipControlTable.TopPanelMouseover:DoAutoSize();
	
end

-- Tourism Tooltip
function TourismTipHandler( control )

	local iPlayerID = g_activePlayerObserver;
	local pPlayer = Players[iPlayerID];
	
	local iTotalGreatWorks = pPlayer:GetNumGreatWorks();
	local iTotalSlots = pPlayer:GetNumGreatWorkSlots();
	
	local strText1 = Locale.ConvertTextKey("TXT_KEY_TOP_PANEL_TOURISM_TOOLTIP_1", iTotalGreatWorks);
	local strText2 = Locale.ConvertTextKey("TXT_KEY_TOP_PANEL_TOURISM_TOOLTIP_2", (iTotalSlots - iTotalGreatWorks));
		
	local strText = strText1 .. "[NEWLINE]" .. strText2;
		
	local cultureVictory = GameInfo.Victories["VICTORY_CULTURAL"];
	if(cultureVictory ~= nil and PreGame.IsVictory(cultureVictory.ID)) then
	    local iNumInfluential = pPlayer:GetNumCivsInfluentialOn();
		local iNumToBeInfluential = pPlayer:GetNumCivsToBeInfluentialOn();
		local szText = Locale.ConvertTextKey("TXT_KEY_CO_VICTORY_INFLUENTIAL_OF", iNumInfluential, iNumToBeInfluential);

		local strText3 = Locale.ConvertTextKey("TXT_KEY_TOP_PANEL_TOURISM_TOOLTIP_3", szText);
		
		strText = strText .. "[NEWLINE][NEWLINE]" .. strText3;
	end

	tipControlTable.TooltipLabel:SetText( strText );
	tipControlTable.TopPanelMouseover:SetHide(false);
    
    -- Autosize tooltip
    tipControlTable.TopPanelMouseover:DoAutoSize();
	
end

-- FaithTooltip
function FaithTipHandler( control )

	local strText = "";

	if (Game.IsOption(GameOptionTypes.GAMEOPTION_NO_RELIGION)) then
		strText = Locale.ConvertTextKey("TXT_KEY_TOP_PANEL_RELIGION_OFF_TOOLTIP");
	else
	
		local iPlayerID = g_activePlayerObserver;
		local pPlayer = Players[iPlayerID];

	    if (pPlayer:IsAnarchy()) then
			strText = strText .. Locale.ConvertTextKey("TXT_KEY_TP_ANARCHY", pPlayer:GetAnarchyNumTurns());
			strText = strText .. "[NEWLINE]";
			strText = strText .. "[NEWLINE]";
		end
	    
		strText = strText .. Locale.ConvertTextKey("TXT_KEY_TP_FAITH_ACCUMULATED", pPlayer:GetFaithTimes100() / 100);
		strText = strText .. "[NEWLINE]";
	
		-- Faith from Cities
		local iFaithFromCities = pPlayer:GetYieldRateFromCitiesTimes100(YieldTypes.YIELD_FAITH) / 100;
		if (iFaithFromCities ~= 0) then
			strText = strText .. "[NEWLINE]";
			strText = strText .. Locale.ConvertTextKey("TXT_KEY_TP_FAITH_FROM_CITIES", iFaithFromCities);
		end
	
		-- Faith from Minor Civs
		local iFaithFromMinorCivs = pPlayer:GetFaithPerTurnFromMinorCivs();
		if (iFaithFromMinorCivs ~= 0) then
			strText = strText .. "[NEWLINE]";
			strText = strText .. Locale.ConvertTextKey("TXT_KEY_TP_FAITH_FROM_MINORS", iFaithFromMinorCivs);
		end
-- CBP
		-- Faith from Annexed Minors
		local iFaithFromAnnexedMinors = pPlayer:GetFaithPerTurnFromAnnexedMinors();
		if (iFaithFromAnnexedMinors ~= 0) then
			strText = strText .. "[NEWLINE]";
			strText = strText .. Locale.ConvertTextKey("TXT_KEY_TP_FAITH_FROM_ANNEXED_MINORS", iFaithFromAnnexedMinors);
		end
-- END

		-- Faith from Religion
		local iFaithFromReligion = pPlayer:GetFaithPerTurnFromReligion();
		if (iFaithFromReligion ~= 0) then
			strText = strText .. "[NEWLINE]";
			strText = strText .. Locale.ConvertTextKey("TXT_KEY_TP_FAITH_FROM_RELIGION", iFaithFromReligion);
		end
-- C4DF
		-- Faith from Vassals
		local iFaithFromVassals = pPlayer:GetYieldPerTurnFromVassalsTimes100(YieldTypes.YIELD_FAITH) / 100;
		if (iFaithFromVassals ~= 0) then
			strText = strText .. "[NEWLINE]";
			strText = strText .. Locale.ConvertTextKey("TXT_KEY_TP_FAITH_VASSALS", iFaithFromVassals);
		end
		-- Faith from Espionage
		local iFaithFromEspionage = pPlayer:GetYieldPerTurnFromEspionageEvents(YieldTypes.YIELD_FAITH, true) - pPlayer:GetYieldPerTurnFromEspionageEvents(YieldTypes.YIELD_FAITH, false);
		if (iFaithFromEspionage > 0) then
			strText = strText .. "[NEWLINE]";
			strText = strText .. Locale.ConvertTextKey("TXT_KEY_TP_FAITH_FROM_ESPIONAGE_POSITIVE", iFaithFromEspionage);
		elseif (iFaithFromEspionage < 0) then
			strText = strText .. "[NEWLINE]";
			strText = strText .. Locale.ConvertTextKey("TXT_KEY_TP_FAITH_FROM_ESPIONAGE_NEGATIVE", iFaithFromEspionage);
		end
-- END	
		
		if (iFaithFromCities ~= 0 or iFaithFromMinorCivs ~= 0 or iFaithFromReligion ~= 0 or iFaithFromAnnexedMinors ~= 0 or iFaithFromVassals ~= 0 or iFaithFromEspionage ~= 0) then
			strText = strText .. "[NEWLINE]";
		end
	
		strText = strText .. "[NEWLINE]";

		if (pPlayer:HasCreatedPantheon()) then
			if (Game.GetNumReligionsStillToFound(false, g_activePlayerObserver) > 0 or pPlayer:OwnsReligion()) then
				if (pPlayer:GetCurrentEra() < GameInfo.Eras["ERA_INDUSTRIAL"].ID) then
					strText = strText .. Locale.ConvertTextKey("TXT_KEY_TP_FAITH_NEXT_PROPHET", pPlayer:GetMinimumFaithNextGreatProphet());
					strText = strText .. "[NEWLINE]";
					strText = strText .. "[NEWLINE]";
				end
			end
		else
			if (pPlayer:CanCreatePantheon(false)) then
				strText = strText .. Locale.ConvertTextKey("TXT_KEY_TP_FAITH_NEXT_PANTHEON", Game.GetMinimumFaithNextPantheon());
				strText = strText .. "[NEWLINE]";
			else
				strText = strText .. Locale.ConvertTextKey("TXT_KEY_TP_FAITH_PANTHEONS_LOCKED");
				strText = strText .. "[NEWLINE]";
			end
			strText = strText .. "[NEWLINE]";
		end

		if (Game.GetNumReligionsStillToFound(false, g_activePlayerObserver) < 0) then
			strText = strText .. Locale.ConvertTextKey("TXT_KEY_TP_FAITH_RELIGIONS_LEFT", 0);
		else
			strText = strText .. Locale.ConvertTextKey("TXT_KEY_TP_FAITH_RELIGIONS_LEFT", Game.GetNumReligionsStillToFound(false, g_activePlayerObserver));
		end
		
		if (pPlayer:GetCurrentEra() >= GameInfo.Eras["ERA_INDUSTRIAL"].ID) then
		    local bAnyFound = false;
			strText = strText .. "[NEWLINE]";		
			strText = strText .. "[NEWLINE]";		
			strText = strText .. Locale.ConvertTextKey("TXT_KEY_TP_FAITH_NEXT_GREAT_PERSON", pPlayer:GetMinimumFaithNextGreatProphet());	
			
			local capital = pPlayer:GetCapitalCity();
			if(capital ~= nil) then	
				for info in GameInfo.Units{Special = "SPECIALUNIT_PEOPLE"} do
					local infoID = info.ID;
					local faithCost = capital:GetUnitFaithPurchaseCost(infoID, true);
					if(faithCost > 0 and pPlayer:IsCanPurchaseAnyCity(false, true, infoID, -1, YieldTypes.YIELD_FAITH)) then
						if (pPlayer:DoesUnitPassFaithPurchaseCheck(infoID)) then
							strText = strText .. "[NEWLINE]";
							strText = strText .. "[ICON_BULLET]" .. Locale.ConvertTextKey(info.Description);
							bAnyFound = true;
						end
					end
				end
			end
						
			if (not bAnyFound) then
				strText = strText .. "[NEWLINE]";
				strText = strText .. "[ICON_BULLET]" .. Locale.ConvertTextKey("TXT_KEY_RO_YR_NO_GREAT_PEOPLE");
			end
		end
	end

	tipControlTable.TooltipLabel:SetText( strText );
	tipControlTable.TopPanelMouseover:SetHide(false);
    
    -- Autosize tooltip
    tipControlTable.TopPanelMouseover:DoAutoSize();
	
end

function UnitSupplyHandler(control)

	local strUnitSupplyToolTip = "";
	local iPlayerID = g_activePlayerObserver;
	local pPlayer = Players[iPlayerID];

	local iUnitSupplyMod = pPlayer:GetUnitProductionMaintenanceMod();
	local iUnitsSupplied = pPlayer:GetNumUnitsSupplied();
	local iUnitsTotal = pPlayer:GetNumUnitsToSupply();
	local iUnitsTotalMilitary = pPlayer:GetNumMilitaryUnits();
	local iSupplyFromGreatPeople = pPlayer:GetUnitSupplyFromExpendedGreatPeople();
	local iPercentPerPop = pPlayer:GetNumUnitsSuppliedByPopulation();
	local iPerCity = pPlayer:GetNumUnitsSuppliedByCities();
	local iPerHandicap = pPlayer:GetNumUnitsSuppliedByHandicap();
	local iUnitsOver = pPlayer:GetNumUnitsOutOfSupply();
	local iTechReduction = pPlayer:GetTechSupplyReduction();
	local iEmpireSizeReduction = pPlayer:GetEmpireSizeSupplyReduction();
	local iWarWearinessPercentReduction = pPlayer:GetSupplyReductionPercentFromWarWeariness();
	local iWarWearinessReduction = pPlayer:GetSupplyReductionFromWarWeariness();
	local iWarWearinessCostIncrease = pPlayer:GetUnitCostIncreaseFromWarWeariness();
	local iHighestWarWearyPlayer = pPlayer:GetHighestWarWearinessPlayer();
	local iPerCityGross = pPlayer:GetNumUnitsSuppliedByCities(true);
	local iTechReductionPerCity = iPerCityGross - iPerCity;
	local iPercentPerPopGross = pPlayer:GetNumUnitsSuppliedByPopulation(true);
	local iTechReductionPerPop = iPercentPerPopGross - iPercentPerPop;
	local iPerHandicapGross = pPlayer:GetNumUnitsSuppliedByHandicap(true);
	-- Bonuses from unlisted sources are added to the handicap value
	local iExtra = iUnitsSupplied - (iPerHandicapGross + iPerCityGross + iPercentPerPopGross + iSupplyFromGreatPeople - iTechReduction - iEmpireSizeReduction - iWarWearinessReduction);
	iPerHandicap = iPerHandicap + iExtra;
	iPerHandicapGross = iPerHandicapGross + iExtra;
	local iTechReductionPerEra = iPerHandicapGross - iPerHandicap;

	local strUnitSupplyToolTip = "";
	if (iUnitsOver > 0) then
		strUnitSupplyToolTip = "[COLOR_NEGATIVE_TEXT]";
		strUnitSupplyToolTip = strUnitSupplyToolTip .. Locale.ConvertTextKey("TXT_KEY_UNIT_SUPPLY_REACHED_TOOLTIP", iUnitsSupplied, iUnitsOver, -iUnitSupplyMod);
		strUnitSupplyToolTip = strUnitSupplyToolTip .. "[ENDCOLOR]";
	end

	local strUnitSupplyToolUnderTip = "";
	if (iHighestWarWearyPlayer == -1) then
		strUnitSupplyToolUnderTip = Locale.ConvertTextKey("TXT_KEY_UNIT_SUPPLY_REMAINING_TOOLTIP_NOT_WEARY", iUnitsSupplied, iUnitsTotal, iPercentPerPop, iPerCity, iPerHandicap, iWarWearinessPercentReduction, iWarWearinessReduction, iTechReduction, iEmpireSizeReduction, iWarWearinessCostIncrease, iSupplyFromGreatPeople, iUnitsTotalMilitary, iPerCityGross, iTechReductionPerCity, iPercentPerPopGross, iTechReductionPerPop, iPerHandicapGross, iTechReductionPerEra);
	else
		local iWarWearyTargetPercent = pPlayer:GetWarWearinessPercent(iHighestWarWearyPlayer);
		strUnitSupplyToolUnderTip = Locale.ConvertTextKey("TXT_KEY_UNIT_SUPPLY_REMAINING_TOOLTIP", iUnitsSupplied, iUnitsTotal, iPercentPerPop, iPerCity, iPerHandicap, iWarWearinessPercentReduction, iWarWearinessReduction, iTechReduction, iEmpireSizeReduction, iWarWearinessCostIncrease, iSupplyFromGreatPeople, iUnitsTotalMilitary, iPerCityGross, iTechReductionPerCity, iPercentPerPopGross, iTechReductionPerPop, iPerHandicapGross, iTechReductionPerEra, Players[iHighestWarWearyPlayer]:GetCivilizationShortDescription(), iWarWearyTargetPercent);
	end
	if (strUnitSupplyToolTip ~= "") then
		strUnitSupplyToolTip = strUnitSupplyToolTip .. "[NEWLINE][NEWLINE]" .. strUnitSupplyToolUnderTip;
	else
		strUnitSupplyToolTip = strUnitSupplyToolUnderTip;
	end
	if (strUnitSupplyToolTip ~= "") then
		tipControlTable.TopPanelMouseover:SetHide(false);
		tipControlTable.TooltipLabel:SetText( strUnitSupplyToolTip );
	else
		tipControlTable.TopPanelMouseover:SetHide(true);
	end

	-- Autosize tooltip
	tipControlTable.TopPanelMouseover:DoAutoSize();
end

function InstantYieldHandler( control )

	local iPlayerID = g_activePlayerObserver;
	local pPlayer = Players[iPlayerID];

	local strInstantYieldToolTip = pPlayer:GetInstantYieldHistoryTooltip(10);

	if(strInstantYieldToolTip ~= "") then
		tipControlTable.TopPanelMouseover:SetHide(false);
		tipControlTable.TooltipLabel:SetText( strInstantYieldToolTip );
	else
		tipControlTable.TopPanelMouseover:SetHide(true);
	end
    
    -- Autosize tooltip
    tipControlTable.TopPanelMouseover:DoAutoSize();
end

-- Spy Points Tooptip (hidden for now)
--[[function SpyPointsTipHandler( control )

	local iPlayerID = g_activePlayerObserver;
	local pPlayer = Players[iPlayerID];
	local strSpiesStr;
	if (Game.IsOption(GameOptionTypes.GAMEOPTION_NO_ESPIONAGE) or Game.GetSpyThreshold() == 0) then
		strSpiesStr = "";
	else
		strSpiesStr = Locale.ConvertTextKey("TXT_KEY_SPY_POINTS_TT", pPlayer:GetSpyPoints(false), Game.GetSpyThreshold(), pPlayer:GetSpyPoints(true));
	end

	if(strSpiesStr ~= "") then
		tipControlTable.TopPanelMouseover:SetHide(false);
		tipControlTable.TooltipLabel:SetText( strSpiesStr );
	else
		tipControlTable.TopPanelMouseover:SetHide(true);
	end
    
    -- Autosize tooltip
    tipControlTable.TopPanelMouseover:DoAutoSize();
end]]

-- Resources Tooltip
function ResourcesTipHandler( control )

	local strText;
	local iPlayerID = g_activePlayerObserver;
	local pPlayer = Players[iPlayerID];
	local pTeam = Teams[pPlayer:GetTeam()];
	local pCity = UI.GetHeadSelectedCity();
	
	strText = "";
	
	local pResource;
	local bShowResource;
	local bThisIsFirstResourceShown = true;
	local iNumAvailable;
	local iNumUsed;
	local iNumTotal;
	
	for pResource in GameInfo.Resources() do
		local iResourceLoop = pResource.ID;
		
		if (Game.GetResourceUsageType(iResourceLoop) == ResourceUsageTypes.RESOURCEUSAGE_STRATEGIC) then
			
			bShowResource = false;
			
			if (pPlayer:IsResourceRevealed(iResourceLoop)) then
				if (pPlayer:IsResourceCityTradeable(iResourceLoop)) then
					bShowResource = true;
				end
			end
			
			if (bShowResource) then
				iNumAvailable = pPlayer:GetNumResourceAvailable(iResourceLoop, true);
				iNumUsed = pPlayer:GetNumResourceUsed(iResourceLoop);
				iNumTotal = pPlayer:GetNumResourceTotal(iResourceLoop, true);
				
				-- Add newline to the front of all entries that AREN'T the first
				if (bThisIsFirstResourceShown) then
					strText = "";
					bThisIsFirstResourceShown = false;
				else
					strText = strText .. "[NEWLINE][NEWLINE]";
				end
				
				strText = strText .. iNumAvailable .. " " .. pResource.IconString .. " " .. Locale.ConvertTextKey(pResource.Description);
				
				-- Details
				if (iNumUsed ~= 0 or iNumTotal ~= 0) then
					strText = strText .. ": ";
					strText = strText .. Locale.ConvertTextKey("TXT_KEY_TP_RESOURCE_INFO", iNumTotal, iNumUsed);
				end
			end
		end
	end
	
	print(strText);
	if(strText ~= "") then
		tipControlTable.TopPanelMouseover:SetHide(false);
		tipControlTable.TooltipLabel:SetText( strText );
	else
		tipControlTable.TopPanelMouseover:SetHide(true);
	end
    
    -- Autosize tooltip
    tipControlTable.TopPanelMouseover:DoAutoSize();
	
end

-- International Trade Route Tooltip
function InternationalTradeRoutesTipHandler( control )

	local iPlayerID = g_activePlayerObserver;
	local pPlayer = Players[iPlayerID];
	
	local strTT = "";
	
	local iNumLandTradeUnitsAvail = pPlayer:GetNumAvailableTradeUnits(DomainTypes.DOMAIN_LAND);
	if (iNumLandTradeUnitsAvail > 0) then
		local iTradeUnitType = pPlayer:GetTradeUnitType(DomainTypes.DOMAIN_LAND);
		local strUnusedTradeUnitWarning = Locale.ConvertTextKey("TXT_KEY_TOP_PANEL_INTERNATIONAL_TRADE_ROUTES_TT_UNASSIGNED", iNumLandTradeUnitsAvail, GameInfo.Units[iTradeUnitType].Description);
		strTT = strTT .. strUnusedTradeUnitWarning;
	end
	
	local iNumSeaTradeUnitsAvail = pPlayer:GetNumAvailableTradeUnits(DomainTypes.DOMAIN_SEA);
	if (iNumSeaTradeUnitsAvail > 0) then
		local iTradeUnitType = pPlayer:GetTradeUnitType(DomainTypes.DOMAIN_SEA);
		local strUnusedTradeUnitWarning = Locale.ConvertTextKey("TXT_KEY_TOP_PANEL_INTERNATIONAL_TRADE_ROUTES_TT_UNASSIGNED", iNumLandTradeUnitsAvail, GameInfo.Units[iTradeUnitType].Description);	
		strTT = strTT .. strUnusedTradeUnitWarning;
	end
	
	if (strTT ~= "") then
		strTT = strTT .. "[NEWLINE]";
	end
	
	local iUsedTradeRoutes = pPlayer:GetNumInternationalTradeRoutesUsed();
	local iAvailableTradeRoutes = pPlayer:GetNumInternationalTradeRoutesAvailable();
	
	local strText = Locale.ConvertTextKey("TXT_KEY_TOP_PANEL_INTERNATIONAL_TRADE_ROUTES_TT", iUsedTradeRoutes, iAvailableTradeRoutes);
	strTT = strTT .. strText;
	
	local strYourTradeRoutes = pPlayer:GetTradeYourRoutesTTString();
	if (strYourTradeRoutes ~= "") then
		strTT = strTT .. "[NEWLINE][NEWLINE]"
		strTT = strTT .. Locale.ConvertTextKey("TXT_KEY_TOP_PANEL_ITR_ESTABLISHED_BY_PLAYER_TT");
		strTT = strTT .. "[NEWLINE]";
		strTT = strTT .. strYourTradeRoutes;
	end

	local strToYouTradeRoutes = pPlayer:GetTradeToYouRoutesTTString();
	if (strToYouTradeRoutes ~= "") then
		strTT = strTT .. "[NEWLINE][NEWLINE]"
		strTT = strTT .. Locale.ConvertTextKey("TXT_KEY_TOP_PANEL_ITR_ESTABLISHED_BY_OTHER_TT");
		strTT = strTT .. "[NEWLINE]";
		strTT = strTT .. strToYouTradeRoutes;
	end
	
	--print(strText);
	if(strText ~= "") then
		tipControlTable.TopPanelMouseover:SetHide(false);
		tipControlTable.TooltipLabel:SetText( strTT );
	else
		tipControlTable.TopPanelMouseover:SetHide(true);
	end
    
    -- Autosize tooltip
    tipControlTable.TopPanelMouseover:DoAutoSize();
end

-- COMMUNITY PATCH CHANGE
-------------------------------------------------
-------------------------------------------------
function OpenEcon()
	Events.SerialEventGameMessagePopup( { Type = ButtonPopupTypes.BUTTONPOPUP_ECONOMIC_OVERVIEW } );
end
Controls.HappinessString:RegisterCallback( Mouse.eLClick, OpenEcon );
--END

-------------------------------------------------
-- On Top Panel mouseover exited
-------------------------------------------------
--function HelpClose()
	---- Hide the help text box
	--Controls.HelpTextBox:SetHide( true );
--end

function OnAIPlayerChanged(iPlayerID, szTag)
	local oldActivePlayerObserver = g_activePlayerObserver;
	local player = Players[Game.GetActivePlayer()];
	if player:IsObserver() then
		if (Game:GetObserverUIOverridePlayer() > -1) then
			g_activePlayerObserver = Game:GetObserverUIOverridePlayer()
		else
			g_activePlayerObserver = Players[iPlayerID]:IsMajorCiv() and iPlayerID or -1;
		end
	else
		g_activePlayerObserver = Game.GetActivePlayer();
	end
	if g_activePlayerObserver ~= oldActivePlayerObserver then
		UpdateData()
	end
end

function OnEventActivePlayerChanged( iActivePlayer, iPrevActivePlayer )
	g_activePlayerObserver = iActivePlayer;
end

-- Register Events
Events.GameplaySetActivePlayer.Add(OnEventActivePlayerChanged);
Events.AIProcessingStartedForPlayer.Add(OnAIPlayerChanged);
Events.SerialEventGameDataDirty.Add(OnTopPanelDirty);
Events.SerialEventTurnTimerDirty.Add(OnTopPanelDirty);
Events.SerialEventCityInfoDirty.Add(OnTopPanelDirty);

-- Update data at initialization
UpdateData();
DoInitTooltips();
