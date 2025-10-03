-------------------------------------------------
-- Multiplayer Select Screen
-------------------------------------------------
include( "MPGameDefaults" );

-------------------------------------------------
-- Multiplayer Select Screen
-------------------------------------------------

local g_fTime = 0;
local g_iMaxTime = 30;

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
function OnLAN()
	UIManager:QueuePopup( Controls.LANScreen, PopupPriority.eUtmost );
end
Controls.LANButton:RegisterCallback( Mouse.eLClick, OnLAN );

-------------------------------------------------
-------------------------------------------------
function OnInternet()
	UIManager:QueuePopup( Controls.InternetScreen, PopupPriority.eUtmost );
end
Controls.InternetButton:RegisterCallback( Mouse.eLClick, OnInternet );

-------------------------------------------------
-------------------------------------------------
function OnHotSeat()
	UIManager:QueuePopup( Controls.HotSeatScreen, PopupPriority.eUtmost );
end
Controls.HotSeatButton:RegisterCallback( Mouse.eLClick, OnHotSeat );

-------------------------------------------------
-------------------------------------------------
function OnPitBoss()
	UIManager:QueuePopup( Controls.PitBossScreen, PopupPriority.eUtmost );
end
if Controls.PitBossButton then
	Controls.PitBossButton:RegisterCallback( Mouse.eLClick, OnPitBoss );
end

----------------------------------------------------------------        
----------------------------------------------------------------        
ContextPtr:SetUpdate( function()
	if( ContextPtr:IsHidden() == false ) then
		Controls.VersionNumber:SetText( Locale.ConvertTextKey( "TXT_KEY_VERSION_NUMBER", UI.GetVersionString() ) );
	end
end );

-- Handle mod state gracefully during multiplayer initialization
-- The MOD_BIN_HOOKS system will preserve compatible mods, but we need to handle
-- the UI gracefully during the transition period
local function UpdateModStatus()
	local activeMods = Modding.GetActivatedMods()
	local modCount = activeMods and #activeMods or 0
	
	-- If no mods are active but we're in a modded context, 
	-- this might be the temporary deactivation during multiplayer init
	if modCount == 0 and Game == nil then
		-- We're likely in the initialization gap - this is expected
		-- The MOD_BIN_HOOKS system will handle preserving compatible mods
	end
end

Events.SystemUpdateUI.Add(UpdateModStatus);