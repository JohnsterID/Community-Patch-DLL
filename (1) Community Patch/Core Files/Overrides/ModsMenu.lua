-------------------------------------------------
-- Mods Menu
-------------------------------------------------
include( "IconSupport" );
include( "InstanceManager" );
include( "SupportFunctions" );

local g_InstanceManager = InstanceManager:new( "ItemInstance", "Button", Controls.ItemStack );

-------------------------------------------------
-- Enable/Disable buttons based on mod compatibility
-------------------------------------------------
ContextPtr:SetShowHideHandler(function(isHiding)
	if(not isHiding) then
		local supportsSinglePlayer = Modding.AllEnabledModsContainPropertyValue("SupportsSinglePlayer", 1);
		local supportsMultiplayer = Modding.AllEnabledModsContainPropertyValue("SupportsMultiplayer", 1);
		
		Controls.SinglePlayerButton:SetDisabled(not supportsSinglePlayer);
		-- Always enable multiplayer button for our mod preservation system
		Controls.MultiPlayerButton:SetDisabled(false);
		
		g_InstanceManager:ResetInstances();
		
		local mods = Modding.GetEnabledModsByActivationOrder();
		
		if(#mods == 0) then
			Controls.ModsInUseLabel:SetHide(true);
		else
			Controls.ModsInUseLabel:SetHide(false);
			for i,v in ipairs(mods) do
				local displayName = Modding.GetModProperty(v.ModID, v.Version, "Name");
				local displayNameVersion = string.format("[ICON_BULLET] %s (v. %i)", displayName, v.Version);			
				local listing = g_InstanceManager:GetInstance();
				listing.Label:SetText(displayNameVersion);
				listing.Label:SetToolTipString(displayNameVersion);
			end
		end
	end
end);

-------------------------------------------------
-------------------------------------------------
function InputHandler( uiMsg, wParam, lParam )
    if uiMsg == KeyEvents.KeyDown then
        if wParam == Keys.VK_ESCAPE or wParam == Keys.VK_RETURN then
			OnBack();
        end
    end
    return true;
end
ContextPtr:SetInputHandler( InputHandler );

-------------------------------------------------
-------------------------------------------------
function OnBack()
	UIManager:DequeuePopup( ContextPtr );
end
Controls.BackButton:RegisterCallback( Mouse.eLClick, OnBack );

-------------------------------------------------
-------------------------------------------------
function OnSinglePlayer()
	PreGame.SetLoadWBScenario(false);
	
	Events.SerialEventStartGame();
	UIManager:SetUICursor( 1 );
end
Controls.SinglePlayerButton:RegisterCallback( Mouse.eLClick, OnSinglePlayer );

-------------------------------------------------
-------------------------------------------------
function OnMultiPlayer()
	UIManager:QueuePopup( Controls.MultiplayerSelectScreen, PopupPriority.eUtmost );
end
Controls.MultiPlayerButton:RegisterCallback( Mouse.eLClick, OnMultiPlayer );

-------------------------------------------------
-------------------------------------------------
function OnWorldBuilder()
	UIManager:QueuePopup( Controls.WorldBuilderScreen, PopupPriority.eUtmost );
end
if Controls.WorldBuilderButton then
	Controls.WorldBuilderButton:RegisterCallback( Mouse.eLClick, OnWorldBuilder );
end

----------------------------------------------------------------        
----------------------------------------------------------------        
ContextPtr:SetUpdate( function()
	if( ContextPtr:IsHidden() == false ) then
		Controls.VersionNumber:SetText( Locale.ConvertTextKey( "TXT_KEY_VERSION_NUMBER", UI.GetVersionString() ) );
	end
end );

----------------------------------------------------------------        
----------------------------------------------------------------        
Events.SystemUpdateUI.Add(function()
	if( ContextPtr:IsHidden() == false ) then
		Controls.VersionNumber:SetText( Locale.ConvertTextKey( "TXT_KEY_VERSION_NUMBER", UI.GetVersionString() ) );
	end
end);

----------------------------------------------------------------        
----------------------------------------------------------------        
Events.AfterModsActivate.Add(function()
	if( ContextPtr:IsHidden() == false ) then
		Controls.VersionNumber:SetText( Locale.ConvertTextKey( "TXT_KEY_VERSION_NUMBER", UI.GetVersionString() ) );
	end
end);

----------------------------------------------------------------        
----------------------------------------------------------------        
Events.AfterModsDeactivate.Add(function()
	if( ContextPtr:IsHidden() == false ) then
		Controls.VersionNumber:SetText( Locale.ConvertTextKey( "TXT_KEY_VERSION_NUMBER", UI.GetVersionString() ) );
	end
end);

-- Enable multiplayer button for modded games
-- This works in conjunction with MOD_BIN_HOOKS to preserve compatible mods
Controls.MultiPlayerButton:SetHide(false);