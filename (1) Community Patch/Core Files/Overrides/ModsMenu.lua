-------------------------------------------------
-- Mods Menu
-------------------------------------------------
include( "IconSupport" );
include( "InstanceManager" );
include( "SupportFunctions" );

local g_InstanceManager = InstanceManager:new( "ItemInstance", "Button", Controls.ItemStack );

-------------------------------------------------
-------------------------------------------------
function ShowHideHandler( bIsHide, bInitState )

	if( not bInitState ) then
		if( not bIsHide ) then
			OnBack();
		end
    end
end
ContextPtr:SetShowHideHandler( ShowHideHandler );

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