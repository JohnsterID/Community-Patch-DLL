/*	-------------------------------------------------------------------------------------------------------
	© 1991-2012 Take-Two Interactive Software and its subsidiaries.  Developed by Firaxis Games.  
	Sid Meier's Civilization V, Civ, Civilization, 2K Games, Firaxis Games, Take-Two Interactive Software 
	and their respective logos are all trademarks of Take-Two interactive Software, Inc.  
	All other marks and trademarks are the property of their respective owners.  
	All rights reserved. 
	------------------------------------------------------------------------------------------------------- */
//
//  AUTHOR:	Eric MacDonald  --  8/2003
//					Mustafa Thamer 11/2004
//					Jon Shafer - 03/2005
//
//  PURPOSE: The base class for all info classes to inherit from.  This gives us the base description
//				and type strings
//

#include "CvGameCoreDLLPCH.h"
#include "CvInfos.h"
#include "CvGlobals.h"
#include "CvGameTextMgr.h"
#include "CvGameCoreUtils.h"
#include "CvImprovementClasses.h"
#include "CvInfosSerializationHelper.h"
#include <FireWorks/Win32/FKBInputDevice.h>

// must be included after all other headers
#include "LintFree.h"
#ifdef _MSC_VER
#pragma warning ( disable : 4505 ) // unreferenced local function has been removed.. needed by REMARK below
#endif//_MSC_VER

//////////////////////////////////////////////////////////////////////////
// CvBaseInfo Members
//////////////////////////////////////////////////////////////////////////
CvBaseInfo::CvBaseInfo()
	: m_iID(-1),
	m_strCivilopedia("unknown"),
	m_strDescription("unknown"),
	m_strDescriptionKey("unknown"),
	m_strHelp("unknown"),
	m_strDisabledHelp("unknown"),
	m_strStrategy("unknown"),
	m_strType("unknown"),
	m_strTextKey("unknown"),
	m_strText("unknown")
{}
//------------------------------------------------------------------------------
bool CvBaseInfo::CacheResult(Database::Results& kResults)
{
	CvDatabaseUtility kUtility;
	return CacheResults(kResults, kUtility);
}
//------------------------------------------------------------------------------
bool CvBaseInfo::CacheResults(Database::Results& kResults, CvDatabaseUtility&)
{
	m_iID				= kResults.GetInt("ID");
	m_strType			= kResults.GetText("Type");
	m_strTextKey		= kResults.GetText("Text");
	m_strCivilopedia	= kResults.GetText("Civilopedia");
	m_strStrategy		= kResults.GetText("Strategy");
	m_strHelp			= kResults.GetText("Help");
	m_strDisabledHelp	= kResults.GetText("DisabledHelp");

	if(!m_strTextKey.empty())
		m_strText = GetLocalizedText(m_strTextKey);


	const char* szDescription = kResults.GetText("Description");
	if(szDescription)
	{
		m_strDescriptionKey = szDescription;
		m_strDescription = GetLocalizedText(szDescription);

		//CvInfoBase did this, gotta support it for now...
		if(m_strTextKey.empty())
		{
			m_strTextKey = szDescription;
			m_strText = m_strDescription;
		}
	}

	return true;
}
//------------------------------------------------------------------------------
const char* CvBaseInfo::GetText() const
{
	return (m_strText.empty())? NULL : m_strText.c_str();
}
//------------------------------------------------------------------------------
const char* CvBaseInfo::GetTextKey() const
{
	return (m_strTextKey.empty())? NULL : m_strTextKey.c_str();
}

bool CvBaseInfo::operator==(const CvBaseInfo& rhs) const
{
	if(this == &rhs) return true;
	if(m_iID != rhs.m_iID) return false;
	if(m_strCivilopedia != rhs.m_strCivilopedia) return false;
	if(m_strDescription != rhs.m_strDescription) return false;
	if(m_strHelp != rhs.m_strHelp) return false;
	if(m_strDisabledHelp != rhs.m_strDisabledHelp) return false;
	if(m_strStrategy != rhs.m_strStrategy) return false;
	if(m_strType != rhs.m_strType) return false;
	if(m_strTextKey != rhs.m_strTextKey) return false;
	if(m_strText != rhs.m_strText) return false;
	return true;
}

template<typename BaseInfo, typename Visitor>
void CvBaseInfo::Serialize(BaseInfo& baseInfo, Visitor& visitor)
{
	visitor(baseInfo.m_iID);
	visitor(baseInfo.m_strCivilopedia);
	visitor(baseInfo.m_strDescription);
	visitor(baseInfo.m_strHelp);
	visitor(baseInfo.m_strDisabledHelp);
	visitor(baseInfo.m_strStrategy);
	visitor(baseInfo.m_strType);
	visitor(baseInfo.m_strTextKey);
	visitor(baseInfo.m_strText);
}

void CvBaseInfo::readFrom(FDataStream& loadFrom)
{
	CvStreamLoadVisitor serialVisitor(loadFrom);
	Serialize(*this, serialVisitor);
}

void CvBaseInfo::writeTo(FDataStream& saveTo) const
{
	CvStreamSaveVisitor serialVisitor(saveTo);
	Serialize(*this, serialVisitor);
}

FDataStream& operator<<(FDataStream& saveTo, const CvBaseInfo& readFrom)
{
	readFrom.writeTo(saveTo);
	return saveTo;
}

FDataStream& operator>>(FDataStream& loadFrom, CvBaseInfo& writeTo)
{
	writeTo.readFrom(loadFrom);
	return loadFrom;
}

//======================================================================================================
//			CvHotKeyInfo
//======================================================================================================
CvHotKeyInfo::CvHotKeyInfo() :
	m_iActionInfoIndex(-1),
	m_iHotKeyVal(-1),
	m_iHotKeyPriority(-1),
	m_iHotKeyValAlt(-1),
	m_iHotKeyPriorityAlt(-1),
	m_iOrderPriority(0),
	m_bAltDown(false),
	m_bShiftDown(false),
	m_bCtrlDown(false),
	m_bAltDownAlt(false),
	m_bShiftDownAlt(false),
	m_bCtrlDownAlt(false)
{
}

//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   GetHotKeyInt(char* pszHotKeyVal)
//
//  PURPOSE :   returns either the integer value of the keyboard mapping for the hot key or -1 if it
//				doesn't exist.
//
//------------------------------------------------------------------------------------------------------
int CvHotKeyInfo::GetHotKeyInt(const char* pszHotKeyVal)
{
	// SPEEDUP
	struct CvKeyBoardMapping
	{
		char szDefineString[25];
		int iIntVal;
	};


	const int iNumKeyBoardMappings=108;
	const CvKeyBoardMapping asCvKeyBoardMapping[iNumKeyBoardMappings] =
	{
		{"KB_ESCAPE",FKBInputDevice::KB_ESCAPE},
		{"KB_0",FKBInputDevice::KB_0},
		{"KB_1",FKBInputDevice::KB_1},
		{"KB_2",FKBInputDevice::KB_2},
		{"KB_3",FKBInputDevice::KB_3},
		{"KB_4",FKBInputDevice::KB_4},
		{"KB_5",FKBInputDevice::KB_5},
		{"KB_6",FKBInputDevice::KB_6},
		{"KB_7",FKBInputDevice::KB_7},
		{"KB_8",FKBInputDevice::KB_8},
		{"KB_9",FKBInputDevice::KB_9},
		{"KB_MINUS",FKBInputDevice::KB_MINUS},	    /* - on main keyboard */
		{"KB_A",FKBInputDevice::KB_A},
		{"KB_B",FKBInputDevice::KB_B},
		{"KB_C",FKBInputDevice::KB_C},
		{"KB_D",FKBInputDevice::KB_D},
		{"KB_E",FKBInputDevice::KB_E},
		{"KB_F",FKBInputDevice::KB_F},
		{"KB_G",FKBInputDevice::KB_G},
		{"KB_H",FKBInputDevice::KB_H},
		{"KB_I",FKBInputDevice::KB_I},
		{"KB_J",FKBInputDevice::KB_J},
		{"KB_K",FKBInputDevice::KB_K},
		{"KB_L",FKBInputDevice::KB_L},
		{"KB_M",FKBInputDevice::KB_M},
		{"KB_N",FKBInputDevice::KB_N},
		{"KB_O",FKBInputDevice::KB_O},
		{"KB_P",FKBInputDevice::KB_P},
		{"KB_Q",FKBInputDevice::KB_Q},
		{"KB_R",FKBInputDevice::KB_R},
		{"KB_S",FKBInputDevice::KB_S},
		{"KB_T",FKBInputDevice::KB_T},
		{"KB_U",FKBInputDevice::KB_U},
		{"KB_V",FKBInputDevice::KB_V},
		{"KB_W",FKBInputDevice::KB_W},
		{"KB_X",FKBInputDevice::KB_X},
		{"KB_Y",FKBInputDevice::KB_Y},
		{"KB_Z",FKBInputDevice::KB_Z},
		{"KB_EQUALS",FKBInputDevice::KB_EQUALS},
		{"KB_BACKSPACE",FKBInputDevice::KB_BACKSPACE},
		{"KB_TAB",FKBInputDevice::KB_TAB},
		{"KB_LBRACKET",FKBInputDevice::KB_LBRACKET},
		{"KB_RBRACKET",FKBInputDevice::KB_RBRACKET},
		{"KB_RETURN",FKBInputDevice::KB_RETURN},		/* Enter on main keyboard */
		{"KB_LCONTROL",FKBInputDevice::KB_LCONTROL},
		{"KB_SEMICOLON",FKBInputDevice::KB_SEMICOLON},
		{"KB_APOSTROPHE",FKBInputDevice::KB_APOSTROPHE},
		{"KB_GRAVE",FKBInputDevice::KB_GRAVE},		/* accent grave */
		{"KB_LSHIFT",FKBInputDevice::KB_LSHIFT},
		{"KB_BACKSLASH",FKBInputDevice::KB_BACKSLASH},
		{"KB_COMMA",FKBInputDevice::KB_COMMA},
		{"KB_PERIOD",FKBInputDevice::KB_PERIOD},
		{"KB_SLASH",FKBInputDevice::KB_SLASH},
		{"KB_RSHIFT",FKBInputDevice::KB_RSHIFT},
		{"KB_NUMPADSTAR",FKBInputDevice::KB_NUMPADSTAR},
		{"KB_LALT",FKBInputDevice::KB_LALT},
		{"KB_SPACE",FKBInputDevice::KB_SPACE},
		{"KB_CAPSLOCK",FKBInputDevice::KB_CAPSLOCK},
		{"KB_F1",FKBInputDevice::KB_F1},
		{"KB_F2",FKBInputDevice::KB_F2},
		{"KB_F3",FKBInputDevice::KB_F3},
		{"KB_F4",FKBInputDevice::KB_F4},
		{"KB_F5",FKBInputDevice::KB_F5},
		{"KB_F6",FKBInputDevice::KB_F6},
		{"KB_F7",FKBInputDevice::KB_F7},
		{"KB_F8",FKBInputDevice::KB_F8},
		{"KB_F9",FKBInputDevice::KB_F9},
		{"KB_F10",FKBInputDevice::KB_F10},
		{"KB_NUMLOCK",FKBInputDevice::KB_NUMLOCK},
		{"KB_SCROLL",FKBInputDevice::KB_SCROLL},
		{"KB_NUMPAD7",FKBInputDevice::KB_NUMPAD7},
		{"KB_NUMPAD8",FKBInputDevice::KB_NUMPAD8},
		{"KB_NUMPAD9",FKBInputDevice::KB_NUMPAD9},
		{"KB_NUMPADMINUS",FKBInputDevice::KB_NUMPADMINUS},
		{"KB_NUMPAD4",FKBInputDevice::KB_NUMPAD4},
		{"KB_NUMPAD5",FKBInputDevice::KB_NUMPAD5},
		{"KB_NUMPAD6",FKBInputDevice::KB_NUMPAD6},
		{"KB_NUMPADPLUS",FKBInputDevice::KB_NUMPADPLUS},
		{"KB_NUMPAD1",FKBInputDevice::KB_NUMPAD1},
		{"KB_NUMPAD2",FKBInputDevice::KB_NUMPAD2},
		{"KB_NUMPAD3",FKBInputDevice::KB_NUMPAD3},
		{"KB_NUMPAD0",FKBInputDevice::KB_NUMPAD0},
		{"KB_NUMPADPERIOD",FKBInputDevice::KB_NUMPADPERIOD},
		{"KB_F11",FKBInputDevice::KB_F11},
		{"KB_F12",FKBInputDevice::KB_F12},
		{"KB_NUMPADEQUALS",FKBInputDevice::KB_NUMPADEQUALS},
		{"KB_AT",FKBInputDevice::KB_AT},
		{"KB_UNDERLINE",FKBInputDevice::KB_UNDERLINE},
		{"KB_COLON",FKBInputDevice::KB_COLON},
		{"KB_NUMPADENTER",FKBInputDevice::KB_NUMPADENTER},
		{"KB_RCONTROL",FKBInputDevice::KB_RCONTROL},
		{"KB_VOLUMEDOWN",FKBInputDevice::KB_VOLUMEDOWN},
		{"KB_VOLUMEUP",FKBInputDevice::KB_VOLUMEUP},
		{"KB_NUMPADCOMMA",FKBInputDevice::KB_NUMPADCOMMA},
		{"KB_NUMPADSLASH",FKBInputDevice::KB_NUMPADSLASH},
		{"KB_SYSRQ",FKBInputDevice::KB_SYSRQ},
		{"KB_RALT",FKBInputDevice::KB_RALT},
		{"KB_PAUSE",FKBInputDevice::KB_PAUSE},
		{"KB_HOME",FKBInputDevice::KB_HOME},
		{"KB_UP",FKBInputDevice::KB_UP},
		{"KB_PGUP",FKBInputDevice::KB_PGUP},
		{"KB_LEFT",FKBInputDevice::KB_LEFT},
		{"KB_RIGHT",FKBInputDevice::KB_RIGHT},
		{"KB_END",FKBInputDevice::KB_END},
		{"KB_DOWN",FKBInputDevice::KB_DOWN},
		{"KB_PGDN",FKBInputDevice::KB_PGDN},
		{"KB_INSERT",FKBInputDevice::KB_INSERT},
		{"KB_DELETE",FKBInputDevice::KB_DELETE},
	};

	if(pszHotKeyVal)
	{
		for(int i = 0; i < iNumKeyBoardMappings; ++i)
		{
			if(strcmp(asCvKeyBoardMapping [i].szDefineString, pszHotKeyVal) == 0)
			{
				return asCvKeyBoardMapping[i].iIntVal;
			}
		}
	}


	return -1;
}


//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   CreateHotKeyFromDescription(bool bShift, bool bAlt, bool bCtrl)
//
//  PURPOSE :   create a hot key from a description and return it
//
//------------------------------------------------------------------------------------------------------
CvString CvHotKeyInfo::CreateHotKeyFromDescription(const char* pszHotKey, bool bShift, bool bAlt, bool bCtrl)
{
	CvString strHotKey;

	if(pszHotKey && strcmp(pszHotKey,"") != 0)
	{
		if(bShift)
		{
			strHotKey += GetLocalizedText("TXT_KEY_SHIFT");
		}

		if(bAlt)
		{
			strHotKey += GetLocalizedText("TXT_KEY_ALT");
		}

		if(bCtrl)
		{
			strHotKey += GetLocalizedText("TXT_KEY_CTRL");
		}

		strHotKey += CreateKeyStringFromKBCode(pszHotKey);
	}

	return strHotKey;
}

//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   CreateKeyStringFromKBCode(const char* pszHotKey)
//
//  PURPOSE :   Create a keyboard string from a KB code, Delete would be returned for KB_DELETE
//
//------------------------------------------------------------------------------------------------------
CvString CvHotKeyInfo::CreateKeyStringFromKBCode(const char* pszHotKey)
{
	struct CvKeyBoardMapping
	{
		const char* szDefineString;
		const char* szKeyString;
		bool bIsLocalizedText;
	};

	// TODO - this should be a stl map instead of looping strcmp
	const CvKeyBoardMapping asCvKeyBoardMapping[] =
	{
		{"KB_ESCAPE", "TXT_KEY_KEYBOARD_ESCAPE", true},
		{"KB_0","0", false},
		{"KB_1","1", false},
		{"KB_2","2", false},
		{"KB_3","3", false},
		{"KB_4","4", false},
		{"KB_5","5", false},
		{"KB_6","6", false},
		{"KB_7","7", false},
		{"KB_8","8", false},
		{"KB_9","9", false},
		{"KB_MINUS","-", false},	    // - on main keyboard
		{"KB_A","A", false},
		{"KB_B","B", false},
		{"KB_C","C", false},
		{"KB_D","D", false},
		{"KB_E","E", false},
		{"KB_F","F", false},
		{"KB_G","G", false},
		{"KB_H","H", false},
		{"KB_I","I", false},
		{"KB_J","J", false},
		{"KB_K","K", false},
		{"KB_L","L", false},
		{"KB_M","M", false},
		{"KB_N","N", false},
		{"KB_O","O", false},
		{"KB_P","P", false},
		{"KB_Q","Q", false},
		{"KB_R","R", false},
		{"KB_S","S", false},
		{"KB_T","T", false},
		{"KB_U","U", false},
		{"KB_V","V", false},
		{"KB_W","W", false},
		{"KB_X","X", false},
		{"KB_Y","Y", false},
		{"KB_Z","Z", false},
		{"KB_EQUALS","=", false},
		{"KB_BACKSPACE", "TXT_KEY_KEYBOARD_BACKSPACE", true},
		{"KB_TAB","TAB", false},
		{"KB_LBRACKET","[", false},
		{"KB_RBRACKET","]", false},
		{"KB_RETURN", "TXT_KEY_KEYBOARD_ENTER", true},		// Enter on main keyboard
		{"KB_LCONTROL", "TXT_KEY_KEYBOARD_LEFT_CONTROL_KEY", true},
		{"KB_SEMICOLON",";", false},
		{"KB_APOSTROPHE","'", false},
		{"KB_GRAVE","`", false},		// accent grave
		{"KB_LSHIFT", "TXT_KEY_KEYBOARD_LEFT_SHIFT_KEY", true},
		{"KB_BACKSLASH","\\"},
		{"KB_COMMA",",", false},
		{"KB_PERIOD",".", false},
		{"KB_SLASH","/", false},
		{"KB_RSHIFT", "TXT_KEY_KEYBOARD_RIGHT_SHIFT_KEY", true},
		{"KB_NUMPADSTAR", "TXT_KEY_KEYBOARD_NUM_PAD_STAR", true},
		{"KB_LALT", "TXT_KEY_KEYBOARD_LEFT_ALT_KEY", true},
		{"KB_SPACE", "TXT_KEY_KEYBOARD_SPACE_KEY", true},
		{"KB_CAPSLOCK", "TXT_KEY_KEYBOARD_CAPS_LOCK", true},
		{"KB_F1","F1", false},
		{"KB_F2","F2", false},
		{"KB_F3","F3", false},
		{"KB_F4","F4", false},
		{"KB_F5","F5", false},
		{"KB_F6","F6", false},
		{"KB_F7","F7", false},
		{"KB_F8","F8", false},
		{"KB_F9","F9", false},
		{"KB_F10","F10", false},
		{"KB_NUMLOCK", "TXT_KEY_KEYBOARD_NUM_LOCK", true},
		{"KB_SCROLL", "TXT_KEY_KEYBOARD_SCROLL_KEY", true},
		{"KB_NUMPAD7", "TXT_KEY_KEYBOARD_NUMPAD_NUMBER7", true},
		{"KB_NUMPAD8", "TXT_KEY_KEYBOARD_NUMPAD_NUMBER8", true},
		{"KB_NUMPAD9", "TXT_KEY_KEYBOARD_NUMPAD_NUMBER9", true},
		{"KB_NUMPADMINUS", "TXT_KEY_KEYBOARD_NUMPAD_MINUS", true},
		{"KB_NUMPAD4", "TXT_KEY_KEYBOARD_NUMPAD_NUMBER4", true},
		{"KB_NUMPAD5", "TXT_KEY_KEYBOARD_NUMPAD_NUMBER5", true},
		{"KB_NUMPAD6", "TXT_KEY_KEYBOARD_NUMPAD_NUMBER6", true},
		{"KB_NUMPADPLUS", "TXT_KEY_KEYBOARD_NUMPAD_PLUS", true},
		{"KB_NUMPAD1", "TXT_KEY_KEYBOARD_NUMPAD_NUMBER1", true},
		{"KB_NUMPAD2", "TXT_KEY_KEYBOARD_NUMPAD_NUMBER2", true},
		{"KB_NUMPAD3", "TXT_KEY_KEYBOARD_NUMPAD_NUMBER3", true},
		{"KB_NUMPAD0", "TXT_KEY_KEYBOARD_NUMPAD_NUMBER0", true},
		{"KB_NUMPADPERIOD", "TXT_KEY_KEYBOARD_NUMPAD_PERIOD", true},
		{"KB_F11","F11", false},
		{"KB_F12","F12", false},
		{"KB_NUMPADEQUALS", "TXT_KEY_KEYBOARD_NUMPAD_EQUALS", true},
		{"KB_AT","@", false},
		{"KB_UNDERLINE","_", false},
		{"KB_COLON",":", false},
		{"KB_NUMPADENTER", "TXT_KEY_KEYBOARD_NUMPAD_ENTER_KEY", true},
		{"KB_RCONTROL", "TXT_KEY_KEYBOARD_RIGHT_CONTROL_KEY", true},
		{"KB_VOLUMEDOWN", "TXT_KEY_KEYBOARD_VOLUME_DOWN", true},
		{"KB_VOLUMEUP", "TXT_KEY_KEYBOARD_VOLUME_UP", true},
		{"KB_NUMPADCOMMA", "TXT_KEY_KEYBOARD_NUMPAD_COMMA", true},
		{"KB_NUMPADSLASH", "TXT_KEY_KEYBOARD_NUMPAD_SLASH", true},
		{"KB_SYSRQ", "TXT_KEY_KEYBOARD_SYSRQ", true},
		{"KB_RALT", "TXT_KEY_KEYBOARD_RIGHT_ALT_KEY", true},
		{"KB_PAUSE", "TXT_KEY_KEYBOARD_PAUSE_KEY", true},
		{"KB_HOME", "TXT_KEY_KEYBOARD_HOME_KEY", true},
		{"KB_UP", "TXT_KEY_KEYBOARD_UP_ARROW", true},
		{"KB_PGUP", "TXT_KEY_KEYBOARD_PAGE_UP", true},
		{"KB_LEFT", "TXT_KEY_KEYBOARD_LEFT_ARROW", true},
		{"KB_RIGHT", "TXT_KEY_KEYBOARD_RIGHT_ARROW", true},
		{"KB_END", "TXT_KEY_KEYBOARD_END_KEY", true},
		{"KB_DOWN", "TXT_KEY_KEYBOARD_DOWN_ARROW", true},
		{"KB_PGDN", "TXT_KEY_KEYBOARD_PAGE_DOWN", true},
		{"KB_INSERT", "TXT_KEY_KEYBOARD_INSERT_KEY", true},
		{"KB_DELETE", "TXT_KEY_KEYBOARD_DELETE_KEY", true},
		{NULL, NULL, false},
	};

	size_t i = 0;
	while(asCvKeyBoardMapping[i].szDefineString != NULL)
	{
		if(strcmp(pszHotKey, asCvKeyBoardMapping[i].szDefineString) == 0)
		{
			if(asCvKeyBoardMapping[i].bIsLocalizedText)
			{
				return GetLocalizedText(asCvKeyBoardMapping[i].szKeyString);
			}
			else
			{
				return asCvKeyBoardMapping[i].szKeyString;
			}
		}
		i++;
	}

	return "";
}

bool CvHotKeyInfo::CacheResults(Database::Results& kResults, CvDatabaseUtility& kUtility)
{
	if(!CvBaseInfo::CacheResults(kResults, kUtility))
		return false;

	setHotKey(kResults.GetText("HotKey"));
	const int iHotKeyVal = GetHotKeyInt(kResults.GetText("HotKey"));
	setHotKeyVal(iHotKeyVal);
	setHotKeyPriority(kResults.GetInt("HotKeyPriority"));

	const int iHotKeyValAlt = GetHotKeyInt(kResults.GetText("HotKeyAlt"));
	setHotKeyValAlt(iHotKeyValAlt);
	setHotKeyPriorityAlt(kResults.GetInt("HotKeyPriorityAlt"));

	setAltDown(kResults.GetBool("AltDown"));
	setAltDownAlt(kResults.GetBool("AltDownAlt"));

	setShiftDown(kResults.GetBool("ShiftDown"));
	setShiftDownAlt(kResults.GetBool("ShiftDownAlt"));

	setCtrlDown(kResults.GetBool("CtrlDown"));
	setCtrlDownAlt(kResults.GetBool("CtrlDownAlt"));

	setOrderPriority(kResults.GetInt("OrderPriority"));

	const char* szHelp = kResults.GetText("Help");
	if(szHelp)
		m_strHelp = szHelp;

	const char* szDisabledHelp = kResults.GetText("DisabledHelp");
	if(szDisabledHelp)
		m_strDisabledHelp = szDisabledHelp;


	setHotKeyDescription(GetTextKey(), NULL, CreateHotKeyFromDescription(getHotKey(), m_bShiftDown, m_bAltDown, m_bCtrlDown));

	return true;
}
//------------------------------------------------------------------------------
int CvHotKeyInfo::getActionInfoIndex() const
{
	return m_iActionInfoIndex;
}
//------------------------------------------------------------------------------
void CvHotKeyInfo::setActionInfoIndex(int i)
{
	m_iActionInfoIndex = i;
}
//------------------------------------------------------------------------------
int CvHotKeyInfo::getHotKeyVal() const
{
	return m_iHotKeyVal;
}
//------------------------------------------------------------------------------
void CvHotKeyInfo::setHotKeyVal(int i)
{
	m_iHotKeyVal = i;
}
//------------------------------------------------------------------------------
int CvHotKeyInfo::getHotKeyPriority() const
{
	return m_iHotKeyPriority;
}
//------------------------------------------------------------------------------
void CvHotKeyInfo::setHotKeyPriority(int i)
{
	m_iHotKeyPriority = i;
}
//------------------------------------------------------------------------------
int CvHotKeyInfo::getHotKeyValAlt() const
{
	return m_iHotKeyValAlt;
}
//------------------------------------------------------------------------------
void CvHotKeyInfo::setHotKeyValAlt(int i)
{
	m_iHotKeyValAlt = i;
}
//------------------------------------------------------------------------------
int CvHotKeyInfo::getHotKeyPriorityAlt() const
{
	return m_iHotKeyPriorityAlt;
}
//------------------------------------------------------------------------------
void CvHotKeyInfo::setHotKeyPriorityAlt(int i)
{
	m_iHotKeyPriorityAlt = i;
}
//------------------------------------------------------------------------------
int CvHotKeyInfo::getOrderPriority() const
{
	return m_iOrderPriority;
}
//------------------------------------------------------------------------------
void CvHotKeyInfo::setOrderPriority(int i)
{
	m_iOrderPriority = i;
}
//------------------------------------------------------------------------------
bool CvHotKeyInfo::isAltDown() const
{
	return m_bAltDown;
}
//------------------------------------------------------------------------------
void CvHotKeyInfo::setAltDown(bool b)
{
	m_bAltDown = b;
}
//------------------------------------------------------------------------------
bool CvHotKeyInfo::isShiftDown() const
{
	return m_bShiftDown;
}
//------------------------------------------------------------------------------
void CvHotKeyInfo::setShiftDown(bool b)
{
	m_bShiftDown = b;
}
//------------------------------------------------------------------------------
bool CvHotKeyInfo::isCtrlDown() const
{
	return m_bCtrlDown;
}
//------------------------------------------------------------------------------
void CvHotKeyInfo::setCtrlDown(bool b)
{
	m_bCtrlDown = b;
}
//------------------------------------------------------------------------------
bool CvHotKeyInfo::isAltDownAlt() const
{
	return m_bAltDownAlt;
}
//------------------------------------------------------------------------------
void CvHotKeyInfo::setAltDownAlt(bool b)
{
	m_bAltDownAlt = b;
}
//------------------------------------------------------------------------------
bool CvHotKeyInfo::isShiftDownAlt() const
{
	return m_bShiftDownAlt;
}
//------------------------------------------------------------------------------
void CvHotKeyInfo::setShiftDownAlt(bool b)
{
	m_bShiftDownAlt = b;
}
//------------------------------------------------------------------------------
bool CvHotKeyInfo::isCtrlDownAlt() const
{
	return m_bCtrlDownAlt;
}
//------------------------------------------------------------------------------
void CvHotKeyInfo::setCtrlDownAlt(bool b)
{
	m_bCtrlDownAlt = b;
}
//------------------------------------------------------------------------------
const char* CvHotKeyInfo::getHotKey() const
{
	return m_strHotKey;
}
//------------------------------------------------------------------------------
void CvHotKeyInfo::setHotKey(const char* szVal)
{
	m_strHotKey = szVal;
}
//------------------------------------------------------------------------------
const char* CvHotKeyInfo::getHelp() const
{
	return m_strHelp.c_str();
}
//------------------------------------------------------------------------------
const char* CvHotKeyInfo::getDisabledHelp() const
{
	return m_strDisabledHelp.c_str();
}
//------------------------------------------------------------------------------
std::string CvHotKeyInfo::getHotKeyDescription() const
{
	Localization::String strTempText;
	std::string strHotKeyDescription;
	if(!m_strHotKeyAltDescriptionKey.empty())
	{
		strTempText = "{1: textkey} ({2: textkey})";
		strTempText << m_strHotKeyAltDescriptionKey.c_str();
		strTempText << m_strHotKeyDescriptionKey.c_str();
	}
	else
	{
		strTempText = Localization::Lookup(m_strHotKeyDescriptionKey.c_str());
	}

	strHotKeyDescription = strTempText.toUTF8();

	if(!m_strHotKeyString.empty())
	{
		strHotKeyDescription += m_strHotKeyString;
	}

	return strHotKeyDescription;
}
//------------------------------------------------------------------------------
const char* CvHotKeyInfo::getHotKeyString() const
{
	return m_strHotKeyString.c_str();
}

//------------------------------------------------------------------------------
void CvHotKeyInfo::setHotKeyDescription(const char* szHotKeyDescKey, const char* szHotKeyAltDescKey, const char* szHotKeyString)
{
	m_strHotKeyDescriptionKey = szHotKeyDescKey;
	m_strHotKeyAltDescriptionKey = szHotKeyAltDescKey;
	m_strHotKeyString = szHotKeyString;
}

//======================================================================================================
//					CvSpecialistInfo
//======================================================================================================
CvSpecialistInfo::CvSpecialistInfo() :
	m_iCost(0),
	m_iGreatPeopleUnitClass(NO_UNITCLASS),
	m_iGreatPeopleRateChange(0),
	m_iCulturePerTurn(0),
	m_iMissionType(NO_MISSION),
	m_bVisible(false),
	m_piYieldChange(NULL),
	m_iExperience(0)
{
}
//------------------------------------------------------------------------------
CvSpecialistInfo::~CvSpecialistInfo()
{
	SAFE_DELETE_ARRAY(m_piYieldChange);
}
//------------------------------------------------------------------------------
int CvSpecialistInfo::getCost() const
{
	return m_iCost;
}
//------------------------------------------------------------------------------
int CvSpecialistInfo::getGreatPeopleUnitClass() const
{
	return m_iGreatPeopleUnitClass;
}
//------------------------------------------------------------------------------
int CvSpecialistInfo::getGreatPeopleRateChange() const
{
	return m_iGreatPeopleRateChange;
}
//------------------------------------------------------------------------------
int CvSpecialistInfo::getCulturePerTurn() const
{
	return m_iCulturePerTurn;
}

int CvSpecialistInfo::getMissionType() const
{
	return m_iMissionType;
}
//------------------------------------------------------------------------------
void CvSpecialistInfo::setMissionType(int iNewType)
{
	m_iMissionType = iNewType;
}
//------------------------------------------------------------------------------
bool CvSpecialistInfo::isVisible() const
{
	return m_bVisible;
}
//------------------------------------------------------------------------------
int CvSpecialistInfo::getExperience() const
{
	return m_iExperience;
}
//------------------------------------------------------------------------------
int CvSpecialistInfo::getYieldChange(int i) const
{
	ASSERT_DEBUG(i < NUM_YIELD_TYPES, "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_piYieldChange ? m_piYieldChange[i] : -1;
}
//------------------------------------------------------------------------------
const int* CvSpecialistInfo::getYieldChangeArray() const
{
	return m_piYieldChange;
}
//------------------------------------------------------------------------------
const char* CvSpecialistInfo::getTexture() const
{
	return m_strTexture;
}
//------------------------------------------------------------------------------
void CvSpecialistInfo::setTexture(const char* szVal)
{
	m_strTexture = szVal;
}
//------------------------------------------------------------------------------
bool CvSpecialistInfo::CacheResults(Database::Results& kResults, CvDatabaseUtility& kUtility)
{
	if(!CvHotKeyInfo::CacheResults(kResults, kUtility))
		return false;

	m_bVisible = kResults.GetBool("Visible");
	m_iCost = kResults.GetInt("Cost");
	m_iExperience = kResults.GetInt("Experience");
	m_iGreatPeopleRateChange = kResults.GetInt("GreatPeopleRateChange");
	m_iCulturePerTurn = kResults.GetInt("CulturePerTurn");

	setTexture(kResults.GetText("Texture"));

	const char* szGreatPeople = kResults.GetText("GreatPeopleUnitClass");
	m_iGreatPeopleUnitClass = GC.getInfoTypeForString(szGreatPeople, true);

	//Arrays
	const char* szType = GetType();
	kUtility.SetYields(m_piYieldChange, "SpecialistYields", "SpecialistType", szType);

	return true;
}

//======================================================================================================
//					CvMissionInfo
//======================================================================================================
CvMissionInfo::CvMissionInfo() :
	m_iTime(0),
	m_bSound(false),
	m_bTarget(false),
	m_bBuild(false),
	m_bVisible(false),
	m_eEntityEvent(ENTITY_EVENT_NONE)
{
}

int CvMissionInfo::getTime() const
{
	return m_iTime;
}

bool CvMissionInfo::isSound() const
{
	return m_bSound;
}

bool CvMissionInfo::isTarget() const
{
	return m_bTarget;
}

bool CvMissionInfo::isBuild() const
{
	return m_bBuild;
}

bool CvMissionInfo::getVisible() const
{
	return m_bVisible;
}

const char* CvMissionInfo::getWaypoint() const
{
	return m_strWaypoint;
}

EntityEventTypes CvMissionInfo::getEntityEvent() const
{
	return m_eEntityEvent;
}

bool CvMissionInfo::CacheResults(Database::Results& kResults, CvDatabaseUtility& kUtility)
{
	if(!CvHotKeyInfo::CacheResults(kResults, kUtility))
		return false;

	m_strWaypoint	= kResults.GetText("Waypoint");
	m_iTime			= kResults.GetInt("Time");
	m_bSound		= kResults.GetBool("Sound");
	m_bTarget		= kResults.GetBool("Target");
	m_bBuild		= kResults.GetBool("Build");
	m_bVisible		= kResults.GetBool("Visible");

	const char* szEntityEventType = kResults.GetText("EntityEventType");
	if(szEntityEventType)
	{
		m_eEntityEvent = (EntityEventTypes)GC.getInfoTypeForString(szEntityEventType, true);
	}
	else
	{
		m_eEntityEvent = ENTITY_EVENT_NONE;
	}

	return true;
}

//======================================================================================================
//					CvCommandInfo
//======================================================================================================
CvCommandInfo::CvCommandInfo() :
	m_iAutomate(NO_AUTOMATE),
	m_bConfirmCommand(false),
	m_bVisible(false),
	m_bAll(false)
{
}
//------------------------------------------------------------------------------
int CvCommandInfo::getAutomate() const
{
	return m_iAutomate;
}
//------------------------------------------------------------------------------
void CvCommandInfo::setAutomate(int i)
{
	m_iAutomate = i;
}
//------------------------------------------------------------------------------
bool CvCommandInfo::getConfirmCommand() const
{
	return m_bConfirmCommand;
}
//------------------------------------------------------------------------------
bool CvCommandInfo::getVisible() const
{
	return m_bVisible;
}
//------------------------------------------------------------------------------
bool CvCommandInfo::getAll() const
{
	return m_bAll;
}
//------------------------------------------------------------------------------
bool CvCommandInfo::CacheResults(Database::Results& kResults, CvDatabaseUtility& kUtility)
{
	if(!CvHotKeyInfo::CacheResults(kResults, kUtility))
		return false;

	const char* szAutomate = kResults.GetText("Automate");
	if(szAutomate)
		setAutomate(GC.getInfoTypeForString(szAutomate, true));

	m_bConfirmCommand = kResults.GetBool("ConfirmCommand");
	m_bVisible = kResults.GetBool("Visible");
	m_bAll = kResults.GetBool("All");

	return true;
}

//======================================================================================================
//					CvAutomateInfo
//======================================================================================================
CvAutomateInfo::CvAutomateInfo() :
	m_iCommand(NO_COMMAND),
	m_iAutomate(NO_AUTOMATE),
	m_bConfirmCommand(false),
	m_bVisible(false)
{
}
//------------------------------------------------------------------------------
int CvAutomateInfo::getCommand() const
{
	return m_iCommand;
}
//------------------------------------------------------------------------------
void CvAutomateInfo::setCommand(int i)
{
	m_iCommand = i;
}
//------------------------------------------------------------------------------
int CvAutomateInfo::getAutomate() const
{
	return m_iAutomate;
}
//------------------------------------------------------------------------------
void CvAutomateInfo::setAutomate(int i)
{
	m_iAutomate = i;
}
//------------------------------------------------------------------------------
bool CvAutomateInfo::getConfirmCommand() const
{
	return m_bConfirmCommand;
}
//------------------------------------------------------------------------------
bool CvAutomateInfo::getVisible() const
{
	return m_bVisible;
}
//------------------------------------------------------------------------------
bool CvAutomateInfo::CacheResults(Database::Results& kResults, CvDatabaseUtility& kUtility)
{
	if(!CvHotKeyInfo::CacheResults(kResults, kUtility))
		return false;

	const char* szCommand = kResults.GetText("Command");
	setCommand(GC.getInfoTypeForString(szCommand, true));

	const char* szAutomate = kResults.GetText("Automate");
	setAutomate(GC.getInfoTypeForString(szAutomate, true));

	m_bConfirmCommand = kResults.GetBool("ConfirmCommand");
	m_bVisible = kResults.GetBool("Visible");

	return true;
}

//======================================================================================================
//					CvActionInfo
//======================================================================================================
CvActionInfo::CvActionInfo() :
	m_iOriginalIndex(-1),
	m_eSubType(NO_ACTIONSUBTYPE)
{
}
//------------------------------------------------------------------------------
int CvActionInfo::getMissionData() const
{

	if(
	    (ACTIONSUBTYPE_BUILD == m_eSubType)			||
	    (ACTIONSUBTYPE_SPECIALIST == m_eSubType)
	)
	{
		return m_iOriginalIndex;
	}

	return -1;
}
//------------------------------------------------------------------------------
int CvActionInfo::getCommandData() const
{

	if(ACTIONSUBTYPE_PROMOTION == m_eSubType)
	{
		return m_iOriginalIndex;
	}
	else if(ACTIONSUBTYPE_COMMAND == m_eSubType)
	{
		return GC.getCommandInfo((CommandTypes)m_iOriginalIndex)->getAutomate();
	}
	else if(ACTIONSUBTYPE_AUTOMATE == m_eSubType)
	{
		return GC.getAutomateInfo(m_iOriginalIndex)->getAutomate();
	}

	return -1;
}
//------------------------------------------------------------------------------
int CvActionInfo::getAutomateType() const
{

	if(ACTIONSUBTYPE_COMMAND == m_eSubType)
	{
		CvCommandInfo* pkCommandInfo = GC.getCommandInfo((CommandTypes)m_iOriginalIndex);
		if(pkCommandInfo)
		{
			return pkCommandInfo->getAutomate();
		}
	}
	else if(ACTIONSUBTYPE_AUTOMATE == m_eSubType)
	{
		CvAutomateInfo* pkAutomateInfo = GC.getAutomateInfo(m_iOriginalIndex);
		if(pkAutomateInfo)
		{
			return pkAutomateInfo->getAutomate();
		}
	}

	return NO_AUTOMATE;
}
//------------------------------------------------------------------------------
int CvActionInfo::getInterfaceModeType() const
{
	if(ACTIONSUBTYPE_INTERFACEMODE == m_eSubType)
	{
		return m_iOriginalIndex;
	}
	return NO_INTERFACEMODE;
}
//------------------------------------------------------------------------------
int CvActionInfo::getMissionType() const
{
	if(ACTIONSUBTYPE_BUILD == m_eSubType)
	{
		CvBuildInfo* pkBuildInfo = GC.getBuildInfo((BuildTypes)m_iOriginalIndex);
		if(pkBuildInfo)
			return pkBuildInfo->getMissionType();
	}
	else if(ACTIONSUBTYPE_SPECIALIST == m_eSubType)
	{
		CvSpecialistInfo* pkSpecialistInfo = GC.getSpecialistInfo((SpecialistTypes)m_iOriginalIndex);
		if(pkSpecialistInfo)
			return pkSpecialistInfo->getMissionType();
	}
	else if(ACTIONSUBTYPE_MISSION == m_eSubType)
	{
		return m_iOriginalIndex;
	}

	return NO_MISSION;
}
//------------------------------------------------------------------------------
int CvActionInfo::getCommandType() const
{
	if(ACTIONSUBTYPE_COMMAND == m_eSubType)
	{
		return m_iOriginalIndex;
	}
	else if(ACTIONSUBTYPE_PROMOTION == m_eSubType)
	{
		CvPromotionEntry* pkPromotionInfo = GC.getPromotionInfo((PromotionTypes)m_iOriginalIndex);
		if(pkPromotionInfo)
		{
			return pkPromotionInfo->GetCommandType();
		}
	}
	else if(ACTIONSUBTYPE_AUTOMATE == m_eSubType)
	{
		CvAutomateInfo* pkAutomateInfo = GC.getAutomateInfo(m_iOriginalIndex);
		if(pkAutomateInfo)
		{
			return pkAutomateInfo->getCommand();
		}
	}

	return NO_COMMAND;
}
//------------------------------------------------------------------------------
int CvActionInfo::getControlType() const
{
	if(ACTIONSUBTYPE_CONTROL == m_eSubType)
	{
		return m_iOriginalIndex;
	}
	return -1;
}
//------------------------------------------------------------------------------
int CvActionInfo::getOriginalIndex() const
{
	return m_iOriginalIndex;
}
//------------------------------------------------------------------------------
void CvActionInfo::setOriginalIndex(int i)
{
	m_iOriginalIndex = i;
}
//------------------------------------------------------------------------------
bool CvActionInfo::isConfirmCommand() const
{
	if(ACTIONSUBTYPE_COMMAND == m_eSubType)
	{
		CvCommandInfo* pkCommandInfo = GC.getCommandInfo((CommandTypes)m_iOriginalIndex);
		if(pkCommandInfo)
		{
			return pkCommandInfo->getConfirmCommand();
		}
	}
	else if(ACTIONSUBTYPE_AUTOMATE == m_eSubType)
	{
		CvAutomateInfo* pkAutomateInfo = GC.getAutomateInfo(m_iOriginalIndex);
		if(pkAutomateInfo)
		{
			return pkAutomateInfo->getConfirmCommand();
		}
	}

	return false;
}
//------------------------------------------------------------------------------
bool CvActionInfo::isVisible() const
{
	if(ACTIONSUBTYPE_CONTROL == m_eSubType)
	{
		return false;
	}
	else if(ACTIONSUBTYPE_COMMAND == m_eSubType)
	{
		CvCommandInfo* pkCommandInfo = GC.getCommandInfo((CommandTypes)m_iOriginalIndex);
		if(pkCommandInfo)
		{
			return pkCommandInfo->getVisible();
		}
	}
	else if(ACTIONSUBTYPE_AUTOMATE == m_eSubType)
	{
		CvAutomateInfo* pkAutomateInfo = GC.getAutomateInfo((AutomateTypes)m_iOriginalIndex);
		if(pkAutomateInfo)
		{
			return pkAutomateInfo->getVisible();
		}
	}
	else if(ACTIONSUBTYPE_MISSION == m_eSubType)
	{
		CvMissionInfo* pkMissionInfo = GC.getMissionInfo((MissionTypes)m_iOriginalIndex);
		if(pkMissionInfo)
		{
			return pkMissionInfo->getVisible();
		}
	}
	else if(ACTIONSUBTYPE_INTERFACEMODE== m_eSubType)
	{
		CvInterfaceModeInfo* pkInterfaceModeInfo = GC.getInterfaceModeInfo((InterfaceModeTypes)m_iOriginalIndex);
		if(pkInterfaceModeInfo)
		{
			return pkInterfaceModeInfo->getVisible();
		}
	}

	return true;
}
//------------------------------------------------------------------------------
ActionSubTypes CvActionInfo::getSubType() const
{
	return m_eSubType;
}
//------------------------------------------------------------------------------
void CvActionInfo::setSubType(ActionSubTypes eSubType)
{
	m_eSubType = eSubType;
}
//------------------------------------------------------------------------------
CvHotKeyInfo* CvActionInfo::getHotkeyInfo() const
{
	switch(getSubType())
	{
	case NO_ACTIONSUBTYPE:
		break;
	case ACTIONSUBTYPE_INTERFACEMODE:
	{
		CvInterfaceModeInfo* pkInterfaceModInfo = GC.getInterfaceModeInfo((InterfaceModeTypes)getOriginalIndex());
		if(pkInterfaceModInfo)
		{
			return pkInterfaceModInfo;
		}
	}
	break;
	case ACTIONSUBTYPE_COMMAND:
	{
		CvCommandInfo* pkCommandInfo = GC.getCommandInfo((CommandTypes)getOriginalIndex());
		if(pkCommandInfo)
		{
			return pkCommandInfo;
		}
	}
	break;
	case ACTIONSUBTYPE_BUILD:
	{
		CvBuildInfo* pkBuildInfo = GC.getBuildInfo((BuildTypes)getOriginalIndex());
		if(pkBuildInfo)
		{
			return pkBuildInfo;
		}
	}
	break;
	case ACTIONSUBTYPE_PROMOTION:
	{
		CvPromotionEntry* pkPromotionInfo = GC.getPromotionInfo((PromotionTypes)getOriginalIndex());
		if(pkPromotionInfo)
		{
			return pkPromotionInfo;
		}
	}
	break;
	case ACTIONSUBTYPE_SPECIALIST:
	{
		CvSpecialistInfo* pkSpecialistInfo = GC.getSpecialistInfo((SpecialistTypes)getOriginalIndex());
		if(pkSpecialistInfo)
		{
			return pkSpecialistInfo;
		}
	}
	break;
	case ACTIONSUBTYPE_CONTROL:
	{
		CvControlInfo* pkControlInfo = GC.getControlInfo((ControlTypes)getOriginalIndex());
		if(pkControlInfo)
		{
			return pkControlInfo;
		}
	}
	break;
	case ACTIONSUBTYPE_AUTOMATE:
	{
		CvAutomateInfo* pkAutomateInfo = GC.getAutomateInfo((AutomateTypes)getOriginalIndex());
		if(pkAutomateInfo)
		{
			return pkAutomateInfo;
		}
	}
	break;
	case ACTIONSUBTYPE_MISSION:
	{
		CvMissionInfo* pkMissionInfo = GC.getMissionInfo((MissionTypes)getOriginalIndex());
		if(pkMissionInfo)
		{
			return pkMissionInfo;
		}
	}
	break;
	}

	ASSERT_DEBUG((0) ,"Unknown Action Subtype in CvActionInfo::getHotkeyInfo");
	return NULL;
}
//------------------------------------------------------------------------------
const char* CvActionInfo::GetType() const
{
	if(getHotkeyInfo())
	{
		return getHotkeyInfo()->GetType();
	}

	return NULL;
}
//------------------------------------------------------------------------------
const char* CvActionInfo::GetDescription() const
{
	if(getHotkeyInfo())
	{
		return getHotkeyInfo()->GetDescription();
	}

	return "";
}
//------------------------------------------------------------------------------
const char* CvActionInfo::GetCivilopedia() const
{
	if(getHotkeyInfo())
	{
		return getHotkeyInfo()->GetCivilopedia();
	}

	return "";
}
//------------------------------------------------------------------------------
const char* CvActionInfo::GetHelp() const
{
	if(getHotkeyInfo())
	{
		return getHotkeyInfo()->getHelp();
	}

	return "";
}
//------------------------------------------------------------------------------
const char* CvActionInfo::GetDisabledHelp() const
{
	if(getHotkeyInfo())
	{
		return getHotkeyInfo()->getDisabledHelp();
	}

	return "";
}
//------------------------------------------------------------------------------
const char* CvActionInfo::GetStrategy() const
{
	if(getHotkeyInfo())
	{
		return getHotkeyInfo()->GetStrategy();
	}

	return "";
}
//------------------------------------------------------------------------------
const char* CvActionInfo::GetTextKey() const
{
	if(getHotkeyInfo())
	{
		return getHotkeyInfo()->GetTextKey();
	}

	return NULL;
}
//------------------------------------------------------------------------------
int CvActionInfo::getActionInfoIndex() const
{
	if(getHotkeyInfo())
	{
		return getHotkeyInfo()->getActionInfoIndex();
	}

	return -1;
}
//------------------------------------------------------------------------------
int CvActionInfo::getHotKeyVal() const
{
	if(getHotkeyInfo())
	{
		return getHotkeyInfo()->getHotKeyVal();
	}

	return -1;
}
//------------------------------------------------------------------------------
int CvActionInfo::getHotKeyPriority() const
{
	if(getHotkeyInfo())
	{
		return getHotkeyInfo()->getHotKeyPriority();
	}

	return -1;
}
//------------------------------------------------------------------------------
int CvActionInfo::getHotKeyValAlt() const
{
	if(getHotkeyInfo())
	{
		return getHotkeyInfo()->getHotKeyValAlt();
	}

	return -1;
}
//------------------------------------------------------------------------------
int CvActionInfo::getHotKeyPriorityAlt() const
{
	if(getHotkeyInfo())
	{
		return getHotkeyInfo()->getHotKeyPriorityAlt();
	}

	return -1;
}
//------------------------------------------------------------------------------
int CvActionInfo::getOrderPriority() const
{
	if(getHotkeyInfo())
	{
		return getHotkeyInfo()->getOrderPriority();
	}

	return -1;
}
//------------------------------------------------------------------------------
bool CvActionInfo::isAltDown() const
{
	if(getHotkeyInfo())
	{
		return getHotkeyInfo()->isAltDown();
	}

	return false;
}
//------------------------------------------------------------------------------
bool CvActionInfo::isShiftDown() const
{
	if(getHotkeyInfo())
	{
		return getHotkeyInfo()->isShiftDown();
	}

	return false;
}
//------------------------------------------------------------------------------
bool CvActionInfo::isCtrlDown() const
{
	if(getHotkeyInfo())
	{
		return getHotkeyInfo()->isCtrlDown();
	}

	return false;
}
//------------------------------------------------------------------------------
bool CvActionInfo::isAltDownAlt() const
{
	if(getHotkeyInfo())
	{
		return getHotkeyInfo()->isAltDownAlt();
	}

	return false;
}
//------------------------------------------------------------------------------
bool CvActionInfo::isShiftDownAlt() const
{
	if(getHotkeyInfo())
	{
		return getHotkeyInfo()->isShiftDownAlt();
	}

	return false;
}
//------------------------------------------------------------------------------
bool CvActionInfo::isCtrlDownAlt() const
{
	if(getHotkeyInfo())
	{
		return getHotkeyInfo()->isCtrlDownAlt();
	}

	return false;
}
//------------------------------------------------------------------------------
const char* CvActionInfo::getHotKey() const
{
	if(getHotkeyInfo())
	{
		return getHotkeyInfo()->getHotKey();
	}

	return NULL;
}
//------------------------------------------------------------------------------
std::string CvActionInfo::getHotKeyDescription() const
{
	if(getHotkeyInfo())
	{
		return getHotkeyInfo()->getHotKeyDescription();
	}

	return "";
}
//------------------------------------------------------------------------------
const char* CvActionInfo::getHotKeyString() const
{
	if(getHotkeyInfo())
	{
		return getHotkeyInfo()->getHotKeyString();
	}

	return "";
}

//======================================================================================================
//					CvMultiUnitFormationInfo
//======================================================================================================
const char* CvMultiUnitFormationInfo::getFormationName() const
{
	return m_strFormationName;
}

size_t CvMultiUnitFormationInfo::getNumFormationSlotEntries() const
{
	return m_vctSlotEntries.size();
}

size_t CvMultiUnitFormationInfo::getNumFormationSlotEntriesRequired() const
{
	size_t iCount = 0;
	for (size_t i = 0; i < m_vctSlotEntries.size(); i++)
		if (m_vctSlotEntries[i].m_requiredSlot)
			iCount++;
	return iCount;
}

const CvFormationSlotEntry& CvMultiUnitFormationInfo::getFormationSlotEntry(size_t index) const
{
	return m_vctSlotEntries[index];
}

//------------------------------------------------------------------------------
void CvMultiUnitFormationInfo::addFormationSlotEntry(const CvFormationSlotEntry& slotEntry)
{
	m_vctSlotEntries.push_back(slotEntry);
}
//------------------------------------------------------------------------------
bool CvMultiUnitFormationInfo::CacheResults(Database::Results& kResults, CvDatabaseUtility& kUtility)
{
	if(!CvBaseInfo::CacheResults(kResults, kUtility))
		return false;

	// Unused, but could be useful for logging purposes so it stays
	m_strFormationName = kResults.GetText("Name");

	//Slot entries
	{
		Database::Results kSlotEntries;
		if(DB.SelectAt(kSlotEntries, "MultiUnitFormation_SlotEntries", "MultiUnitFormationType", GetType()))
		{
			while(kSlotEntries.Step())
			{
				CvFormationSlotEntry slotEntry;

				//Basic Properties
				slotEntry.m_requiredSlot = kSlotEntries.GetBool("RequiredSlot");

				//References
				const char* szTextVal = NULL;
				szTextVal = kSlotEntries.GetText("MultiUnitPositionType");
				slotEntry.m_ePositionType = (MultiunitPositionTypes)GC.getInfoTypeForString(szTextVal, true);

				szTextVal = kSlotEntries.GetText("PrimaryUnitType");
				slotEntry.m_primaryUnitType = (UnitAITypes)GC.getInfoTypeForString(szTextVal, true);

				szTextVal = kSlotEntries.GetText("SecondaryUnitType");
				slotEntry.m_secondaryUnitType = (UnitAITypes)GC.getInfoTypeForString(szTextVal, true);

				m_vctSlotEntries.push_back(slotEntry);
			}
		}
	}

	return true;
}

//======================================================================================================
//					CvSpecialUnitInfo
//======================================================================================================
CvSpecialUnitInfo::CvSpecialUnitInfo() :
	m_bValid(false),
	m_bCityLoad(false),
	m_pbCarrierUnitAITypes(NULL),
	m_piProductionTraits(NULL)
{
}
//------------------------------------------------------------------------------
CvSpecialUnitInfo::~CvSpecialUnitInfo()
{
	SAFE_DELETE_ARRAY(m_pbCarrierUnitAITypes);
	SAFE_DELETE_ARRAY(m_piProductionTraits);
}
//------------------------------------------------------------------------------
bool CvSpecialUnitInfo::isValid() const
{
	return m_bValid;
}
//------------------------------------------------------------------------------
bool CvSpecialUnitInfo::isCityLoad() const
{
	return m_bCityLoad;
}
//------------------------------------------------------------------------------
bool CvSpecialUnitInfo::isCarrierUnitAIType(int i) const
{
	ASSERT_DEBUG(i < NUM_UNITAI_TYPES, "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_pbCarrierUnitAITypes ? m_pbCarrierUnitAITypes[i] : false;
}
//------------------------------------------------------------------------------
int CvSpecialUnitInfo::getProductionTraits(int i) const
{
	ASSERT_DEBUG(i < GC.getNumTraitInfos(), "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_piProductionTraits ? m_piProductionTraits[i] : -1;
}
//------------------------------------------------------------------------------
bool CvSpecialUnitInfo::CacheResults(Database::Results& kResults, CvDatabaseUtility& kUtility)
{
	if(!CvBaseInfo::CacheResults(kResults, kUtility))
		return false;

	m_bValid = kResults.GetBool("Valid");
	m_bCityLoad = kResults.GetBool("CityLoad");

	const char* szSpecialUnitType = GetType();
	kUtility.PopulateArrayByExistence(m_pbCarrierUnitAITypes, "UnitAIInfos", "SpecialUnit_CarrierUnitAI", "UnitAIType", "SpecialUnitType", szSpecialUnitType);
	kUtility.PopulateArrayByValue(m_piProductionTraits, "Traits", "SpecialUnit_ProductionTraits", "TraitType", "SpecialUnitType", szSpecialUnitType, "Trait");

	return true;
}

//======================================================================================================
//					CvUnitClassInfo
//======================================================================================================
CvUnitClassInfo::CvUnitClassInfo() :
	m_iMaxGlobalInstances(0),
	m_iMaxTeamInstances(0),
	m_iMaxPlayerInstances(0),
	m_iInstanceCostModifier(0),
	m_iDefaultUnitIndex(NO_UNIT)
#if defined(MOD_BALANCE_CORE)
	, m_iUnitInstancePerCity(0)
#endif
{
}
//------------------------------------------------------------------------------
int CvUnitClassInfo::getMaxGlobalInstances() const
{
	return m_iMaxGlobalInstances;
}
//------------------------------------------------------------------------------
int CvUnitClassInfo::getMaxTeamInstances() const
{
	return m_iMaxTeamInstances;
}
//------------------------------------------------------------------------------
int CvUnitClassInfo::getMaxPlayerInstances() const
{
	return m_iMaxPlayerInstances;
}
//------------------------------------------------------------------------------
int CvUnitClassInfo::getInstanceCostModifier() const
{
	return m_iInstanceCostModifier;
}
//------------------------------------------------------------------------------
int CvUnitClassInfo::getDefaultUnitIndex() const
{
	return m_iDefaultUnitIndex;
}
//------------------------------------------------------------------------------
void CvUnitClassInfo::setDefaultUnitIndex(int i)
{
	m_iDefaultUnitIndex = i;
}
//------------------------------------------------------------------------------
#if defined(MOD_BALANCE_CORE)
int CvUnitClassInfo::getUnitInstancePerCity() const
{
	return m_iUnitInstancePerCity;
}
#endif
//------------------------------------------------------------------------------
bool CvUnitClassInfo::CacheResults(Database::Results& kResults, CvDatabaseUtility& kUtility)
{
	if(!CvBaseInfo::CacheResults(kResults, kUtility))
		return false;

	m_iMaxGlobalInstances = kResults.GetInt("MaxGlobalInstances");
	m_iMaxTeamInstances = kResults.GetInt("MaxTeamInstances");
	m_iMaxPlayerInstances = kResults.GetInt("MaxPlayerInstances");
	m_iInstanceCostModifier = kResults.GetInt("InstanceCostModifier");
#if defined(MOD_BALANCE_CORE)
	m_iUnitInstancePerCity = kResults.GetInt("UnitInstancePerCity");
#endif
	m_iDefaultUnitIndex = GC.getInfoTypeForString(kResults.GetText("DefaultUnit"), true);

	return true;
}

//======================================================================================================
//					CvBuildingClassInfo
//======================================================================================================
CvBuildingClassInfo::CvBuildingClassInfo() :
	m_iMaxGlobalInstances(0),
	m_iMaxTeamInstances(0),
	m_iMaxPlayerInstances(0),
	m_iExtraPlayerInstances(0),
	m_iDefaultBuildingIndex(NO_BUILDING),
	m_bNoLimit(false),
	m_bMonument(false),
#if defined(MOD_BALANCE_CORE)
	m_eCorporationType(NO_CORPORATION),
	m_bIsHeadquarters(false),
	m_bIsOffice(false),
	m_bIsFranchise(false),
#endif
	m_piVictoryThreshold(NULL)
{
}
//------------------------------------------------------------------------------
CvBuildingClassInfo::~CvBuildingClassInfo()
{
	SAFE_DELETE_ARRAY(m_piVictoryThreshold);
}
//------------------------------------------------------------------------------
int CvBuildingClassInfo::getMaxGlobalInstances() const
{
	return m_iMaxGlobalInstances;
}
//------------------------------------------------------------------------------
int CvBuildingClassInfo::getMaxTeamInstances() const
{
	return m_iMaxTeamInstances;
}
//------------------------------------------------------------------------------
int CvBuildingClassInfo::getMaxPlayerInstances() const
{
	return m_iMaxPlayerInstances;
}
//------------------------------------------------------------------------------
int CvBuildingClassInfo::getExtraPlayerInstances() const
{
	return m_iExtraPlayerInstances;
}
//------------------------------------------------------------------------------
int CvBuildingClassInfo::getDefaultBuildingIndex() const
{
	return m_iDefaultBuildingIndex;
}
//------------------------------------------------------------------------------
void CvBuildingClassInfo::setDefaultBuildingIndex(int i)
{
	m_iDefaultBuildingIndex = i;
}
//------------------------------------------------------------------------------
bool CvBuildingClassInfo::isNoLimit() const
{
	return m_bNoLimit;
}
//------------------------------------------------------------------------------
bool CvBuildingClassInfo::isMonument() const
{
	return m_bMonument;
}
//------------------------------------------------------------------------------
int CvBuildingClassInfo::getVictoryThreshold(int i) const
{
	ASSERT_DEBUG(i < GC.getNumVictoryInfos(), "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_piVictoryThreshold ? m_piVictoryThreshold[i] : -1;
}
#if defined(MOD_BALANCE_CORE)
//------------------------------------------------------------------------------
CorporationTypes CvBuildingClassInfo::getCorporationType() const
{
	return m_eCorporationType;
}
bool CvBuildingClassInfo::IsHeadquarters() const
{
	return m_bIsHeadquarters;
}
bool CvBuildingClassInfo::IsOffice() const
{
	return m_bIsOffice;
}
bool CvBuildingClassInfo::IsFranchise() const
{
	return m_bIsFranchise;
}
#endif
//------------------------------------------------------------------------------
bool CvBuildingClassInfo::CacheResults(Database::Results& kResults, CvDatabaseUtility& kUtility)
{
	if(!CvBaseInfo::CacheResults(kResults, kUtility))
		return false;

	//Basic Properties
	m_iMaxGlobalInstances = kResults.GetInt("MaxGlobalInstances");
	m_iMaxTeamInstances = kResults.GetInt("MaxTeamInstances");
	m_iMaxPlayerInstances = kResults.GetInt("MaxPlayerInstances");
	m_iExtraPlayerInstances = kResults.GetInt("ExtraPlayerInstances");

	m_bNoLimit = kResults.GetBool("NoLimit");
	m_bMonument = kResults.GetBool("Monument");

	//References
	const char* szDefaultBuilding = kResults.GetText("DefaultBuilding");
	m_iDefaultBuildingIndex = GC.getInfoTypeForString(szDefaultBuilding, true);

	//Arrays
	kUtility.PopulateArrayByValue(m_piVictoryThreshold, "Victories", "BuildingClass_VictoryThresholds", "VictoryType", "BuildingClassType", GetType(), "Threshold");

	return true;
}

/// Helper function to read in an integer array of data sized according to number of building class types
void BuildingClassArrayHelpers::Read(FDataStream& kStream, int* paiArray)
{
	int iNumEntries = 0;
	int iType = 0;

	kStream >> iNumEntries;

	for(int iI = 0; iI < iNumEntries; iI++)
	{
		bool bValid = true;
		iType = CvInfosSerializationHelper::ReadHashed(kStream, &bValid);
		if(iType != -1 || !bValid)
		{
			if(iType != -1)
			{
				kStream >> paiArray[iType];
			}
			else
			{
				CvString szError;
				szError.Format("LOAD ERROR: Building Class Type not found");
				GC.LogMessage(szError.GetCString());
				ASSERT_DEBUG(false, szError);
				int iDummy = 0;
				kStream >> iDummy;	// Skip it.
			}
		}
	}
}

/// Helper function to write out an integer array of data sized according to number of building class types
void BuildingClassArrayHelpers::Write(FDataStream& kStream, int* paiArray, int iArraySize)
{
	kStream << iArraySize;

	for(int iI = 0; iI < iArraySize; iI++)
	{
		const BuildingClassTypes eBuildingClass = static_cast<BuildingClassTypes>(iI);
		CvBuildingClassInfo* pkBuildingClassInfo = GC.getBuildingClassInfo(eBuildingClass);
		if(pkBuildingClassInfo)
		{
			CvInfosSerializationHelper::WriteHashed(kStream, pkBuildingClassInfo);
			kStream << paiArray[iI];
		}
		else
		{
			kStream << 0;
		}
	}
}

/// Helper function to read in an integer array of data sized according to number of unit class types
void UnitClassArrayHelpers::Read(FDataStream& kStream, int* paiArray)
{
	int iNumEntries = 0;
	int iType = 0;

	kStream >> iNumEntries;

	for(int iI = 0; iI < iNumEntries; iI++)
	{
		bool bValid = true;
		iType = CvInfosSerializationHelper::ReadHashed(kStream, &bValid);
		if(iType != -1 || !bValid)
		{
			if(iType != -1)
			{
				kStream >> paiArray[iType];
			}
			else
			{
				CvString szError;
				szError.Format("LOAD ERROR: Unit Class Type not found");
				GC.LogMessage(szError.GetCString());
				ASSERT_DEBUG(false, szError);

				int iDummy = 0;
				kStream >> iDummy;
			}
		}
	}
}

/// Helper function to write out an integer array of data sized according to number of unit class types
void UnitClassArrayHelpers::Write(FDataStream& kStream, int* paiArray, int iArraySize)
{
	kStream << iArraySize;

	for(int iI = 0; iI < iArraySize; iI++)
	{
		const UnitClassTypes eUnitClass = static_cast<UnitClassTypes>(iI);
		CvUnitClassInfo* pkUnitClassInfo = GC.getUnitClassInfo(eUnitClass);
		if(pkUnitClassInfo)
		{
			CvInfosSerializationHelper::WriteHashed(kStream, pkUnitClassInfo);
			kStream << paiArray[iI];
		}
		else
		{			
			kStream << 0;
		}
	}
}

//======================================================================================================
//					CvCivilizationBaseInfo
//======================================================================================================
CvCivilizationBaseInfo::CvCivilizationBaseInfo():
	m_bPlayable(false),
	m_bAIPlayable(false)
{
	memset((void*)&m_kPackageID, 0, sizeof(m_kPackageID));
}

//------------------------------------------------------------------------------
CvCivilizationBaseInfo::~CvCivilizationBaseInfo()
{
}

//------------------------------------------------------------------------------
bool CvCivilizationBaseInfo::isAIPlayable() const
{
	return m_bAIPlayable;
}
//------------------------------------------------------------------------------
bool CvCivilizationBaseInfo::isPlayable() const
{
	return m_bPlayable;
}
//------------------------------------------------------------------------------
const char* CvCivilizationBaseInfo::getShortDescription() const
{
	return m_strShortDescription.c_str();
}
//------------------------------------------------------------------------------
void CvCivilizationBaseInfo::setShortDescriptionKey(const char* szVal)
{
	m_strShortDescriptionKey = szVal;
	m_strShortDescription = GetLocalizedText(m_strShortDescriptionKey.c_str());
}
//------------------------------------------------------------------------------
const char* CvCivilizationBaseInfo::getShortDescriptionKey() const
{
	return m_strShortDescriptionKey.c_str();
}
//------------------------------------------------------------------------------
const GUID& CvCivilizationBaseInfo::getPackageID() const
{
	return m_kPackageID;
}

//------------------------------------------------------------------------------
bool CvCivilizationBaseInfo::CacheResults(Database::Results& kResults, CvDatabaseUtility& kUtility)
{
	if(!CvBaseInfo::CacheResults(kResults, kUtility))
		return false;

	const char* szTextVal = NULL;	//! temporary val

	//Basic Properties
	m_bPlayable = kResults.GetBool("Playable");
	m_bAIPlayable = kResults.GetBool("AIPlayable");

	szTextVal = kResults.GetText("ShortDescription");
	setShortDescriptionKey(szTextVal);

	szTextVal = kResults.GetText("PackageID");
	if(szTextVal)
	{
		ExtractGUID(szTextVal, m_kPackageID);
	}
	return true;
}
//======================================================================================================
//					CvCivilizationInfo
//======================================================================================================
CvCivilizationInfo::CvCivilizationInfo():
	CvCivilizationBaseInfo(),
	m_iDefaultPlayerColor(NO_PLAYERCOLOR),
	m_iArtStyleType(NO_ARTSTYLE),
	m_iNumLeaders(0),
	m_piCivilizationBuildings(NULL),
	m_piCivilizationUnits(NULL),
	m_piCivilizationFreeUnitsClass(NULL),
	m_piCivilizationFreeUnitsDefaultUnitAI(NULL),
	m_pbLeaders(NULL),
	m_pbCivilizationFreeBuildingClass(NULL),
	m_pbCivilizationFreeTechs(NULL),
	m_pbCivilizationDisableTechs(NULL),
	m_bCoastalCiv(NULL),
	m_bPlaceFirst(NULL),
	m_pbReligions(NULL)
{

}
//------------------------------------------------------------------------------
CvCivilizationInfo::~CvCivilizationInfo()
{
	SAFE_DELETE_ARRAY(m_piCivilizationBuildings);
	SAFE_DELETE_ARRAY(m_piCivilizationUnits);
	SAFE_DELETE_ARRAY(m_piCivilizationFreeUnitsClass);
	SAFE_DELETE_ARRAY(m_piCivilizationFreeUnitsDefaultUnitAI);
	SAFE_DELETE_ARRAY(m_pbLeaders);
	SAFE_DELETE_ARRAY(m_pbCivilizationFreeBuildingClass);
	SAFE_DELETE_ARRAY(m_pbCivilizationFreeTechs);
	SAFE_DELETE_ARRAY(m_pbCivilizationDisableTechs);
	SAFE_DELETE_ARRAY(m_pbReligions);
}

//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   InitBuildingDefaults(int*& piDefaults)
//
//  PURPOSE :   allocate and initialize the civilization's default buildings
//
//------------------------------------------------------------------------------------------------------
void CvCivilizationInfo::InitBuildingDefaults(int*& piDefaults, CvDatabaseUtility& kUtility)
{
	kUtility.InitializeArray(piDefaults, "BuildingClasses", -1);

	std::string strKey("InitBuildingDefaults");
	Database::Results* pResults = kUtility.GetResults(strKey);
	if(pResults == NULL)
	{
		pResults = kUtility.PrepareResults(strKey, "select BuildingClasses.ID, Buildings.ID as BuildingID from BuildingClasses inner join Buildings on Buildings.Type = DefaultBuilding");
	}

	while(pResults->Step())
	{
		const int idx = pResults->GetInt(0);
		const int buildingID = pResults->GetInt(1);

		piDefaults[idx] = buildingID;
	}
}


//------------------------------------------------------------------------------------------------------
//  FUNCTION:   InitUnitDefaults(int*& piDefaults)
//
//  PURPOSE :   allocate and initialize the civilization's default Units
//
//------------------------------------------------------------------------------------------------------
void CvCivilizationInfo::InitUnitDefaults(int*& piDefaults, CvDatabaseUtility& kUtility)
{
	kUtility.InitializeArray(piDefaults, "UnitClasses", -1);

	std::string strKey("InitUnitDefaults");
	Database::Results* pResults = kUtility.GetResults(strKey);
	if(pResults == NULL)
	{
		pResults = kUtility.PrepareResults(strKey, "select UnitClasses.ID, Units.ID as UnitID from UnitClasses inner join Units on Units.Type = DefaultUnit");
	}

	while(pResults->Step())
	{
		const int idx		= pResults->GetInt(0);
		const int unitID	= pResults->GetInt(1);

		piDefaults[idx] = unitID;
	}
}
//------------------------------------------------------------------------------
int CvCivilizationInfo::getDefaultPlayerColor() const
{
	return m_iDefaultPlayerColor;
}
//------------------------------------------------------------------------------
int CvCivilizationInfo::getArtStyleType() const
{
	return m_iArtStyleType;
}
//------------------------------------------------------------------------------
const char* CvCivilizationInfo::getArtStyleSuffix() const
{
	return m_strArtStyleSuffix.c_str();
}
//------------------------------------------------------------------------------
const char* CvCivilizationInfo::getArtStylePrefix() const
{
	return m_strArtStylePrefix.c_str();
}
//------------------------------------------------------------------------------
int CvCivilizationInfo::getNumCityNames() const
{
	return m_vCityNames.size();
}
//------------------------------------------------------------------------------
int CvCivilizationInfo::getNumSpyNames() const
{
	return m_vSpyNames.size();
}

//------------------------------------------------------------------------------
int CvCivilizationInfo::getNumLeaders() const
{
	// the number of leaders the Civ has, this is needed so that random leaders can be generated easily
	return m_iNumLeaders;
}
//------------------------------------------------------------------------------
const char* CvCivilizationInfo::GetDawnOfManAudio() const
{
	return m_strDawnOfManAudio.c_str();
}
//------------------------------------------------------------------------------
const char* CvCivilizationInfo::getSoundtrackKey() const
{
	return m_strSoundtrackKey.c_str();
}

//------------------------------------------------------------------------------
const char* CvCivilizationInfo::getAdjective() const
{
	return m_strAdjective.c_str();
}
//------------------------------------------------------------------------------
void CvCivilizationInfo::setAdjectiveKey(const char* szVal)
{
	m_strAdjectiveKey = szVal;
	m_strAdjective = GetLocalizedText(m_strAdjectiveKey.c_str());
}
//------------------------------------------------------------------------------
const char* CvCivilizationInfo::getAdjectiveKey() const
{
	return m_strAdjectiveKey.c_str();
}
//------------------------------------------------------------------------------
const char* CvCivilizationInfo::getFlagTexture() const
{
	return NULL;
}
//------------------------------------------------------------------------------
const char* CvCivilizationInfo::getArtDefineTag() const
{
	return m_strArtDefineTag.c_str();
}
//------------------------------------------------------------------------------
void CvCivilizationInfo::setArtDefineTag(const char* szVal)
{
	m_strArtDefineTag = szVal;
}
//------------------------------------------------------------------------------
void CvCivilizationInfo::setArtStyleSuffix(const char* szVal)
{
	m_strArtStyleSuffix = szVal;
}
//------------------------------------------------------------------------------
void CvCivilizationInfo::setArtStylePrefix(const char* szVal)
{
	m_strArtStylePrefix = szVal;
}
//------------------------------------------------------------------------------
ReligionTypes CvCivilizationInfo::GetReligion() const
{
	// Only one per civ supported now
	for(int iI = 0; iI < GC.getNumReligionInfos(); iI++)
	{
		if(m_pbReligions[iI])
		{
			return (ReligionTypes)iI;
		}
	}
	return NO_RELIGION;
}
//------------------------------------------------------------------------------
int CvCivilizationInfo::getCivilizationBuildings(int i) const
{
	ASSERT_DEBUG(i < GC.getNumBuildingClassInfos(), "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_piCivilizationBuildings && i>=0 ? m_piCivilizationBuildings[i] : -1;
}
//------------------------------------------------------------------------------
int CvCivilizationInfo::getCivilizationUnits(int i) const
{
	ASSERT_DEBUG(i < GC.getNumUnitClassInfos(), "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_piCivilizationUnits && i>=0 ? m_piCivilizationUnits[i] : -1;
}
//------------------------------------------------------------------------------
bool CvCivilizationInfo::isCivilizationBuildingOverridden(int i) const
{
	ASSERT_DEBUG(i < GC.getNumBuildingClassInfos(), "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_CivilizationBuildingOverridden[i];
}
//------------------------------------------------------------------------------
bool CvCivilizationInfo::isCivilizationUnitOverridden(int i) const
{
	ASSERT_DEBUG(i < GC.getNumUnitClassInfos(), "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_CivilizationUnitOverridden[i];
}
//------------------------------------------------------------------------------
int CvCivilizationInfo::getCivilizationFreeUnitsClass(int i) const
{
	ASSERT_DEBUG(i < GC.getNumUnitClassInfos(), "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_piCivilizationFreeUnitsClass && i>=0 ? m_piCivilizationFreeUnitsClass[i] : -1;
}
//------------------------------------------------------------------------------
int CvCivilizationInfo::getCivilizationFreeUnitsDefaultUnitAI(int i) const
{
	ASSERT_DEBUG(i < GC.getNumUnitClassInfos(), "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_piCivilizationFreeUnitsDefaultUnitAI && i>=0 ? m_piCivilizationFreeUnitsDefaultUnitAI[i] : -1;
}
//------------------------------------------------------------------------------
bool CvCivilizationInfo::isLeaders(int i) const
{
	ASSERT_DEBUG(i < GC.getNumLeaderHeadInfos(), "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_pbLeaders && i>=0 ? m_pbLeaders[i] : false;
}
//------------------------------------------------------------------------------
bool CvCivilizationInfo::IsBlocksMinor(int i) const
{
	ASSERT_DEBUG(i < GC.getNumMinorCivInfos(), "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_BlockedMinors[i];
}
//------------------------------------------------------------------------------
bool CvCivilizationInfo::isCivilizationFreeBuildingClass(int i) const
{
	ASSERT_DEBUG(i < GC.getNumBuildingClassInfos(), "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_pbCivilizationFreeBuildingClass && i>=0 ? m_pbCivilizationFreeBuildingClass[i] : false;
}
//------------------------------------------------------------------------------
bool CvCivilizationInfo::isCivilizationFreeTechs(int i) const
{
	ASSERT_DEBUG(i < GC.getNumTechInfos(), "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_pbCivilizationFreeTechs && i>=0 ? m_pbCivilizationFreeTechs[i] : false;
}
//------------------------------------------------------------------------------
bool CvCivilizationInfo::isCivilizationDisableTechs(int i) const
{
	ASSERT_DEBUG(i < GC.getNumTechInfos(), "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_pbCivilizationDisableTechs && i>=0 ? m_pbCivilizationDisableTechs[i] : false;
}
//------------------------------------------------------------------------------
const char* CvCivilizationInfo::getCityNames(int i) const
{
	return i>=0 ? m_vCityNames[i].c_str() : "vilanova";
}
//------------------------------------------------------------------------------
const char* CvCivilizationInfo::getSpyNames(int i) const
{
	return i>=0 ? m_vSpyNames[i].c_str() : "john doe";
}
//------------------------------------------------------------------------------
bool CvCivilizationInfo::isCoastalCiv() const
{
	return m_bCoastalCiv;
}
//------------------------------------------------------------------------------
bool CvCivilizationInfo::isFirstCoastalStart() const
{
	return m_bPlaceFirst;
}
//------------------------------------------------------------------------------
bool CvCivilizationInfo::CacheResults(Database::Results& kResults, CvDatabaseUtility& kUtility)
{
	if(!CvCivilizationBaseInfo::CacheResults(kResults, kUtility))
		return false;

	const size_t maxUnitClasses = kUtility.MaxRows("UnitClasses");
	const size_t maxBuildingClasses = kUtility.MaxRows("BuildingClasses");
	const size_t NumMinorCivInfos = kUtility.MaxRows("MinorCivilizations");

	const char* szTextVal = NULL;	//! temporary val

	//Basic Properties
	szTextVal = kResults.GetText("Adjective");
	setAdjectiveKey(szTextVal);

	//References
	szTextVal = kResults.GetText("DefaultPlayerColor");
	m_iDefaultPlayerColor = GC.getInfoTypeForString(szTextVal, true);

	szTextVal = kResults.GetText("ArtDefineTag");
	setArtDefineTag(szTextVal);

	szTextVal = kResults.GetText("ArtStyleType");
	m_iArtStyleType = GC.getInfoTypeForString(szTextVal, true);

	szTextVal = kResults.GetText("ArtStylePrefix");
	setArtStylePrefix(szTextVal);

	szTextVal = kResults.GetText("ArtStyleSuffix");
	setArtStyleSuffix(szTextVal);

	szTextVal = kResults.GetText("DawnOfManAudio");
	m_strDawnOfManAudio = szTextVal;

	szTextVal = kResults.GetText("SoundtrackTag");
	m_strSoundtrackKey = szTextVal;

	const char* szType = GetType();

	//coastal start
	{
		m_bCoastalCiv = false;

		std::string strKey = "Civilization_Start_Along_Ocean";
		Database::Results* pResults = kUtility.GetResults(strKey);
		if(pResults == NULL)
		{
			pResults = kUtility.PrepareResults(strKey, "select StartAlongOcean from Civilization_Start_Along_Ocean where CivilizationType = ?");
		}

		pResults->Bind(1, szType, -1, false);

		while(pResults->Step())
		{
			m_bCoastalCiv = pResults->GetBool(0);
		}

		pResults->Reset();
	}

	//place first
	{
		m_bPlaceFirst = false;

		std::string strKey = "Civilization_Start_Place_First";
		Database::Results* pResults = kUtility.GetResults(strKey);
		if(pResults == NULL)
		{
			pResults = kUtility.PrepareResults(strKey, "select PlaceFirst from Civilization_Start_Place_First_Along_Ocean where CivilizationType = ?");
		}

		pResults->Bind(1, szType, -1, false);

		while(pResults->Step())
		{
			m_bPlaceFirst = pResults->GetBool(0);
		}

		pResults->Reset();
	}

	//Arrays

	//City Names
	{
		m_vCityNames.clear();

		std::string strKey = "Civilization - CityNames";
		Database::Results* pResults = kUtility.GetResults(strKey);
		if(pResults == NULL)
		{
			pResults = kUtility.PrepareResults(strKey, "select CityName from Civilization_CityNames where CivilizationType = ?");
		}

		pResults->Bind(1, szType, -1, false);

		while(pResults->Step())
		{
			m_vCityNames.push_back(pResults->GetText(0));
		}

		pResults->Reset();
	}

	//Building Types
	{
		// call the function that sets the default civilization buildings
		InitBuildingDefaults(m_piCivilizationBuildings, kUtility);

		m_CivilizationBuildingOverridden.reserve(maxBuildingClasses);
		m_CivilizationBuildingOverridden.clear();
		m_CivilizationBuildingOverridden.resize(maxBuildingClasses, false);

		std::string key = "Civilization_BuildingClassOverrides";
		Database::Results* pResults = kUtility.GetResults(key);
		if(pResults == NULL)
		{
			pResults = kUtility.PrepareResults(key, "select BuildingClasses.ID, coalesce(Buildings.ID, -1) from Civilization_BuildingClassOverrides inner join BuildingClasses on BuildingClassType = BuildingClasses.Type left outer join Buildings on BuildingType = Buildings.Type where CivilizationType = ?");

		}

		pResults->Bind(1, szType);

		while(pResults->Step())
		{
			const int idx = pResults->GetInt(0);
			const int buildingID = pResults->GetInt(1);

			m_piCivilizationBuildings[idx] = buildingID;
			m_CivilizationBuildingOverridden[idx] = true;
		}

		pResults->Reset();
	}

	//Unit Types
	{
		// call the function that sets the default civilization units
		InitUnitDefaults(m_piCivilizationUnits, kUtility);

		m_CivilizationUnitOverridden.reserve(maxUnitClasses);
		m_CivilizationUnitOverridden.clear();
		m_CivilizationUnitOverridden.resize(maxUnitClasses,false);

		std::string key = "Civilization_UnitClassOverrides";
		Database::Results* pResults = kUtility.GetResults(key);
		if(pResults == NULL)
		{
			pResults = kUtility.PrepareResults(key, "select UnitClasses.ID, coalesce(Units.ID, -1) from Civilization_UnitClassOverrides inner join UnitClasses on UnitClassType = UnitClasses.Type left outer join Units on UnitType = Units.Type where CivilizationType = ?");
		}

		pResults->Bind(1, szType);

		while(pResults->Step())
		{
			const int idx = pResults->GetInt(0);
			const int unitID = pResults->GetInt(1);

			m_piCivilizationUnits[idx] = unitID;
			m_CivilizationUnitOverridden[idx] = true;
		}

		pResults->Reset();

	}

	//FreeUnits
	{
		kUtility.InitializeArray(m_piCivilizationFreeUnitsClass, maxUnitClasses, -1);
		kUtility.InitializeArray(m_piCivilizationFreeUnitsDefaultUnitAI, maxUnitClasses, -1);

		std::string strKey = "Civilizations - FreeUnits";
		Database::Results* pResults = kUtility.GetResults(strKey);
		if(pResults == NULL)
		{
			pResults = kUtility.PrepareResults(strKey, "select UnitClasses.ID as UnitClassID, UnitAIInfos.ID as UnitAITypeID, Count from Civilization_FreeUnits inner join UnitClasses on UnitClassType = UnitClasses.Type inner join UnitAIInfos on UnitAIType = UnitAIInfos.Type where CivilizationType = ?");
		}

		pResults->Bind(1, szType, -1, false);

		while(pResults->Step())
		{
			const int iUnitClassID = pResults->GetInt(0);
			const int iUnitAITypeID = pResults->GetInt(1);
			const int iCount = pResults->GetInt(2);

			m_piCivilizationFreeUnitsClass[iUnitClassID] = iCount;
			m_piCivilizationFreeUnitsDefaultUnitAI[iUnitClassID] = iUnitAITypeID;
		}

		pResults->Reset();

	}

	kUtility.PopulateArrayByExistence(m_pbCivilizationFreeBuildingClass,
	                                  "BuildingClasses", "Civilization_FreeBuildingClasses",
	                                  "BuildingClassType", "CivilizationType", szType);


	kUtility.PopulateArrayByExistence(m_pbCivilizationFreeTechs, "Technologies",
	                                  "Civilization_FreeTechs", "TechType",
	                                  "CivilizationType", szType);


	kUtility.PopulateArrayByExistence(m_pbCivilizationDisableTechs, "Technologies",
	                                  "Civilization_DisableTechs", "TechType",
	                                  "CivilizationType", szType);


	kUtility.PopulateArrayByExistence(m_pbLeaders, "Leaders", "Civilization_Leaders",
	                                  "LeaderheadType", "CivilizationType", szType);

	kUtility.PopulateArrayByExistence(m_pbReligions, "Religions", "Civilization_Religions",
	                                  "ReligionType", "CivilizationType", szType);

	//Blocked Minors
	{
		m_BlockedMinors.clear();
		m_BlockedMinors.resize(NumMinorCivInfos, false);

		std::string strKey = "MajorBlocksMinor";
		Database::Results* pResults = kUtility.GetResults(strKey);
		if (pResults == NULL)
		{
			pResults = kUtility.PrepareResults(strKey, "select MinorCivilizations.ID from MajorBlocksMinor inner join MinorCivilizations on MinorCiv = MinorCivilizations.Type where MajorCiv = ?");
		}

		pResults->Bind(1, szType);

		while (pResults->Step())
		{
			const int BlockedMinor = pResults->GetInt(0);
			m_BlockedMinors[BlockedMinor] = true;
		}

		pResults->Reset();
	}

	//Spy Names
	{
		m_vSpyNames.clear();

		std::string strKey = "Civilization - SpyNames";
		Database::Results* pResults = kUtility.GetResults(strKey);
		if(pResults == NULL)
		{
			pResults = kUtility.PrepareResults(strKey, "select SpyName from Civilization_SpyNames where CivilizationType = ?");
		}

		pResults->Bind(1, szType, -1, false);

		while(pResults->Step())
		{
			m_vSpyNames.push_back(pResults->GetText(0));
		}

		pResults->Reset();
	}

	return true;
}
//------------------------------------------------------------------------------

//======================================================================================================
//					CvVictoryInfo
//======================================================================================================
CvVictoryInfo::CvVictoryInfo() :
	m_iPopulationPercentLead(0),
	m_iLandPercent(0),
	m_iMinLandPercent(0),
	m_iCityCulture(0),
	m_iNumCultureCities(0),
	m_iTotalCultureRatio(0),
	m_iVictoryDelayTurns(0),
	m_bWinsGame(false),
	m_bEndScore(false),
	m_bConquest(false),
	m_bInfluential(false),
	m_bDiploVote(false),
	m_bPermanent(false),
	m_bReligionInAllCities(false),
	m_bFindAllNaturalWonders(false),
	m_piVictoryPointAwards(NULL),
	m_bTargetScore(false)
{
}
//------------------------------------------------------------------------------
CvVictoryInfo::~CvVictoryInfo()
{
	SAFE_DELETE_ARRAY(m_piVictoryPointAwards);
}
//------------------------------------------------------------------------------
int CvVictoryInfo::getPopulationPercentLead() const
{
	return m_iPopulationPercentLead;
}
//------------------------------------------------------------------------------
int CvVictoryInfo::getLandPercent() const
{
	return m_iLandPercent;
}
//------------------------------------------------------------------------------
int CvVictoryInfo::getMinLandPercent() const
{
	return m_iMinLandPercent;
}
//------------------------------------------------------------------------------
int CvVictoryInfo::getCityCulture() const
{
	return m_iCityCulture;
}
//------------------------------------------------------------------------------
int CvVictoryInfo::getNumCultureCities() const
{
	return m_iNumCultureCities;
}
//------------------------------------------------------------------------------
int CvVictoryInfo::getTotalCultureRatio() const
{
	return m_iTotalCultureRatio;
}
//------------------------------------------------------------------------------
int CvVictoryInfo::getVictoryDelayTurns() const
{
	return m_iVictoryDelayTurns;
}
//------------------------------------------------------------------------------
bool CvVictoryInfo::IsWinsGame() const
{
	return m_bWinsGame;
}
//------------------------------------------------------------------------------
bool CvVictoryInfo::isTargetScore() const
{
	return m_bTargetScore;
}
//------------------------------------------------------------------------------
bool CvVictoryInfo::isEndScore() const
{
	return m_bEndScore;
}
//------------------------------------------------------------------------------
bool CvVictoryInfo::isConquest() const
{
	return m_bConquest;
}
//------------------------------------------------------------------------------
bool CvVictoryInfo::isInfluential() const
{
	return m_bInfluential;
}
//------------------------------------------------------------------------------
bool CvVictoryInfo::isDiploVote() const
{
	return m_bDiploVote;
}
//------------------------------------------------------------------------------
bool CvVictoryInfo::isPermanent() const
{
	return m_bPermanent;
}
//------------------------------------------------------------------------------
bool CvVictoryInfo::IsReligionInAllCities() const
{
	return m_bReligionInAllCities;
}
//------------------------------------------------------------------------------
bool CvVictoryInfo::IsFindAllNaturalWonders() const
{
	return m_bFindAllNaturalWonders;
}
//------------------------------------------------------------------------------
const char* CvVictoryInfo::getMovie() const
{
	return m_strMovie.c_str();
}
//------------------------------------------------------------------------------
int CvVictoryInfo::GetVictoryPointAward(int i) const
{
	return m_piVictoryPointAwards[i];
}
//------------------------------------------------------------------------------
bool CvVictoryInfo::CacheResults(Database::Results& kResults, CvDatabaseUtility& kUtility)
{
	if(!CvBaseInfo::CacheResults(kResults, kUtility))
		return false;

	m_bWinsGame = kResults.GetBool("WinsGame");
	m_bTargetScore = kResults.GetBool("TargetScore");
	m_bEndScore = kResults.GetBool("EndScore");
	m_bConquest = kResults.GetBool("Conquest");
	m_bInfluential = kResults.GetBool("Influential");
	m_bDiploVote = kResults.GetBool("DiploVote");
	m_bPermanent = kResults.GetBool("Permanent");
	m_bReligionInAllCities = kResults.GetBool("ReligionInAllCities");
	m_bFindAllNaturalWonders = kResults.GetBool("FindAllNaturalWonders");
	m_iPopulationPercentLead = kResults.GetInt("PopulationPercentLead");
	m_iLandPercent = kResults.GetInt("LandPercent");
	m_iMinLandPercent = kResults.GetInt("MinLandPercent");

	m_iNumCultureCities = kResults.GetInt("NumCultureCities");
	m_iTotalCultureRatio = kResults.GetInt("TotalCultureRatio");
	m_iVictoryDelayTurns = kResults.GetInt("VictoryDelayTurns");

	m_strMovie = kResults.GetText("VictoryMovie");

	const char* szCityCulture = kResults.GetText("CityCulture");
	m_iCityCulture = GC.getInfoTypeForString(szCityCulture, true);

	//VictoryPointAwards
	{
		const char* szVictoryType = GetType();

		const int iNumVictoryPoints = /*5*/ GD_INT_GET(NUM_VICTORY_POINT_AWARDS);
		kUtility.InitializeArray(m_piVictoryPointAwards, iNumVictoryPoints);

		std::string strKey = "CvVictoryInfo_VictoryPointAwards";
		Database::Results* pVictoryPointResults = kUtility.GetResults(strKey);
		if(pVictoryPointResults == NULL)
		{
			pVictoryPointResults = kUtility.PrepareResults(strKey, "select VictoryPoints from VictoryPointAwards where VictoryType == ? order by VictoryPoints desc;");
		}

		pVictoryPointResults->Bind(1, szVictoryType);

		int i = 0;
		while(pVictoryPointResults->Step())
		{
			ASSERT_DEBUG(i < iNumVictoryPoints);
			m_piVictoryPointAwards[i++] = pVictoryPointResults->GetInt(0);
		}

		pVictoryPointResults->Reset();
	}

	return true;
}

//------------------------------------------------------------------------------

//======================================================================================================
//					CvSmallAwardInfo
//======================================================================================================
CvSmallAwardInfo::CvSmallAwardInfo() :
	m_szNotification(""),
	m_szTeamNotification(""),
	m_iNumVictoryPoints(0),
	m_iNumCities(0),
	m_iCityPopulation(0),
#if defined(MOD_BALANCE_CORE)
	m_iCSInfluence(0),
	m_iDuration(0),
	m_iGPPointsGlobal(0),
	m_iGPPoints(0),
	m_iExperience(0),
	m_iGold(0),
	m_iCulture(0),
	m_iFaith(0),
	m_iScience(0),
	m_iFood(0),
	m_iProduction(0),
	m_iGAP(0),
	m_iTourism(0),
	m_iHappiness(0),
	m_iGeneralPoints(0),
	m_iAdmiralPoints(0),
	m_iJuggernauts(0),
	m_iRand(0)
#endif
{
}
//------------------------------------------------------------------------------
CvSmallAwardInfo::~CvSmallAwardInfo()
{
}
//------------------------------------------------------------------------------
int CvSmallAwardInfo::GetNumVictoryPoints() const
{
	return m_iNumVictoryPoints;
}
//------------------------------------------------------------------------------
CvString CvSmallAwardInfo::GetNotificationString() const
{
	return m_szNotification;
}
//------------------------------------------------------------------------------
CvString CvSmallAwardInfo::GetTeamNotificationString() const
{
	return m_szTeamNotification;
}
//------------------------------------------------------------------------------
int CvSmallAwardInfo::GetNumCities() const
{
	return m_iNumCities;
}
//------------------------------------------------------------------------------
int CvSmallAwardInfo::GetCityPopulation() const
{
	return m_iCityPopulation;
}
#if defined(MOD_BALANCE_CORE)
//------------------------------------------------------------------------------
int CvSmallAwardInfo::GetInfluence() const
{
	return m_iCSInfluence;
}
//------------------------------------------------------------------------------
int CvSmallAwardInfo::GetDuration() const
{
	return m_iDuration;
}
//------------------------------------------------------------------------------
int CvSmallAwardInfo::GetGPPointsGlobal() const
{
	return m_iGPPointsGlobal;
}
//------------------------------------------------------------------------------
int CvSmallAwardInfo::GetGPPoints() const
{
	return m_iGPPoints;
}
//------------------------------------------------------------------------------
int CvSmallAwardInfo::GetExperience() const
{
	return m_iExperience;
}
//------------------------------------------------------------------------------
int CvSmallAwardInfo::GetGold() const
{
	return m_iGold;
}
//------------------------------------------------------------------------------
int CvSmallAwardInfo::GetCulture() const
{
	return m_iCulture;
}
//------------------------------------------------------------------------------
int CvSmallAwardInfo::GetFaith() const
{
	return m_iFaith;
}
//------------------------------------------------------------------------------
int CvSmallAwardInfo::GetScience() const
{
	return m_iScience;
}
//------------------------------------------------------------------------------
int CvSmallAwardInfo::GetFood() const
{
	return m_iFood;
}
//------------------------------------------------------------------------------
int CvSmallAwardInfo::GetProduction() const
{
	return m_iProduction;
}
//------------------------------------------------------------------------------
int CvSmallAwardInfo::GetGAP() const
{
	return m_iGAP;
}
//------------------------------------------------------------------------------
int CvSmallAwardInfo::GetHappiness() const
{
	return m_iHappiness;
}
int CvSmallAwardInfo::GetTourism() const
{
	return m_iTourism;
}
//------------------------------------------------------------------------------
int CvSmallAwardInfo::GetGeneralPoints() const
{
	return m_iGeneralPoints;
}
//------------------------------------------------------------------------------
int CvSmallAwardInfo::GetAdmiralPoints() const
{
	return m_iAdmiralPoints;
}
//------------------------------------------------------------------------------
int CvSmallAwardInfo::GetJuggernauts() const
{
	return m_iJuggernauts;
}
//------------------------------------------------------------------------------
int CvSmallAwardInfo::GetRandom() const
{
	return m_iRand;
}
#endif
//------------------------------------------------------------------------------
bool CvSmallAwardInfo::CacheResults(Database::Results& kResults, CvDatabaseUtility& kUtility)
{
	if(!CvBaseInfo::CacheResults(kResults, kUtility))
		return false;

	m_szNotification = kResults.GetText("Notification");
	m_szTeamNotification = kResults.GetText("TeamNotification");

	m_iNumVictoryPoints = kResults.GetInt("NumVictoryPoints");

	m_iNumCities = kResults.GetInt("NumCities");
	m_iCityPopulation = kResults.GetInt("CityPopulation");

#if defined(MOD_BALANCE_CORE)
	m_iCSInfluence = kResults.GetInt("Influence");
	m_iDuration = kResults.GetInt("QuestDuration");
	m_iGPPointsGlobal = kResults.GetInt("GlobalGPPoints");
	m_iGPPoints = kResults.GetInt("CapitalGPPoints");
	m_iExperience = kResults.GetInt("GlobalExperience");
	m_iGold = kResults.GetInt("Gold");
	m_iCulture = kResults.GetInt("Culture");
	m_iFaith = kResults.GetInt("Faith");
	m_iScience = kResults.GetInt("Science");
	m_iFood = kResults.GetInt("Food");
	m_iProduction = kResults.GetInt("Production");
	m_iGAP = kResults.GetInt("GoldenAgePoints");
	m_iHappiness = kResults.GetInt("Happiness");
	m_iGeneralPoints = kResults.GetInt("GeneralPoints");
	m_iAdmiralPoints = kResults.GetInt("AdmiralPoints");
	m_iJuggernauts = kResults.GetInt("Juggernauts");
	m_iTourism = kResults.GetInt("Tourism");
	m_iRand = kResults.GetInt("RandomMod");
#endif

	return true;
}

//======================================================================================================
//					CvHurryInfo
//======================================================================================================
bool CvHurryInfo::CacheResults(Database::Results& kResults, CvDatabaseUtility& kUtility)
{
	return CvBaseInfo::CacheResults(kResults, kUtility);
}


//======================================================================================================
//					CvHandicapInfo
//======================================================================================================
CvHandicapInfo::CvHandicapInfo() :
	// Player Bonuses
	m_iMapPlacementPriority(0),
	m_iStartingGold(0),
	m_iStartingPolicyPoints(0),
	m_iAdvancedStartPointsMod(0),
	m_iHappinessDefault(0),
	m_iHappinessDefaultCapital(0),
	m_iExtraHappinessPerLuxury(0),
	m_iEmpireSizeUnhappinessMod(0),
	m_iPopulationUnhappinessMod(0),
	m_iFreeCulturePerTurn(0),
	m_iMaintenanceFreeUnits(0),
	m_iUnitSupplyBase(0),
	m_iUnitSupplyPerCity(0),
	m_iUnitSupplyPopulationPercent(0),
	m_iUnitSupplyPerEraFlat(0),
	m_iUnitSupplyPerEraModifier(0),
	m_iUnitSupplyBonusPercent(0),
	m_iStartingUnitMultiplier(0),
	m_iStartingWorkerUnits(0),
	m_iStartingDefenseUnits(0),
	m_iStartingExploreUnits(0),
	m_iWorkRateModifier(0),
	m_iImprovementCostPercent(0),
	m_iBuildingCostPercent(0),
	m_iUnitCostPercent(0),
	m_iInflationPercent(0),
	m_iUnitUpgradePercent(0),
	m_iUnitUpgradePerEraModifier(0),
	m_iGrowthPercent(0),
	m_iGrowthPerEraModifier(0),
	m_iResearchPercent(0),
	m_iResearchPerEraModifier(0),
	m_iTechCatchUpMod(0),
	m_iPolicyPercent(0),
	m_iPolicyPerEraModifier(0),
	m_iPolicyCatchUpMod(0),
	m_iProphetPercent(0),
	m_iGreatPeoplePercent(0),
	m_iGoldenAgePercent(0),
	m_iCivilianPercent(0),
	m_iCivilianPerEraModifier(0),
	m_iTrainPercent(0),
	m_iTrainPerEraModifier(0),
	m_iWorldTrainPercent(0),
	m_iConstructPercent(0),
	m_iConstructPerEraModifier(0),
	m_iWorldConstructPercent(0),
	m_iCreatePercent(0),
	m_iCreatePerEraModifier(0),
	m_iWorldCreatePercent(0),
	m_iProcessBonus(0),
	m_iProcessPerEraModifier(0),
	m_iFreeXP(0),
	m_iFreeXPPercent(0),
	m_iFreeXPPercentVSHuman(0),
	m_iCombatBonus(0),
	m_iResistanceCap(0),
	m_iVisionBonus(0),
	m_iSpySecurityModifier(0),
	// VP Difficulty Bonus
	m_iDifficultyBonusTurnInterval(0),

	// AI Bonuses
	m_iAIStartingGold(0),
	m_iAIStartingPolicyPoints(0),
	m_iAIAdvancedStartPointsMod(0),
	m_iAIHappinessDefault(0),
	m_iAIHappinessDefaultCapital(0),
	m_iAIExtraHappinessPerLuxury(0),
	m_iAIEmpireSizeUnhappinessMod(0),
	m_iAIPopulationUnhappinessMod(0),
	m_iAIFreeCulturePerTurn(0),
	m_iAIMaintenanceFreeUnits(0),
	m_iAIUnitSupplyBase(0),
	m_iAIUnitSupplyPerCity(0),
	m_iAIUnitSupplyPopulationPercent(0),
	m_iAIUnitSupplyPerEraFlat(0),
	m_iAIUnitSupplyPerEraModifier(0),
	m_iAIUnitSupplyBonusPercent(0),
	m_iAIStartingUnitMultiplier(0),
	m_iAIStartingWorkerUnits(0),
	m_iAIStartingDefenseUnits(0),
	m_iAIStartingExploreUnits(0),
	m_iAIWorkRateModifier(0),
	m_iAIImprovementCostPercent(0),
	m_iAIBuildingCostPercent(0),
	m_iAIUnitCostPercent(0),
	m_iAIInflationPercent(0),
	m_iAIUnitUpgradePercent(0),
	m_iAIUnitUpgradePerEraModifier(0),
	m_iAIGrowthPercent(0),
	m_iAIGrowthPerEraModifier(0),
	m_iAIResearchPercent(0),
	m_iAIResearchPerEraModifier(0),
	m_iAITechCatchUpMod(0),
	m_iAIPolicyPercent(0),
	m_iAIPolicyPerEraModifier(0),
	m_iAIPolicyCatchUpMod(0),
	m_iAIProphetPercent(0),
	m_iAIGreatPeoplePercent(0),
	m_iAIGoldenAgePercent(0),
	m_iAICivilianPercent(0),
	m_iAICivilianPerEraModifier(0),
	m_iAITrainPercent(0),
	m_iAITrainPerEraModifier(0),
	m_iAIWorldTrainPercent(0),
	m_iAIConstructPercent(0),
	m_iAIConstructPerEraModifier(0),
	m_iAIWorldConstructPercent(0),
	m_iAICreatePercent(0),
	m_iAICreatePerEraModifier(0),
	m_iAIWorldCreatePercent(0),
	m_iAIProcessBonus(0),
	m_iAIProcessPerEraModifier(0),
	m_iAIFreeXP(0),
	m_iAIFreeXPPercent(0),
	m_iAIFreeXPPercentVSHuman(0),
	m_iAICombatBonus(0),
	m_iAIResistanceCap(0),
	m_iAIVisionBonus(0),
	m_iAISpySecurityModifier(0),
	// VP Difficulty Bonus
	m_iAIDifficultyBonusTurnInterval(0),

	// City-States
	m_iStartingCityStateWorkerUnits(0),
	m_iStartingCityStateDefenseUnits(0),
	m_iCityStateUnitSupplyBase(0),
	m_iCityStateUnitSupplyPerCity(0),
	m_iCityStateUnitSupplyPopulationPercent(0),
	m_iCityStateUnitSupplyPerEraFlat(0),
	m_iCityStateUnitSupplyPerEraModifier(0),
	m_iCityStateUnitSupplyBonusPercent(0),
	m_iCityStateWorkRateModifier(0),
	m_iCityStateGrowthPercent(0),
	m_iCityStateGrowthPerEraModifier(0),
	m_iCityStateCivilianPercent(0),
	m_iCityStateCivilianPerEraModifier(0),
	m_iCityStateTrainPercent(0),
	m_iCityStateTrainPerEraModifier(0),
	m_iCityStateConstructPercent(0),
	m_iCityStateConstructPerEraModifier(0),
	m_iCityStateCreatePercent(0),
	m_iCityStateCreatePerEraModifier(0),
	m_iCityStateFreeXP(0),
	m_iCityStateFreeXPPercent(0),
	m_iCityStateCombatBonus(0),
	m_iCityStateVisionBonus(0),

	// Barbarians
	m_iEarliestBarbarianReleaseTurn(0),
	m_iBonusVSBarbarians(0),
	m_iAIBonusVSBarbarians(0),
	m_iBarbarianCampGold(0),
	m_iAIBarbarianCampGold(0),
	m_iBarbarianSpawnDelay(0),
	m_iBarbarianLandTargetRange(0),
	m_iBarbarianSeaTargetRange(0),

	// AI Behavior Modifiers
	// Weighted Randomized Choices
	m_iCityProductionChoiceCutoffThreshold(0),
	m_iTechChoiceCutoffThreshold(0),
	m_iPolicyChoiceCutoffThreshold(0),
	m_iBeliefChoiceCutoffThreshold(0),
	// Tactical AI
	m_iTacticalSimMaxCompletedPositions(23),
	m_iTacticalSimMaxBranches(3),
	m_iTacticalSimMaxChoicesPerUnit(3),
	// Diplomacy AI
	m_iLandDisputePercent(0),
	m_iWonderDisputePercent(0),
	m_iMinorCivDisputePercent(0),
	m_iVictoryDisputePercent(0),
	m_iVictoryDisputeMod(0),
	m_iVictoryBlockPercent(0),
	m_iVictoryBlockMod(0),
	m_iWonderBlockPercent(0),
	m_iWonderBlockMod(0),
	m_iTechBlockPercent(0),
	m_iTechBlockMod(0),
	m_iPolicyBlockPercent(0),
	m_iPolicyBlockMod(0),
	m_iPeaceTreatyDampenerTurns(0),
	m_iAggressionIncrease(0),
	m_iHumanStrengthPerceptionMod(0),
	m_iHumanTradeModifier(0),
	m_iHumanOpinionChange(0),
	m_iHumanWarApproachChangeFlat(0),
	m_iHumanWarApproachChangePercent(0),
	m_iHumanHostileApproachChangeFlat(0),
	m_iHumanHostileApproachChangePercent(0),
	m_iHumanDeceptiveApproachChangeFlat(0),
	m_iHumanDeceptiveApproachChangePercent(0),
	m_iHumanGuardedApproachChangeFlat(0),
	m_iHumanGuardedApproachChangePercent(0),
	m_iHumanAfraidApproachChangeFlat(0),
	m_iHumanAfraidApproachChangePercent(0),
	m_iHumanNeutralApproachChangeFlat(0),
	m_iHumanNeutralApproachChangePercent(0),
	m_iHumanFriendlyApproachChangeFlat(0),
	m_iHumanFriendlyApproachChangePercent(0),
	m_iAIOpinionChange(0),
	m_iAIWarApproachChangeFlat(0),
	m_iAIWarApproachChangePercent(0),
	m_iAIHostileApproachChangeFlat(0),
	m_iAIHostileApproachChangePercent(0),
	m_iAIDeceptiveApproachChangeFlat(0),
	m_iAIDeceptiveApproachChangePercent(0),
	m_iAIGuardedApproachChangeFlat(0),
	m_iAIGuardedApproachChangePercent(0),
	m_iAIAfraidApproachChangeFlat(0),
	m_iAIAfraidApproachChangePercent(0),
	m_iAINeutralApproachChangeFlat(0),
	m_iAINeutralApproachChangePercent(0),
	m_iAIFriendlyApproachChangeFlat(0),
	m_iAIFriendlyApproachChangePercent(0),

	m_iNumGoodies(0),
	m_piGoodies(NULL),
	m_pbFreeTechs(NULL),
	m_pbAIFreeTechs(NULL),
	m_pppiDifficultyBonus(NULL),
	m_pppiAIDifficultyBonus(NULL)
{
}

//------------------------------------------------------------------------------
CvHandicapInfo::~CvHandicapInfo()
{
	SAFE_DELETE_ARRAY(m_piGoodies);
	SAFE_DELETE_ARRAY(m_pbFreeTechs);
	SAFE_DELETE_ARRAY(m_pbAIFreeTechs);
	SAFE_DELETE_ARRAY(m_pppiDifficultyBonus);
	SAFE_DELETE_ARRAY(m_pppiAIDifficultyBonus);
}

/// PLAYER BONUSES
//------------------------------------------------------------------------------
int CvHandicapInfo::getMapPlacementPriority() const
{
	return m_iMapPlacementPriority;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getStartingGold() const
{
	return m_iStartingGold;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getStartingPolicyPoints() const
{
	return m_iStartingPolicyPoints;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAdvancedStartPointsMod() const
{
	return m_iAdvancedStartPointsMod;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getHappinessDefault() const
{
	return m_iHappinessDefault;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getHappinessDefaultCapital() const
{
	return m_iHappinessDefaultCapital;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getExtraHappinessPerLuxury() const
{
	return m_iExtraHappinessPerLuxury;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getEmpireSizeUnhappinessMod() const
{
	return m_iEmpireSizeUnhappinessMod;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getPopulationUnhappinessMod() const
{
	return m_iPopulationUnhappinessMod;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getFreeCulturePerTurn() const
{
	return m_iFreeCulturePerTurn;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getMaintenanceFreeUnits() const
{
	return m_iMaintenanceFreeUnits;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getUnitSupplyBase() const
{
	return m_iUnitSupplyBase;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getUnitSupplyPerCity() const
{
	return m_iUnitSupplyPerCity;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getUnitSupplyPopulationPercent() const
{
	return m_iUnitSupplyPopulationPercent;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getUnitSupplyPerEraFlat() const
{
	return m_iUnitSupplyPerEraFlat;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getUnitSupplyPerEraModifier() const
{
	return m_iUnitSupplyPerEraModifier;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getUnitSupplyBonusPercent() const
{
	return m_iUnitSupplyBonusPercent;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getStartingUnitMultiplier() const
{
	return m_iStartingUnitMultiplier;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getStartingWorkerUnits() const
{
	return m_iStartingWorkerUnits;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getStartingDefenseUnits() const
{
	return m_iStartingDefenseUnits;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getStartingExploreUnits() const
{
	return m_iStartingExploreUnits;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getWorkRateModifier() const
{
	return m_iWorkRateModifier;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getImprovementCostPercent() const
{
	return m_iImprovementCostPercent;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getBuildingCostPercent() const
{
	return m_iBuildingCostPercent;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getUnitCostPercent() const
{
	return m_iUnitCostPercent;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getInflationPercent() const
{
	return m_iInflationPercent;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getUnitUpgradePercent() const
{
	return m_iUnitUpgradePercent;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getUnitUpgradePerEraModifier() const
{
	return m_iUnitUpgradePerEraModifier;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getGrowthPercent() const
{
	return m_iGrowthPercent;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getGrowthPerEraModifier() const
{
	return m_iGrowthPerEraModifier;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getResearchPercent() const
{
	return m_iResearchPercent;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getResearchPerEraModifier() const
{
	return m_iResearchPerEraModifier;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getTechCatchUpMod() const
{
	return m_iTechCatchUpMod;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getPolicyPercent() const
{
	return m_iPolicyPercent;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getPolicyPerEraModifier() const
{
	return m_iPolicyPerEraModifier;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getPolicyCatchUpMod() const
{
	return m_iPolicyCatchUpMod;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getProphetPercent() const
{
	return m_iProphetPercent;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getGreatPeoplePercent() const
{
	return m_iGreatPeoplePercent;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getGoldenAgePercent() const
{
	return m_iGoldenAgePercent;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getCivilianPercent() const
{
	return m_iCivilianPercent;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getCivilianPerEraModifier() const
{
	return m_iCivilianPerEraModifier;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getTrainPercent() const
{
	return m_iTrainPercent;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getTrainPerEraModifier() const
{
	return m_iTrainPerEraModifier;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getWorldTrainPercent() const
{
	return m_iWorldTrainPercent;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getConstructPercent() const
{
	return m_iConstructPercent;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getConstructPerEraModifier() const
{
	return m_iConstructPerEraModifier;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getWorldConstructPercent() const
{
	return m_iWorldConstructPercent;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getCreatePercent() const
{
	return m_iCreatePercent;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getCreatePerEraModifier() const
{
	return m_iCreatePerEraModifier;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getWorldCreatePercent() const
{
	return m_iWorldCreatePercent;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getProcessBonus() const
{
	return m_iProcessBonus;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getProcessPerEraModifier() const
{
	return m_iProcessPerEraModifier;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getFreeXP() const
{
	return m_iFreeXP;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getFreeXPPercent() const
{
	return m_iFreeXPPercent;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getFreeXPPercentVSHuman() const
{
	return m_iFreeXPPercentVSHuman;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getCombatBonus() const
{
	return m_iCombatBonus;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getResistanceCap() const
{
	return m_iResistanceCap;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getVisionBonus() const
{
	return m_iVisionBonus;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getSpySecurityModifier() const
{
	return m_iSpySecurityModifier;
}
/// VP DIFFICULTY BONUS
//------------------------------------------------------------------------------
int CvHandicapInfo::getDifficultyBonusTurnInterval() const
{
	return m_iDifficultyBonusTurnInterval;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getYieldAmountForDifficultyBonus(int iEra, int iHistoricEvent, int iYield) const
{
	const int x = GC.getNumEraInfos();
	const int y = NUM_HISTORIC_EVENT_TYPES;
	const int z = NUM_YIELD_TYPES;
	ASSERT_DEBUG(iEra >= 0 && iEra < x);
	ASSERT_DEBUG(iHistoricEvent >= 0 && iHistoricEvent < y);
	ASSERT_DEBUG(iYield >= 0 && iYield < z);
	const int index = iEra * y * z + iHistoricEvent * z + iYield;
	return m_pppiDifficultyBonus[index];
}

/// AI BONUSES
//------------------------------------------------------------------------------
int CvHandicapInfo::getAIStartingGold() const
{
	return m_iAIStartingGold;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAIStartingPolicyPoints() const
{
	return m_iAIStartingPolicyPoints;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAIAdvancedStartPointsMod() const
{
	return m_iAIAdvancedStartPointsMod;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAIHappinessDefault() const
{
	return m_iAIHappinessDefault;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAIHappinessDefaultCapital() const
{
	return m_iAIHappinessDefaultCapital;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAIExtraHappinessPerLuxury() const
{
	return m_iAIExtraHappinessPerLuxury;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAIEmpireSizeUnhappinessMod() const
{
	return m_iAIEmpireSizeUnhappinessMod;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAIPopulationUnhappinessMod() const
{
	return m_iAIPopulationUnhappinessMod;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAIFreeCulturePerTurn() const
{
	return m_iAIFreeCulturePerTurn;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAIMaintenanceFreeUnits() const
{
	return m_iAIMaintenanceFreeUnits;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAIUnitSupplyBase() const
{
	return m_iAIUnitSupplyBase;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAIUnitSupplyPerCity() const
{
	return m_iAIUnitSupplyPerCity;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAIUnitSupplyPopulationPercent() const
{
	return m_iAIUnitSupplyPopulationPercent;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAIUnitSupplyPerEraFlat() const
{
	return m_iAIUnitSupplyPerEraFlat;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAIUnitSupplyPerEraModifier() const
{
	return m_iAIUnitSupplyPerEraModifier;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAIUnitSupplyBonusPercent() const
{
	return m_iAIUnitSupplyBonusPercent;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAIStartingUnitMultiplier() const
{
	return m_iAIStartingUnitMultiplier;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAIStartingWorkerUnits() const
{
	return m_iAIStartingWorkerUnits;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAIStartingDefenseUnits() const
{
	return m_iAIStartingDefenseUnits;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAIStartingExploreUnits() const
{
	return m_iAIStartingExploreUnits;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAIWorkRateModifier() const
{
	return m_iAIWorkRateModifier;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAIImprovementCostPercent() const
{
	return m_iAIImprovementCostPercent;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAIBuildingCostPercent() const
{
	return m_iAIBuildingCostPercent;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAIUnitCostPercent() const
{
	return m_iAIUnitCostPercent;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAIInflationPercent() const
{
	return m_iAIInflationPercent;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAIUnitUpgradePercent() const
{
	return m_iAIUnitUpgradePercent;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAIUnitUpgradePerEraModifier() const
{
	return m_iAIUnitUpgradePerEraModifier;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAIGrowthPercent() const
{
	return m_iAIGrowthPercent;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAIGrowthPerEraModifier() const
{
	return m_iAIGrowthPerEraModifier;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAIResearchPercent() const
{
	return m_iAIResearchPercent;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAIResearchPerEraModifier() const
{
	return m_iAIResearchPerEraModifier;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAITechCatchUpMod() const
{
	return m_iAITechCatchUpMod;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAIPolicyPercent() const
{
	return m_iAIPolicyPercent;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAIPolicyPerEraModifier() const
{
	return m_iAIPolicyPerEraModifier;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAIPolicyCatchUpMod() const
{
	return m_iAIPolicyCatchUpMod;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAIProphetPercent() const
{
	return m_iAIProphetPercent;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAIGreatPeoplePercent() const
{
	return m_iAIGreatPeoplePercent;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAIGoldenAgePercent() const
{
	return m_iAIGoldenAgePercent;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAICivilianPercent() const
{
	return m_iAICivilianPercent;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAICivilianPerEraModifier() const
{
	return m_iAICivilianPerEraModifier;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAITrainPercent() const
{
	return m_iAITrainPercent;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAITrainPerEraModifier() const
{
	return m_iAITrainPerEraModifier;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAIWorldTrainPercent() const
{
	return m_iAIWorldTrainPercent;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAIConstructPercent() const
{
	return m_iAIConstructPercent;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAIConstructPerEraModifier() const
{
	return m_iAIConstructPerEraModifier;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAIWorldConstructPercent() const
{
	return m_iAIWorldConstructPercent;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAICreatePercent() const
{
	return m_iAICreatePercent;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAICreatePerEraModifier() const
{
	return m_iAICreatePerEraModifier;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAIWorldCreatePercent() const
{
	return m_iAIWorldCreatePercent;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAIProcessBonus() const
{
	return m_iAIProcessBonus;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAIProcessPerEraModifier() const
{
	return m_iAIProcessPerEraModifier;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAIFreeXP() const
{
	return m_iAIFreeXP;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAIFreeXPPercent() const
{
	return m_iAIFreeXPPercent;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAIFreeXPPercentVSHuman() const
{
	return m_iAIFreeXPPercentVSHuman;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAICombatBonus() const
{
	return m_iAICombatBonus;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAIResistanceCap() const
{
	return m_iAIResistanceCap;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAIVisionBonus() const
{
	return m_iAIVisionBonus;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAISpySecurityModifier() const
{
	return m_iAISpySecurityModifier;
}
/// VP DIFFICULTY BONUS
//------------------------------------------------------------------------------
int CvHandicapInfo::getAIDifficultyBonusTurnInterval() const
{
	return m_iAIDifficultyBonusTurnInterval;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getYieldAmountForAIDifficultyBonus(int iEra, int iHistoricEvent, int iYield) const
{
	const int x = GC.getNumEraInfos();
	const int y = NUM_HISTORIC_EVENT_TYPES;
	const int z = NUM_YIELD_TYPES;
	ASSERT_DEBUG(iEra >= 0 && iEra < x);
	ASSERT_DEBUG(iHistoricEvent >= 0 && iHistoricEvent < y);
	ASSERT_DEBUG(iYield >= 0 && iYield < z);
	const int index = iEra * y * z + iHistoricEvent * z + iYield;
	return m_pppiAIDifficultyBonus[index];
}

/// CITY-STATES
//------------------------------------------------------------------------------
int CvHandicapInfo::getStartingCityStateWorkerUnits() const
{
	return m_iStartingCityStateWorkerUnits;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getStartingCityStateDefenseUnits() const
{
	return m_iStartingCityStateDefenseUnits;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getCityStateUnitSupplyBase() const
{
	return m_iCityStateUnitSupplyBase;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getCityStateUnitSupplyPerCity() const
{
	return m_iCityStateUnitSupplyPerCity;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getCityStateUnitSupplyPopulationPercent() const
{
	return m_iCityStateUnitSupplyPopulationPercent;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getCityStateUnitSupplyPerEraFlat() const
{
	return m_iCityStateUnitSupplyPerEraFlat;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getCityStateUnitSupplyPerEraModifier() const
{
	return m_iCityStateUnitSupplyPerEraModifier;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getCityStateUnitSupplyBonusPercent() const
{
	return m_iCityStateUnitSupplyBonusPercent;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getCityStateWorkRateModifier() const
{
	return m_iCityStateWorkRateModifier;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getCityStateGrowthPercent() const
{
	return m_iCityStateGrowthPercent;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getCityStateGrowthPerEraModifier() const
{
	return m_iCityStateGrowthPerEraModifier;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getCityStateCivilianPercent() const
{
	return m_iCityStateCivilianPercent;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getCityStateCivilianPerEraModifier() const
{
	return m_iCityStateCivilianPerEraModifier;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getCityStateTrainPercent() const
{
	return m_iCityStateTrainPercent;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getCityStateTrainPerEraModifier() const
{
	return m_iCityStateTrainPerEraModifier;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getCityStateConstructPercent() const
{
	return m_iCityStateConstructPercent;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getCityStateConstructPerEraModifier() const
{
	return m_iCityStateConstructPerEraModifier;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getCityStateCreatePercent() const
{
	return m_iCityStateCreatePercent;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getCityStateCreatePerEraModifier() const
{
	return m_iCityStateCreatePerEraModifier;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getCityStateFreeXP() const
{
	return m_iCityStateFreeXP;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getCityStateFreeXPPercent() const
{
	return m_iCityStateFreeXPPercent;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getCityStateCombatBonus() const
{
	return m_iCityStateCombatBonus;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getCityStateVisionBonus() const
{
	return m_iCityStateVisionBonus;
}

/// BARBARIANS
//------------------------------------------------------------------------------
int CvHandicapInfo::getEarliestBarbarianReleaseTurn() const
{
	return m_iEarliestBarbarianReleaseTurn;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getBonusVSBarbarians() const
{
	return m_iBonusVSBarbarians;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAIBonusVSBarbarians() const
{
	return m_iAIBonusVSBarbarians;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getBarbarianCampGold() const
{
	return m_iBarbarianCampGold;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAIBarbarianCampGold() const
{
	return m_iAIBarbarianCampGold;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getBarbarianSpawnDelay() const
{
	return m_iBarbarianSpawnDelay;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getBarbarianLandTargetRange() const
{
	return m_iBarbarianLandTargetRange;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getBarbarianSeaTargetRange() const
{
	return m_iBarbarianSeaTargetRange;
}

/// AI BEHAVIOR MODIFIERS
/// WEIGHTED RANDOMIZED CHOICES
//------------------------------------------------------------------------------
int CvHandicapInfo::getCityProductionChoiceCutoffThreshold() const
{
	return m_iCityProductionChoiceCutoffThreshold;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getTechChoiceCutoffThreshold() const
{
	return m_iTechChoiceCutoffThreshold;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getPolicyChoiceCutoffThreshold() const
{
	return m_iPolicyChoiceCutoffThreshold;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getBeliefChoiceCutoffThreshold() const
{
	return m_iBeliefChoiceCutoffThreshold;
}

/// TACTICAL AI
//------------------------------------------------------------------------------
int CvHandicapInfo::getTacticalSimMaxCompletedPositions() const
{
	return m_iTacticalSimMaxCompletedPositions;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getTacticalSimMaxBranches() const
{
	return m_iTacticalSimMaxBranches;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getTacticalSimMaxChoicesPerUnit() const
{
	return m_iTacticalSimMaxChoicesPerUnit;
}

/// DIPLOMACY AI
//------------------------------------------------------------------------------
int CvHandicapInfo::getLandDisputePercent() const
{
	return m_iLandDisputePercent;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getWonderDisputePercent() const
{
	return m_iWonderDisputePercent;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getMinorCivDisputePercent() const
{
	return m_iMinorCivDisputePercent;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getVictoryDisputePercent() const
{
	return m_iVictoryDisputePercent;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getVictoryDisputeMod() const
{
	return m_iVictoryDisputeMod;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getVictoryBlockPercent() const
{
	return m_iVictoryBlockPercent;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getVictoryBlockMod() const
{
	return m_iVictoryBlockMod;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getWonderBlockPercent() const
{
	return m_iWonderBlockPercent;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getWonderBlockMod() const
{
	return m_iWonderBlockMod;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getTechBlockPercent() const
{
	return m_iTechBlockPercent;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getTechBlockMod() const
{
	return m_iTechBlockMod;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getPolicyBlockPercent() const
{
	return m_iPolicyBlockPercent;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getPolicyBlockMod() const
{
	return m_iPolicyBlockMod;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getPeaceTreatyDampenerTurns() const
{
	return m_iPeaceTreatyDampenerTurns;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAggressionIncrease() const
{
	return m_iAggressionIncrease;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getHumanStrengthPerceptionMod() const
{
	return m_iHumanStrengthPerceptionMod;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getHumanTradeModifier() const
{
	return m_iHumanTradeModifier;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getHumanOpinionChange() const
{
	return m_iHumanOpinionChange;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getHumanWarApproachChangeFlat() const
{
	return m_iHumanWarApproachChangeFlat;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getHumanWarApproachChangePercent() const
{
	return m_iHumanWarApproachChangePercent;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getHumanHostileApproachChangeFlat() const
{
	return m_iHumanHostileApproachChangeFlat;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getHumanHostileApproachChangePercent() const
{
	return m_iHumanHostileApproachChangePercent;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getHumanDeceptiveApproachChangeFlat() const
{
	return m_iHumanDeceptiveApproachChangeFlat;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getHumanDeceptiveApproachChangePercent() const
{
	return m_iHumanDeceptiveApproachChangePercent;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getHumanGuardedApproachChangeFlat() const
{
	return m_iHumanGuardedApproachChangeFlat;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getHumanGuardedApproachChangePercent() const
{
	return m_iHumanGuardedApproachChangePercent;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getHumanAfraidApproachChangeFlat() const
{
	return m_iHumanAfraidApproachChangeFlat;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getHumanAfraidApproachChangePercent() const
{
	return m_iHumanAfraidApproachChangePercent;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getHumanNeutralApproachChangeFlat() const
{
	return m_iHumanNeutralApproachChangeFlat;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getHumanNeutralApproachChangePercent() const
{
	return m_iHumanNeutralApproachChangePercent;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getHumanFriendlyApproachChangeFlat() const
{
	return m_iHumanFriendlyApproachChangeFlat;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getHumanFriendlyApproachChangePercent() const
{
	return m_iHumanFriendlyApproachChangePercent;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAIOpinionChange() const
{
	return m_iAIOpinionChange;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAIWarApproachChangeFlat() const
{
	return m_iAIWarApproachChangeFlat;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAIWarApproachChangePercent() const
{
	return m_iAIWarApproachChangePercent;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAIHostileApproachChangeFlat() const
{
	return m_iAIHostileApproachChangeFlat;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAIHostileApproachChangePercent() const
{
	return m_iAIHostileApproachChangePercent;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAIDeceptiveApproachChangeFlat() const
{
	return m_iAIDeceptiveApproachChangeFlat;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAIDeceptiveApproachChangePercent() const
{
	return m_iAIDeceptiveApproachChangePercent;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAIGuardedApproachChangeFlat() const
{
	return m_iAIGuardedApproachChangeFlat;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAIGuardedApproachChangePercent() const
{
	return m_iAIGuardedApproachChangePercent;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAIAfraidApproachChangeFlat() const
{
	return m_iAIAfraidApproachChangeFlat;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAIAfraidApproachChangePercent() const
{
	return m_iAIAfraidApproachChangePercent;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAINeutralApproachChangeFlat() const
{
	return m_iAINeutralApproachChangeFlat;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAINeutralApproachChangePercent() const
{
	return m_iAINeutralApproachChangePercent;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAIFriendlyApproachChangeFlat() const
{
	return m_iAIFriendlyApproachChangeFlat;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getAIFriendlyApproachChangePercent() const
{
	return m_iAIFriendlyApproachChangePercent;
}

/// ARRAYS
//------------------------------------------------------------------------------
int CvHandicapInfo::getNumGoodies() const
{
	return m_iNumGoodies;
}
//------------------------------------------------------------------------------
int CvHandicapInfo::getGoodies(int i) const
{
	ASSERT_DEBUG(i < getNumGoodies(), "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_piGoodies[i];
}
//------------------------------------------------------------------------------
int CvHandicapInfo::isFreeTechs(int i) const
{
	ASSERT_DEBUG(i < GC.getNumTechInfos(), "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_pbFreeTechs[i];
}
//------------------------------------------------------------------------------
int CvHandicapInfo::isAIFreeTechs(int i) const
{
	ASSERT_DEBUG(i < GC.getNumTechInfos(), "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_pbAIFreeTechs[i];
}

//------------------------------------------------------------------------------
bool CvHandicapInfo::CacheResults(Database::Results& kResults, CvDatabaseUtility& kUtility)
{
	if (!CvBaseInfo::CacheResults(kResults, kUtility))
		return false;

	// Player Bonuses
	m_iMapPlacementPriority = kResults.GetInt("MapPlacementPriority");
	m_iStartingGold = kResults.GetInt("StartingGold");
	m_iStartingPolicyPoints = kResults.GetInt("StartingPolicyPoints");
	m_iAdvancedStartPointsMod = kResults.GetInt("AdvancedStartPointsMod");
	m_iHappinessDefault = kResults.GetInt("HappinessDefault");
	m_iHappinessDefaultCapital = kResults.GetInt("HappinessDefaultCapital");
	m_iExtraHappinessPerLuxury = kResults.GetInt("ExtraHappinessPerLuxury");
	m_iEmpireSizeUnhappinessMod = kResults.GetInt("EmpireSizeUnhappinessMod");
	m_iPopulationUnhappinessMod = kResults.GetInt("PopulationUnhappinessMod");
	m_iFreeCulturePerTurn = kResults.GetInt("FreeCulturePerTurn");
	m_iMaintenanceFreeUnits = kResults.GetInt("MaintenanceFreeUnits");
	m_iUnitSupplyBase = kResults.GetInt("UnitSupplyBase");
	m_iUnitSupplyPerCity = kResults.GetInt("UnitSupplyPerCity");
	m_iUnitSupplyPopulationPercent = kResults.GetInt("UnitSupplyPopulationPercent");
	m_iUnitSupplyPerEraFlat = kResults.GetInt("UnitSupplyPerEraFlat");
	m_iUnitSupplyPerEraModifier = kResults.GetInt("UnitSupplyPerEraModifier");
	m_iUnitSupplyBonusPercent = kResults.GetInt("UnitSupplyBonusPercent");
	m_iStartingUnitMultiplier = kResults.GetInt("StartingUnitMultiplier");
	m_iStartingWorkerUnits = kResults.GetInt("StartingWorkerUnits");
	m_iStartingDefenseUnits = kResults.GetInt("StartingDefenseUnits");
	m_iStartingExploreUnits = kResults.GetInt("StartingExploreUnits");
	m_iWorkRateModifier = kResults.GetInt("WorkRateModifier");
	m_iImprovementCostPercent = kResults.GetInt("ImprovementCostPercent");
	m_iBuildingCostPercent = kResults.GetInt("BuildingCostPercent");
	m_iUnitCostPercent = kResults.GetInt("UnitCostPercent");
	m_iInflationPercent = kResults.GetInt("InflationPercent");
	m_iUnitUpgradePercent = kResults.GetInt("UnitUpgradePercent");
	m_iUnitUpgradePerEraModifier = kResults.GetInt("UnitUpgradePerEraModifier");
	m_iGrowthPercent = kResults.GetInt("GrowthPercent");
	m_iGrowthPerEraModifier = kResults.GetInt("GrowthPerEraModifier");
	m_iResearchPercent = kResults.GetInt("ResearchPercent");
	m_iResearchPerEraModifier = kResults.GetInt("ResearchPerEraModifier");
	m_iTechCatchUpMod = kResults.GetInt("TechCatchUpMod");
	m_iPolicyPercent = kResults.GetInt("PolicyPercent");
	m_iPolicyPerEraModifier = kResults.GetInt("PolicyPerEraModifier");
	m_iPolicyCatchUpMod = kResults.GetInt("PolicyCatchUpMod");
	m_iProphetPercent = kResults.GetInt("ProphetPercent");
	m_iGreatPeoplePercent = kResults.GetInt("GreatPeoplePercent");
	m_iGoldenAgePercent = kResults.GetInt("GoldenAgePercent");
	m_iCivilianPercent = kResults.GetInt("CivilianPercent");
	m_iCivilianPerEraModifier = kResults.GetInt("CivilianPerEraModifier");
	m_iTrainPercent = kResults.GetInt("TrainPercent");
	m_iTrainPerEraModifier = kResults.GetInt("TrainPerEraModifier");
	m_iWorldTrainPercent = kResults.GetInt("WorldTrainPercent");
	m_iConstructPercent = kResults.GetInt("ConstructPercent");
	m_iConstructPerEraModifier = kResults.GetInt("ConstructPerEraModifier");
	m_iWorldConstructPercent = kResults.GetInt("WorldConstructPercent");
	m_iCreatePercent = kResults.GetInt("CreatePercent");
	m_iCreatePerEraModifier = kResults.GetInt("CreatePerEraModifier");
	m_iWorldCreatePercent = kResults.GetInt("WorldCreatePercent");
	m_iProcessBonus = kResults.GetInt("ProcessBonus");
	m_iProcessPerEraModifier = kResults.GetInt("ProcessPerEraModifier");
	m_iFreeXP = kResults.GetInt("FreeXP");
	m_iFreeXPPercent = kResults.GetInt("FreeXPPercent");
	m_iFreeXPPercent = kResults.GetInt("FreeXPPercentVSHuman");
	m_iCombatBonus = kResults.GetInt("CombatBonus");
	m_iResistanceCap = kResults.GetInt("ResistanceCap");
	m_iVisionBonus = kResults.GetInt("VisionBonus");
	m_iSpySecurityModifier = kResults.GetInt("SpySecurityModifier");
	// VP Difficulty Bonus
	m_iDifficultyBonusTurnInterval = kResults.GetInt("DifficultyBonusTurnInterval");

	// AI Bonuses
	m_iAIStartingGold = kResults.GetInt("AIStartingGold");
	m_iAIStartingPolicyPoints = kResults.GetInt("AIStartingPolicyPoints");
	m_iAIAdvancedStartPointsMod = kResults.GetInt("AIAdvancedStartPointsMod");
	m_iAIHappinessDefault = kResults.GetInt("AIHappinessDefault");
	m_iAIHappinessDefaultCapital = kResults.GetInt("AIHappinessDefaultCapital");
	m_iAIExtraHappinessPerLuxury = kResults.GetInt("AIExtraHappinessPerLuxury");
	m_iAIEmpireSizeUnhappinessMod = kResults.GetInt("AIEmpireSizeUnhappinessMod");
	m_iAIPopulationUnhappinessMod = kResults.GetInt("AIPopulationUnhappinessMod");
	m_iAIFreeCulturePerTurn = kResults.GetInt("AIFreeCulturePerTurn");
	m_iAIMaintenanceFreeUnits = kResults.GetInt("AIMaintenanceFreeUnits");
	m_iAIUnitSupplyBase = kResults.GetInt("AIUnitSupplyBase");
	m_iAIUnitSupplyPerCity = kResults.GetInt("AIUnitSupplyPerCity");
	m_iAIUnitSupplyPopulationPercent = kResults.GetInt("AIUnitSupplyPopulationPercent");
	m_iAIUnitSupplyPerEraFlat = kResults.GetInt("AIUnitSupplyPerEraFlat");
	m_iAIUnitSupplyPerEraModifier = kResults.GetInt("AIUnitSupplyPerEraModifier");
	m_iAIUnitSupplyBonusPercent = kResults.GetInt("AIUnitSupplyBonusPercent");
	m_iAIStartingUnitMultiplier = kResults.GetInt("AIStartingUnitMultiplier");
	m_iAIStartingWorkerUnits = kResults.GetInt("AIStartingWorkerUnits");
	m_iAIStartingDefenseUnits = kResults.GetInt("AIStartingDefenseUnits");
	m_iAIStartingExploreUnits = kResults.GetInt("AIStartingExploreUnits");
	m_iAIWorkRateModifier = kResults.GetInt("AIWorkRateModifier");
	m_iAIImprovementCostPercent = kResults.GetInt("AIImprovementCostPercent");
	m_iAIBuildingCostPercent = kResults.GetInt("AIBuildingCostPercent");
	m_iAIUnitCostPercent = kResults.GetInt("AIUnitCostPercent");
	m_iAIInflationPercent = kResults.GetInt("AIInflationPercent");
	m_iAIUnitUpgradePercent = kResults.GetInt("AIUnitUpgradePercent");
	m_iAIUnitUpgradePerEraModifier = kResults.GetInt("AIUnitUpgradePerEraModifier");
	m_iAIGrowthPercent = kResults.GetInt("AIGrowthPercent");
	m_iAIGrowthPerEraModifier = kResults.GetInt("AIGrowthPerEraModifier");
	m_iAIResearchPercent = kResults.GetInt("AIResearchPercent");
	m_iAIResearchPerEraModifier = kResults.GetInt("AIResearchPerEraModifier");
	m_iAITechCatchUpMod = kResults.GetInt("AITechCatchUpMod");
	m_iAIPolicyPercent = kResults.GetInt("AIPolicyPercent");
	m_iAIPolicyPerEraModifier = kResults.GetInt("AIPolicyPerEraModifier");
	m_iAIPolicyCatchUpMod = kResults.GetInt("AIPolicyCatchUpMod");
	m_iAIProphetPercent = kResults.GetInt("AIProphetPercent");
	m_iAIGreatPeoplePercent = kResults.GetInt("AIGreatPeoplePercent");
	m_iAIGoldenAgePercent = kResults.GetInt("AIGoldenAgePercent");
	m_iAICivilianPercent = kResults.GetInt("AICivilianPercent");
	m_iAICivilianPerEraModifier = kResults.GetInt("AICivilianPerEraModifier");
	m_iAITrainPercent = kResults.GetInt("AITrainPercent");
	m_iAITrainPerEraModifier = kResults.GetInt("AITrainPerEraModifier");
	m_iAIWorldTrainPercent = kResults.GetInt("AIWorldTrainPercent");
	m_iAIConstructPercent = kResults.GetInt("AIConstructPercent");
	m_iAIConstructPerEraModifier = kResults.GetInt("AIConstructPerEraModifier");
	m_iAIWorldConstructPercent = kResults.GetInt("AIWorldConstructPercent");
	m_iAICreatePercent = kResults.GetInt("AICreatePercent");
	m_iAICreatePerEraModifier = kResults.GetInt("AICreatePerEraModifier");
	m_iAIWorldCreatePercent = kResults.GetInt("AIWorldCreatePercent");
	m_iAIProcessBonus = kResults.GetInt("AIProcessBonus");
	m_iAIProcessPerEraModifier = kResults.GetInt("AIProcessPerEraModifier");
	m_iAIFreeXP = kResults.GetInt("AIFreeXP");
	m_iAIFreeXPPercent = kResults.GetInt("AIFreeXPPercent");
	m_iAIFreeXPPercent = kResults.GetInt("AIFreeXPPercentVSHuman");
	m_iAICombatBonus = kResults.GetInt("AICombatBonus");
	m_iAIResistanceCap = kResults.GetInt("AIResistanceCap");
	m_iAIVisionBonus = kResults.GetInt("AIVisionBonus");
	m_iAISpySecurityModifier = kResults.GetInt("AISpySecurityModifier");
	// VP Difficulty Bonus
	m_iAIDifficultyBonusTurnInterval = kResults.GetInt("AIDifficultyBonusTurnInterval");

	// City-States
	m_iStartingCityStateWorkerUnits = kResults.GetInt("StartingCityStateWorkerUnits");
	m_iStartingCityStateDefenseUnits = kResults.GetInt("StartingCityStateDefenseUnits");
	m_iCityStateUnitSupplyBase = kResults.GetInt("CityStateUnitSupplyBase");
	m_iCityStateUnitSupplyPerCity = kResults.GetInt("CityStateUnitSupplyPerCity");
	m_iCityStateUnitSupplyPopulationPercent = kResults.GetInt("CityStateUnitSupplyPopulationPercent");
	m_iCityStateUnitSupplyPerEraFlat = kResults.GetInt("CityStateUnitSupplyPerEraFlat");
	m_iCityStateUnitSupplyPerEraModifier = kResults.GetInt("CityStateUnitSupplyPerEraModifier");
	m_iCityStateUnitSupplyBonusPercent = kResults.GetInt("CityStateUnitSupplyBonusPercent");
	m_iCityStateWorkRateModifier = kResults.GetInt("CityStateWorkRateModifier");
	m_iCityStateGrowthPercent = kResults.GetInt("CityStateGrowthPercent");
	m_iCityStateGrowthPerEraModifier = kResults.GetInt("CityStateGrowthPerEraModifier");
	m_iCityStateCivilianPercent = kResults.GetInt("CityStateCivilianPercent");
	m_iCityStateCivilianPerEraModifier = kResults.GetInt("CityStateCivilianPerEraModifier");
	m_iCityStateTrainPercent = kResults.GetInt("CityStateTrainPercent");
	m_iCityStateTrainPerEraModifier = kResults.GetInt("CityStateTrainPerEraModifier");
	m_iCityStateConstructPercent = kResults.GetInt("CityStateConstructPercent");
	m_iCityStateConstructPerEraModifier = kResults.GetInt("CityStateConstructPerEraModifier");
	m_iCityStateCreatePercent = kResults.GetInt("CityStateCreatePercent");
	m_iCityStateCreatePerEraModifier = kResults.GetInt("CityStateCreatePerEraModifier");
	m_iCityStateFreeXP = kResults.GetInt("CityStateFreeXP");
	m_iCityStateFreeXPPercent = kResults.GetInt("CityStateFreeXPPercent");
	m_iCityStateCombatBonus = kResults.GetInt("CityStateCombatBonus");
	m_iCityStateVisionBonus = kResults.GetInt("CityStateVisionBonus");

	// Barbarians
	m_iEarliestBarbarianReleaseTurn = kResults.GetInt("EarliestBarbarianReleaseTurn");
	m_iBonusVSBarbarians = kResults.GetInt("BonusVSBarbarians");
	m_iAIBonusVSBarbarians = kResults.GetInt("AIBonusVSBarbarians");
	m_iBarbarianCampGold = kResults.GetInt("BarbarianCampGold");
	m_iAIBarbarianCampGold = kResults.GetInt("AIBarbarianCampGold");
	m_iBarbarianSpawnDelay = kResults.GetInt("BarbarianSpawnDelay");
	m_iBarbarianLandTargetRange = kResults.GetInt("BarbarianLandTargetRange");
	m_iBarbarianSeaTargetRange = kResults.GetInt("BarbarianSeaTargetRange");

	// AI Behavior Modifiers
	// Weighted Randomized Choices
	m_iCityProductionChoiceCutoffThreshold = kResults.GetInt("CityProductionChoiceCutoffThreshold");
	m_iTechChoiceCutoffThreshold = kResults.GetInt("TechChoiceCutoffThreshold");
	m_iPolicyChoiceCutoffThreshold = kResults.GetInt("PolicyChoiceCutoffThreshold");
	m_iBeliefChoiceCutoffThreshold = kResults.GetInt("BeliefChoiceCutoffThreshold");
	// Tactical AI
	m_iTacticalSimMaxCompletedPositions = kResults.GetInt("TacticalSimMaxCompletedPositions");
	m_iTacticalSimMaxBranches = kResults.GetInt("TacticalSimMaxBranches");
	m_iTacticalSimMaxChoicesPerUnit = kResults.GetInt("TacticalSimMaxChoicesPerUnit");
	// Diplomacy AI
	m_iLandDisputePercent = kResults.GetInt("LandDisputePercent");
	m_iWonderDisputePercent = kResults.GetInt("WonderDisputePercent");
	m_iMinorCivDisputePercent = kResults.GetInt("MinorCivDisputePercent");
	m_iVictoryDisputePercent = kResults.GetInt("VictoryDisputePercent");
	m_iVictoryDisputeMod = kResults.GetInt("VictoryDisputeMod");
	m_iVictoryBlockPercent = kResults.GetInt("VictoryBlockPercent");
	m_iVictoryBlockMod = kResults.GetInt("VictoryBlockMod");
	m_iWonderBlockPercent = kResults.GetInt("WonderBlockPercent");
	m_iWonderBlockMod = kResults.GetInt("WonderBlockMod");
	m_iTechBlockPercent = kResults.GetInt("TechBlockPercent");
	m_iTechBlockMod = kResults.GetInt("TechBlockMod");
	m_iPolicyBlockPercent = kResults.GetInt("PolicyBlockPercent");
	m_iPolicyBlockMod = kResults.GetInt("PolicyBlockMod");
	m_iPeaceTreatyDampenerTurns = kResults.GetInt("PeaceTreatyDampenerTurns");
	m_iAggressionIncrease = kResults.GetInt("AggressionIncrease");
	m_iHumanStrengthPerceptionMod = kResults.GetInt("HumanStrengthPerceptionMod");
	m_iHumanTradeModifier = kResults.GetInt("HumanTradeModifier");
	m_iHumanOpinionChange = kResults.GetInt("HumanOpinionChange");
	m_iHumanWarApproachChangeFlat = kResults.GetInt("HumanWarApproachChangeFlat");
	m_iHumanWarApproachChangePercent = kResults.GetInt("HumanWarApproachChangePercent");
	m_iHumanHostileApproachChangeFlat = kResults.GetInt("HumanHostileApproachChangeFlat");
	m_iHumanHostileApproachChangePercent = kResults.GetInt("HumanHostileApproachChangePercent");
	m_iHumanDeceptiveApproachChangeFlat = kResults.GetInt("HumanDeceptiveApproachChangeFlat");
	m_iHumanDeceptiveApproachChangePercent = kResults.GetInt("HumanDeceptiveApproachChangePercent");
	m_iHumanGuardedApproachChangeFlat = kResults.GetInt("HumanGuardedApproachChangeFlat");
	m_iHumanGuardedApproachChangePercent = kResults.GetInt("HumanGuardedApproachChangePercent");
	m_iHumanAfraidApproachChangeFlat = kResults.GetInt("HumanAfraidApproachChangeFlat");
	m_iHumanAfraidApproachChangePercent = kResults.GetInt("HumanAfraidApproachChangePercent");
	m_iHumanNeutralApproachChangeFlat = kResults.GetInt("HumanNeutralApproachChangeFlat");
	m_iHumanNeutralApproachChangePercent = kResults.GetInt("HumanNeutralApproachChangePercent");
	m_iHumanFriendlyApproachChangeFlat = kResults.GetInt("HumanFriendlyApproachChangeFlat");
	m_iHumanFriendlyApproachChangePercent = kResults.GetInt("HumanFriendlyApproachChangePercent");
	m_iAIOpinionChange = kResults.GetInt("AIOpinionChange");
	m_iAIWarApproachChangeFlat = kResults.GetInt("AIWarApproachChangeFlat");
	m_iAIWarApproachChangePercent = kResults.GetInt("AIWarApproachChangePercent");
	m_iAIHostileApproachChangeFlat = kResults.GetInt("AIHostileApproachChangeFlat");
	m_iAIHostileApproachChangePercent = kResults.GetInt("AIHostileApproachChangePercent");
	m_iAIDeceptiveApproachChangeFlat = kResults.GetInt("AIDeceptiveApproachChangeFlat");
	m_iAIDeceptiveApproachChangePercent = kResults.GetInt("AIDeceptiveApproachChangePercent");
	m_iAIGuardedApproachChangeFlat = kResults.GetInt("AIGuardedApproachChangeFlat");
	m_iAIGuardedApproachChangePercent = kResults.GetInt("AIGuardedApproachChangePercent");
	m_iAIAfraidApproachChangeFlat = kResults.GetInt("AIAfraidApproachChangeFlat");
	m_iAIAfraidApproachChangePercent = kResults.GetInt("AIAfraidApproachChangePercent");
	m_iAINeutralApproachChangeFlat = kResults.GetInt("AINeutralApproachChangeFlat");
	m_iAINeutralApproachChangePercent = kResults.GetInt("AINeutralApproachChangePercent");
	m_iAIFriendlyApproachChangeFlat = kResults.GetInt("AIFriendlyApproachChangeFlat");
	m_iAIFriendlyApproachChangePercent = kResults.GetInt("AIFriendlyApproachChangePercent");

	//Arrays
	const char* szHandicapType = GetType();

	//Goodies
	{
		//First find out how many goodies there are.
		Database::SingleResult kCount("count(*)");
		if (DB.SelectAt(kCount, "HandicapInfo_Goodies", "HandicapType", szHandicapType))
		{
			m_iNumGoodies = kCount.GetInt(0);
		}

		kUtility.InitializeArray(m_piGoodies, m_iNumGoodies, 0);

		Database::Results kArrayResults;
		char szSQL[512];
		sprintf_s(szSQL, "select GoodyHuts.ID from HandicapInfo_Goodies inner join GoodyHuts on GoodyType = GoodyHuts.Type where HandicapType = '%s';", szHandicapType);

		if (DB.Execute(kArrayResults, szSQL))
		{
			int i = 0;
			while (kArrayResults.Step())
			{
				m_piGoodies[i++] = kArrayResults.GetInt(0);
			}
		}
	}

	kUtility.PopulateArrayByExistence(m_pbFreeTechs, "Technologies", "HandicapInfo_FreeTechs", "TechType", "HandicapType", szHandicapType);
	kUtility.PopulateArrayByExistence(m_pbAIFreeTechs, "Technologies", "HandicapInfo_AIFreeTechs", "TechType", "HandicapType", szHandicapType);

	const int iNumEras = kUtility.MaxRows("Eras");
	const int iNumHistoricEvents = kUtility.MaxRows("HistoricEventTypes");
	const int iNumYields = kUtility.MaxRows("Yields");
	const int iDifficultyBonusArrSize = iNumEras * iNumHistoricEvents * iNumYields;

	//Difficulty Bonus Yield Multipliers
	{
		kUtility.InitializeArray(m_pppiDifficultyBonus, iDifficultyBonusArrSize, 0);

		std::string strKey = "HandicapInfos - DifficultyBonus";
		Database::Results* pResults = kUtility.GetResults(strKey);
		if (pResults == NULL)
		{
			pResults = kUtility.PrepareResults(strKey, "select Yields.ID as YieldID, HistoricEventTypes.ID as HistoricEventID, Eras.ID as EraID, Amount from HandicapInfo_DifficultyBonus inner join Yields on YieldType = Yields.Type inner join HistoricEventTypes on HistoricEventType = HistoricEventTypes.Type inner join Eras on EraType = Eras.Type where HandicapType = ?");
		}

		pResults->Bind(1, szHandicapType, strlen(szHandicapType), false);

		while (pResults->Step())
		{
			const int yield_idx = pResults->GetInt(0);
			ASSERT_DEBUG(yield_idx > -1);

			const int historicevent_idx = pResults->GetInt(1);
			ASSERT_DEBUG(historicevent_idx > -1);

			const int era_idx = pResults->GetInt(2);
			ASSERT_DEBUG(era_idx > -1);

			const int amount = pResults->GetInt(3);

			// Manually index the array
			const int index = era_idx * iNumHistoricEvents * iNumYields + historicevent_idx * iNumYields + yield_idx;
			m_pppiDifficultyBonus[index] = amount;
		}
	}
	//AI Difficulty Bonus Yield Multipliers
	{
		kUtility.InitializeArray(m_pppiAIDifficultyBonus, iDifficultyBonusArrSize, 0);

		std::string strKey = "HandicapInfos - AIDifficultyBonus";
		Database::Results* pResults = kUtility.GetResults(strKey);
		if (pResults == NULL)
		{
			pResults = kUtility.PrepareResults(strKey, "select Yields.ID as YieldID, HistoricEventTypes.ID as HistoricEventID, Eras.ID as EraID, Amount from HandicapInfo_AIDifficultyBonus inner join Yields on YieldType = Yields.Type inner join HistoricEventTypes on HistoricEventType = HistoricEventTypes.Type inner join Eras on EraType = Eras.Type where HandicapType = ?");
		}

		pResults->Bind(1, szHandicapType, strlen(szHandicapType), false);

		while (pResults->Step())
		{
			const int yield_idx = pResults->GetInt(0);
			ASSERT_DEBUG(yield_idx > -1);

			const int historicevent_idx = pResults->GetInt(1);
			ASSERT_DEBUG(historicevent_idx > -1);

			const int era_idx = pResults->GetInt(2);
			ASSERT_DEBUG(era_idx > -1);

			const int amount = pResults->GetInt(3);

			// Manually index the array
			const int index = era_idx * iNumHistoricEvents * iNumYields + historicevent_idx * iNumYields + yield_idx;
			m_pppiAIDifficultyBonus[index] = amount;
		}
	}

	return true;
}

//======================================================================================================
//					CvGameSpeedInfo
//======================================================================================================
CvGameSpeedInfo::CvGameSpeedInfo() :
	m_iDealDuration(0),
#if defined(MOD_BALANCE_CORE)
	m_iStartingHappiness(0),
#endif
	m_iGrowthPercent(0),
	m_iTrainPercent(0),
	m_iInstantYieldPercent(0),
	m_iDifficultyBonusPercent(0),
	m_iConstructPercent(0),
	m_iCreatePercent(0),
	m_iResearchPercent(0),
	m_iGoldPercent(100),
	m_iGoldGiftMod(0),
	m_iBuildPercent(0),
	m_iImprovementPercent(0),
	m_iGreatPeoplePercent(0),
	m_iCulturePercent(0),
	m_iFaithPercent(0),
	m_iBarbPercent(0),
	m_iFeatureProductionPercent(0),
	m_iUnitDiscoverPercent(0),
	m_iUnitHurryPercent(0),
	m_iUnitTradePercent(0),
	m_iGoldenAgePercent(0),
	m_iHurryPercent(0),
	m_iInflationOffset(0),
	m_iInflationPercent(0),
	m_iReligiousPressureAdjacentCity(0),
	m_iVictoryDelayPercent(0),
	m_iMinorCivElectionFreqMod(0),
#if defined(MOD_TRADE_ROUTE_SCALING)
	m_iTradeRouteSpeedMod(100),
#endif
#if defined(MOD_BALANCE_CORE)
	m_iPietyMax(0),
	m_iPietyMin(0),
#endif
	m_iMilitaryRatingDecayPercent(0),
	m_iTechCostPerTurnMultiplier(0),
	m_iMinimumVoluntaryVassalTurns(15),
	m_iMinimumVassalTurns(75),
	m_iMinimumVassalTaxTurns(0),
	m_iNumTurnsBetweenVassals(0),
	m_iMinimumVassalLiberateTurns(0),
	m_iLeaguePercent(0),
	m_iNumTurnIncrements(0),
	m_pGameTurnInfo(NULL)
{
}
//------------------------------------------------------------------------------
CvGameSpeedInfo::~CvGameSpeedInfo()
{
	SAFE_DELETE_ARRAY(m_pGameTurnInfo);
}
//------------------------------------------------------------------------------
int CvGameSpeedInfo::GetDealDuration() const
{
	return m_iDealDuration;
}
#if defined(MOD_BALANCE_CORE)
int CvGameSpeedInfo::GetStartingHappiness() const
{
	return m_iStartingHappiness;
}
#endif
//------------------------------------------------------------------------------
int CvGameSpeedInfo::getGrowthPercent() const
{
	return m_iGrowthPercent;
}
//------------------------------------------------------------------------------
int CvGameSpeedInfo::getTrainPercent() const
{
	return m_iTrainPercent;
}
//------------------------------------------------------------------------------
int CvGameSpeedInfo::getInstantYieldPercent() const
{
	return m_iInstantYieldPercent;
}
//------------------------------------------------------------------------------
int CvGameSpeedInfo::getDifficultyBonusPercent() const
{
	return m_iDifficultyBonusPercent;
}
//------------------------------------------------------------------------------
int CvGameSpeedInfo::getConstructPercent() const
{
	return m_iConstructPercent;
}
//------------------------------------------------------------------------------
int CvGameSpeedInfo::getCreatePercent() const
{
	return m_iCreatePercent;
}
//------------------------------------------------------------------------------
int CvGameSpeedInfo::getResearchPercent() const
{
	return m_iResearchPercent;
}
//------------------------------------------------------------------------------
int CvGameSpeedInfo::getGoldPercent() const
{
	return m_iGoldPercent;
}
//------------------------------------------------------------------------------
int CvGameSpeedInfo::getGoldGiftMod() const
{
	return m_iGoldGiftMod;
}
//------------------------------------------------------------------------------
int CvGameSpeedInfo::getBuildPercent() const
{
	return m_iBuildPercent;
}
//------------------------------------------------------------------------------
int CvGameSpeedInfo::getImprovementPercent() const
{
	return m_iImprovementPercent;
}
//------------------------------------------------------------------------------
int CvGameSpeedInfo::getGreatPeoplePercent() const
{
	return m_iGreatPeoplePercent;
}
//------------------------------------------------------------------------------
int CvGameSpeedInfo::getCulturePercent() const
{
	return m_iCulturePercent;
}
//------------------------------------------------------------------------------
int CvGameSpeedInfo::getFaithPercent() const
{
	return m_iFaithPercent;
}
//------------------------------------------------------------------------------
int CvGameSpeedInfo::getBarbPercent() const
{
	return m_iBarbPercent;
}
//------------------------------------------------------------------------------
int CvGameSpeedInfo::getFeatureProductionPercent() const
{
	return m_iFeatureProductionPercent;
}
//------------------------------------------------------------------------------
int CvGameSpeedInfo::getUnitDiscoverPercent() const
{
	return m_iUnitDiscoverPercent;
}
//------------------------------------------------------------------------------
int CvGameSpeedInfo::getUnitHurryPercent() const
{
	return m_iUnitHurryPercent;
}
//------------------------------------------------------------------------------
int CvGameSpeedInfo::getUnitTradePercent() const
{
	return m_iUnitTradePercent;
}
//------------------------------------------------------------------------------
int CvGameSpeedInfo::getGoldenAgePercent() const
{
	return m_iGoldenAgePercent;
}
//------------------------------------------------------------------------------
int CvGameSpeedInfo::getHurryPercent() const
{
	return m_iHurryPercent;
}
//------------------------------------------------------------------------------
int CvGameSpeedInfo::getInflationOffset() const
{
	return m_iInflationOffset;
}
//------------------------------------------------------------------------------
int CvGameSpeedInfo::getReligiousPressureAdjacentCity() const
{
	return m_iReligiousPressureAdjacentCity;
}
//------------------------------------------------------------------------------
int CvGameSpeedInfo::getInflationPercent() const
{
	return m_iInflationPercent;
}
//------------------------------------------------------------------------------
int CvGameSpeedInfo::getVictoryDelayPercent() const
{
	return m_iVictoryDelayPercent;
}
//------------------------------------------------------------------------------
int CvGameSpeedInfo::getMinorCivElectionFreqMod() const
{
	return m_iMinorCivElectionFreqMod;
}
//------------------------------------------------------------------------------
int CvGameSpeedInfo::getOpinionDurationPercent() const
{
	return m_iOpinionDurationPercent;
}
//------------------------------------------------------------------------------
int CvGameSpeedInfo::getSpyRatePercent() const
{
	return m_iSpyRatePercent;
}
//------------------------------------------------------------------------------
int CvGameSpeedInfo::getPeaceDealDuration() const
{
	return m_iPeaceDealDuration;
}
//------------------------------------------------------------------------------
int CvGameSpeedInfo::getRelationshipDuration() const
{
	return m_iRelationshipDuration;
}
#if defined(MOD_TRADE_ROUTE_SCALING)
//------------------------------------------------------------------------------
int CvGameSpeedInfo::getTradeRouteSpeedMod() const
{
	return m_iTradeRouteSpeedMod;
}
#endif
//------------------------------------------------------------------------------
int CvGameSpeedInfo::getMilitaryRatingDecayPercent() const
{
	return m_iMilitaryRatingDecayPercent;
}
#if defined(MOD_BALANCE_CORE)
//------------------------------------------------------------------------------
int CvGameSpeedInfo::getPietyMax() const
{
	return m_iPietyMax;
}
//------------------------------------------------------------------------------
int CvGameSpeedInfo::getPietyMin() const
{
	return m_iPietyMin;
}
#endif
//------------------------------------------------------------------------------
int CvGameSpeedInfo::getLeaguePercent() const
{
	return m_iLeaguePercent;
}
//------------------------------------------------------------------------------
int CvGameSpeedInfo::getNumTurnIncrements() const
{
	return m_iNumTurnIncrements;
}
//------------------------------------------------------------------------------
GameTurnInfo& CvGameSpeedInfo::getGameTurnInfo(int iIndex) const
{
	return m_pGameTurnInfo[iIndex];
}
//------------------------------------------------------------------------------
void CvGameSpeedInfo::allocateGameTurnInfos(const int iSize)
{
	m_pGameTurnInfo = FNEW(GameTurnInfo[iSize], c_eCiv5GameplayDLL, 0);
}
//------------------------------------------------------------------------------
bool CvGameSpeedInfo::CacheResults(Database::Results& kResults, CvDatabaseUtility& kUtility)
{
	if(!CvBaseInfo::CacheResults(kResults, kUtility))
		return false;

	m_iDealDuration					= kResults.GetInt("DealDuration");
#if defined(MOD_BALANCE_CORE)
	m_iStartingHappiness			= kResults.GetInt("StartingHappiness");
#endif
	m_iGrowthPercent				= kResults.GetInt("GrowthPercent");
	m_iTrainPercent					= kResults.GetInt("TrainPercent");
	m_iInstantYieldPercent			= kResults.GetInt("InstantYieldPercent");
	m_iDifficultyBonusPercent		= kResults.GetInt("DifficultyBonusPercent");
	m_iConstructPercent				= kResults.GetInt("ConstructPercent");
	m_iCreatePercent				= kResults.GetInt("CreatePercent");
	m_iResearchPercent				= kResults.GetInt("ResearchPercent");
	m_iGoldPercent					= kResults.GetInt("GoldPercent");
	m_iGoldGiftMod					= kResults.GetInt("GoldGiftMod");
	m_iBuildPercent					= kResults.GetInt("BuildPercent");
	m_iImprovementPercent			= kResults.GetInt("ImprovementPercent");
	m_iGreatPeoplePercent			= kResults.GetInt("GreatPeoplePercent");
	m_iCulturePercent				= kResults.GetInt("CulturePercent");
	m_iFaithPercent					= kResults.GetInt("FaithPercent");
	m_iBarbPercent					= kResults.GetInt("BarbPercent");
	m_iFeatureProductionPercent		= kResults.GetInt("FeatureProductionPercent");
	m_iUnitDiscoverPercent			= kResults.GetInt("UnitDiscoverPercent");
	m_iUnitHurryPercent				= kResults.GetInt("UnitHurryPercent");
	m_iUnitTradePercent				= kResults.GetInt("UnitTradePercent");
	m_iGoldenAgePercent				= kResults.GetInt("GoldenAgePercent");
	m_iHurryPercent					= kResults.GetInt("HurryPercent");
	m_iInflationOffset				= kResults.GetInt("InflationOffset");
	m_iInflationPercent				= kResults.GetInt("InflationPercent");
	m_iReligiousPressureAdjacentCity= kResults.GetInt("ReligiousPressureAdjacentCity");
	m_iVictoryDelayPercent			= kResults.GetInt("VictoryDelayPercent");
	m_iMinorCivElectionFreqMod		= kResults.GetInt("MinorCivElectionFreqMod");
	m_iOpinionDurationPercent		= kResults.GetInt("OpinionDurationPercent");
	m_iSpyRatePercent				= kResults.GetInt("SpyRatePercent");
	m_iPeaceDealDuration			= kResults.GetInt("PeaceDealDuration");
	m_iRelationshipDuration			= kResults.GetInt("RelationshipDuration");
#if defined(MOD_TRADE_ROUTE_SCALING)
	if (MOD_TRADE_ROUTE_SCALING) {
		m_iTradeRouteSpeedMod		= kResults.GetInt("TradeRouteSpeedMod");
	}
#endif
	m_iMilitaryRatingDecayPercent	= kResults.GetInt("MilitaryRatingDecayPercent");
#if defined(MOD_BALANCE_CORE)
	m_iPietyMin						= kResults.GetInt("PietyMin");
	m_iPietyMax						= kResults.GetInt("PietyMax");
#endif
	m_iLeaguePercent				= kResults.GetInt("LeaguePercent");

	m_iTechCostPerTurnMultiplier	= kResults.GetInt("TechCostPerTurnMultiplier");
	m_iMinimumVoluntaryVassalTurns	= kResults.GetInt("MinimumVoluntaryVassalTurns");
	m_iMinimumVassalTurns			= kResults.GetInt("MinimumVassalTurns");
	m_iMinimumVassalTaxTurns		= kResults.GetInt("MinimumVassalTaxTurns");
	m_iNumTurnsBetweenVassals		= kResults.GetInt("NumTurnsBetweenVassals");
	m_iMinimumVassalLiberateTurns		= kResults.GetInt("MinimumVassalLiberateTurns");

	//GameTurnInfos
	{
		const char* szGameSpeedInfoType = GetType();

		//Calculate number of turn increments
		char szCountSQL[256];
		sprintf_s(szCountSQL, "select count(*) from GameSpeed_Turns where GameSpeedType = '%s'", szGameSpeedInfoType);
		Database::SingleResult kCount;
		if(DB.Execute(kCount, szCountSQL))
		{
			m_iNumTurnIncrements = kCount.GetInt(0);
		}

		//Update turn increments
		allocateGameTurnInfos(getNumTurnIncrements());
		char szSQL[256];
		sprintf_s(szSQL, "select * from GameSpeed_Turns where GameSpeedType = '%s'", szGameSpeedInfoType);
		Database::Results kArrayResults;
		if(DB.Execute(kArrayResults, szSQL))
		{
			int i = 0;
			while(kArrayResults.Step())
			{
				GameTurnInfo& kInfo = getGameTurnInfo(i++);
				kInfo.iMonthIncrement = kArrayResults.GetInt("MonthIncrement");
				kInfo.iNumGameTurnsPerIncrement = kArrayResults.GetInt("TurnsPerIncrement");
			}
		}
	}

	return true;
}


//======================================================================================================
//					CvTurnTimerInfo
//======================================================================================================
CvTurnTimerInfo::CvTurnTimerInfo() :
	m_iBaseTime(0),
	m_iCityResource(0),
	m_iUnitResource(0),
	m_iFirstTurnMultiplier(0)
{}
//------------------------------------------------------------------------------
int CvTurnTimerInfo::getBaseTime() const
{
	return m_iBaseTime;
}
//------------------------------------------------------------------------------
int CvTurnTimerInfo::getCityResource() const
{
	return m_iCityResource;
}
//------------------------------------------------------------------------------
int CvTurnTimerInfo::getUnitResource() const
{
	return m_iUnitResource;
}
//------------------------------------------------------------------------------
int CvTurnTimerInfo::getFirstTurnMultiplier() const
{
	return m_iFirstTurnMultiplier;
}
//------------------------------------------------------------------------------
bool CvTurnTimerInfo::CacheResults(Database::Results& results, CvDatabaseUtility& kUtility)
{
	if(CvBaseInfo::CacheResults(results, kUtility))
	{
		results.GetValue("BaseTime", m_iBaseTime);
		results.GetValue("CityResource", m_iCityResource);
		results.GetValue("UnitResource", m_iUnitResource);
		results.GetValue("FirstTurnMultiplier", m_iFirstTurnMultiplier);

		return true;
	}

	return false;
}

bool CvTurnTimerInfo::operator==(const CvTurnTimerInfo& rhs) const
{
	if(this == &rhs) return true;
	if(!CvBaseInfo::operator==(rhs)) return false;
	if(m_iBaseTime != rhs.m_iBaseTime) return false;
	if(m_iCityResource != rhs.m_iCityResource) return false;
	if(m_iUnitResource != rhs.m_iUnitResource) return false;
	if(m_iFirstTurnMultiplier != rhs.m_iFirstTurnMultiplier) return false;
	return true;
}

template<typename TurnTimerInfo, typename Visitor>
void CvTurnTimerInfo::Serialize(TurnTimerInfo& turnTimerInfo, Visitor& visitor)
{
	visitor(turnTimerInfo.m_iBaseTime);
	visitor(turnTimerInfo.m_iCityResource);
	visitor(turnTimerInfo.m_iUnitResource);
	visitor(turnTimerInfo.m_iFirstTurnMultiplier);
}

void CvTurnTimerInfo::writeTo(FDataStream& saveTo) const
{
	CvBaseInfo::writeTo(saveTo);
	CvStreamSaveVisitor serialVisitor(saveTo);
	Serialize(*this, serialVisitor);
}

void CvTurnTimerInfo::readFrom(FDataStream& loadFrom)
{
	CvBaseInfo::readFrom(loadFrom);
	CvStreamLoadVisitor serialVisitor(loadFrom);
	Serialize(*this, serialVisitor);
}

FDataStream& operator<<(FDataStream& saveTo, const CvTurnTimerInfo& readFrom)
{
	readFrom.writeTo(saveTo);
	return saveTo;
}

FDataStream& operator>>(FDataStream& loadFrom, CvTurnTimerInfo& writeTo)
{
	writeTo.readFrom(loadFrom);
	return loadFrom;
}


#if defined(MOD_EVENTS_DIPLO_MODIFIERS)
//======================================================================================================
//					CvDiploModifierInfo
//======================================================================================================
CvDiploModifierInfo::CvDiploModifierInfo() :
	m_eFromCiv(NO_CIVILIZATION),
	m_eToCiv(NO_CIVILIZATION)
{}
//------------------------------------------------------------------------------
bool CvDiploModifierInfo::isForFromCiv(CivilizationTypes eFromCiv)
{
	return (m_eFromCiv == NO_CIVILIZATION || m_eFromCiv == eFromCiv);
}
//------------------------------------------------------------------------------
bool CvDiploModifierInfo::isForToCiv(CivilizationTypes eToCiv)
{
	return (m_eToCiv == NO_CIVILIZATION || m_eToCiv == eToCiv);
}
//------------------------------------------------------------------------------
bool CvDiploModifierInfo::CacheResults(Database::Results& results, CvDatabaseUtility& kUtility)
{
	if(CvBaseInfo::CacheResults(results, kUtility))
	{
		const char* szTextVal = NULL;

		szTextVal = results.GetText("FromCivilizationType");
		m_eFromCiv = (CivilizationTypes) GC.getInfoTypeForString(szTextVal, true);

		szTextVal = results.GetText("ToCivilizationType");
		m_eToCiv = (CivilizationTypes) GC.getInfoTypeForString(szTextVal, true);

		return true;
	}

	return false;
}

bool CvDiploModifierInfo::operator==(const CvDiploModifierInfo& rhs) const
{
	if(this == &rhs) return true;
	if(!CvBaseInfo::operator==(rhs)) return false;
	if(m_eFromCiv != rhs.m_eFromCiv) return false;
	if(m_eToCiv != rhs.m_eToCiv) return false;
	return true;
}

template<typename DiploModifierInfo, typename Visitor>
void CvDiploModifierInfo::Serialize(DiploModifierInfo& diploModifierInfo, Visitor& visitor)
{
	visitor(diploModifierInfo.m_eFromCiv);
	visitor(diploModifierInfo.m_eToCiv);
}

void CvDiploModifierInfo::writeTo(FDataStream& saveTo) const
{
	CvBaseInfo::writeTo(saveTo);
	CvStreamSaveVisitor serialVisitor(saveTo);
	Serialize(*this, serialVisitor);
}

void CvDiploModifierInfo::readFrom(FDataStream& loadFrom)
{
	CvBaseInfo::readFrom(loadFrom);
	CvStreamLoadVisitor serialVisitor(loadFrom);
	Serialize(*this, serialVisitor);
}

FDataStream& operator<<(FDataStream& saveTo, const CvDiploModifierInfo& readFrom)
{
	readFrom.writeTo(saveTo);
	return saveTo;
}

FDataStream& operator>>(FDataStream& loadFrom, CvDiploModifierInfo& writeTo)
{
	writeTo.readFrom(loadFrom);
	return loadFrom;
}
#endif


//======================================================================================================
//					CvBuildInfo
//======================================================================================================
CvBuildInfo::CvBuildInfo() :
	m_iTime(0),
	m_iCost(0),
#if defined(MOD_CIV6_WORKER)
	m_iBuilderCost(0),
#endif
	m_iCostIncreasePerImprovement(0),
	m_iTechPrereq(NO_TECH),
#if defined(MOD_BALANCE_CORE)
	m_iTechObsolete(NO_TECH),
	m_bKillOnlyCivilian(false),
	m_bFreeBestDomainUnit(false),
	m_bCultureBoost(false),
#endif
	m_iImprovement(NO_IMPROVEMENT),
	m_iRoute(NO_ROUTE),
	m_iEntityEvent(ENTITY_EVENT_NONE),
	m_iMissionType(NO_MISSION),
	m_bKill(false),
	m_bRepair(false),
	m_bRemoveRoute(false),
	m_bWater(false),
	m_bCanBeEmbarked(false),
	m_paiFeatureTech(NULL),
	m_paiFeatureTime(NULL),
	m_paiFeatureProduction(NULL),
	m_paiFeatureCost(NULL),
	m_paiTechTimeChange(NULL),
	m_paiFeatureObsoleteTech(NULL),
	m_pabFeatureRemoveOnly(NULL),
	m_pabFeatureRemove(NULL)
{
}
//------------------------------------------------------------------------------
CvBuildInfo::~CvBuildInfo()
{
	SAFE_DELETE_ARRAY(m_paiFeatureTech);
	SAFE_DELETE_ARRAY(m_paiFeatureTime);
	SAFE_DELETE_ARRAY(m_paiFeatureProduction);
	SAFE_DELETE_ARRAY(m_paiFeatureCost);
	SAFE_DELETE_ARRAY(m_paiTechTimeChange);
	SAFE_DELETE_ARRAY(m_pabFeatureRemove);
	SAFE_DELETE_ARRAY(m_paiFeatureObsoleteTech);
	SAFE_DELETE_ARRAY(m_pabFeatureRemoveOnly);
}
//------------------------------------------------------------------------------
int CvBuildInfo::getTime() const
{
	return m_iTime;
}
//------------------------------------------------------------------------------
int CvBuildInfo::getCost() const
{
	return m_iCost;
}
#if defined(MOD_CIV6_WORKER)
//------------------------------------------------------------------------------
int CvBuildInfo::getBuilderCost() const
{
	return m_iBuilderCost;
}
#endif
//------------------------------------------------------------------------------
int CvBuildInfo::getCostIncreasePerImprovement() const
{
	return m_iCostIncreasePerImprovement;
}
//------------------------------------------------------------------------------
int CvBuildInfo::getTechPrereq() const
{
	return m_iTechPrereq;
}

#if defined(MOD_BALANCE_CORE)
bool CvBuildInfo::IsFreeBestDomainUnit() const
{
	return m_bFreeBestDomainUnit;
}
bool CvBuildInfo::IsCultureBoost() const
{
	return m_bCultureBoost;
}
//------------------------------------------------------------------------------
int CvBuildInfo::getTechObsolete() const
{
	return m_iTechObsolete;
}
#endif
//------------------------------------------------------------------------------
int CvBuildInfo::getImprovement() const
{
	return m_iImprovement;
}
//------------------------------------------------------------------------------
int CvBuildInfo::getRoute() const
{
	return m_iRoute;
}
//------------------------------------------------------------------------------
int CvBuildInfo::getEntityEvent() const
{
	return m_iEntityEvent;
}
//------------------------------------------------------------------------------
int CvBuildInfo::getMissionType() const
{
	return m_iMissionType;
}
//------------------------------------------------------------------------------
void CvBuildInfo::setMissionType(int iNewType)
{
	m_iMissionType = iNewType;
}
//------------------------------------------------------------------------------
bool CvBuildInfo::isKill() const
{
	return m_bKill;
}
#if defined(MOD_BALANCE_CORE)
//------------------------------------------------------------------------------
bool CvBuildInfo::isKillOnlyCivilian() const
{
	return m_bKillOnlyCivilian;
}
#endif
//------------------------------------------------------------------------------
bool CvBuildInfo::isRepair() const
{
	return m_bRepair;
}
//------------------------------------------------------------------------------
bool CvBuildInfo::IsRemoveRoute() const
{
	return m_bRemoveRoute;
}
//------------------------------------------------------------------------------
bool CvBuildInfo::IsWater() const
{
	return m_bWater;
}
//------------------------------------------------------------------------------
bool CvBuildInfo::IsCanBeEmbarked() const
{
	return m_bCanBeEmbarked;
}

//------------------------------------------------------------------------------
int CvBuildInfo::getFeatureTech(int i) const
{
	ASSERT_DEBUG(i < GC.getNumFeatureInfos(), "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_paiFeatureTech ? m_paiFeatureTech[i] : -1;
}
//------------------------------------------------------------------------------
int CvBuildInfo::getFeatureTime(int i) const
{
	ASSERT_DEBUG(i < GC.getNumFeatureInfos(), "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_paiFeatureTime ? m_paiFeatureTime[i] : -1;
}
//------------------------------------------------------------------------------
int CvBuildInfo::getFeatureProduction(int i) const
{
	ASSERT_DEBUG(i < GC.getNumFeatureInfos(), "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_paiFeatureProduction ? m_paiFeatureProduction[i] : -1;
}
//------------------------------------------------------------------------------
int CvBuildInfo::getFeatureCost(int i) const
{
	ASSERT_DEBUG(i < GC.getNumFeatureInfos(), "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_paiFeatureCost ? m_paiFeatureCost[i] : -1;
}
//------------------------------------------------------------------------------
int CvBuildInfo::getTechTimeChange(int i) const
{
	ASSERT_DEBUG(i < GC.getNumTechInfos(), "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_paiTechTimeChange ? m_paiTechTimeChange[i] : -1;
}
//------------------------------------------------------------------------------
bool CvBuildInfo::isFeatureRemove(int i) const
{
	ASSERT_DEBUG(i < GC.getNumFeatureInfos(), "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_pabFeatureRemove ? m_pabFeatureRemove[i] : false;
}

//------------------------------------------------------------------------------
int CvBuildInfo::getFeatureObsoleteTech(int i) const
{
	ASSERT_DEBUG(i < GC.getNumFeatureInfos(), "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_paiFeatureObsoleteTech ? m_paiFeatureObsoleteTech[i] : -1;
}
//------------------------------------------------------------------------------
bool CvBuildInfo::isFeatureRemoveOnly(int i) const
{
	ASSERT_DEBUG(i < GC.getNumFeatureInfos(), "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_pabFeatureRemoveOnly ? m_pabFeatureRemoveOnly[i] : false;
}

//------------------------------------------------------------------------------
bool CvBuildInfo::CacheResults(Database::Results& kResults, CvDatabaseUtility& kUtility)
{
	if(!CvHotKeyInfo::CacheResults(kResults, kUtility))
		return false;

	m_iTime = kResults.GetInt("Time");
	m_iCost = kResults.GetInt("Cost");
#if defined(MOD_CIV6_WORKER)
	m_iBuilderCost = kResults.GetInt("BuilderCost");
#endif
	m_iCostIncreasePerImprovement = kResults.GetInt("CostIncreasePerImprovement");
	m_bKill = kResults.GetBool("Kill");
	m_bRepair = kResults.GetBool("Repair");
	m_bRemoveRoute = kResults.GetBool("RemoveRoute");
	m_bWater = kResults.GetBool("Water");
	m_bCanBeEmbarked = kResults.GetBool("CanBeEmbarked");

	const char* szPrereqTech = kResults.GetText("PrereqTech");
	m_iTechPrereq = GC.getInfoTypeForString(szPrereqTech, true);

#if defined(MOD_BALANCE_CORE)
	m_bKillOnlyCivilian = kResults.GetBool("KillOnlyCivilian");
	const char* szObsoleteTech = kResults.GetText("ObsoleteTech");
	m_iTechObsolete = GC.getInfoTypeForString(szObsoleteTech, true);
	m_bFreeBestDomainUnit = kResults.GetBool("IsFreeBestDomainUnit");
	m_bCultureBoost = kResults.GetBool("CultureBoost");
#endif

	const char* szImprovementType = kResults.GetText("ImprovementType");
	m_iImprovement = GC.getInfoTypeForString(szImprovementType, true);

	const char* szRouteType = kResults.GetText("RouteType");
	m_iRoute = GC.getInfoTypeForString(szRouteType, true);

	const char* szEntityEvent = kResults.GetText("EntityEvent");
	m_iEntityEvent = GC.getInfoTypeForString(szEntityEvent, true);

	//NOTE: Why isn't this really a struct? o_O
	//HACK: Temporary until the stored proc system is finished
	//FeatureStructs
	{
		kUtility.InitializeArray(m_paiFeatureTech, "Features");
		kUtility.InitializeArray(m_paiFeatureTime, "Features");
		kUtility.InitializeArray(m_paiFeatureProduction, "Features");
		kUtility.InitializeArray(m_paiFeatureCost, "Features");
		kUtility.InitializeArray(m_pabFeatureRemove, "Features");
		kUtility.InitializeArray(m_paiFeatureObsoleteTech, "Features");
		kUtility.InitializeArray(m_pabFeatureRemoveOnly, "Features");

		char szQuery[512];
		const char* szFeatureQuery = "select * from BuildFeatures where BuildType = '%s'";
		sprintf_s(szQuery, 512, szFeatureQuery, GetType());

		Database::Results kArrayResults;
		if(DB.Execute(kArrayResults, szQuery))
		{
			while(kArrayResults.Step())
			{
				const char* szFeatureType			= kArrayResults.GetText("FeatureType");
				const int iFeatureIdx				= GC.getInfoTypeForString(szFeatureType, true);

				const char* szFeatureTech			= kArrayResults.GetText("PrereqTech");

				ASSERT_DEBUG(iFeatureIdx > -1);
				m_paiFeatureTech[iFeatureIdx]		= GC.getInfoTypeForString(szFeatureTech, true);
				m_paiFeatureTime[iFeatureIdx]		= kArrayResults.GetInt("Time");
				m_paiFeatureProduction[iFeatureIdx] = kArrayResults.GetInt("Production");
				m_paiFeatureCost[iFeatureIdx]		= kArrayResults.GetInt("Cost");
				m_pabFeatureRemove[iFeatureIdx]		= kArrayResults.GetBool("Remove");
				m_paiFeatureObsoleteTech[iFeatureIdx]= GC.getInfoTypeForString(kArrayResults.GetText("ObsoleteTech"), true);
				m_pabFeatureRemoveOnly[iFeatureIdx]	= kArrayResults.GetBool("RemoveOnly");
			}
		}
	}
	
	const char* szBuildType = GetType();
	kUtility.PopulateArrayByValue(m_paiTechTimeChange, "Technologies", "Build_TechTimeChanges", "TechType", "BuildType", szBuildType, "TimeChange");

	return true;
}

/// Helper function to read in an integer array of data sized according to number of build types
void BuildArrayHelpers::Read(FDataStream& kStream, short* paiBuildArray)
{
	int iNumEntries = 0;
	int iType = 0;

	kStream >> iNumEntries;

	for(int iI = 0; iI < iNumEntries; iI++)
	{
		bool bValid = true;
		iType = CvInfosSerializationHelper::ReadHashed(kStream, &bValid);
		if(iType != -1 || !bValid)
		{
			if(iType != -1)
			{
				kStream >> paiBuildArray[iType];
			}
			else
			{
				CvString szError;
				szError.Format("LOAD ERROR: Build Type not found");
				GC.LogMessage(szError.GetCString());
				ASSERT_DEBUG(false, szError);

				int iDummy = 0;
				kStream >> iDummy;
			}
		}
	}
}

/// Helper function to write out an integer array of data sized according to number of building types
void BuildArrayHelpers::Write(FDataStream& kStream, short* paiBuildArray, int iArraySize)
{
	kStream << iArraySize;

	for(int iI = 0; iI < iArraySize; iI++)
	{
		const BuildTypes eBuild = static_cast<BuildTypes>(iI);
		CvBuildInfo* pkBuildInfo = GC.getBuildInfo(eBuild);
		if(pkBuildInfo)
		{
			CvInfosSerializationHelper::WriteHashed(kStream, pkBuildInfo);
			kStream << paiBuildArray[iI];
		}
		else
		{
			kStream << 0;
		}
	}
}

//======================================================================================================
//					CvGoodyInfo
//======================================================================================================

//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   CvGoodyInfo()
//
//  PURPOSE :   Default constructor
//
//------------------------------------------------------------------------------------------------------
CvGoodyInfo::CvGoodyInfo() : CvBaseInfo()
	, m_iGold(0)
	, m_iNumGoldRandRolls(0)
	, m_iGoldRandAmount(0)
	, m_iMapOffset(0)
	, m_iMapRange(0)
	, m_iMapProb(0)
	, m_iExperience(0)
	, m_iHealing(0)
	, m_iDamagePrereq(0)
	, m_iPopulation(0)
#if defined(MOD_BALANCE_CORE)
	, m_iProduction(0)
	, m_iGoldenAge(0)
	, m_iFreeTiles(0)
	, m_iScience(0)
// New Goodies modmod
	, m_iFood(0)
	, m_iBorderGrowth(0)
#endif
	, m_iCulture(0)
	, m_iFaith(0)
	, m_iProphetPercent(0)
	, m_iPantheonPercent(0)
	, m_iRevealNearbyBarbariansRange(0)
	, m_iBarbarianUnitProb(0)
	, m_iMinBarbarians(0)
	, m_iUnitClassType(NO_UNITCLASS)
	, m_iBarbarianUnitClass(NO_UNITCLASS)
	, m_bTech(false)
	, m_bRevealUnknownResource(false)
	, m_bUpgradeUnit(false)
	, m_bPantheonFaith(false)
	, m_bBad(false)
{
}

int CvGoodyInfo::getGold() const
{
	return m_iGold;
}

int CvGoodyInfo::getNumGoldRandRolls() const
{
	return m_iNumGoldRandRolls;
}

int CvGoodyInfo::getGoldRandAmount() const
{
	return m_iGoldRandAmount;
}

int CvGoodyInfo::getMapOffset() const
{
	return m_iMapOffset;
}

int CvGoodyInfo::getMapRange() const
{
	return m_iMapRange;
}

int CvGoodyInfo::getMapProb() const
{
	return m_iMapProb;
}

int CvGoodyInfo::getExperience() const
{
	return m_iExperience;
}

int CvGoodyInfo::getHealing() const
{
	return m_iHealing;
}

int CvGoodyInfo::getDamagePrereq() const
{
	return m_iDamagePrereq;
}

int CvGoodyInfo::getCulture() const
{
	return m_iCulture;
}

int CvGoodyInfo::getFaith() const
{
	return m_iFaith;
}

int CvGoodyInfo::getProphetPercent() const
{
	return m_iProphetPercent;
}

int CvGoodyInfo::getPantheonPercent() const
{
	return m_iPantheonPercent;
}

int CvGoodyInfo::getRevealNearbyBarbariansRange() const
{
	return m_iRevealNearbyBarbariansRange;
}

int CvGoodyInfo::getPopulation() const
{
	return m_iPopulation;
}
#if defined(MOD_BALANCE_CORE)
int CvGoodyInfo::getProduction() const
{
	return m_iProduction;
}
int CvGoodyInfo::getGoldenAge() const
{
	return m_iGoldenAge;
}
int CvGoodyInfo::getFreeTiles() const
{
	return m_iFreeTiles;
}
int CvGoodyInfo::getScience() const
{
	return m_iScience;
}
// New Goodies modmod
int CvGoodyInfo::getFood() const
{
	return m_iFood;
}
int CvGoodyInfo::getBorderGrowth() const
{
	return m_iBorderGrowth;
}
#endif
//
int CvGoodyInfo::getBarbarianUnitProb() const
{
	return m_iBarbarianUnitProb;
}

int CvGoodyInfo::getMinBarbarians() const
{
	return m_iMinBarbarians;
}

int CvGoodyInfo::getUnitClassType() const
{
	return m_iUnitClassType;
}

int CvGoodyInfo::getBarbarianUnitClass() const
{
	return m_iBarbarianUnitClass;
}

bool CvGoodyInfo::isTech() const
{
	return m_bTech;
}

bool CvGoodyInfo::isRevealUnknownResource() const
{
	return m_bRevealUnknownResource;
}

bool CvGoodyInfo::isUpgradeUnit() const
{
	return m_bUpgradeUnit;
}

bool CvGoodyInfo::isPantheonFaith() const
{
	return m_bPantheonFaith;
}

bool CvGoodyInfo::isBad() const
{
	return m_bBad;
}

const char* CvGoodyInfo::getSound() const
{
	return m_strSound.c_str();
}

void CvGoodyInfo::setSound(const char* szVal)
{
	m_strSound = szVal;
}

const char* CvGoodyInfo::GetChooseDesc() const
{
	return m_strChooseDesc;
}

void CvGoodyInfo::SetChooseDesc(const char* szVal)
{
	m_strChooseDesc = szVal;
}

bool CvGoodyInfo::CacheResults(Database::Results& results, CvDatabaseUtility& kUtility)
{
	if(!CvBaseInfo::CacheResults(results, kUtility))
	{
		return false;
	}

	const char* szChooseDesc = results.GetText("ChooseDescription");
	if (szChooseDesc)
		m_strChooseDesc = szChooseDesc;

	const char* szSound = results.GetText("Sound");
	if(szSound)
		m_strSound = szSound;

	m_iGold = results.GetInt("Gold");
	m_iNumGoldRandRolls = results.GetInt("NumGoldRandRolls");
	m_iGoldRandAmount = results.GetInt("GoldRandAmount");
	m_iMapOffset = results.GetInt("MapOffset");
	m_iMapRange = results.GetInt("MapRange");
	m_iMapProb = results.GetInt("MapProb");
	m_iExperience = results.GetInt("Experience");
	m_iHealing = results.GetInt("Healing");
	m_iDamagePrereq = results.GetInt("DamagePrereq");
	m_iPopulation = results.GetInt("Population");
#if defined(MOD_BALANCE_CORE)
	m_iProduction = results.GetInt("Production");
	m_iGoldenAge = results.GetInt("GoldenAge");
	m_iFreeTiles = results.GetInt("FreeTiles");
	m_iScience = results.GetInt("Science");
	m_iFood = results.GetInt("Food");
	m_iBorderGrowth = results.GetInt("BorderGrowth");
#endif
	m_iCulture = results.GetInt("Culture");
	m_iFaith = results.GetInt("Faith");
	m_iProphetPercent = results.GetInt("ProphetPercent");
	m_iPantheonPercent = results.GetInt("PantheonPercent");
	m_iRevealNearbyBarbariansRange = results.GetInt("RevealNearbyBarbariansRange");
	m_iBarbarianUnitProb = results.GetInt("BarbarianUnitProb");
	m_iMinBarbarians = results.GetInt("MinBarbarians");
	m_bTech = results.GetBool("Tech");
	m_bBad = results.GetBool("Bad");
	m_bRevealUnknownResource = results.GetBool("RevealUnknownResource");
	m_bUpgradeUnit = results.GetBool("UpgradeUnit");
	m_bPantheonFaith = results.GetBool("PantheonFaith");

	//TEMP TEMP TEMP TEMP
	m_iUnitClassType = GC.getInfoTypeForString(results.GetText("UnitClass"), true);
	m_iBarbarianUnitClass = GC.getInfoTypeForString(results.GetText("BarbarianUnitClass"), true);


	return true;
}

//======================================================================================================
//					CvRouteInfo
//======================================================================================================
CvRouteInfo::CvRouteInfo() :
	m_iGoldMaintenance(0),
	m_iValue(0),
	m_iMovementCost(0),
	m_iFlatMovementCost(0),
	m_bIndustrial(false),
	m_piYieldChange(NULL),
	m_piTechMovementChange(NULL),
	m_piResourceQuantityRequirements(NULL)
{
}
//------------------------------------------------------------------------------
CvRouteInfo::~CvRouteInfo()
{
	SAFE_DELETE_ARRAY(m_piYieldChange);
	SAFE_DELETE_ARRAY(m_piTechMovementChange);
	SAFE_DELETE_ARRAY(m_piResourceQuantityRequirements);
}
//------------------------------------------------------------------------------
int CvRouteInfo::GetGoldMaintenance() const
{
	return m_iGoldMaintenance;
}
//------------------------------------------------------------------------------
int CvRouteInfo::getValue() const
{
	return m_iValue;
}
//------------------------------------------------------------------------------
int CvRouteInfo::getMovementCost() const
{
	return m_iMovementCost;
}
//------------------------------------------------------------------------------
int CvRouteInfo::getFlatMovementCost() const
{
	return m_iFlatMovementCost;
}
//------------------------------------------------------------------------------
bool CvRouteInfo::IsIndustrial() const
{
	return m_bIndustrial;
}
//------------------------------------------------------------------------------
int CvRouteInfo::getYieldChange(int i) const
{
	ASSERT_DEBUG(i < NUM_YIELD_TYPES, "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_piYieldChange ? m_piYieldChange[i] : -1;
}
//------------------------------------------------------------------------------
int CvRouteInfo::getTechMovementChange(int i) const
{
	ASSERT_DEBUG(i < GC.getNumTechInfos(), "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_piTechMovementChange ? m_piTechMovementChange[i] : -1;
}
//------------------------------------------------------------------------------
int CvRouteInfo::getResourceQuantityRequirement(int i) const
{
	ASSERT_DEBUG(i < GC.getNumResourceInfos(), "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_piResourceQuantityRequirements ? m_piResourceQuantityRequirements[i] : -1;
}
//------------------------------------------------------------------------------
bool CvRouteInfo::CacheResults(Database::Results& kResults, CvDatabaseUtility& kUtility)
{
	if(!CvBaseInfo::CacheResults(kResults, kUtility))
		return false;

	m_iGoldMaintenance = kResults.GetInt("GoldMaintenance");
	m_iValue = kResults.GetInt("Value");
	m_iMovementCost = kResults.GetInt("Movement");
	m_iFlatMovementCost = kResults.GetInt("FlatMovement");

	m_bIndustrial = kResults.GetBool("Industrial");

	//Arrays
	const char* szRouteType = GetType();
	kUtility.SetYields(m_piYieldChange, "Route_Yields", "RouteType", szRouteType);

	kUtility.PopulateArrayByValue(m_piTechMovementChange, "Technologies", "Route_TechMovementChanges", "TechType", "RouteType", szRouteType, "MovementChange");
	kUtility.PopulateArrayByValue(m_piResourceQuantityRequirements, "Resources", "Route_ResourceQuantityRequirements", "ResourceType", "RouteType", szRouteType, "Cost");

	return true;
}

//======================================================================================================
//					CvResourceClassInfo
//======================================================================================================
bool CvResourceClassInfo::CacheResults(Database::Results& kResults, CvDatabaseUtility& kUtility)
{
	return CvBaseInfo::CacheResults(kResults, kUtility);
}

//======================================================================================================
//					CvResourceInfo
//======================================================================================================
CvResourceInfo::CvResourceInfo() :
	m_iResourceClassType(NO_RESOURCECLASS),
	m_iChar(0),
	m_iTechReveal(0),
	m_iPolicyReveal(NO_POLICY),
	m_iTechCityTrade(0),
	m_iTechImproveable(0),
	m_iTechObsolete(0),
	m_iAIStopTradingEra(-1),
	m_iStartingResourceQuantity(0),
	m_iHappiness(0),
	m_iWonderProductionMod(0),
	m_eWonderProductionModObsoleteEra(NO_ERA),
	m_iMinAreaSize(0),
	m_iMinLatitude(0),
	m_iMaxLatitude(0),
	m_eResourceUsage(RESOURCEUSAGE_BONUS),
#if defined(MOD_BALANCE_CORE_RESOURCE_MONOPOLIES)
	m_iMonopolyHappiness(0),
	m_iMonopolyGALength(0),
	m_bIsMonopoly(false),
#endif
	m_bPresentOnAllValidPlots(false),
	m_bOneArea(false),
	m_bHills(false),
	m_bFlatlands(false),
	m_bNoRiverSide(false),
	m_bOnlyMinorCivs(false),
	m_eRequiredCivilization(NO_CIVILIZATION),
	m_piYieldChange(NULL),
#if defined(MOD_BALANCE_CORE_RESOURCE_MONOPOLIES)
	m_piYieldChangeFromMonopoly(NULL),
	m_piCityYieldModFromMonopoly(NULL),
	m_piiMonopolyCombatModifiers(),
	m_piMonopolyGreatPersonRateModifiers(),
	m_piMonopolyGreatPersonRateChanges(),
#endif
#if defined(MOD_RESOURCES_PRODUCTION_COST_MODIFIERS)
	m_piiiUnitCombatProductionCostModifiersLocal(),
	m_aiiiBuildingProductionCostModifiersLocal(),
#endif
	m_piResourceQuantityTypes(NULL),
	m_piImprovementChange(NULL),
	m_pbTerrain(NULL),
	m_pbFeature(NULL),
	m_pbFeatureTerrain(NULL)
{
}
//------------------------------------------------------------------------------
CvResourceInfo::~CvResourceInfo()
{
	SAFE_DELETE_ARRAY(m_piYieldChange);
#if defined(MOD_BALANCE_CORE_RESOURCE_MONOPOLIES)
	SAFE_DELETE_ARRAY(m_piYieldChangeFromMonopoly);
	SAFE_DELETE_ARRAY(m_piCityYieldModFromMonopoly);
	m_piiMonopolyCombatModifiers.clear();
	m_piMonopolyGreatPersonRateModifiers.clear();
	m_piMonopolyGreatPersonRateChanges.clear();
#endif
#if defined(MOD_RESOURCES_PRODUCTION_COST_MODIFIERS)
	m_piiiUnitCombatProductionCostModifiersLocal.clear();
	m_aiiiBuildingProductionCostModifiersLocal.clear();
#endif
	SAFE_DELETE_ARRAY(m_piResourceQuantityTypes);
	SAFE_DELETE_ARRAY(m_piImprovementChange);
	SAFE_DELETE_ARRAY(m_pbTerrain);
	SAFE_DELETE_ARRAY(m_pbFeature);
	SAFE_DELETE_ARRAY(m_pbFeatureTerrain);	// free memory - MT
}
//------------------------------------------------------------------------------
int CvResourceInfo::getResourceClassType() const
{
	return m_iResourceClassType;
}
//------------------------------------------------------------------------------
int CvResourceInfo::getChar() const
{
	return m_iChar;
}
//------------------------------------------------------------------------------
void CvResourceInfo::setChar(int i)
{
	m_iChar = i;
}
//------------------------------------------------------------------------------
int CvResourceInfo::getTechReveal() const
{
	return m_iTechReveal;
}
//------------------------------------------------------------------------------
int CvResourceInfo::getPolicyReveal() const
{
	return m_iPolicyReveal;
}
//------------------------------------------------------------------------------
int CvResourceInfo::getTechCityTrade() const
{
	return m_iTechCityTrade;
}
//------------------------------------------------------------------------------
int CvResourceInfo::getImproveTech() const
{
	return m_iTechImproveable;
}
//------------------------------------------------------------------------------
int CvResourceInfo::getTechObsolete() const
{
	return m_iTechObsolete;
}
//------------------------------------------------------------------------------
int CvResourceInfo::getAIStopTradingEra() const
{
	return m_iAIStopTradingEra;
}
//------------------------------------------------------------------------------
int CvResourceInfo::getStartingResourceQuantity() const
{
	return m_iStartingResourceQuantity;
}
//------------------------------------------------------------------------------
int CvResourceInfo::getHappiness() const
{
	return m_iHappiness;
}
//------------------------------------------------------------------------------
int CvResourceInfo::getWonderProductionMod() const
{
	return m_iWonderProductionMod;
}
//------------------------------------------------------------------------------
EraTypes CvResourceInfo::getWonderProductionModObsoleteEra() const
{
	return m_eWonderProductionModObsoleteEra;
}
//------------------------------------------------------------------------------
int CvResourceInfo::getMinAreaSize() const
{
	return m_iMinAreaSize;
}
//------------------------------------------------------------------------------
int CvResourceInfo::getMinLatitude() const
{
	return m_iMinLatitude;
}
//------------------------------------------------------------------------------
int CvResourceInfo::getMaxLatitude() const
{
	return m_iMaxLatitude;
}
//------------------------------------------------------------------------------
ResourceUsageTypes CvResourceInfo::getResourceUsage() const
{
	return m_eResourceUsage;
}
#if defined(MOD_BALANCE_CORE_RESOURCE_MONOPOLIES)
int CvResourceInfo::getMonopolyHappiness() const
{
	return m_iMonopolyHappiness;
}
int CvResourceInfo::getMonopolyGALength() const
{
	return m_iMonopolyGALength;
}
int CvResourceInfo::getMonopolyAttackBonus() const
{
	return m_iMonopolyAttackBonus;
}
int CvResourceInfo::getMonopolyDefenseBonus() const
{
	return m_iMonopolyDefenseBonus;
}
int CvResourceInfo::getMonopolyMovementBonus() const
{
	return m_iMonopolyMovementBonus;
}
int CvResourceInfo::getMonopolyHealBonus() const
{
	return m_iMonopolyHealBonus;
}
int CvResourceInfo::getMonopolyXPBonus() const
{
	return m_iMonopolyXPBonus;
}
bool CvResourceInfo::isMonopoly() const
{
	return m_bIsMonopoly;
}
#endif
//------------------------------------------------------------------------------
bool CvResourceInfo::isPresentOnAllValidPlots() const
{
	return m_bPresentOnAllValidPlots;
}
//------------------------------------------------------------------------------
bool CvResourceInfo::isOneArea() const
{
	return m_bOneArea;
}
//------------------------------------------------------------------------------
bool CvResourceInfo::isHills() const
{
	return m_bHills;
}
//------------------------------------------------------------------------------
bool CvResourceInfo::isFlatlands() const
{
	return m_bFlatlands;
}
//------------------------------------------------------------------------------
bool CvResourceInfo::isNoRiverSide() const
{
	return m_bNoRiverSide;
}
//------------------------------------------------------------------------------
bool CvResourceInfo::isOnlyMinorCivs() const
{
	return m_bOnlyMinorCivs;
}
//------------------------------------------------------------------------------
CivilizationTypes CvResourceInfo::GetRequiredCivilization() const
{
	return m_eRequiredCivilization;
}
//------------------------------------------------------------------------------
const char* CvResourceInfo::GetIconString() const
{
	return m_strIconString;
}
//------------------------------------------------------------------------------
void CvResourceInfo::SetIconString(const char* szVal)
{
	m_strIconString = szVal;
}
//------------------------------------------------------------------------------
const char* CvResourceInfo::getArtDefineTag() const
{
	return m_strArtDefineTag;
}
//------------------------------------------------------------------------------
void CvResourceInfo::setArtDefineTag(const char* szVal)
{
	m_strArtDefineTag = szVal;
}
//------------------------------------------------------------------------------
const char* CvResourceInfo::getArtDefineTagHeavy() const
{
	return m_strArtDefineTagHeavy;
}
//------------------------------------------------------------------------------
void CvResourceInfo::setArtDefineTagHeavy(const char* szVal)
{
	m_strArtDefineTagHeavy = szVal;
}
//------------------------------------------------------------------------------
const char* CvResourceInfo::getAltArtDefineTag() const
{
	return m_strAltArtDefineTag;
}
//------------------------------------------------------------------------------
void CvResourceInfo::setAltArtDefineTag(const char* szVal)
{
	m_strAltArtDefineTag = szVal;
}
//------------------------------------------------------------------------------
const char* CvResourceInfo::getAltArtDefineTagHeavy() const
{
	return m_strAltArtDefineTagHeavy;
}
//------------------------------------------------------------------------------
void CvResourceInfo::setAltArtDefineTagHeavy(const char* szVal)
{
	m_strAltArtDefineTagHeavy = szVal;
}
//------------------------------------------------------------------------------
int CvResourceInfo::getYieldChange(int i) const
{
	ASSERT_DEBUG(i < NUM_YIELD_TYPES, "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_piYieldChange ? m_piYieldChange[i] : -1;
}
//------------------------------------------------------------------------------
int* CvResourceInfo::getYieldChangeArray()
{
	return m_piYieldChange;
}
#if defined(MOD_BALANCE_CORE_RESOURCE_MONOPOLIES)
//------------------------------------------------------------------------------
int CvResourceInfo::getYieldChangeFromMonopoly(int i) const
{
	ASSERT_DEBUG(i < NUM_YIELD_TYPES, "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_piYieldChangeFromMonopoly ? m_piYieldChangeFromMonopoly[i] : -1;
}
//------------------------------------------------------------------------------
int* CvResourceInfo::getYieldChangeFromMonopolyArray()
{
	return m_piYieldChangeFromMonopoly;
}
//------------------------------------------------------------------------------
int CvResourceInfo::getCityYieldModFromMonopoly(int i) const
{
	ASSERT_DEBUG(i < NUM_YIELD_TYPES, "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_piCityYieldModFromMonopoly ? m_piCityYieldModFromMonopoly[i] : -1;
}
//------------------------------------------------------------------------------
int* CvResourceInfo::getCityYieldModFromMonopolyArray()
{
	return m_piCityYieldModFromMonopoly ;
}
//------------------------------------------------------------------------------
int CvResourceInfo::getMonopolyAttackBonus(MonopolyTypes eMonopoly) const
{
	ResourceMonopolySettings sKey;
	int iMod = 0;
	std::map<ResourceMonopolySettings, CombatModifiers>::const_iterator it;

	if (eMonopoly == MONOPOLY_STRATEGIC)
	{
		sKey.m_bGlobalMonopoly = true;
		sKey.m_bStrategicMonopoly = true;

		it = m_piiMonopolyCombatModifiers.find(sKey);

		if (it != m_piiMonopolyCombatModifiers.end())
		{
			iMod += it->second.m_iAttackMod;
		}

		sKey.m_bGlobalMonopoly = false;

		it = m_piiMonopolyCombatModifiers.find(sKey);

		if (it != m_piiMonopolyCombatModifiers.end())
		{
			iMod += it->second.m_iAttackMod;
		}
	}

	else if (eMonopoly == MONOPOLY_GLOBAL)
	{
		sKey.m_bGlobalMonopoly = true;
		sKey.m_bStrategicMonopoly = false;

		it = m_piiMonopolyCombatModifiers.find(sKey);

		if (it != m_piiMonopolyCombatModifiers.end())
		{
			iMod += it->second.m_iAttackMod;
		}

		sKey.m_bStrategicMonopoly = true;

		it = m_piiMonopolyCombatModifiers.find(sKey);

		if (it != m_piiMonopolyCombatModifiers.end())
		{
			iMod += it->second.m_iAttackMod;
		}
	}

	return iMod;
}
//------------------------------------------------------------------------------
int CvResourceInfo::getMonopolyDefenseBonus(MonopolyTypes eMonopoly) const
{
	ResourceMonopolySettings sKey;
	int iMod = 0;
	std::map<ResourceMonopolySettings, CombatModifiers>::const_iterator it;

	if (eMonopoly == MONOPOLY_STRATEGIC)
	{
		sKey.m_bGlobalMonopoly = true;
		sKey.m_bStrategicMonopoly = true;

		it = m_piiMonopolyCombatModifiers.find(sKey);

		if (it != m_piiMonopolyCombatModifiers.end())
		{
			iMod += it->second.m_iDefenseMod;
		}

		sKey.m_bGlobalMonopoly = false;

		it = m_piiMonopolyCombatModifiers.find(sKey);

		if (it != m_piiMonopolyCombatModifiers.end())
		{
			iMod += it->second.m_iDefenseMod;
		}
	}

	if (eMonopoly == MONOPOLY_GLOBAL)
	{
		sKey.m_bGlobalMonopoly = true;
		sKey.m_bStrategicMonopoly = false;

		it = m_piiMonopolyCombatModifiers.find(sKey);

		if (it != m_piiMonopolyCombatModifiers.end())
		{
			iMod += it->second.m_iDefenseMod;
		}

		sKey.m_bStrategicMonopoly = true;

		it = m_piiMonopolyCombatModifiers.find(sKey);

		if (it != m_piiMonopolyCombatModifiers.end())
		{
			iMod += it->second.m_iDefenseMod;
		}
	}

	return iMod;
}
//------------------------------------------------------------------------------
int CvResourceInfo::getMonopolyGreatPersonRateModifier(SpecialistTypes eSpecialist, MonopolyTypes eMonopoly) const
{
	MonopolyGreatPersonRateModifierKey sKey;
	sKey.m_iSpecialist = (int)eSpecialist;
	int iMod = 0;

	std::map<MonopolyGreatPersonRateModifierKey, int>::const_iterator it;

	if (eMonopoly == MONOPOLY_STRATEGIC)
	{
		sKey.m_sMonopoly.m_bGlobalMonopoly = true;
		sKey.m_sMonopoly.m_bStrategicMonopoly = true;

		it = m_piMonopolyGreatPersonRateModifiers.find(sKey);

		if (it != m_piMonopolyGreatPersonRateModifiers.end())
		{
			iMod += it->second;
		}

		sKey.m_sMonopoly.m_bGlobalMonopoly = false;

		it = m_piMonopolyGreatPersonRateModifiers.find(sKey);

		if (it != m_piMonopolyGreatPersonRateModifiers.end())
		{
			iMod += it->second;
		}
	}

	else if (eMonopoly == MONOPOLY_GLOBAL)
	{
		sKey.m_sMonopoly.m_bGlobalMonopoly = true;
		sKey.m_sMonopoly.m_bStrategicMonopoly = false;

		it = m_piMonopolyGreatPersonRateModifiers.find(sKey);

		if (it != m_piMonopolyGreatPersonRateModifiers.end())
		{
			iMod += it->second;
		}

		sKey.m_sMonopoly.m_bStrategicMonopoly = true;

		it = m_piMonopolyGreatPersonRateModifiers.find(sKey);

		if (it != m_piMonopolyGreatPersonRateModifiers.end())
		{
			iMod += it->second;
		}
	}

	return iMod;
}
//------------------------------------------------------------------------------
int CvResourceInfo::getMonopolyGreatPersonRateChange(SpecialistTypes eSpecialist, MonopolyTypes eMonopoly) const
{
	MonopolyGreatPersonRateModifierKey sKey;
	sKey.m_iSpecialist = (int)eSpecialist;
	int iMod = 0;

	std::map<MonopolyGreatPersonRateModifierKey, int>::const_iterator it;

	if (eMonopoly == MONOPOLY_STRATEGIC)
	{
		sKey.m_sMonopoly.m_bGlobalMonopoly = true;
		sKey.m_sMonopoly.m_bStrategicMonopoly = true;

		it = m_piMonopolyGreatPersonRateChanges.find(sKey);

		if (it != m_piMonopolyGreatPersonRateChanges.end())
		{
			iMod += it->second;
		}

		sKey.m_sMonopoly.m_bGlobalMonopoly = false;

		it = m_piMonopolyGreatPersonRateChanges.find(sKey);

		if (it != m_piMonopolyGreatPersonRateChanges.end())
		{
			iMod += it->second;
		}
	}

	else if (eMonopoly == MONOPOLY_GLOBAL)
	{
		sKey.m_sMonopoly.m_bGlobalMonopoly = true;
		sKey.m_sMonopoly.m_bStrategicMonopoly = false;

		it = m_piMonopolyGreatPersonRateChanges.find(sKey);

		if (it != m_piMonopolyGreatPersonRateChanges.end())
		{
			iMod += it->second;
		}

		sKey.m_sMonopoly.m_bStrategicMonopoly = true;

		it = m_piMonopolyGreatPersonRateChanges.find(sKey);

		if (it != m_piMonopolyGreatPersonRateChanges.end())
		{
			iMod += it->second;
		}
	}

	return iMod;
}
#endif

#if defined(MOD_RESOURCES_PRODUCTION_COST_MODIFIERS)
//------------------------------------------------------------------------------
bool CvResourceInfo::isHasUnitCombatProductionCostModifiersLocal() const
{
	return !m_piiiUnitCombatProductionCostModifiersLocal.empty();
}
//------------------------------------------------------------------------------
int CvResourceInfo::getUnitCombatProductionCostModifiersLocal(UnitCombatTypes eUnitCombat, EraTypes eUnitEra) const
{
	ASSERT_DEBUG(eUnitCombat < GC.getNumUnitCombatClassInfos(), "Index out of bounds");
	ASSERT_DEBUG(eUnitCombat > -1, "Index out of bounds");

	ASSERT_DEBUG(eUnitEra < GC.getNumEraInfos(), "Index out of bounds");
	ASSERT_DEBUG(eUnitEra > -1, "Index out of bounds");

	int iUnitCombat = (int)eUnitCombat;
	int iUnitEra = (int)eUnitEra;
	int iMod = 0;

	std::map<int, std::vector<ProductionCostModifiers>>::const_iterator itMap = m_piiiUnitCombatProductionCostModifiersLocal.find(iUnitCombat);
	if (itMap != m_piiiUnitCombatProductionCostModifiersLocal.end()) // find returns the iterator to map::end if the key iUnitCombat is not present in the map
	{
		for (std::vector<ProductionCostModifiers>::const_iterator itVector = itMap->second.begin(); itVector != itMap->second.end(); ++itVector)
		{
			EraTypes eRequiredEra = (EraTypes)itVector->m_iRequiredEra;
			EraTypes eObsoleteEra = (EraTypes)itVector->m_iObsoleteEra;

			if (eUnitEra != NO_ERA)
			{
				// Our unit's era needs to be greater than or equal to the required era
				if (eRequiredEra != NO_ERA && iUnitEra < itVector->m_iRequiredEra)
				{
					continue;
				}

				// Our unit's era needs to be less than the obsolete era
				if (eObsoleteEra != NO_ERA && iUnitEra >= itVector->m_iObsoleteEra)
				{
					continue;
				}
			}

			iMod += itVector->m_iCostModifier;
		}
	}

	return iMod;
}
//------------------------------------------------------------------------------
std::vector<ProductionCostModifiers> CvResourceInfo::getUnitCombatProductionCostModifiersLocal(UnitCombatTypes eUnitCombat) const
{
	ASSERT_DEBUG(eUnitCombat < GC.getNumUnitCombatClassInfos(), "Index out of bounds");
	ASSERT_DEBUG(eUnitCombat > -1, "Index out of bounds");

	int iUnitCombat = (int)eUnitCombat;

	std::map<int, std::vector<ProductionCostModifiers>>::const_iterator it = m_piiiUnitCombatProductionCostModifiersLocal.find(iUnitCombat);
	if (it != m_piiiUnitCombatProductionCostModifiersLocal.end()) // find returns the iterator to map::end if the key iUnitCombat is not present in the map
	{
		return it->second;
	}

	return std::vector<ProductionCostModifiers>();
}
//------------------------------------------------------------------------------
bool CvResourceInfo::isHasBuildingProductionCostModifiersLocal() const
{
	return !m_aiiiBuildingProductionCostModifiersLocal.empty();
}
//------------------------------------------------------------------------------
int CvResourceInfo::getBuildingProductionCostModifiersLocal(EraTypes eBuildingEra) const
{
	ASSERT_DEBUG(eBuildingEra < GC.getNumEraInfos(), "Index out of bounds");
	ASSERT_DEBUG(eBuildingEra > -1, "Index out of bounds");

	int iBuildingEra = (int)eBuildingEra;
	int iMod = 0;

	for (std::vector<ProductionCostModifiers>::const_iterator it = m_aiiiBuildingProductionCostModifiersLocal.begin(); it != m_aiiiBuildingProductionCostModifiersLocal.end(); ++it)
	{
		EraTypes eRequiredEra = (EraTypes)it->m_iRequiredEra;
		EraTypes eObsoleteEra = (EraTypes)it->m_iObsoleteEra;

		if (eBuildingEra != NO_ERA)
		{
			// Our building's era needs to be greater than or equal to the required era
			if (eRequiredEra != NO_ERA && iBuildingEra < it->m_iRequiredEra)
			{
				continue;
			}

			// Our building's era needs to be less than the obsolete era
			if (eObsoleteEra != NO_ERA && iBuildingEra >= it->m_iObsoleteEra)
			{
				continue;
			}
		}

		iMod += it->m_iCostModifier;
	}

	return iMod;
}

//------------------------------------------------------------------------------
std::vector<ProductionCostModifiers> CvResourceInfo::getBuildingProductionCostModifiersLocal() const
{
	return m_aiiiBuildingProductionCostModifiersLocal;
}
#endif
//------------------------------------------------------------------------------
int CvResourceInfo::getResourceQuantityType(int i) const
{
	ASSERT_DEBUG(i < /*4*/ GD_INT_GET(NUM_RESOURCE_QUANTITY_TYPES), "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_piResourceQuantityTypes ? m_piResourceQuantityTypes[i] : -1;
}

int CvResourceInfo::getImprovementChange(int i) const
{
	ASSERT_DEBUG(i < GC.getNumImprovementInfos(), "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_piImprovementChange ? m_piImprovementChange[i] : -1;
}
//------------------------------------------------------------------------------
bool CvResourceInfo::isTerrain(int i) const
{
	ASSERT_DEBUG(i < GC.getNumTerrainInfos(), "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_pbTerrain ?	m_pbTerrain[i] : false;
}
//------------------------------------------------------------------------------
bool CvResourceInfo::isFeature(int i) const
{
	ASSERT_DEBUG(i < GC.getNumFeatureInfos(), "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_pbFeature ? m_pbFeature[i] : false;
}
//------------------------------------------------------------------------------
bool CvResourceInfo::isFeatureTerrain(int i) const
{
	ASSERT_DEBUG(i < GC.getNumTerrainInfos(), "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_pbFeatureTerrain ?	m_pbFeatureTerrain[i] : false;
}
//------------------------------------------------------------------------------
bool CvResourceInfo::CacheResults(Database::Results& kResults, CvDatabaseUtility& kUtility)
{
	if(!CvBaseInfo::CacheResults(kResults, kUtility))
		return false;

	//Basic properties
	m_iStartingResourceQuantity = kResults.GetInt("StartingResourceQuantity");
	m_iHappiness = kResults.GetInt("Happiness");
	m_iWonderProductionMod = kResults.GetInt("WonderProductionMod");

	const char* szEraType = kResults.GetText("WonderProductionModObsoleteEra");
	m_eWonderProductionModObsoleteEra = (EraTypes)GC.getInfoTypeForString(szEraType, true);

	m_iMinAreaSize = kResults.GetInt("MinAreaSize");
	m_iMinLatitude = kResults.GetInt("MinLatitude");
	m_iMaxLatitude = kResults.GetInt("MaxLatitude");

#if defined(MOD_BALANCE_CORE_RESOURCE_MONOPOLIES)
	m_iMonopolyHappiness = kResults.GetInt("MonopolyHappiness");
	m_iMonopolyGALength = kResults.GetInt("MonopolyGALength");
	m_iMonopolyAttackBonus = kResults.GetInt("MonopolyAttackBonus");
	m_iMonopolyDefenseBonus = kResults.GetInt("MonopolyDefenseBonus");
	m_iMonopolyMovementBonus = kResults.GetInt("MonopolyMovementBonus");
	m_iMonopolyHealBonus = kResults.GetInt("MonopolyHealBonus");
	m_iMonopolyXPBonus = kResults.GetInt("MonopolyXPBonus");
	m_bIsMonopoly = kResults.GetBool("IsMonopoly");
#endif
	m_bPresentOnAllValidPlots = kResults.GetBool("PresentOnAllValidPlots");
	m_bOneArea = kResults.GetBool("Area");
	m_bHills = kResults.GetBool("Hills");
	m_bFlatlands = kResults.GetBool("Flatlands");
	m_bNoRiverSide = kResults.GetBool("NoRiverSide");
	m_bOnlyMinorCivs = kResults.GetBool("OnlyMinorCivs");

	const char* szCivilizationType = kResults.GetText("CivilizationType");
	m_eRequiredCivilization = (CivilizationTypes)GC.getInfoTypeForString(szCivilizationType, true);

	m_eResourceUsage   = (ResourceUsageTypes)kResults.GetInt("ResourceUsage");

	//Basic references
	const char* szResourceClassType = kResults.GetText("ResourceClassType");
	m_iResourceClassType = GC.getInfoTypeForString(szResourceClassType, true);

	const char* szIconString = kResults.GetText("IconString");
	SetIconString(szIconString);

	const char* szArtDefineTag = kResults.GetText("ArtDefineTag");
	setArtDefineTag(szArtDefineTag);

	const char* szArtDefineTagHeavy = kResults.GetText("ArtDefineTagHeavy");
	setArtDefineTagHeavy(szArtDefineTagHeavy);

	const char* szAltArtDefineTag = kResults.GetText("AltArtDefineTag");
	setAltArtDefineTag(szAltArtDefineTag);

	const char* szAltArtDefineTagHeavy = kResults.GetText("AltArtDefineTagHeavy");
	setAltArtDefineTagHeavy(szAltArtDefineTagHeavy);

	const char* szTechReveal = kResults.GetText("TechReveal");
	m_iTechReveal = GC.getInfoTypeForString(szTechReveal, true);

	const char* szPolicyReveal = kResults.GetText("PolicyReveal");
	m_iPolicyReveal = GC.getInfoTypeForString(szPolicyReveal, true);

	const char* szTechCityTrade = kResults.GetText("TechCityTrade");
	m_iTechCityTrade = GC.getInfoTypeForString(szTechCityTrade, true);

	const char* szTechImproveable = kResults.GetText("TechImproveable");
	m_iTechImproveable = GC.getInfoTypeForString(szTechImproveable, true);

	const char* szTechObsolete = kResults.GetText("TechObsolete");
	m_iTechObsolete = GC.getInfoTypeForString(szTechObsolete, true);

	const char* szAIStopTradingEra = kResults.GetText("AIStopTradingEra");
	m_iAIStopTradingEra = GC.getInfoTypeForString(szAIStopTradingEra, true);

	//Arrays
	const char* szResourceType = GetType();
	kUtility.SetYields(m_piYieldChange, "Resource_YieldChanges", "ResourceType", szResourceType);
#if defined(MOD_BALANCE_CORE_RESOURCE_MONOPOLIES)
	kUtility.SetYields(m_piYieldChangeFromMonopoly, "Resource_YieldChangeFromMonopoly", "ResourceType", szResourceType);
	kUtility.SetYields(m_piCityYieldModFromMonopoly, "Resource_CityYieldModFromMonopoly", "ResourceType", szResourceType);
#endif

	kUtility.PopulateArrayByExistence(m_pbTerrain, "Terrains", "Resource_TerrainBooleans", "TerrainType", "ResourceType", szResourceType);
	kUtility.PopulateArrayByExistence(m_pbFeature, "Features", "Resource_FeatureBooleans", "FeatureType", "ResourceType", szResourceType);
	kUtility.PopulateArrayByExistence(m_pbFeatureTerrain, "Terrains", "Resource_FeatureTerrainBooleans", "TerrainType", "ResourceType", szResourceType);

	//Resource_QuantityTypes
	{
		const int iNumQuantityTypes = /*4*/ GD_INT_GET(NUM_RESOURCE_QUANTITY_TYPES);
		kUtility.InitializeArray(m_piResourceQuantityTypes, iNumQuantityTypes);

		//Default it to 1
		m_piResourceQuantityTypes[0] = 1;

		Database::Results kArrayResults;
		char szQuery[512];
		sprintf_s(szQuery, "select Quantity from Resource_QuantityTypes where ResourceType = '%s';", szResourceType);

		if(DB.Execute(kArrayResults, szQuery))
		{
			int i = 0;
			while(kArrayResults.Step())
			{
				ASSERT_DEBUG(i < iNumQuantityTypes, "Too many resource quantities.");
				const int quantity = kArrayResults.GetInt(0);
				m_piResourceQuantityTypes[i++] = quantity;
			}
		}

	}

#if defined(MOD_BALANCE_CORE_RESOURCE_MONOPOLIES)
	//Resource_MonopolyCombatModifiers
	{

		std::string sqlKey = "Resource_MonopolyCombatModifiers";
		Database::Results* pResults = kUtility.GetResults(sqlKey);
		if (pResults == NULL)
		{
			const char* szSQL = "select IsGlobalMonopoly, IsStrategicMonopoly, Attack, Defense from Resource_MonopolyCombatModifiers where ResourceType = ?";
			pResults = kUtility.PrepareResults(sqlKey, szSQL);
		}

		pResults->Bind(1, szResourceType);

		while (pResults->Step())
		{
			const bool bGlobalMonopoly = pResults->GetBool(0);
			const bool bStrategicMonopoly = pResults->GetBool(1);
			const int iAttackMod = pResults->GetInt(2);
			const int iDefenseMod = pResults->GetInt(3);

			ResourceMonopolySettings sKey;
			sKey.m_bGlobalMonopoly = bGlobalMonopoly;
			sKey.m_bStrategicMonopoly = bStrategicMonopoly;

			m_piiMonopolyCombatModifiers[sKey].m_iAttackMod += iAttackMod;
			m_piiMonopolyCombatModifiers[sKey].m_iDefenseMod += iDefenseMod;
		}

		pResults->Reset();

		//Trim extra memory off container since this is mostly read-only.
		std::map<ResourceMonopolySettings, CombatModifiers>(m_piiMonopolyCombatModifiers).swap(m_piiMonopolyCombatModifiers);
	}

	//Resource_MonopolyGreatPersonRateModifiers
	{

		std::string sqlKey = "Resource_MonopolyGreatPersonRateModifiers";
		Database::Results* pResults = kUtility.GetResults(sqlKey);
		if (pResults == NULL)
		{
			const char* szSQL = "select Specialists.ID as SpecialistID, IsGlobalMonopoly, IsStrategicMonopoly, Modifier from Resource_MonopolyGreatPersonRateModifiers inner join Specialists on Specialists.Type = SpecialistType where ResourceType = ?";
			pResults = kUtility.PrepareResults(sqlKey, szSQL);
		}

		pResults->Bind(1, szResourceType);

		while (pResults->Step())
		{
			const int iSpecialist = pResults->GetInt(0);
			const bool bGlobalMonopoly = pResults->GetBool(1);
			const bool bStrategicMonopoly = pResults->GetBool(2);
			const int iModifier = pResults->GetInt(3);

			MonopolyGreatPersonRateModifierKey sKey;
			sKey.m_iSpecialist = iSpecialist;
			sKey.m_sMonopoly.m_bGlobalMonopoly = bGlobalMonopoly;
			sKey.m_sMonopoly.m_bStrategicMonopoly = bStrategicMonopoly;

			m_piMonopolyGreatPersonRateModifiers[sKey] += iModifier;
		}

		pResults->Reset();

		//Trim extra memory off container since this is mostly read-only.
		std::map<MonopolyGreatPersonRateModifierKey, int>(m_piMonopolyGreatPersonRateModifiers).swap(m_piMonopolyGreatPersonRateModifiers);
	}

	//Resource_MonopolyGreatPersonRateChanges
	{

		std::string sqlKey = "Resource_MonopolyGreatPersonRateChanges";
		Database::Results* pResults = kUtility.GetResults(sqlKey);
		if (pResults == NULL)
		{
			const char* szSQL = "select Specialists.ID as SpecialistID, IsGlobalMonopoly, IsStrategicMonopoly, Value from Resource_MonopolyGreatPersonRateChanges inner join Specialists on Specialists.Type = SpecialistType where ResourceType = ?";
			pResults = kUtility.PrepareResults(sqlKey, szSQL);
		}

		pResults->Bind(1, szResourceType);

		while (pResults->Step())
		{
			const int iSpecialist = pResults->GetInt(0);
			const bool bGlobalMonopoly = pResults->GetBool(1);
			const bool bStrategicMonopoly = pResults->GetBool(2);
			const int iValue = pResults->GetInt(3);

			MonopolyGreatPersonRateModifierKey sKey;
			sKey.m_iSpecialist = iSpecialist;
			sKey.m_sMonopoly.m_bGlobalMonopoly = bGlobalMonopoly;
			sKey.m_sMonopoly.m_bStrategicMonopoly = bStrategicMonopoly;

			m_piMonopolyGreatPersonRateChanges[sKey] += iValue;
		}

		pResults->Reset();

		//Trim extra memory off container since this is mostly read-only.
		std::map<MonopolyGreatPersonRateModifierKey, int>(m_piMonopolyGreatPersonRateChanges).swap(m_piMonopolyGreatPersonRateChanges);
	}
#endif

#if defined(MOD_RESOURCES_PRODUCTION_COST_MODIFIERS)
	//Resource_UnitCombatProductionCostModifiersLocal
	{

		std::string sqlKey = "Resource_UnitCombatProductionCostModifiersLocal";
		Database::Results* pResults = kUtility.GetResults(sqlKey);
		if (pResults == NULL)
		{
			const char* szSQL = "select UnitCombatInfos.ID as UnitCombatInfosID, RequiredEra, ObsoleteEra, CostModifier from Resource_UnitCombatProductionCostModifiersLocal inner join UnitCombatInfos on UnitCombatType = UnitCombatInfos.Type where ResourceType = ?";
			pResults = kUtility.PrepareResults(sqlKey, szSQL);
		}

		pResults->Bind(1, szResourceType);

		while (pResults->Step())
		{
			const int iUnitCombat = pResults->GetInt(0);
			const int iRequiredEra = GC.getInfoTypeForString(pResults->GetText(1), true);
			const int iObsoleteEra = GC.getInfoTypeForString(pResults->GetText(2), true);
			const int iCostMod = pResults->GetInt(3);

			ProductionCostModifiers sElement;
			sElement.m_iRequiredEra = iRequiredEra;
			sElement.m_iObsoleteEra = iObsoleteEra;
			sElement.m_iCostModifier = iCostMod;

			m_piiiUnitCombatProductionCostModifiersLocal[iUnitCombat].push_back(sElement);
		}

		pResults->Reset();

		//Trim extra memory off container since this is mostly read-only.
		std::map<int, std::vector<ProductionCostModifiers>>(m_piiiUnitCombatProductionCostModifiersLocal).swap(m_piiiUnitCombatProductionCostModifiersLocal);
	}

	//Resource_BuildingProductionCostModifiersLocal
		{

			std::string sqlKey = "Resource_BuildingProductionCostModifiersLocal";
			Database::Results* pResults = kUtility.GetResults(sqlKey);
			if (pResults == NULL)
			{
				const char* szSQL = "select RequiredEra, ObsoleteEra, CostModifier from Resource_BuildingProductionCostModifiersLocal where ResourceType = ?";
				pResults = kUtility.PrepareResults(sqlKey, szSQL);
			}

			pResults->Bind(1, szResourceType);

			while (pResults->Step())
			{
				const int iRequiredEra = GC.getInfoTypeForString(pResults->GetText(0), true);
				const int iObsoleteEra = GC.getInfoTypeForString(pResults->GetText(1), true);
				const int iCostMod = pResults->GetInt(2);

				ProductionCostModifiers sElement;
				sElement.m_iRequiredEra = iRequiredEra;
				sElement.m_iObsoleteEra = iObsoleteEra;
				sElement.m_iCostModifier = iCostMod;

				m_aiiiBuildingProductionCostModifiersLocal.push_back(sElement);
			}

			pResults->Reset();
		}
#endif


	return true;
}

//======================================================================================================
//					CvFeatureInfo
//======================================================================================================
CvFeatureInfo::CvFeatureInfo() :
	m_iStartingLocationWeight(0),
	m_iMovementCost(0),
	m_iSeeThroughChange(0),
	m_iAppearanceProbability(0),
	m_iDisappearanceProbability(0),
	m_iGrowthProbability(0),
	m_iGrowthTerrainType(-1),
	m_iDefenseModifier(0),
	m_iInfluenceCost(0),
	m_iTurnDamage(0),
	m_iExtraTurnDamage(0),
	m_iFirstFinderGold(0),
	m_iInBorderHappiness(0),
	m_iAdjacentUnitFreePromotion(NO_PROMOTION),
#if defined(MOD_BALANCE_CORE)
	m_iPrereqTechPassable(NO_TECH),
	m_iPromotionIfOwned(NO_PROMOTION),
	m_iLocationUnitFreePromotion(NO_PROMOTION),
	m_iSpawnLocationUnitFreePromotion(NO_PROMOTION),
	m_iAdjacentSpawnLocationUnitFreePromotion(NO_PROMOTION),
#endif
	m_bYieldNotAdditive(false),
	m_bNoCoast(false),
	m_bNoRiver(false),
	m_bNoAdjacent(false),
	m_bRequiresFlatlands(false),
	m_bRequiresRiver(false),
	m_bAddsFreshWater(false),
	m_bImpassable(false),
	m_bNoCity(false),
	m_bNoImprovement(false),
	m_bVisibleAlways(false),
	m_bNukeImmune(false),
	m_bRough(false),
	m_bNaturalWonder(false),
#if defined(MOD_PSEUDO_NATURAL_WONDER)
	m_bPseudoNaturalWonder(false),
#endif
	m_iWorldSoundscapeScriptId(0),
	m_iEffectProbability(0),
	m_piYieldChange(NULL),
	m_piRiverYieldChange(NULL),
	m_piHillsYieldChange(NULL),
	m_piCoastalLandYieldChange(NULL),
	m_piFreshWaterChange(NULL),
	m_ppiTechYieldChanges(NULL),
	m_pi3DAudioScriptFootstepIndex(NULL),
	m_pbTerrain(NULL),
	m_bClearable(false)
{
}
//------------------------------------------------------------------------------
CvFeatureInfo::~CvFeatureInfo()
{
	SAFE_DELETE_ARRAY(m_piYieldChange);
	SAFE_DELETE_ARRAY(m_piRiverYieldChange);
	SAFE_DELETE_ARRAY(m_piHillsYieldChange);
	SAFE_DELETE_ARRAY(m_piCoastalLandYieldChange);
	SAFE_DELETE_ARRAY(m_piFreshWaterChange);
	if(m_ppiTechYieldChanges != NULL)
	{
		CvDatabaseUtility::SafeDelete2DArray(m_ppiTechYieldChanges);
	}
	SAFE_DELETE_ARRAY(m_piEraYieldChange);
	SAFE_DELETE_ARRAY(m_pi3DAudioScriptFootstepIndex);
	SAFE_DELETE_ARRAY(m_pbTerrain);
}
//------------------------------------------------------------------------------
int CvFeatureInfo::getStartingLocationWeight() const
{
	return m_iStartingLocationWeight;
}
//------------------------------------------------------------------------------
int CvFeatureInfo::getMovementCost() const
{
	return m_iMovementCost;
}
//------------------------------------------------------------------------------
int CvFeatureInfo::getSeeThroughChange() const
{
	return m_iSeeThroughChange;
}
//------------------------------------------------------------------------------
int CvFeatureInfo::getAppearanceProbability() const
{
	return m_iAppearanceProbability;
}
//------------------------------------------------------------------------------
int CvFeatureInfo::getDisappearanceProbability() const
{
	return m_iDisappearanceProbability;
}
//------------------------------------------------------------------------------
int CvFeatureInfo::getGrowthProbability() const
{
	return m_iGrowthProbability;
}
//------------------------------------------------------------------------------
int CvFeatureInfo::getGrowthTerrainType() const
{
	return m_iGrowthTerrainType;
}
//------------------------------------------------------------------------------
int CvFeatureInfo::getDefenseModifier() const
{
	return m_iDefenseModifier;
}
//------------------------------------------------------------------------------
int CvFeatureInfo::getInfluenceCost() const
{
	return m_iInfluenceCost;
}
//------------------------------------------------------------------------------
int CvFeatureInfo::getTurnDamage() const
{
	return m_iTurnDamage;
}
//------------------------------------------------------------------------------
int CvFeatureInfo::getExtraTurnDamage() const
{
	return m_iExtraTurnDamage;
}
//------------------------------------------------------------------------------
int CvFeatureInfo::getFirstFinderGold() const
{
	return m_iFirstFinderGold;
}
//------------------------------------------------------------------------------
int CvFeatureInfo::getInBorderHappiness() const
{
	return m_iInBorderHappiness;
}
//------------------------------------------------------------------------------
int CvFeatureInfo::getAdjacentUnitFreePromotion() const
{
	return m_iAdjacentUnitFreePromotion;
}
#if defined(MOD_BALANCE_CORE)
int CvFeatureInfo::getPromotionIfOwned() const
{
	return m_iPromotionIfOwned;
}
int CvFeatureInfo::getLocationUnitFreePromotion() const
{
	return m_iLocationUnitFreePromotion;
}
int CvFeatureInfo::getSpawnLocationUnitFreePromotion() const
{
	return m_iSpawnLocationUnitFreePromotion;
}
int CvFeatureInfo::getAdjacentSpawnLocationUnitFreePromotion() const
{
	return m_iAdjacentSpawnLocationUnitFreePromotion;
}
#endif
//------------------------------------------------------------------------------
bool CvFeatureInfo::isYieldNotAdditive() const
{
	return m_bYieldNotAdditive;
}
//------------------------------------------------------------------------------
bool CvFeatureInfo::isNoCoast() const
{
	return m_bNoCoast;
}
//------------------------------------------------------------------------------
bool CvFeatureInfo::isNoRiver() const
{
	return m_bNoRiver;
}
//------------------------------------------------------------------------------
bool CvFeatureInfo::isNoAdjacent() const
{
	return m_bNoAdjacent;
}
//------------------------------------------------------------------------------
bool CvFeatureInfo::isRequiresFlatlands() const
{
	return m_bRequiresFlatlands;
}
//------------------------------------------------------------------------------
bool CvFeatureInfo::isRequiresRiver() const
{
	return m_bRequiresRiver;
}
//------------------------------------------------------------------------------
bool CvFeatureInfo::isAddsFreshWater() const
{
	return m_bAddsFreshWater;
}
//------------------------------------------------------------------------------
bool CvFeatureInfo::isImpassable() const
{
	return m_bImpassable;
}
#if defined(MOD_BALANCE_CORE)
/// Techs required for this feature to become passable
int CvFeatureInfo::GetPrereqPassable() const
{
	return m_iPrereqTechPassable;
}
#endif
//------------------------------------------------------------------------------
bool CvFeatureInfo::isNoCity() const
{
	return m_bNoCity;
}
//------------------------------------------------------------------------------
bool CvFeatureInfo::isNoImprovement() const
{
	return m_bNoImprovement;
}
//------------------------------------------------------------------------------
bool CvFeatureInfo::isVisibleAlways() const
{
	return m_bVisibleAlways;
}
//------------------------------------------------------------------------------
bool CvFeatureInfo::isNukeImmune() const
{
	return m_bNukeImmune;
}
//------------------------------------------------------------------------------
bool CvFeatureInfo::IsRough() const
{
	return m_bRough;
}
//------------------------------------------------------------------------------
bool CvFeatureInfo::IsNaturalWonder(bool orPseudoNatural) const
{
	return m_bNaturalWonder || (orPseudoNatural && IsPseudoNaturalWonder());
}
//------------------------------------------------------------------------------
bool CvFeatureInfo::IsPseudoNaturalWonder() const
{
	return m_bPseudoNaturalWonder;
}
//------------------------------------------------------------------------------
const char* CvFeatureInfo::getArtDefineTag() const
{
	return m_strArtDefineTag;
}
//------------------------------------------------------------------------------
void CvFeatureInfo::setArtDefineTag(const char* szTag)
{
	m_strArtDefineTag = szTag;
}
//------------------------------------------------------------------------------
int CvFeatureInfo::getWorldSoundscapeScriptId() const
{
	return m_iWorldSoundscapeScriptId;
}
//------------------------------------------------------------------------------
const char* CvFeatureInfo::getEffectType() const
{
	return m_strEffectType;
}
//------------------------------------------------------------------------------
int CvFeatureInfo::getEffectProbability() const
{
	return m_iEffectProbability;
}
//------------------------------------------------------------------------------
int CvFeatureInfo::getYieldChange(int i) const
{
	ASSERT_DEBUG(i < NUM_YIELD_TYPES, "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_piYieldChange ? m_piYieldChange[i] : -1;
}
//------------------------------------------------------------------------------
int CvFeatureInfo::getRiverYieldChange(int i) const
{
	ASSERT_DEBUG(i < NUM_YIELD_TYPES, "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_piRiverYieldChange ? m_piRiverYieldChange[i] : -1;
}
//------------------------------------------------------------------------------
int CvFeatureInfo::getHillsYieldChange(int i) const
{
	ASSERT_DEBUG(i < NUM_YIELD_TYPES, "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_piHillsYieldChange ? m_piHillsYieldChange[i] : -1;
}
//------------------------------------------------------------------------------
int CvFeatureInfo::getCoastalLandYieldChange(int i) const
{
	ASSERT_DEBUG(i < NUM_YIELD_TYPES, "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_piCoastalLandYieldChange ? m_piCoastalLandYieldChange[i] : -1;
}
//------------------------------------------------------------------------------
int CvFeatureInfo::getFreshWaterYieldChange(int i) const
{
	ASSERT_DEBUG(i < NUM_YIELD_TYPES, "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_piFreshWaterChange ? m_piFreshWaterChange[i] : -1;
}
//------------------------------------------------------------------------------
int CvFeatureInfo::GetTechYieldChanges(int i, int j) const
{
	ASSERT_DEBUG(i < GC.getNumTechInfos(), "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	ASSERT_DEBUG(j < NUM_YIELD_TYPES, "Index out of bounds");
	ASSERT_DEBUG(j > -1, "Index out of bounds");
	return m_ppiTechYieldChanges[i][j];
}
//------------------------------------------------------------------------------
int CvFeatureInfo::GetEraYieldChanges(int i) const
{
	ASSERT_DEBUG(i < NUM_YIELD_TYPES, "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_piEraYieldChange ? m_piEraYieldChange[i] : -1;
}
//------------------------------------------------------------------------------
int CvFeatureInfo::get3DAudioScriptFootstepIndex(int i) const
{
	//	ASSERT_DEBUG(i < ?, "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_pi3DAudioScriptFootstepIndex ? m_pi3DAudioScriptFootstepIndex[i] : -1;
}
//------------------------------------------------------------------------------
bool CvFeatureInfo::isTerrain(int i) const
{
	ASSERT_DEBUG(i < GC.getNumTerrainInfos(), "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_pbTerrain ? m_pbTerrain[i] : false;
}

// Set each time the game is started
bool CvFeatureInfo::IsClearable() const
{
	return m_bClearable;
}
// Set each time the game is started
void CvFeatureInfo::SetClearable(bool bValue)
{
	m_bClearable = bValue;
}

//------------------------------------------------------------------------------
const char* CvFeatureInfo::getEffectTypeTag() const
{
	return m_strEffectTypeTag;
}

//------------------------------------------------------------------------------
bool CvFeatureInfo::CacheResults(Database::Results& kResults, CvDatabaseUtility& kUtility)
{
	if(!CvBaseInfo::CacheResults(kResults, kUtility))
		return false;

	const char* szArtDefineTag = kResults.GetText("ArtDefineTag");
	setArtDefineTag(szArtDefineTag);

	// Basic properties
	m_iStartingLocationWeight = kResults.GetInt("StartingLocationWeight");
	m_iMovementCost = kResults.GetInt("Movement");
	m_iSeeThroughChange = kResults.GetInt("SeeThrough");
	m_iDefenseModifier = kResults.GetInt("Defense");
	m_iInfluenceCost = kResults.GetInt("InfluenceCost");
	m_iTurnDamage = kResults.GetInt("TurnDamage");
	m_iExtraTurnDamage = kResults.GetInt("ExtraTurnDamage");
	m_iAppearanceProbability = kResults.GetInt("AppearanceProbability");
	m_iDisappearanceProbability = kResults.GetInt("DisappearanceProbability");
	m_iGrowthProbability = kResults.GetInt("Growth");
	m_iFirstFinderGold = kResults.GetInt("FirstFinderGold");
	m_iInBorderHappiness = kResults.GetInt("InBorderHappiness");

	const char* szTextVal = NULL;
	szTextVal = kResults.GetText("AdjacentUnitFreePromotion");
	m_iAdjacentUnitFreePromotion = GC.getInfoTypeForString(szTextVal, true);

#if defined(MOD_BALANCE_CORE)
	szTextVal = kResults.GetText("PassableTechFeature");
	m_iPrereqTechPassable = GC.getInfoTypeForString(szTextVal, true);

	szTextVal = kResults.GetText("FreePromotionIfOwned");
	m_iPromotionIfOwned = GC.getInfoTypeForString(szTextVal, true);

	szTextVal = kResults.GetText("LocationUnitFreePromotion");
	m_iLocationUnitFreePromotion = GC.getInfoTypeForString(szTextVal, true);
	
	szTextVal = kResults.GetText("SpawnLocationUnitFreePromotion");
	m_iSpawnLocationUnitFreePromotion = GC.getInfoTypeForString(szTextVal, true);

	szTextVal = kResults.GetText("AdjacentSpawnLocationUnitFreePromotion");
	m_iAdjacentSpawnLocationUnitFreePromotion = GC.getInfoTypeForString(szTextVal, true);
#endif

	const char* szTerrainType = kResults.GetText("GrowthTerrainType");
	if(szTerrainType != NULL)
	{
		m_iGrowthTerrainType = GC.getInfoTypeForString(szTerrainType, true);
	}

	m_iEffectProbability = kResults.GetInt("EffectProbability");
	m_bYieldNotAdditive = kResults.GetBool("YieldNotAdditive");
	m_bNoCoast = kResults.GetBool("NoCoast");
	m_bNoRiver = kResults.GetBool("NoRiver");
	m_bNoAdjacent = kResults.GetBool("NoAdjacent");
	m_bRequiresFlatlands = kResults.GetBool("RequiresFlatlands");
	m_bRequiresRiver = kResults.GetBool("RequiresRiver");
	m_bAddsFreshWater = kResults.GetBool("AddsFreshWater");
	m_bImpassable = kResults.GetBool("Impassable");
	m_bNoCity = kResults.GetBool("NoCity");
	m_bNoImprovement = kResults.GetBool("NoImprovement");
	m_bVisibleAlways = kResults.GetBool("VisibleAlways");
	m_bNukeImmune = kResults.GetBool("NukeImmune");
	m_bRough = kResults.GetBool("Rough");
	m_bNaturalWonder = kResults.GetBool("NaturalWonder");
#if defined(MOD_PSEUDO_NATURAL_WONDER)
	m_bPseudoNaturalWonder = kResults.GetBool("PseudoNaturalWonder");
#endif
	m_strEffectType = kResults.GetText("EffectType");
	m_strEffectTypeTag = kResults.GetText("EffectTypeTag");

	const char* szWorldsoundscapeAudioScript = kResults.GetText("WorldSoundscapeAudioScript");
	if(szWorldsoundscapeAudioScript != NULL)
	{
		m_iWorldSoundscapeScriptId = gDLL->GetAudioTagIndex(szWorldsoundscapeAudioScript, AUDIOTAG_SOUNDSCAPE);
	}
	else
	{
		m_iWorldSoundscapeScriptId = -1;
	}

	// Array properties
	const char* szFeatureType = GetType();
	kUtility.SetYields(m_piYieldChange, "Feature_YieldChanges", "FeatureType", szFeatureType);
	kUtility.SetYields(m_piRiverYieldChange, "Feature_RiverYieldChanges", "FeatureType", szFeatureType);
	kUtility.SetYields(m_piHillsYieldChange, "Feature_HillsYieldChanges", "FeatureType", szFeatureType);
	kUtility.SetYields(m_piEraYieldChange, "Feature_EraYieldChanges", "FeatureType", szFeatureType);
	kUtility.SetYields(m_piCoastalLandYieldChange, "Feature_CoastalLandYields", "FeatureType", szFeatureType);
	kUtility.SetYields(m_piFreshWaterChange, "Feature_FreshWaterYields", "FeatureType", szFeatureType);

	const int iNumYields = kUtility.MaxRows("Yields");
	const int iNumTechs = GC.getNumTechInfos();
	ASSERT_DEBUG(iNumTechs > 0, "Num Tech Infos <= 0");

	//TechYieldChanges
	{
		kUtility.Initialize2DArray(m_ppiTechYieldChanges, iNumTechs, iNumYields);

		std::string strKey = "Features - TechYieldChanges";
		Database::Results* pResults = kUtility.GetResults(strKey);
		if(pResults == NULL)
		{
			pResults = kUtility.PrepareResults(strKey, "select Yields.ID as YieldID, Technologies.ID as TechID, Yield from Feature_TechYieldChanges inner join Yields on YieldType = Yields.Type inner join Technologies on TechType = Technologies.Type where FeatureType = ?");
		}

		pResults->Bind(1, szFeatureType, strlen(szFeatureType), false);

		while(pResults->Step())
		{
			const int yield_idx = pResults->GetInt(0);
			ASSERT_DEBUG(yield_idx > -1);

			const int tech_idx = pResults->GetInt(1);
			ASSERT_DEBUG(tech_idx > -1);

			const int yield = pResults->GetInt(2);

			m_ppiTechYieldChanges[tech_idx][yield_idx] = yield;
		}
	}

	kUtility.PopulateArrayByExistence(m_pbTerrain, "Terrains", "Feature_TerrainBooleans", "TerrainType", "FeatureType", szFeatureType);

	// Determine of this feature is clearable - set each time the game is started
	m_bClearable = false;

	return true;
}

//======================================================================================================
//					CvYieldInfo
//======================================================================================================
CvYieldInfo::CvYieldInfo() :
	m_strIconString(""),
	m_strColorString(""),
	m_iHillsChange(0),
	m_iMountainChange(0),
	m_iLakeChange(0),
	m_iCityChange(0),
	m_iPopulationChangeOffset(0),
	m_iPopulationChangeDivisor(0),
	m_iMinCity(0),
#if defined(MOD_BALANCE_CORE)
	m_iMinCityFlatFreshWater(0),
	m_iMinCityFlatNoFreshWater(0),
	m_iMinCityHillFreshWater(0),
	m_iMinCityHillNoFreshWater(0),
	m_iMinCityMountainFreshWater(0),
	m_iMinCityMountainNoFreshWater(0),
#endif
	m_iGoldenAgeYield(0),
	m_iGoldenAgeYieldThreshold(0),
	m_iGoldenAgeYieldMod(0)
{
}
//------------------------------------------------------------------------------
const char* CvYieldInfo::getIconString() const
{
	return m_strIconString;
}
//------------------------------------------------------------------------------
const char* CvYieldInfo::getColorString() const
{
	return m_strColorString;
}
//------------------------------------------------------------------------------
int CvYieldInfo::getHillsChange() const
{
	return m_iHillsChange;
}
//------------------------------------------------------------------------------
int CvYieldInfo::getMountainChange() const
{
	return m_iMountainChange;
}
//------------------------------------------------------------------------------
int CvYieldInfo::getLakeChange() const
{
	return m_iLakeChange;
}
//------------------------------------------------------------------------------
int CvYieldInfo::getCityChange() const
{
	return m_iCityChange;
}
//------------------------------------------------------------------------------
int CvYieldInfo::getPopulationChangeOffset() const
{
	return m_iPopulationChangeOffset;
}
//------------------------------------------------------------------------------
int CvYieldInfo::getPopulationChangeDivisor() const
{
	return m_iPopulationChangeDivisor;
}
//------------------------------------------------------------------------------
int CvYieldInfo::getMinCity() const
{
	return m_iMinCity;
}
#if defined (MOD_BALANCE_CORE)
//------------------------------------------------------------------------------
int CvYieldInfo::getMinCityFlatFreshWater() const
{
	return m_iMinCityFlatFreshWater;
}
//------------------------------------------------------------------------------
int CvYieldInfo::getMinCityFlatNoFreshWater() const
{
	return m_iMinCityFlatNoFreshWater;
}
//------------------------------------------------------------------------------
int CvYieldInfo::getMinCityHillFreshWater() const
{
	return m_iMinCityHillFreshWater;
}
//------------------------------------------------------------------------------
int CvYieldInfo::getMinCityHillNoFreshWater() const
{
	return m_iMinCityHillNoFreshWater;
}
//------------------------------------------------------------------------------
int CvYieldInfo::getMinCityMountainFreshWater() const
{
	return m_iMinCityMountainFreshWater;
}
//------------------------------------------------------------------------------
int CvYieldInfo::getMinCityMountainNoFreshWater() const
{
	return m_iMinCityMountainNoFreshWater;
}
#endif
//------------------------------------------------------------------------------
int CvYieldInfo::getGoldenAgeYield() const
{
	return m_iGoldenAgeYield;
}
//------------------------------------------------------------------------------
int CvYieldInfo::getGoldenAgeYieldThreshold() const
{
	return m_iGoldenAgeYieldThreshold;
}
//------------------------------------------------------------------------------
int CvYieldInfo::getGoldenAgeYieldMod() const
{
	return m_iGoldenAgeYieldMod;
}
//------------------------------------------------------------------------------
bool CvYieldInfo::CacheResults(Database::Results& kResults, CvDatabaseUtility& kUtility)
{
	if(!CvBaseInfo::CacheResults(kResults, kUtility))
		return false;

	m_strIconString = kResults.GetText("IconString");
	m_strColorString = kResults.GetText("ColorString");
	kResults.GetValue("HillsChange", m_iHillsChange);
	kResults.GetValue("MountainChange", m_iMountainChange);
	kResults.GetValue("LakeChange", m_iLakeChange);
	kResults.GetValue("CityChange", m_iCityChange);
	kResults.GetValue("PopulationChangeOffset", m_iPopulationChangeOffset);
	kResults.GetValue("PopulationChangeDivisor", m_iPopulationChangeDivisor);
	kResults.GetValue("MinCity", m_iMinCity);
#if defined(MOD_BALANCE_CORE)
	kResults.GetValue("MinCityFlatFreshWater", m_iMinCityFlatFreshWater);
	kResults.GetValue("MinCityFlatNoFreshWater", m_iMinCityFlatNoFreshWater);
	kResults.GetValue("MinCityHillFreshWater", m_iMinCityHillFreshWater);
	kResults.GetValue("MinCityHillNoFreshWater", m_iMinCityHillNoFreshWater);
	kResults.GetValue("MinCityMountainFreshWater", m_iMinCityMountainFreshWater);
	kResults.GetValue("MinCityMountainNoFreshWater", m_iMinCityMountainNoFreshWater);
#endif
	kResults.GetValue("GoldenAgeYield", m_iGoldenAgeYield);
	kResults.GetValue("GoldenAgeYieldThreshold", m_iGoldenAgeYieldThreshold);
	kResults.GetValue("GoldenAgeYieldMod", m_iGoldenAgeYieldMod);

	return true;

}

//======================================================================================================
//					CvTerrainInfo
//======================================================================================================
CvTerrainInfo::CvTerrainInfo() :
	m_iMovementCost(0),
	m_iSeeFromLevel(0),
	m_iSeeThroughLevel(0),
	m_iBuildModifier(0),
	m_iDefenseModifier(0),
	m_iInfluenceCost(0),
	m_iTurnDamage(0),
	m_iExtraTurnDamage(0),
#if defined(MOD_BALANCE_CORE)
	m_iPrereqTechPassable(NO_TECH),
	m_iLocationUnitFreePromotionTerrain(NO_PROMOTION),
	m_iSpawnLocationUnitFreePromotionTerrain(NO_PROMOTION),
	m_iAdjacentSpawnLocationUnitFreePromotionTerrain(NO_PROMOTION),
	m_iAdjacentUnitFreePromotionTerrain(NO_PROMOTION),
#endif
	m_bWater(false),
	m_bImpassable(false),
	m_bFound(false),
	m_bFoundCoast(false),
	m_bFoundFreshWater(false),
	m_iWorldSoundscapeScriptId(0),
	m_piYields(NULL),
	m_piRiverYieldChange(NULL),
	m_piHillsYieldChange(NULL),
	m_piCoastalLandYieldChange(NULL),
	m_piFreshWaterChange(NULL),
	m_ppiTechYieldChanges(NULL),
	m_pi3DAudioScriptFootstepIndex(NULL)
{
}
//------------------------------------------------------------------------------
CvTerrainInfo::~CvTerrainInfo()
{
	SAFE_DELETE_ARRAY(m_piYields);
	SAFE_DELETE_ARRAY(m_piRiverYieldChange);
	SAFE_DELETE_ARRAY(m_piHillsYieldChange);
	SAFE_DELETE_ARRAY(m_piCoastalLandYieldChange);
	SAFE_DELETE_ARRAY(m_piFreshWaterChange);
	if (m_ppiTechYieldChanges != NULL)
	{
		CvDatabaseUtility::SafeDelete2DArray(m_ppiTechYieldChanges);
	}
	SAFE_DELETE_ARRAY(m_pi3DAudioScriptFootstepIndex);
}
//------------------------------------------------------------------------------
int CvTerrainInfo::getMovementCost() const
{
	return m_iMovementCost;
}
//------------------------------------------------------------------------------
int CvTerrainInfo::getSeeFromLevel() const
{
	return m_iSeeFromLevel;
}
//------------------------------------------------------------------------------
int CvTerrainInfo::getSeeThroughLevel() const
{
	return m_iSeeThroughLevel;
}
//------------------------------------------------------------------------------
int CvTerrainInfo::getBuildModifier() const
{
	return m_iBuildModifier;
}
//------------------------------------------------------------------------------
int CvTerrainInfo::getDefenseModifier() const
{
	return m_iDefenseModifier;
}
//------------------------------------------------------------------------------
int CvTerrainInfo::getInfluenceCost() const
{
	return m_iInfluenceCost;
}
//------------------------------------------------------------------------------
int CvTerrainInfo::getTurnDamage() const
{
	return m_iTurnDamage;
}
//------------------------------------------------------------------------------
int CvTerrainInfo::getExtraTurnDamage() const
{
	return m_iExtraTurnDamage;
}
#if defined(MOD_BALANCE_CORE)
int CvTerrainInfo::getLocationUnitFreePromotion() const
{
	return m_iLocationUnitFreePromotionTerrain;
}
int CvTerrainInfo::getSpawnLocationUnitFreePromotion() const
{
	return m_iSpawnLocationUnitFreePromotionTerrain;
}
int CvTerrainInfo::getAdjacentSpawnLocationUnitFreePromotion() const
{
	return m_iAdjacentSpawnLocationUnitFreePromotionTerrain;
}
//------------------------------------------------------------------------------
int CvTerrainInfo::getAdjacentUnitFreePromotion() const
{
	return m_iAdjacentUnitFreePromotionTerrain;
}
#endif
//------------------------------------------------------------------------------
bool CvTerrainInfo::isWater() const
{
	return m_bWater;
}
//------------------------------------------------------------------------------
bool CvTerrainInfo::isImpassable() const
{
	return m_bImpassable;
}
#if defined(MOD_BALANCE_CORE)
/// Techs required for this terrain to become passable
int CvTerrainInfo::GetPrereqPassable() const
{
	return m_iPrereqTechPassable;
}
#endif
//------------------------------------------------------------------------------
bool CvTerrainInfo::isFound() const
{
	return m_bFound;
}
//------------------------------------------------------------------------------
bool CvTerrainInfo::isFoundCoast() const
{
	return m_bFoundCoast;
}
//------------------------------------------------------------------------------
bool CvTerrainInfo::isFoundFreshWater() const
{
	return m_bFoundFreshWater;
}
//------------------------------------------------------------------------------
const char* CvTerrainInfo::getArtDefineTag() const
{
	return m_strArtDefineTag;
}
//------------------------------------------------------------------------------
void CvTerrainInfo::setArtDefineTag(const char* szTag)
{
	m_strArtDefineTag = szTag;
}
//------------------------------------------------------------------------------
int CvTerrainInfo::getWorldSoundscapeScriptId() const
{
	return m_iWorldSoundscapeScriptId;
}
//------------------------------------------------------------------------------
const char* CvTerrainInfo::getEffectTypeTag() const
{
	return m_strEffectTypeTag;
}
//------------------------------------------------------------------------------
int CvTerrainInfo::getYield(int i) const
{
	ASSERT_DEBUG(i < NUM_YIELD_TYPES, "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_piYields ? m_piYields[i] : -1;
}
//------------------------------------------------------------------------------
int CvTerrainInfo::getRiverYieldChange(int i) const
{
	ASSERT_DEBUG(i < NUM_YIELD_TYPES, "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_piRiverYieldChange ? m_piRiverYieldChange[i] : -1;
}
//------------------------------------------------------------------------------
int CvTerrainInfo::getHillsYieldChange(int i) const
{
	ASSERT_DEBUG(i < NUM_YIELD_TYPES, "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_piHillsYieldChange ? m_piHillsYieldChange[i] : -1;
}
//------------------------------------------------------------------------------
int CvTerrainInfo::getCoastalLandYieldChange(int i) const
{
	ASSERT_DEBUG(i < NUM_YIELD_TYPES, "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_piCoastalLandYieldChange ? m_piCoastalLandYieldChange[i] : -1;
}
//------------------------------------------------------------------------------
int CvTerrainInfo::getFreshWaterYieldChange(int i) const
{
	ASSERT_DEBUG(i < NUM_YIELD_TYPES, "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_piFreshWaterChange ? m_piFreshWaterChange[i] : -1;
}
//------------------------------------------------------------------------------
int CvTerrainInfo::GetTechYieldChanges(int i, int j) const
{
	ASSERT_DEBUG(i < GC.getNumTechInfos(), "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	ASSERT_DEBUG(j < NUM_YIELD_TYPES, "Index out of bounds");
	ASSERT_DEBUG(j > -1, "Index out of bounds");
	return m_ppiTechYieldChanges[i][j];
}
//------------------------------------------------------------------------------
int CvTerrainInfo::get3DAudioScriptFootstepIndex(int i) const
{
//	ASSERT_DEBUG(i < ?, "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_pi3DAudioScriptFootstepIndex ? m_pi3DAudioScriptFootstepIndex[i] : -1;
}
//------------------------------------------------------------------------------
bool CvTerrainInfo::CacheResults(Database::Results& kResults, CvDatabaseUtility& kUtility)
{
	if(!CvBaseInfo::CacheResults(kResults, kUtility))
		return false;

	setArtDefineTag(kResults.GetText("ArtDefineTag"));

	m_bWater = kResults.GetBool("Water");
	m_bImpassable = kResults.GetBool("Impassable");
	m_bFound = kResults.GetBool("Found");
	m_bFoundCoast = kResults.GetBool("FoundCoast");
	m_bFoundFreshWater = kResults.GetBool("FoundFreshWater");

	m_iMovementCost = kResults.GetInt("Movement");
	m_iSeeFromLevel = kResults.GetInt("SeeFrom");
	m_iSeeThroughLevel = kResults.GetInt("SeeThrough");
	m_iBuildModifier = kResults.GetInt("BuildModifier");
	m_iDefenseModifier = kResults.GetInt("Defense");
	m_iInfluenceCost = kResults.GetInt("InfluenceCost");
	m_iTurnDamage = kResults.GetInt("TurnDamage");
	m_iExtraTurnDamage = kResults.GetInt("ExtraTurnDamage");

	const char* szTextVal = kResults.GetText("WorldSoundscapeAudioScript");
	if(szTextVal != NULL)
	{
		m_iWorldSoundscapeScriptId = gDLL->GetAudioTagIndex(szTextVal, AUDIOTAG_SOUNDSCAPE);
	}
	else
	{
		m_iWorldSoundscapeScriptId = -1;
	}
#if defined(MOD_BALANCE_CORE)
	szTextVal = kResults.GetText("PassableTechTerrain");
	m_iPrereqTechPassable = GC.getInfoTypeForString(szTextVal, true);

	szTextVal = kResults.GetText("LocationUnitFreePromotion");
	m_iLocationUnitFreePromotionTerrain = GC.getInfoTypeForString(szTextVal, true);

	szTextVal = kResults.GetText("SpawnLocationUnitFreePromotion");
	m_iSpawnLocationUnitFreePromotionTerrain = GC.getInfoTypeForString(szTextVal, true);

	szTextVal = kResults.GetText("AdjacentSpawnLocationUnitFreePromotion");
	m_iAdjacentSpawnLocationUnitFreePromotionTerrain = GC.getInfoTypeForString(szTextVal, true);

	szTextVal = kResults.GetText("AdjacentUnitFreePromotion");
	m_iAdjacentUnitFreePromotionTerrain = GC.getInfoTypeForString(szTextVal, true);
#endif

	//Arrays
	const char* szTerrainType = GetType();
	kUtility.SetYields(m_piYields, "Terrain_Yields", "TerrainType", szTerrainType);
	kUtility.SetYields(m_piRiverYieldChange, "Terrain_RiverYieldChanges", "TerrainType", szTerrainType);
	kUtility.SetYields(m_piHillsYieldChange, "Terrain_HillsYieldChanges", "TerrainType", szTerrainType);
	kUtility.SetYields(m_piCoastalLandYieldChange, "Terrain_CoastalLandYields", "TerrainType", szTerrainType);
	kUtility.SetYields(m_piFreshWaterChange, "Terrain_FreshWaterYields", "TerrainType", szTerrainType);

	const int iNumYields = kUtility.MaxRows("Yields");
	const int iNumTechs = GC.getNumTechInfos();
	ASSERT_DEBUG(iNumTechs > 0, "Num Tech Infos <= 0");

	//TechYieldChanges
	{
		kUtility.Initialize2DArray(m_ppiTechYieldChanges, iNumTechs, iNumYields);

		std::string strKey = "Terrains - TechYieldChanges";
		Database::Results* pResults = kUtility.GetResults(strKey);
		if(pResults == NULL)
		{
			pResults = kUtility.PrepareResults(strKey, "select Yields.ID as YieldID, Technologies.ID as TechID, Yield from Terrain_TechYieldChanges inner join Yields on YieldType = Yields.Type inner join Technologies on TechType = Technologies.Type where TerrainType = ?");
		}

		pResults->Bind(1, szTerrainType, strlen(szTerrainType), false);

		while(pResults->Step())
		{
			const int yield_idx = pResults->GetInt(0);
			ASSERT_DEBUG(yield_idx > -1);

			const int tech_idx = pResults->GetInt(1);
			ASSERT_DEBUG(tech_idx > -1);

			const int yield = pResults->GetInt(2);

			m_ppiTechYieldChanges[tech_idx][yield_idx] = yield;
		}
	}

	m_strEffectTypeTag = kResults.GetText("EffectTypeTag");

	return true;
}

//======================================================================================================
//					CvInterfaceModeInfo
//======================================================================================================
CvInterfaceModeInfo::CvInterfaceModeInfo() :
	m_iCursorIndex(NO_CURSOR),
	m_iMissionType(NO_MISSION),
	m_bVisible(false),
	m_bHighlightPlot(false),
	m_bSelectType(false),
	m_bSelectAll(false)
{
}
//------------------------------------------------------------------------------
int CvInterfaceModeInfo::getCursorIndex() const
{
	return m_iCursorIndex;
}
//------------------------------------------------------------------------------
int CvInterfaceModeInfo::getMissionType() const
{
	return m_iMissionType;
}
//------------------------------------------------------------------------------
bool CvInterfaceModeInfo::getVisible() const
{
	return m_bVisible;
}
//------------------------------------------------------------------------------
bool CvInterfaceModeInfo::getHighlightPlot() const
{
	return m_bHighlightPlot;
}
//------------------------------------------------------------------------------
bool CvInterfaceModeInfo::getSelectType() const
{
	return m_bSelectType;
}
//------------------------------------------------------------------------------
bool CvInterfaceModeInfo::getSelectAll() const
{
	return m_bSelectAll;
}
//------------------------------------------------------------------------------
bool CvInterfaceModeInfo::CacheResults(Database::Results& kResults, CvDatabaseUtility& kUtility)
{
	if(!CvHotKeyInfo::CacheResults(kResults, kUtility))
		return false;

	const char* szCursorType = kResults.GetText("CursorType");
	m_iCursorIndex = GC.getInfoTypeForString(szCursorType, true);

	const char* szMission = kResults.GetText("Mission");
	m_iMissionType = GC.getInfoTypeForString(szMission, true);

	m_bVisible			= kResults.GetBool("Visible");
	m_bHighlightPlot	= kResults.GetBool("HighlightPlot");
	m_bSelectType		= kResults.GetBool("SelectType");
	m_bSelectAll		= kResults.GetBool("SelectAll");

	return true;
}

//======================================================================================================
//					CvLeaderHeadInfo
//======================================================================================================
CvLeaderHeadInfo::CvLeaderHeadInfo() :
	m_iVictoryCompetitiveness(0),
	m_iWonderCompetitiveness(0),
	m_iMinorCivCompetitiveness(0),
	m_iBoldness(0),
	m_iDiploBalance(0),
	m_iWarmongerHate(0),
	m_iDoFWillingness(0),
	m_iDenounceWillingness(0),
	m_iWorkWithWillingness(0),
	m_iWorkAgainstWillingness(0),
	m_iLoyalty(0),
	m_iForgiveness(0),
	m_iNeediness(0),
	m_iMeanness(0),
	m_iChattiness(0),
	m_ePrimaryVictoryPursuit(NO_VICTORY_PURSUIT),
	m_eSecondaryVictoryPursuit(NO_VICTORY_PURSUIT),
	m_piMajorCivApproachBiases(NULL),
	m_piMinorCivApproachBiases(NULL),
	m_pbTraits(NULL),
	m_piFlavorValue(NULL)
{
}
//------------------------------------------------------------------------------
CvLeaderHeadInfo::~CvLeaderHeadInfo()
{
	SAFE_DELETE_ARRAY(m_piMajorCivApproachBiases);
	SAFE_DELETE_ARRAY(m_piMinorCivApproachBiases);
	SAFE_DELETE_ARRAY(m_pbTraits);
	SAFE_DELETE_ARRAY(m_piFlavorValue);
}
//------------------------------------------------------------------------------
int CvLeaderHeadInfo::GetVictoryCompetitiveness() const
{
	return m_iVictoryCompetitiveness;
}
//------------------------------------------------------------------------------
int CvLeaderHeadInfo::GetWonderCompetitiveness() const
{
	return m_iWonderCompetitiveness;
}
//------------------------------------------------------------------------------
int CvLeaderHeadInfo::GetMinorCivCompetitiveness() const
{
	return m_iMinorCivCompetitiveness;
}
//------------------------------------------------------------------------------
int CvLeaderHeadInfo::GetBoldness() const
{
	return m_iBoldness;
}
//------------------------------------------------------------------------------
int CvLeaderHeadInfo::GetDiploBalance() const
{
	return m_iDiploBalance;
}
//------------------------------------------------------------------------------
int CvLeaderHeadInfo::GetWarmongerHate() const
{
	return m_iWarmongerHate;
}
//------------------------------------------------------------------------------
int CvLeaderHeadInfo::GetDoFWillingness() const
{
	return m_iDoFWillingness;
}
//------------------------------------------------------------------------------
int CvLeaderHeadInfo::GetDenounceWillingness() const
{
	return m_iDenounceWillingness;
}
//------------------------------------------------------------------------------
int CvLeaderHeadInfo::GetWorkWithWillingness() const
{
	return m_iWorkWithWillingness;
}
//------------------------------------------------------------------------------
int CvLeaderHeadInfo::GetWorkAgainstWillingness() const
{
	return m_iWorkAgainstWillingness;
}
//------------------------------------------------------------------------------
int CvLeaderHeadInfo::GetLoyalty() const
{
	return m_iLoyalty;
}
//------------------------------------------------------------------------------
int CvLeaderHeadInfo::GetForgiveness() const
{
	return m_iForgiveness;
}
//------------------------------------------------------------------------------
int CvLeaderHeadInfo::GetNeediness() const
{
	return m_iNeediness;
}
//------------------------------------------------------------------------------
int CvLeaderHeadInfo::GetMeanness() const
{
	return m_iMeanness;
}
//------------------------------------------------------------------------------
int CvLeaderHeadInfo::GetChattiness() const
{
	return m_iChattiness;
}
//------------------------------------------------------------------------------
// Recursive: Need to hardcode these references because of how Firaxis set up the table.
int CvLeaderHeadInfo::GetWarBias(bool bMinor) const
{
	if (bMinor)
		return m_piMinorCivApproachBiases ? m_piMinorCivApproachBiases[3] : 5; // xml: MINOR_CIV_APPROACH_CONQUEST

	return m_piMajorCivApproachBiases ? m_piMajorCivApproachBiases[0] : 5; // xml: MAJOR_CIV_APPROACH_WAR
}
//------------------------------------------------------------------------------
int CvLeaderHeadInfo::GetHostileBias(bool bMinor) const
{
	if (bMinor)
		return m_piMinorCivApproachBiases ? m_piMinorCivApproachBiases[4] : 5; // xml: MINOR_CIV_APPROACH_BULLY

	return m_piMajorCivApproachBiases ? m_piMajorCivApproachBiases[1] : 5; // xml: MAJOR_CIV_APPROACH_HOSTILE
}
//------------------------------------------------------------------------------
int CvLeaderHeadInfo::GetDeceptiveBias() const
{
	return m_piMajorCivApproachBiases ? m_piMajorCivApproachBiases[2] : 5; // xml: MAJOR_CIV_APPROACH_DECEPTIVE
}
//------------------------------------------------------------------------------
int CvLeaderHeadInfo::GetGuardedBias() const
{
	return m_piMajorCivApproachBiases ? m_piMajorCivApproachBiases[3] : 5; // xml: MAJOR_CIV_APPROACH_GUARDED
}
//------------------------------------------------------------------------------
int CvLeaderHeadInfo::GetAfraidBias() const
{
	return m_piMajorCivApproachBiases ? m_piMajorCivApproachBiases[4] : 5; // xml: MAJOR_CIV_APPROACH_AFRAID
}
//------------------------------------------------------------------------------
int CvLeaderHeadInfo::GetNeutralBias(bool bMinor) const
{
	if (bMinor)
		return m_piMinorCivApproachBiases ? m_piMinorCivApproachBiases[0] : 5; // xml: MINOR_CIV_APPROACH_IGNORE

	return m_piMajorCivApproachBiases ? m_piMajorCivApproachBiases[6] : 5; // xml: MAJOR_CIV_APPROACH_NEUTRAL
}
//------------------------------------------------------------------------------
int CvLeaderHeadInfo::GetFriendlyBias(bool bMinor) const
{
	// We combine Friendly and Protective into a single approach, removing the dumb Firaxis distinction.
	// Use the higher of the two for the diplomacy AI.
	if (bMinor)
	{
		if (!m_piMinorCivApproachBiases)
			return 5;

		return std::max(m_piMinorCivApproachBiases[1], m_piMinorCivApproachBiases[2]); // xml: MINOR_CIV_APPROACH_FRIENDLY, MINOR_CIV_APPROACH_PROTECTIVE
	}

	return m_piMajorCivApproachBiases ? m_piMajorCivApproachBiases[5] : 5; // xml: MAJOR_CIV_APPROACH_FRIENDLY
}
//------------------------------------------------------------------------------
const char* CvLeaderHeadInfo::getArtDefineTag() const
{
	return m_strArtDefineTag;
}
//------------------------------------------------------------------------------
void CvLeaderHeadInfo::setArtDefineTag(const char* szVal)
{
	m_strArtDefineTag = szVal;
}
//------------------------------------------------------------------------------
VictoryPursuitTypes CvLeaderHeadInfo::GetPrimaryVictoryPursuit() const
{
	return m_ePrimaryVictoryPursuit;
}
//------------------------------------------------------------------------------
VictoryPursuitTypes CvLeaderHeadInfo::GetSecondaryVictoryPursuit() const
{
	return m_eSecondaryVictoryPursuit;
}
//------------------------------------------------------------------------------
VictoryPursuitTypes CvLeaderHeadInfo::VictoryPursuitTypeFromString(const char* szStr)
{
	if (szStr)
	{
		if (0 == _stricmp(szStr, "VICTORY_PURSUIT_DOMINATION"))
		{
			return VICTORY_PURSUIT_DOMINATION;
		}
		else if (0 == _stricmp(szStr, "VICTORY_PURSUIT_DIPLOMACY"))
		{
			return VICTORY_PURSUIT_DIPLOMACY;
		}
		else if (0 == _stricmp(szStr, "VICTORY_PURSUIT_CULTURE"))
		{
			return VICTORY_PURSUIT_CULTURE;
		}
		else if (0 == _stricmp(szStr, "VICTORY_PURSUIT_SCIENCE"))
		{
			return VICTORY_PURSUIT_SCIENCE;
		}
	}
	return NO_VICTORY_PURSUIT;
}
//------------------------------------------------------------------------------
bool CvLeaderHeadInfo::hasTrait(int i) const
{
	ASSERT_DEBUG(i < GC.getNumTraitInfos(), "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_pbTraits ? m_pbTraits[i] : false;
}
//------------------------------------------------------------------------------
int CvLeaderHeadInfo::getFlavorValue(int i) const
{
	ASSERT_DEBUG(i < GC.getNumFlavorTypes(), "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_piFlavorValue ? m_piFlavorValue[i] : 0;
}
//------------------------------------------------------------------------------
const char* CvLeaderHeadInfo::getLeaderHead() const
{
	return NULL;
}
//------------------------------------------------------------------------------
bool CvLeaderHeadInfo::CacheResults(Database::Results& kResults, CvDatabaseUtility& kUtility)
{
	if (!CvBaseInfo::CacheResults(kResults, kUtility))
		return false;

	//Basic Properties
	const char* szTextVal = NULL;	//Temp storage
	szTextVal = kResults.GetText("ArtDefineTag");
	setArtDefineTag(szTextVal);

	szTextVal = kResults.GetText("PrimaryVictoryPursuit");
	m_ePrimaryVictoryPursuit = VictoryPursuitTypeFromString(szTextVal);

	szTextVal = kResults.GetText("SecondaryVictoryPursuit");
	m_eSecondaryVictoryPursuit = VictoryPursuitTypeFromString(szTextVal);

	if (m_ePrimaryVictoryPursuit != NO_VICTORY_PURSUIT)
	{
		if (m_eSecondaryVictoryPursuit == m_ePrimaryVictoryPursuit)
			m_eSecondaryVictoryPursuit = NO_VICTORY_PURSUIT;
	}
	else if (m_eSecondaryVictoryPursuit != NO_VICTORY_PURSUIT)
	{
		m_ePrimaryVictoryPursuit = m_eSecondaryVictoryPursuit;
		m_eSecondaryVictoryPursuit = NO_VICTORY_PURSUIT;
	}

	// Diplomacy Flavors
	m_iVictoryCompetitiveness					= kResults.GetInt("VictoryCompetitiveness");
	m_iWonderCompetitiveness					= kResults.GetInt("WonderCompetitiveness");
	m_iMinorCivCompetitiveness					= kResults.GetInt("MinorCivCompetitiveness");
	m_iBoldness									= kResults.GetInt("Boldness");
	m_iDiploBalance								= kResults.GetInt("DiploBalance");
	m_iWarmongerHate							= kResults.GetInt("WarmongerHate");
	m_iDoFWillingness							= kResults.GetInt("DoFWillingness");
	m_iDenounceWillingness						= kResults.GetInt("DenounceWillingness");
	m_iWorkWithWillingness						= kResults.GetInt("WorkWithWillingness");
	m_iWorkAgainstWillingness					= kResults.GetInt("WorkAgainstWillingness");
	m_iLoyalty									= kResults.GetInt("Loyalty");
	m_iForgiveness								= kResults.GetInt("Forgiveness");
	m_iNeediness								= kResults.GetInt("Neediness");
	m_iMeanness									= kResults.GetInt("Meanness");
	m_iChattiness								= kResults.GetInt("Chattiness");

	//Arrays
	const char* szType = GetType();

	kUtility.SetFlavors(m_piFlavorValue, "Leader_Flavors", "LeaderType", szType);
	kUtility.PopulateArrayByValue(m_piMajorCivApproachBiases, "MajorCivApproachTypes", "Leader_MajorCivApproachBiases", "MajorCivApproachType", "LeaderType", szType, "Bias");
	kUtility.PopulateArrayByValue(m_piMinorCivApproachBiases, "MinorCivApproachTypes", "Leader_MinorCivApproachBiases", "MinorCivApproachType", "LeaderType", szType, "Bias");
	kUtility.PopulateArrayByExistence(m_pbTraits, "Traits", "Leader_Traits", "TraitType", "LeaderType", szType);

	return true;
}

//======================================================================================================
//					CvWorldInfo
//======================================================================================================
CvWorldInfo::CvWorldInfo() :
	m_iDefaultPlayers(0),
	m_iDefaultMinorCivs(0),
	m_iFogTilesPerBarbarianCamp(0),
	m_iNumNaturalWonders(0),
	m_iUnitNameModifier(0),
	m_iTargetNumCities(0),
	m_iNumFreeBuildingResources(0),
	m_iBuildingClassPrereqModifier(0),
	m_iGridWidth(0),
	m_iGridHeight(0),
	m_iMaxActiveReligions(0),
	m_iTerrainGrainChange(0),
	m_iFeatureGrainChange(0),
	m_iResearchPercent(0),
	m_iNumCitiesUnhappinessPercent(100),
	m_iNumCitiesPolicyCostMod(10),
	m_iNumCitiesTechCostMod(5),
#if defined(MOD_TRADE_ROUTE_SCALING)
	m_iNumCitiesTourismCostMod(5),
	m_iNumCitiesUnitSupplyMod(5),
	m_iTradeRouteDistanceMod(100),
#endif
#if defined(MOD_BALANCE_CORE)
	m_iMinDistanceCities(3),
	m_iMinDistanceCityStates(3),
	m_iReformationPercent(100),
#endif
	m_iEstimatedNumCities(0)
{
}
//------------------------------------------------------------------------------
int CvWorldInfo::getDefaultPlayers() const
{
	return m_iDefaultPlayers;
}
//------------------------------------------------------------------------------
int CvWorldInfo::getDefaultMinorCivs() const
{
	return m_iDefaultMinorCivs;
}
//------------------------------------------------------------------------------
int CvWorldInfo::getFogTilesPerBarbarianCamp() const
{
	return m_iFogTilesPerBarbarianCamp;
}
//------------------------------------------------------------------------------
int CvWorldInfo::getNumNaturalWonders() const
{
	return m_iNumNaturalWonders;
}
//------------------------------------------------------------------------------
int CvWorldInfo::getUnitNameModifier() const
{
	return m_iUnitNameModifier;
}
//------------------------------------------------------------------------------
int CvWorldInfo::getTargetNumCities() const
{
	return m_iTargetNumCities;
}
//------------------------------------------------------------------------------
int CvWorldInfo::getNumFreeBuildingResources() const
{
	return m_iNumFreeBuildingResources;
}
//------------------------------------------------------------------------------
int CvWorldInfo::getBuildingClassPrereqModifier() const
{
	return m_iBuildingClassPrereqModifier;
}
//------------------------------------------------------------------------------
int CvWorldInfo::getGridWidth() const
{
	return m_iGridWidth;
}
//------------------------------------------------------------------------------
int CvWorldInfo::getGridHeight() const
{
	return m_iGridHeight;
}
//------------------------------------------------------------------------------
int CvWorldInfo::getMaxActiveReligions() const
{
	return m_iMaxActiveReligions;
}
//------------------------------------------------------------------------------
int CvWorldInfo::getTerrainGrainChange() const
{
	return m_iTerrainGrainChange;
}
//------------------------------------------------------------------------------
int CvWorldInfo::getFeatureGrainChange() const
{
	return m_iFeatureGrainChange;
}
//------------------------------------------------------------------------------
int CvWorldInfo::getResearchPercent() const
{
	return m_iResearchPercent;
}
//------------------------------------------------------------------------------
int CvWorldInfo::getNumCitiesUnhappinessPercent() const
{
	return m_iNumCitiesUnhappinessPercent;
}
//------------------------------------------------------------------------------
int CvWorldInfo::GetNumCitiesPolicyCostMod() const
{
	return m_iNumCitiesPolicyCostMod;
}
//------------------------------------------------------------------------------
int CvWorldInfo::GetNumCitiesTechCostMod() const
{
	return m_iNumCitiesTechCostMod;
}
#if defined(MOD_TRADE_ROUTE_SCALING)
//------------------------------------------------------------------------------
int CvWorldInfo::GetNumCitiesTourismCostMod() const
{
	return m_iNumCitiesTourismCostMod;
}
//------------------------------------------------------------------------------
int CvWorldInfo::GetNumCitiesUnitSupplyMod() const
{
	return m_iNumCitiesUnitSupplyMod;
}
//------------------------------------------------------------------------------
int CvWorldInfo::getTradeRouteDistanceMod() const
{
	return m_iTradeRouteDistanceMod;
}
#endif
#if defined(MOD_BALANCE_CORE)
//------------------------------------------------------------------------------
int CvWorldInfo::getMinDistanceCities() const
{
	return m_iMinDistanceCities;
}
//------------------------------------------------------------------------------
int CvWorldInfo::getMinDistanceCityStates() const
{
	return m_iMinDistanceCityStates;
}
//------------------------------------------------------------------------------
int CvWorldInfo::getReformationPercent() const
{
	return m_iReformationPercent;
}
#endif
//------------------------------------------------------------------------------
int CvWorldInfo::GetEstimatedNumCities() const
{
	return m_iEstimatedNumCities;
}
//------------------------------------------------------------------------------
CvWorldInfo CvWorldInfo::CreateCustomWorldSize(const CvWorldInfo& kTemplate, int iWidth, int iHeight)
{
	CvWorldInfo kWorldInfo(kTemplate);
	kWorldInfo.m_iGridWidth = iWidth;
	kWorldInfo.m_iGridHeight = iHeight;

	return kWorldInfo;
}
//------------------------------------------------------------------------------
CvWorldInfo CvWorldInfo::CreateCustomWorldSize(const CvWorldInfo& kTemplate, int iWidth, int iHeight, int iPlayers, int iMinorCivs)
{
	CvWorldInfo kWorldInfo(kTemplate);
	kWorldInfo.m_iGridWidth = iWidth;
	kWorldInfo.m_iGridHeight = iHeight;
	kWorldInfo.m_iDefaultPlayers = iPlayers;
	kWorldInfo.m_iDefaultMinorCivs = iMinorCivs;

	return kWorldInfo;
}
//------------------------------------------------------------------------------
bool CvWorldInfo::CacheResults(Database::Results& kResults, CvDatabaseUtility& kUtility)
{
	if(!CvBaseInfo::CacheResults(kResults, kUtility))
		return false;

	m_iDefaultPlayers				= kResults.GetInt("DefaultPlayers");
	m_iDefaultMinorCivs				= kResults.GetInt("DefaultMinorCivs");
	m_iFogTilesPerBarbarianCamp		= kResults.GetInt("FogTilesPerBarbarianCamp");
	m_iNumNaturalWonders			= kResults.GetInt("NumNaturalWonders");
	m_iUnitNameModifier				= kResults.GetInt("UnitNameModifier");
	m_iTargetNumCities				= kResults.GetInt("TargetNumCities");
	m_iNumFreeBuildingResources		= kResults.GetInt("NumFreeBuildingResources");
	m_iBuildingClassPrereqModifier	= kResults.GetInt("BuildingClassPrereqModifier");
	m_iGridWidth					= kResults.GetInt("GridWidth");
	m_iGridHeight					= kResults.GetInt("GridHeight");
	m_iMaxActiveReligions			= kResults.GetInt("MaxActiveReligions");
	m_iTerrainGrainChange			= kResults.GetInt("TerrainGrainChange");
	m_iFeatureGrainChange			= kResults.GetInt("FeatureGrainChange");
	m_iResearchPercent				= kResults.GetInt("ResearchPercent");
	m_iNumCitiesUnhappinessPercent	= kResults.GetInt("NumCitiesUnhappinessPercent");
	m_iNumCitiesPolicyCostMod		= kResults.GetInt("NumCitiesPolicyCostMod");
	m_iNumCitiesTechCostMod			= kResults.GetInt("NumCitiesTechCostMod");
#if defined(MOD_TRADE_ROUTE_SCALING)
	if (MOD_TRADE_ROUTE_SCALING) {
		m_iTradeRouteDistanceMod	= kResults.GetInt("TradeRouteDistanceMod");
	}
#endif
#if defined(MOD_BALANCE_CORE)
	m_iNumCitiesTourismCostMod = kResults.GetInt("NumCitiesTourismCostMod");
	m_iNumCitiesUnitSupplyMod = kResults.GetInt("NumCitiesUnitSupplyMod");
	m_iMinDistanceCities = kResults.GetInt("MinDistanceCities");
	m_iMinDistanceCityStates = kResults.GetInt("MinDistanceCityStates");
	m_iReformationPercent = kResults.GetInt("ReformationPercentRequired");
#endif
	m_iEstimatedNumCities			= kResults.GetInt("EstimatedNumCities");

	return true;
}

bool CvWorldInfo::operator==(const CvWorldInfo& rhs) const
{
	if(this == &rhs) return true;
	if(!CvBaseInfo::operator==(rhs)) return false;
	if(m_iDefaultPlayers != rhs.m_iDefaultPlayers) return false;
	if(m_iDefaultMinorCivs != rhs.m_iDefaultMinorCivs) return false;
	if(m_iFogTilesPerBarbarianCamp != rhs.m_iFogTilesPerBarbarianCamp) return false;
	if(m_iNumNaturalWonders != rhs.m_iNumNaturalWonders) return false;
	if(m_iUnitNameModifier != rhs.m_iUnitNameModifier) return false;
	if(m_iTargetNumCities != rhs.m_iTargetNumCities) return false;
	if(m_iNumFreeBuildingResources != rhs.m_iNumFreeBuildingResources) return false;
	if(m_iBuildingClassPrereqModifier != rhs.m_iBuildingClassPrereqModifier) return false;
	if(m_iGridWidth != rhs.m_iGridWidth) return false;
	if(m_iGridHeight != rhs.m_iGridHeight) return false;
	if(m_iMaxActiveReligions != rhs.m_iMaxActiveReligions) return false;
	if(m_iTerrainGrainChange != rhs.m_iTerrainGrainChange) return false;
	if(m_iFeatureGrainChange != rhs.m_iFeatureGrainChange) return false;
	if(m_iResearchPercent != rhs.m_iResearchPercent) return false;
	if(m_iNumCitiesUnhappinessPercent != rhs.m_iNumCitiesUnhappinessPercent) return false;
	if(m_iNumCitiesPolicyCostMod != rhs.m_iNumCitiesPolicyCostMod) return false;
#if defined(MOD_TRADE_ROUTE_SCALING)
	if(m_iTradeRouteDistanceMod != rhs.m_iTradeRouteDistanceMod) return false;
#endif
#if defined(MOD_BALANCE_CORE)
	if (m_iNumCitiesTourismCostMod != rhs.m_iNumCitiesTourismCostMod) return false;
	if (m_iNumCitiesUnitSupplyMod != rhs.m_iNumCitiesUnitSupplyMod) return false;
	if(m_iMinDistanceCities != rhs.m_iMinDistanceCities) return false;
	if(m_iMinDistanceCityStates != rhs.m_iMinDistanceCityStates) return false;
	if(m_iReformationPercent != rhs.m_iReformationPercent) return false;
#endif
	if(m_iNumCitiesTechCostMod != rhs.m_iNumCitiesTechCostMod) return false;
	return true;
}

bool CvWorldInfo::operator!=(const CvWorldInfo& rhs) const
{
	return !(*this == rhs);
}

template<typename WorldInfo, typename Visitor>
void CvWorldInfo::Serialize(WorldInfo& worldInfo, Visitor& visitor)
{
	visitor(worldInfo.m_iDefaultPlayers);
	visitor(worldInfo.m_iDefaultMinorCivs);
	visitor(worldInfo.m_iFogTilesPerBarbarianCamp);
	visitor(worldInfo.m_iNumNaturalWonders);
	visitor(worldInfo.m_iUnitNameModifier);
	visitor(worldInfo.m_iTargetNumCities);
	visitor(worldInfo.m_iNumFreeBuildingResources);
	visitor(worldInfo.m_iBuildingClassPrereqModifier);
	visitor(worldInfo.m_iGridWidth);
	visitor(worldInfo.m_iGridHeight);
	visitor(worldInfo.m_iMaxActiveReligions);
	visitor(worldInfo.m_iTerrainGrainChange);
	visitor(worldInfo.m_iFeatureGrainChange);
	visitor(worldInfo.m_iResearchPercent);
	visitor(worldInfo.m_iNumCitiesUnhappinessPercent);
	visitor(worldInfo.m_iNumCitiesPolicyCostMod);
	visitor(worldInfo.m_iNumCitiesTechCostMod);
	visitor(worldInfo.m_iTradeRouteDistanceMod);
	visitor(worldInfo.m_iNumCitiesTourismCostMod);
	visitor(worldInfo.m_iNumCitiesUnitSupplyMod);
	visitor(worldInfo.m_iMinDistanceCities);
	visitor(worldInfo.m_iMinDistanceCityStates);
	visitor(worldInfo.m_iReformationPercent);
}

void CvWorldInfo::readFrom(FDataStream& loadFrom)
{
	CvBaseInfo::readFrom(loadFrom);
	CvStreamLoadVisitor serialVisitor(loadFrom);
	Serialize(*this, serialVisitor);
}

// A special reader for version 0 (pre-versioning)
void CvWorldInfo::readFromVersion0(FDataStream& loadFrom)
{
	CvBaseInfo::readFrom(loadFrom);
	loadFrom >> m_iDefaultPlayers;
	loadFrom >> m_iDefaultMinorCivs;
	loadFrom >> m_iFogTilesPerBarbarianCamp;
	loadFrom >> m_iNumNaturalWonders;
	loadFrom >> m_iUnitNameModifier;
	loadFrom >> m_iTargetNumCities;
	loadFrom >> m_iNumFreeBuildingResources;
	loadFrom >> m_iBuildingClassPrereqModifier;
	loadFrom >> m_iGridWidth;
	loadFrom >> m_iGridHeight;
	loadFrom >> m_iTerrainGrainChange;
	loadFrom >> m_iFeatureGrainChange;
	loadFrom >> m_iResearchPercent;
	loadFrom >> m_iNumCitiesUnhappinessPercent;
	loadFrom >> m_iNumCitiesPolicyCostMod;
}

void CvWorldInfo::writeTo(FDataStream& saveTo) const
{
	CvBaseInfo::writeTo(saveTo);
	CvStreamSaveVisitor serialVisitor(saveTo);
	Serialize(*this, serialVisitor);
}

FDataStream& operator<<(FDataStream& saveTo, const CvWorldInfo& readFrom)
{
	readFrom.writeTo(saveTo);
	return saveTo;
}

FDataStream& operator>>(FDataStream& loadFrom, CvWorldInfo& writeTo)
{
	writeTo.readFrom(loadFrom);
	return loadFrom;
}

//======================================================================================================
//					CvClimateInfo
//======================================================================================================
CvClimateInfo::CvClimateInfo() :
	m_iDesertPercentChange(0),
	m_iJungleLatitude(0),
	m_iHillRange(0),
	m_iMountainPercent(0),
	m_fSnowLatitudeChange(0.0f),
	m_fTundraLatitudeChange(0.0f),
	m_fGrassLatitudeChange(0.0f),
	m_fDesertBottomLatitudeChange(0.0f),
	m_fDesertTopLatitudeChange(0.0f),
	m_fIceLatitude(0.0f),
	m_fRandIceLatitude(0.0f)
{
}
//------------------------------------------------------------------------------
bool CvClimateInfo::CacheResults(Database::Results& kResults, CvDatabaseUtility& kUtility)
{
	if(!CvBaseInfo::CacheResults(kResults, kUtility))
		return false;

	m_iDesertPercentChange			= kResults.GetInt("DesertPercentChange");
	m_iJungleLatitude				= kResults.GetInt("JungleLatitude");
	m_iHillRange					= kResults.GetInt("HillRange");
	m_iMountainPercent					= kResults.GetInt("MountainPercent");

	m_fSnowLatitudeChange			= kResults.GetFloat("SnowLatitudeChange");
	m_fTundraLatitudeChange			= kResults.GetFloat("TundraLatitudeChange");
	m_fGrassLatitudeChange			= kResults.GetFloat("GrassLatitudeChange");
	m_fDesertBottomLatitudeChange	= kResults.GetFloat("DesertBottomLatitudeChange");
	m_fDesertTopLatitudeChange		= kResults.GetFloat("DesertTopLatitudeChange");
	m_fIceLatitude					= kResults.GetFloat("IceLatitude");
	m_fRandIceLatitude				= kResults.GetFloat("RandIceLatitude");

	return true;
}

template<typename ClimateInfo, typename Visitor>
void CvClimateInfo::Serialize(ClimateInfo& climateInfo, Visitor& visitor)
{
	visitor(climateInfo.m_iDesertPercentChange);
	visitor(climateInfo.m_iJungleLatitude);
	visitor(climateInfo.m_iHillRange);
	visitor(climateInfo.m_iMountainPercent);
	visitor(climateInfo.m_fSnowLatitudeChange);
	visitor(climateInfo.m_fTundraLatitudeChange);
	visitor(climateInfo.m_fGrassLatitudeChange);
	visitor(climateInfo.m_fDesertBottomLatitudeChange);
	visitor(climateInfo.m_fDesertTopLatitudeChange);
	visitor(climateInfo.m_fIceLatitude);
	visitor(climateInfo.m_fRandIceLatitude);
}

void CvClimateInfo::readFrom(FDataStream& loadFrom)
{
	CvBaseInfo::readFrom(loadFrom);
	CvStreamLoadVisitor serialVisitor(loadFrom);
	Serialize(*this, serialVisitor);
}

void CvClimateInfo::writeTo(FDataStream& saveTo) const
{
	CvBaseInfo::writeTo(saveTo);
	CvStreamSaveVisitor serialVisitor(saveTo);
	Serialize(*this, serialVisitor);
}

FDataStream& operator<<(FDataStream& saveTo, const CvClimateInfo& readFrom)
{
	readFrom.writeTo(saveTo);
	return saveTo;
}

FDataStream& operator>>(FDataStream& loadFrom, CvClimateInfo& writeTo)
{
	writeTo.readFrom(loadFrom);
	return loadFrom;
}

//======================================================================================================
//					CvSeaLevelInfo
//======================================================================================================
CvSeaLevelInfo::CvSeaLevelInfo() : CvBaseInfo()
	, m_iSeaLevelChange(0)
{
}
//------------------------------------------------------------------------------
bool CvSeaLevelInfo::CacheResults(Database::Results& kResults, CvDatabaseUtility& kUtility)
{
	if(!CvBaseInfo::CacheResults(kResults, kUtility))
		return false;

	m_iSeaLevelChange = kResults.GetInt("SeaLevelChange");
	return true;
}

template<typename SeaLevelInfo, typename Visitor>
void CvSeaLevelInfo::Serialize(SeaLevelInfo& seaLevelInfo, Visitor& visitor)
{
	visitor(seaLevelInfo.m_iSeaLevelChange);
}

void CvSeaLevelInfo::readFrom(FDataStream& loadFrom)
{
	CvBaseInfo::readFrom(loadFrom);
	CvStreamLoadVisitor serialVisitor(loadFrom);
	Serialize(*this, serialVisitor);
}

void CvSeaLevelInfo::writeTo(FDataStream& saveTo) const
{
	CvBaseInfo::writeTo(saveTo);
	CvStreamSaveVisitor serialVisitor(saveTo);
	Serialize(*this, serialVisitor);
}

FDataStream& operator<<(FDataStream& saveTo, const CvSeaLevelInfo& readFrom)
{
	readFrom.writeTo(saveTo);
	return saveTo;
}

FDataStream& operator>>(FDataStream& loadFrom, CvSeaLevelInfo& writeTo)
{
	writeTo.readFrom(loadFrom);
	return loadFrom;
}

//======================================================================================================
//					CvProcessInfo
//======================================================================================================
CvProcessInfo::CvProcessInfo() :
	m_iTechPrereq(NO_TECH),
	m_iRequiredPolicy(NO_POLICY),
	m_iDefenseValue(0),
#if defined(MOD_CIVILIZATIONS_UNIQUE_PROCESSES)
	m_eRequiredCivilization(NO_CIVILIZATION),
#endif
	m_paiProductionToYieldModifier(NULL),
	m_paiFlavorValue(NULL)
{
}
//------------------------------------------------------------------------------
CvProcessInfo::~CvProcessInfo()
{
	SAFE_DELETE_ARRAY(m_paiProductionToYieldModifier);
	SAFE_DELETE_ARRAY(m_paiFlavorValue);
}
//------------------------------------------------------------------------------
int CvProcessInfo::getTechPrereq() const
{
	return m_iTechPrereq;
}

//------------------------------------------------------------------------------
int CvProcessInfo::getRequiredPolicy() const
{
	return m_iRequiredPolicy;
}

//------------------------------------------------------------------------------
int CvProcessInfo::getDefenseValue() const
{
	return m_iDefenseValue;
}

#if defined(MOD_CIVILIZATIONS_UNIQUE_PROCESSES)
//------------------------------------------------------------------------------
CivilizationTypes CvProcessInfo::GetRequiredCivilization() const
{
	return m_eRequiredCivilization;
}
#endif

//------------------------------------------------------------------------------
int CvProcessInfo::getProductionToYieldModifier(int i) const
{
	ASSERT_DEBUG(i < NUM_YIELD_TYPES, "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_paiProductionToYieldModifier ? m_paiProductionToYieldModifier[i] : -1;
}

//------------------------------------------------------------------------------
int CvProcessInfo::GetFlavorValue(int i) const
{
	ASSERT_DEBUG(i < GC.getNumFlavorTypes(), "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_paiFlavorValue ? m_paiFlavorValue[i] : -1;
}


//------------------------------------------------------------------------------
bool CvProcessInfo::CacheResults(Database::Results& kResults, CvDatabaseUtility& kUtility)
{
	if(!CvBaseInfo::CacheResults(kResults, kUtility))
		return false;

	const char* szTechPrereq = kResults.GetText("TechPrereq");
	m_iTechPrereq = GC.getInfoTypeForString(szTechPrereq, true);

	const char* szRequiredPolicy = kResults.GetText("RequiredPolicy");
	m_iRequiredPolicy = GC.getInfoTypeForString(szRequiredPolicy, true);

	m_iDefenseValue = kResults.GetInt("DefenseValue");

#if defined(MOD_CIVILIZATIONS_UNIQUE_PROCESSES)
	const char* szCivilizationType = kResults.GetText("CivilizationType");
	m_eRequiredCivilization = (CivilizationTypes)GC.getInfoTypeForString(szCivilizationType, true);
#endif

	const char* szProcessType = GetType();

	kUtility.SetYields(m_paiProductionToYieldModifier, "Process_ProductionYields", "ProcessType", szProcessType);
	kUtility.SetFlavors(m_paiFlavorValue, "Process_Flavors", "ProcessType", szProcessType);

	return true;
}
//------------------------------------------------------------------------------

//======================================================================================================
//					CvVoteInfo
//======================================================================================================
CvVoteInfo::CvVoteInfo() :
	m_iPopulationThreshold(0),
	m_iMinVoters(0),
	m_bSecretaryGeneral(false),
	m_bVictory(false),
	m_bNoNukes(false),
	m_bCityVoting(false),
	m_bCivVoting(false),
	m_bDefensivePact(false),
	m_bOpenBorders(false),
	m_bForcePeace(false),
	m_bForceNoTrade(false),
	m_bForceWar(false),
	m_bAssignCity(false),
	m_abVoteSourceTypes(NULL)
{
}
//------------------------------------------------------------------------------
CvVoteInfo::~CvVoteInfo()
{
	SAFE_DELETE_ARRAY(m_abVoteSourceTypes);
}
//------------------------------------------------------------------------------
int CvVoteInfo::getPopulationThreshold() const
{
	return m_iPopulationThreshold;
}
//------------------------------------------------------------------------------
int CvVoteInfo::getMinVoters() const
{
	return m_iMinVoters;
}
//------------------------------------------------------------------------------
bool CvVoteInfo::isSecretaryGeneral() const
{
	return m_bSecretaryGeneral;
}
//------------------------------------------------------------------------------
bool CvVoteInfo::isVictory() const
{
	return m_bVictory;
}
//------------------------------------------------------------------------------
bool CvVoteInfo::isNoNukes() const
{
	return m_bNoNukes;
}
//------------------------------------------------------------------------------
bool CvVoteInfo::isCityVoting() const
{
	return m_bCityVoting;
}
//------------------------------------------------------------------------------
bool CvVoteInfo::isCivVoting() const
{
	return m_bCivVoting;
}
//------------------------------------------------------------------------------
bool CvVoteInfo::isDefensivePact() const
{
	return m_bDefensivePact;
}
//------------------------------------------------------------------------------
bool CvVoteInfo::isOpenBorders() const
{
	return m_bOpenBorders;
}
//------------------------------------------------------------------------------
bool CvVoteInfo::isForcePeace() const
{
	return m_bForcePeace;
}
//------------------------------------------------------------------------------
bool CvVoteInfo::isForceNoTrade() const
{
	return m_bForceNoTrade;
}
//------------------------------------------------------------------------------
bool CvVoteInfo::isForceWar() const
{
	return m_bForceWar;
}
//------------------------------------------------------------------------------
bool CvVoteInfo::isAssignCity() const
{
	return m_bAssignCity;
}
//------------------------------------------------------------------------------
bool CvVoteInfo::isVoteSourceType(int i) const
{
	ASSERT_DEBUG(i < GC.getNumVoteSourceInfos(), "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_abVoteSourceTypes ? m_abVoteSourceTypes[i] : false;
}
//------------------------------------------------------------------------------
bool CvVoteInfo::CacheResults(Database::Results& kResults, CvDatabaseUtility& kUtility)
{
	if(!CvBaseInfo::CacheResults(kResults, kUtility))
		return false;

	m_iPopulationThreshold = kResults.GetInt("PopulationThreshold");
	m_iMinVoters = kResults.GetInt("MinVoters");

	m_bSecretaryGeneral = kResults.GetBool("SecretaryGeneral");
	m_bVictory = kResults.GetBool("Victory");
	m_bNoNukes = kResults.GetBool("NoNukes");
	m_bCityVoting = kResults.GetBool("CityVoting");
	m_bCivVoting = kResults.GetBool("CivVoting");
	m_bDefensivePact = kResults.GetBool("DefensivePact");
	m_bOpenBorders = kResults.GetBool("OpenBorders");
	m_bForcePeace = kResults.GetBool("ForcePeace");
	m_bForceNoTrade = kResults.GetBool("ForceNoTrade");
	m_bForceWar = kResults.GetBool("ForceWar");
	m_bAssignCity = kResults.GetBool("AssignCity");

	const char* szVoteType = GetType();
	kUtility.PopulateArrayByExistence(m_abVoteSourceTypes, "VoteSources", "Vote_DiploVotes", "DiploVoteType", "VoteType", szVoteType);

	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////
// CvEntityEventInfo
/////////////////////////////////////////////////////////////////////////////////////////////
CvEntityEventInfo::CvEntityEventInfo() :
	m_bUpdateFormation(true)
{
}
//------------------------------------------------------------------------------
bool CvEntityEventInfo::CacheResults(Database::Results& kResults, CvDatabaseUtility& kUtility)
{
	if(!CvBaseInfo::CacheResults(kResults, kUtility))
		return false;

	m_bUpdateFormation = kResults.GetBool("UpdateFormation");

	const char* szEntityEventType = GetType();

	//EntityEvent_AnimationPaths
	{
		std::string strKey = "EntityEventInfo - AnimationPaths";
		Database::Results* pResults = kUtility.GetResults(strKey);
		if(pResults == NULL)
		{
			pResults = kUtility.PrepareResults(strKey, "select AnimationPaths.ID from EntityEvent_AnimationPaths inner join AnimationPaths on AnimationPathType = AnimationPaths.Type where EntityEventType = ?");
		}

		pResults->Bind(1, szEntityEventType, -1, false);

		while(pResults->Step())
		{
			const AnimationPathTypes eAnimationPath = (AnimationPathTypes)pResults->GetInt(0);
			m_vctAnimationPathType.push_back(eAnimationPath);
		}

		pResults->Reset();
	}

	return true;
}
//------------------------------------------------------------------------------
AnimationPathTypes CvEntityEventInfo::getAnimationPathType(int iIndex) const
{
	return iIndex >= (int)m_vctAnimationPathType.size() ? ANIMATIONPATH_NONE : m_vctAnimationPathType[iIndex];
}
//------------------------------------------------------------------------------
int CvEntityEventInfo::getAnimationPathCount() const
{
	return m_vctAnimationPathType.size();
}
//------------------------------------------------------------------------------
bool CvEntityEventInfo::getUpdateFormation() const
{
	return m_bUpdateFormation;
}

//------------------------------------------------------------------------------------------------------
//  CvEraInfo
//

CvEraInfo::CvEraInfo() :
	m_iStartingUnitMultiplier(0),
	m_iStartingDefenseUnits(0),
	m_iStartingCityStateDefenseUnits(0),
	m_iStartingWorkerUnits(0),
	m_iStartingExploreUnits(0),
	m_iUnitSupplyBase(0),
	m_iResearchAgreementCost(0),
	m_iEmbarkedUnitDefense(0),
	m_iStartingGold(0),
	m_iStartingCulture(0),
	m_iFreePopulation(0),
	m_iLaterEraBuildingConstructMod(0),
	m_iStartPercent(0),
	m_iBuildingMaintenancePercent(0),
	m_iGrowthPercent(0),
	m_iTrainPercent(0),
	m_iConstructPercent(0),
	m_iCreatePercent(0),
	m_iResearchPercent(0),
	m_iBuildPercent(0),
	m_iImprovementPercent(0),
	m_iGreatPeoplePercent(0),
	m_iEventChancePerTurn(0),
	m_iSpiesGrantedForPlayer(0),
	m_iSpiesGrantedForEveryone(0),
	m_iFaithCostMultiplier(0),
	m_iDiploEmphasisReligion(0),
	m_iDiploEmphasisLatePolicies(0),
	m_iTradeRouteFoodBonusTimes100(0),
	m_iTradeRouteProductionBonusTimes100(0),
	m_iLeaguePercent(0),
	m_iWarmongerPercent(0),
	m_bVassalageEnabled(false),
	m_bNoGoodies(false),
	m_bNoBarbUnits(false),
	m_bNoReligion(false),
	m_uiCityBombardEffectTagHash(0)
{
}
//------------------------------------------------------------------------------
CvEraInfo::~CvEraInfo()
{
}
//------------------------------------------------------------------------------
int CvEraInfo::getStartingUnitMultiplier() const
{
	return m_iStartingUnitMultiplier;
}
//------------------------------------------------------------------------------
int CvEraInfo::getStartingDefenseUnits() const
{
	return m_iStartingDefenseUnits;
}
//------------------------------------------------------------------------------
int CvEraInfo::getStartingCityStateDefenseUnits() const
{
	return m_iStartingCityStateDefenseUnits;
}
//------------------------------------------------------------------------------
int CvEraInfo::getStartingWorkerUnits() const
{
	return m_iStartingWorkerUnits;
}
//------------------------------------------------------------------------------
int CvEraInfo::getStartingExploreUnits() const
{
	return m_iStartingExploreUnits;
}
//------------------------------------------------------------------------------
int CvEraInfo::getUnitSupplyBase() const
{
	return m_iUnitSupplyBase;
}
//------------------------------------------------------------------------------
int CvEraInfo::getResearchAgreementCost() const
{
	return m_iResearchAgreementCost;
}
//------------------------------------------------------------------------------
int CvEraInfo::getEmbarkedUnitDefense() const
{
	return m_iEmbarkedUnitDefense;
}
//------------------------------------------------------------------------------
int CvEraInfo::getStartingGold() const
{
	return m_iStartingGold;
}
//------------------------------------------------------------------------------
int CvEraInfo::getStartingCulture() const
{
	return m_iStartingCulture;
}
//------------------------------------------------------------------------------
int CvEraInfo::getFreePopulation() const
{
	return m_iFreePopulation;
}
//------------------------------------------------------------------------------
int CvEraInfo::getLaterEraBuildingConstructMod() const
{
	return m_iLaterEraBuildingConstructMod;
}
//------------------------------------------------------------------------------
int CvEraInfo::getStartPercent() const
{
	return m_iStartPercent;
}
//------------------------------------------------------------------------------
int CvEraInfo::getBuildingMaintenancePercent() const
{
	return m_iBuildingMaintenancePercent;
}
//------------------------------------------------------------------------------
int CvEraInfo::getGrowthPercent() const
{
	return m_iGrowthPercent;
}
//------------------------------------------------------------------------------
int CvEraInfo::getTrainPercent() const
{
	return m_iTrainPercent;
}
//------------------------------------------------------------------------------
int CvEraInfo::getConstructPercent() const
{
	return m_iConstructPercent;
}
//------------------------------------------------------------------------------
int CvEraInfo::getCreatePercent() const
{
	return m_iCreatePercent;
}
//------------------------------------------------------------------------------
int CvEraInfo::getResearchPercent() const
{
	return m_iResearchPercent;
}
//------------------------------------------------------------------------------
int CvEraInfo::getBuildPercent() const
{
	return m_iBuildPercent;
}
//------------------------------------------------------------------------------
int CvEraInfo::getImprovementPercent() const
{
	return m_iImprovementPercent;
}
//------------------------------------------------------------------------------
int CvEraInfo::getGreatPeoplePercent() const
{
	return m_iGreatPeoplePercent;
}
//------------------------------------------------------------------------------
int CvEraInfo::getEventChancePerTurn() const
{
	return m_iEventChancePerTurn;
}

//------------------------------------------------------------------------------
int CvEraInfo::getSpiesGrantedForPlayer() const
{
	return m_iSpiesGrantedForPlayer;
}

//------------------------------------------------------------------------------
int CvEraInfo::getSpiesGrantedForEveryone() const
{
	return m_iSpiesGrantedForEveryone;
}

//------------------------------------------------------------------------------
int CvEraInfo::getFaithCostMultiplier() const
{
	return m_iFaithCostMultiplier;
}

//------------------------------------------------------------------------------
int CvEraInfo::getDiploEmphasisReligion() const
{
	return m_iDiploEmphasisReligion;
}

//------------------------------------------------------------------------------
int CvEraInfo::getDiploEmphasisLatePolicies() const
{
	return m_iDiploEmphasisLatePolicies;
}

//------------------------------------------------------------------------------
int CvEraInfo::getTradeRouteFoodBonusTimes100() const
{
	return m_iTradeRouteFoodBonusTimes100;
}

//------------------------------------------------------------------------------
int CvEraInfo::getTradeRouteProductionBonusTimes100() const
{
	return m_iTradeRouteProductionBonusTimes100;
}

//------------------------------------------------------------------------------
int CvEraInfo::getLeaguePercent() const
{
	return m_iLeaguePercent;
}
//------------------------------------------------------------------------------
int CvEraInfo::getWarmongerPercent() const
{
	return m_iWarmongerPercent;
}
//------------------------------------------------------------------------------
const char* CvEraInfo::getArtPrefix() const
{
	return m_strArtPrefix.c_str();
}
//------------------------------------------------------------------------------
const char* CvEraInfo::GetCityBombardEffectTag() const
{
	return m_strCityBombardEffectTag;
}
//------------------------------------------------------------------------------
uint CvEraInfo::GetCityBombardEffectTagHash() const
{
	return m_uiCityBombardEffectTagHash;
}
//------------------------------------------------------------------------------
const char* CvEraInfo::getAudioUnitVictoryScript() const
{
	return m_strAudioUnitVictoryScript;
}
//------------------------------------------------------------------------------
const char* CvEraInfo::getAudioUnitDefeatScript() const
{
	return m_strAudioUnitDefeatScript;
}
//------------------------------------------------------------------------------
bool CvEraInfo::isNoGoodies() const
{
	return m_bNoGoodies;
}
//------------------------------------------------------------------------------
bool CvEraInfo::isNoBarbUnits() const
{
	return m_bNoBarbUnits;
}//------------------------------------------------------------------------------
bool CvEraInfo::isNoReligion() const
{
	return m_bNoReligion;
}
//------------------------------------------------------------------------------
int CvEraInfo::GetNumEraVOs() const
{
	return m_vEraVOs.size();
}
//------------------------------------------------------------------------------
const char* CvEraInfo::GetEraVO(int iIndex)
{
	return m_vEraVOs[iIndex].c_str();
}

//------------------------------------------------------------------------------
const char* CvEraInfo::getShortDesc() const
{
	return m_strShortDesc.c_str();
}

//------------------------------------------------------------------------------
const char* CvEraInfo::getAbbreviation() const
{
	return m_strAbbreviation.c_str();
}

//------------------------------------------------------------------------------
bool CvEraInfo::CacheResults(Database::Results& kResults, CvDatabaseUtility& kUtility)
{
	if(!CvBaseInfo::CacheResults(kResults, kUtility))
		return false;

	m_bNoGoodies				= kResults.GetBool("NoGoodies");
	m_bNoBarbUnits				= kResults.GetBool("NoBarbUnits");
	m_bNoReligion				= kResults.GetBool("NoReligion");
	m_iStartingUnitMultiplier	= kResults.GetInt("StartingUnitMultiplier");
	m_iStartingDefenseUnits		= kResults.GetInt("StartingDefenseUnits");
	m_iStartingCityStateDefenseUnits = kResults.GetInt("StartingCityStateDefenseUnits");
	m_iStartingWorkerUnits		= kResults.GetInt("StartingWorkerUnits");
	m_iStartingExploreUnits		= kResults.GetInt("StartingExploreUnits");
	m_iUnitSupplyBase			= kResults.GetInt("UnitSupplyBase");
	m_iResearchAgreementCost	= kResults.GetInt("ResearchAgreementCost");
	m_iEmbarkedUnitDefense		= kResults.GetInt("EmbarkedUnitDefense");
	m_iStartingGold				= kResults.GetInt("StartingGold");
	m_iStartingCulture			= kResults.GetInt("StartingCulture");
	m_iFreePopulation			= kResults.GetInt("FreePopulation");
	m_iLaterEraBuildingConstructMod = kResults.GetInt("LaterEraBuildingConstructMod");
	m_iStartPercent				= kResults.GetInt("StartPercent");
	m_iBuildingMaintenancePercent			= kResults.GetInt("BuildingMaintenancePercent");
	m_iGrowthPercent			= kResults.GetInt("GrowthPercent");
	m_iTrainPercent				= kResults.GetInt("TrainPercent");
	m_iConstructPercent			= kResults.GetInt("ConstructPercent");
	m_iCreatePercent			= kResults.GetInt("CreatePercent");
	m_iResearchPercent			= kResults.GetInt("ResearchPercent");
	m_iBuildPercent				= kResults.GetInt("BuildPercent");
	m_iImprovementPercent		= kResults.GetInt("ImprovementPercent");
	m_iGreatPeoplePercent		= kResults.GetInt("GreatPeoplePercent");
	m_iEventChancePerTurn		= kResults.GetInt("EventChancePerTurn");
	m_iSpiesGrantedForPlayer    = kResults.GetInt("SpiesGrantedForPlayer");
	m_iSpiesGrantedForEveryone  = kResults.GetInt("SpiesGrantedForEveryone");
	m_iFaithCostMultiplier      = kResults.GetInt("FaithCostMultiplier");
	m_iDiploEmphasisReligion    = kResults.GetInt("DiploEmphasisReligion");
	m_iDiploEmphasisLatePolicies= kResults.GetInt("DiploEmphasisLatePolicies");
	m_iTradeRouteFoodBonusTimes100 = kResults.GetInt("TradeRouteFoodBonusTimes100");
	m_iTradeRouteProductionBonusTimes100 = kResults.GetInt("TradeRouteProductionBonusTimes100");
	m_iLeaguePercent			= kResults.GetInt("LeaguePercent");
	m_iWarmongerPercent			= kResults.GetInt("WarmongerPercent");
	m_bVassalageEnabled			= kResults.GetBool("VassalageEnabled");

	m_strCityBombardEffectTag	= kResults.GetText("CityBombardEffectTag");
	m_uiCityBombardEffectTagHash = FStringHash(m_strCityBombardEffectTag);

	m_strAudioUnitVictoryScript	= kResults.GetText("AudioUnitVictoryScript");
	m_strAudioUnitDefeatScript	= kResults.GetText("AudioUnitDefeatScript");

	m_strArtPrefix	= kResults.GetText("ArtPrefix");

	m_strShortDesc = kResults.GetText("ShortDescription");
	m_strAbbreviation = kResults.GetText("Abbreviation");

	//City Names
	{
		m_vEraVOs.clear();

		std::string strKey = "Era - NewEraVOs";
		Database::Results* pResults = kUtility.GetResults(strKey);
		if(pResults == NULL)
		{
			pResults = kUtility.PrepareResults(strKey, "select VOScript from Era_NewEraVOs where EraType = ?");
		}

		pResults->Bind(1, GetType(), -1, false);

		while(pResults->Step())
		{
			m_vEraVOs.push_back(pResults->GetText(0));
		}

		pResults->Reset();
	}

	return true;
}


//------------------------------------------------------------------------------
// CvColorInfo
//------------------------------------------------------------------------------
const CvColorA& CvColorInfo::GetColor() const
{
	return m_Color;
}
//------------------------------------------------------------------------------
bool CvColorInfo::CacheResults(Database::Results& kResults, CvDatabaseUtility& kUtility)
{
	if(!CvBaseInfo::CacheResults(kResults, kUtility))
	{
		return false;
	}

	m_Color = CvColorA(kResults.GetFloat("Red"),
	                   kResults.GetFloat("Green"),
	                   kResults.GetFloat("Blue"),
	                   kResults.GetFloat("Alpha"));

	return true;
}
//------------------------------------------------------------------------------
CvPlayerColorInfo::CvPlayerColorInfo() : CvBaseInfo()
	, m_iColorTypePrimary(NO_COLOR)
	, m_iColorTypeSecondary(NO_COLOR)
	, m_iColorTypeText(NO_COLOR)
{}
//------------------------------------------------------------------------------
int CvPlayerColorInfo::GetColorTypePrimary() const
{
	return m_iColorTypePrimary;
}
//------------------------------------------------------------------------------
int CvPlayerColorInfo::GetColorTypeSecondary() const
{
	return m_iColorTypeSecondary;
}
//------------------------------------------------------------------------------
int CvPlayerColorInfo::GetColorTypeText() const
{
	return m_iColorTypeText;
}
//------------------------------------------------------------------------------
bool CvPlayerColorInfo::CacheResults(Database::Results& kResults, CvDatabaseUtility& kUtility)
{
	if(!CvBaseInfo::CacheResults(kResults, kUtility))
		return false;

	//Perform an inner join
	const char* szSQL =	"select PrimaryColor.ID, SecondaryColor.ID, TextColor.ID FROM PlayerColors INNER JOIN "
	                    "Colors As PrimaryColor ON PlayerColors.PrimaryColor = PrimaryColor.Type, "
	                    "Colors AS SecondaryColor ON PlayerColors.SecondaryColor = SecondaryColor.Type, "
	                    "Colors AS TextColor ON PlayerColors.TextColor = TextColor.Type where PlayerColors.ID = ? LIMIT 1; ";

	std::string strKey("ColorsLookup");
	Database::Results* pResults = kUtility.GetResults(strKey);
	if(pResults == NULL)
	{
		pResults = kUtility.PrepareResults(strKey, szSQL);
	}

	pResults->Bind(1, GetID());

	while(pResults->Step())
	{
		m_iColorTypePrimary		= pResults->GetInt(0);
		m_iColorTypeSecondary	= pResults->GetInt(1);
		m_iColorTypeText		= pResults->GetInt(2);
	}


	return true;
}

//======================================================================================================
//	CvGameOptionInfo
//	Game options and their default values
//======================================================================================================
CvGameOptionInfo::CvGameOptionInfo() :
	m_bDefault(false),
	m_bVisible(true)
{
}
//------------------------------------------------------------------------------
bool CvGameOptionInfo::getDefault() const
{
	return m_bDefault;
}
//------------------------------------------------------------------------------
bool CvGameOptionInfo::getVisible() const
{
	return m_bVisible;
}
//------------------------------------------------------------------------------
bool CvGameOptionInfo::CacheResults(Database::Results& kResults, CvDatabaseUtility& kUtility)
{
	if(!CvBaseInfo::CacheResults(kResults, kUtility))
		return false;

	m_bDefault = kResults.GetBool("Default");
	m_bVisible = kResults.GetBool("Visible");

	return true;
}

//======================================================================================================
//	CvMPOptionInfo
//	Multiplayer options and their default values
//======================================================================================================
CvMPOptionInfo::CvMPOptionInfo() :
	m_bDefault(false)
{
}
//------------------------------------------------------------------------------
bool CvMPOptionInfo::getDefault() const
{
	return m_bDefault;
}
//------------------------------------------------------------------------------
bool CvMPOptionInfo::CacheResults(Database::Results& kResults, CvDatabaseUtility& kUtility)
{
	if(!CvBaseInfo::CacheResults(kResults, kUtility))
		return false;

	m_bDefault = kResults.GetBool("Default");

	return true;
}

//======================================================================================================
//	CvPlayerOptionInfo
//	Player options and their default values
//======================================================================================================
CvPlayerOptionInfo::CvPlayerOptionInfo() :
	m_bDefault(false)
{
}
//------------------------------------------------------------------------------
bool CvPlayerOptionInfo::getDefault() const
{
	return m_bDefault;
}
//------------------------------------------------------------------------------
bool CvPlayerOptionInfo::CacheResults(Database::Results& kResults, CvDatabaseUtility& kUtility)
{
	if(!CvBaseInfo::CacheResults(kResults, kUtility))
		return false;

	m_bDefault = kResults.GetBool("Default");

	return true;
}

//======================================================================================================
//		CvVoteSourceInfo
//======================================================================================================
CvVoteSourceInfo::CvVoteSourceInfo() :
	m_iVoteInterval(0),
	m_iFreeSpecialist(NO_SPECIALIST),
	m_iPolicy(NO_POLICY)
{
}
//------------------------------------------------------------------------------
CvVoteSourceInfo::~CvVoteSourceInfo()
{
}
//------------------------------------------------------------------------------
int CvVoteSourceInfo::getVoteInterval() const
{
	return m_iVoteInterval;
}
//------------------------------------------------------------------------------
int CvVoteSourceInfo::getFreeSpecialist() const
{
	return m_iFreeSpecialist;
}
//------------------------------------------------------------------------------
int CvVoteSourceInfo::getPolicy() const
{
	return m_iPolicy;
}
//------------------------------------------------------------------------------
const CvString& CvVoteSourceInfo::getPopupText() const
{
	return m_strPopupText;
}
//------------------------------------------------------------------------------
const CvString& CvVoteSourceInfo::getSecretaryGeneralText() const
{
	return m_strSecretaryGeneralText;
}
//------------------------------------------------------------------------------
bool CvVoteSourceInfo::CacheResults(Database::Results& kResults, CvDatabaseUtility& kUtility)
{
	if(!CvBaseInfo::CacheResults(kResults, kUtility))
		return false;

	m_iVoteInterval = kResults.GetInt("VoteInterval");

	const char* szTextVal = NULL;
	szTextVal = kResults.GetText("PopupText");
	m_strPopupText = GetLocalizedText(szTextVal);

	szTextVal = kResults.GetText("SecretaryGeneralText");
	m_strSecretaryGeneralText = GetLocalizedText(szTextVal);

	const char* szFreeSpecialist = kResults.GetText("FreeSpecialist");
	m_iFreeSpecialist = GC.getInfoTypeForString(szFreeSpecialist, true);

	const char* szPolicy = kResults.GetText("Policy");
	m_iPolicy = GC.getInfoTypeForString(szPolicy, true);

	return true;
}
#if defined(MOD_BALANCE_CORE_EVENTS)
//======================================================================================================
//		CvModEventInfo
//======================================================================================================
CvModEventInfo::CvModEventInfo() :
	 m_iNumChoices(0),
	 m_iEventClass(-1),
	 m_iCooldown(0),
	 m_iRandomChance(0),
	 m_iRandomChanceDelta(0),
	 m_bGlobal(false),
	 m_bEraScaling(false),
	 m_iPrereqTech(-1),
	 m_iObsoleteTech(-1),
	 m_iMinimumNationalPopulation(0),
	 m_iMinimumNumberCities(0),
	 m_iRequiredCiv(-1),
	 m_iRequiredEra(-1),
	 m_iObsoleteEra(-1),
	 m_iRequiredPolicy(-1),
	 m_iIdeology(-1),
	 m_iRequiredImprovement(-1),
	 m_iUnitTypeRequired(-1),
	 m_iRequiredReligion(-1),
	 m_bRequiredPantheon(false),
	 m_iRequiredStateReligion(-1),
	 m_bHasStateReligion(false),
	 m_bUnhappy(false),
	 m_bSuperUnhappy(false),
	 m_iBuildingRequired(-1),
	 m_iBuildingLimiter(-1),
	 m_bRequiresHolyCity(false),
	 m_bRequiresIdeology(false),
	 m_bRequiresWar(false),
	 m_bRequiresWarMinor(false),
	 m_piRequiredResource(NULL),
	 m_piRequiredFeature(NULL),
	 m_piMinimumYield(NULL),
	 m_strSplashArt(""),
	 m_strEventAudio(""),
	 m_bOneShot(false),
	 m_bInDebt(false),
	 m_bLosingMoney(false),
	 m_bMetAnotherCiv(false),
	 m_bVassal(false),
	 m_bMaster(false),
	 m_iCoastal(0),
	 m_bTradeCapped(false),
	 m_paLinkerInfo(NULL),
	 m_iLinkerInfos(0)
{
}
//------------------------------------------------------------------------------
CvModEventInfo::~CvModEventInfo()
{
	SAFE_DELETE_ARRAY(m_piMinimumYield);
	SAFE_DELETE_ARRAY(m_piRequiredResource);
	SAFE_DELETE_ARRAY(m_piRequiredFeature);
	SAFE_DELETE_ARRAY(m_paLinkerInfo);
}
//------------------------------------------------------------------------------
int CvModEventInfo::getRandomChance() const
{
	return m_iRandomChance;
}
//------------------------------------------------------------------------------
int CvModEventInfo::getEventClass() const
{
	return m_iEventClass;
}
//------------------------------------------------------------------------------
int CvModEventInfo::getRandomChanceDelta() const
{
	return m_iRandomChanceDelta;
}
//------------------------------------------------------------------------------
int CvModEventInfo::getCooldown() const
{
	return m_iCooldown;
}
//------------------------------------------------------------------------------
int CvModEventInfo::getNumChoices() const
{
	return m_iNumChoices;
}
//------------------------------------------------------------------------------
const char* CvModEventInfo::getSplashArt() const
{
	return m_strSplashArt.c_str();
}
//------------------------------------------------------------------------------
const char* CvModEventInfo::getEventAudio() const
{
	return m_strEventAudio.c_str();
}
//------------------------------------------------------------------------------
bool CvModEventInfo::isGlobal() const
{
	return m_bGlobal;
}
//------------------------------------------------------------------------------
bool CvModEventInfo::isEraScaling() const
{
	return m_bEraScaling;
}

// Filters
bool CvModEventInfo::IgnoresGlobalCooldown() const
{
	return m_bIgnoresGlobalCooldown;
}
//------------------------------------------------------------------------------
int CvModEventInfo::getPrereqTech() const
{
	return m_iPrereqTech;
}
//------------------------------------------------------------------------------
int CvModEventInfo::getObsoleteTech() const
{
	return m_iObsoleteTech;
}
//------------------------------------------------------------------------------
int CvModEventInfo::getMinimumNationalPopulation() const
{
	return m_iMinimumNationalPopulation;
}
//------------------------------------------------------------------------------
int CvModEventInfo::getMinimumNumberCities() const
{
	return m_iMinimumNumberCities;
}
//------------------------------------------------------------------------------
int CvModEventInfo::getUnitTypeRequired() const
{
	return m_iUnitTypeRequired;
}
//------------------------------------------------------------------------------
int CvModEventInfo::getRequiredCiv() const
{
	return m_iRequiredCiv;
}
//------------------------------------------------------------------------------
int CvModEventInfo::getRequiredEra() const
{
	return m_iRequiredEra;
}
//------------------------------------------------------------------------------
int CvModEventInfo::getObsoleteEra() const
{
	return m_iObsoleteEra;
}
//------------------------------------------------------------------------------
int CvModEventInfo::getRequiredImprovement() const
{
	return m_iRequiredImprovement;
}
//------------------------------------------------------------------------------
int CvModEventInfo::getRequiredPolicy() const
{
	return m_iRequiredPolicy;
}
//------------------------------------------------------------------------------
int CvModEventInfo::getRequiredIdeology() const
{
	return m_iIdeology;
}
//------------------------------------------------------------------------------
int CvModEventInfo::getRequiredReligion() const
{
	return m_iRequiredReligion;
}
//------------------------------------------------------------------------------
bool CvModEventInfo::hasPantheon() const
{
	return m_bRequiredPantheon;
}
//------------------------------------------------------------------------------
int CvModEventInfo::getRequiredStateReligion() const
{
	return m_iRequiredStateReligion;
}
//------------------------------------------------------------------------------
bool CvModEventInfo::hasStateReligion() const
{
	return m_bHasStateReligion;
}
//------------------------------------------------------------------------------
bool CvModEventInfo::isUnhappy() const
{
	return m_bUnhappy;
}
//------------------------------------------------------------------------------
bool CvModEventInfo::isSuperUnhappy() const
{
	return m_bSuperUnhappy;
}
//------------------------------------------------------------------------------
int CvModEventInfo::getBuildingRequired() const
{
	return m_iBuildingRequired;
}
//------------------------------------------------------------------------------
int CvModEventInfo::getBuildingLimiter() const
{
	return m_iBuildingLimiter;
}
//------------------------------------------------------------------------------
bool CvModEventInfo::isRequiresIdeology() const
{
	return m_bRequiresIdeology;
}
//------------------------------------------------------------------------------
bool CvModEventInfo::isRequiresWar() const
{
	return m_bRequiresWar;
}
//------------------------------------------------------------------------------
bool CvModEventInfo::isRequiresWarMinor() const
{
	return m_bRequiresWarMinor;
}
//------------------------------------------------------------------------------
bool CvModEventInfo::isRequiresHolyCity() const
{
	return m_bRequiresHolyCity;
}
//------------------------------------------------------------------------------
int CvModEventInfo::getYieldMinimum(YieldTypes eYield) const
{
	ASSERT_DEBUG(eYield < NUM_YIELD_TYPES, "Index out of bounds");
	ASSERT_DEBUG(eYield > -1, "Index out of bounds");
	return m_piMinimumYield ? m_piMinimumYield[eYield] : -1;
}
//------------------------------------------------------------------------------
int CvModEventInfo::getResourceRequired(ResourceTypes eResource) const
{
	ASSERT_DEBUG(eResource < GC.getNumResourceInfos(), "Index out of bounds");
	ASSERT_DEBUG(eResource > -1, "Index out of bounds");
	return m_piRequiredResource ? m_piRequiredResource[eResource] : -1;
}
//------------------------------------------------------------------------------
int CvModEventInfo::getFeatureRequired(FeatureTypes eFeature) const
{
	ASSERT_DEBUG(eFeature < GC.getNumFeatureInfos(), "Index out of bounds");
	ASSERT_DEBUG(eFeature > -1, "Index out of bounds");
	return m_piRequiredFeature ? m_piRequiredFeature[eFeature] : -1;
}
//------------------------------------------------------------------------------
bool CvModEventInfo::isOneShot() const
{
	return m_bOneShot;
}
//------------------------------------------------------------------------------
bool CvModEventInfo::hasMetAnotherCiv() const
{
	return m_bMetAnotherCiv;
}
//------------------------------------------------------------------------------
bool CvModEventInfo::isInDebt() const
{
	return m_bInDebt;
}
//------------------------------------------------------------------------------
bool CvModEventInfo::isLosingMoney() const
{
	return m_bLosingMoney;
}
//------------------------------------------------------------------------------
bool CvModEventInfo::isVassal() const
{
	return m_bVassal;
}
//------------------------------------------------------------------------------
bool CvModEventInfo::isMaster() const
{
	return m_bMaster;
}
//------------------------------------------------------------------------------
int CvModEventInfo::getNumCoastalRequired() const
{
	return m_iCoastal;
}
//------------------------------------------------------------------------------
bool CvModEventInfo::isTradeCapped() const
{
	return m_bTradeCapped;
}
CvEventLinkingInfo *CvModEventInfo::GetLinkerInfo(int i) const
{
	ASSERT_DEBUG(i < GetNumLinkers(), "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");

	if (m_paLinkerInfo[0].GetCityLinkingEvent() == -1 && m_paLinkerInfo[0].GetCityLinkingEventChoice() == -1 && m_paLinkerInfo[0].GetLinkingEvent() == -1 && m_paLinkerInfo[0].GetLinkingEventChoice() == -1)
	{
		return NULL;
	}
	else
	{
		return &m_paLinkerInfo[i];
	}
}
//------------------------------------------------------------------------------
bool CvModEventInfo::CacheResults(Database::Results& kResults, CvDatabaseUtility& kUtility)
{
	if(!CvBaseInfo::CacheResults(kResults, kUtility))
		return false;


	m_bOneShot = kResults.GetBool("IsOneShot");

	m_bIgnoresGlobalCooldown = kResults.GetBool("IgnoresGlobalCooldown");

	const char* szTextVal = NULL;

	szTextVal = kResults.GetText("EventClass");
	m_iEventClass = GC.getInfoTypeForString(szTextVal, true);
	
	m_iRandomChance = kResults.GetInt("RandomChance");
	m_iRandomChanceDelta = kResults.GetInt("RandomChanceDelta");

	m_iNumChoices = kResults.GetInt("NumChoices");
	m_bGlobal = kResults.GetBool("Global");
	m_bEraScaling = kResults.GetBool("EraScaling");
	m_iCooldown = kResults.GetInt("EventCooldown");
	
	szTextVal = kResults.GetText("EventAudio");
	m_strEventAudio = szTextVal;

	szTextVal = kResults.GetText("EventArt");
	m_strSplashArt = szTextVal;

	// Filters
	m_bMetAnotherCiv = kResults.GetBool("HasMetAMajorCiv");
	m_bInDebt = kResults.GetBool("InDebt");
	m_bLosingMoney = kResults.GetBool("LosingMoney");

	m_bVassal = kResults.GetBool("IsVassalOfSomeone");
	m_bMaster = kResults.GetBool("IsMasterOfSomeone");

	szTextVal = kResults.GetText("PrereqTech");
	m_iPrereqTech =  GC.getInfoTypeForString(szTextVal, true);

	szTextVal = kResults.GetText("ObsoleteTech");
	m_iObsoleteTech =  GC.getInfoTypeForString(szTextVal, true);

	m_iMinimumNationalPopulation = kResults.GetInt("MinimumNationalPopulation");
	m_iMinimumNumberCities = kResults.GetInt("MinimumNumberCities");

	szTextVal = kResults.GetText("UnitClassRequired");
	m_iUnitTypeRequired =  GC.getInfoTypeForString(szTextVal, true);

	szTextVal = kResults.GetText("ImprovementAnywhereRequired");
	m_iRequiredImprovement =  GC.getInfoTypeForString(szTextVal, true);

	szTextVal = kResults.GetText("RequiredCiv");
	m_iRequiredCiv =  GC.getInfoTypeForString(szTextVal, true);

	szTextVal = kResults.GetText("RequiredEra");
	m_iRequiredEra =  GC.getInfoTypeForString(szTextVal, true);

	szTextVal = kResults.GetText("ObsoleteEra");
	m_iObsoleteEra =  GC.getInfoTypeForString(szTextVal, true);

	szTextVal = kResults.GetText("RequiredPolicy");
	m_iRequiredPolicy =  GC.getInfoTypeForString(szTextVal, true);

	szTextVal = kResults.GetText("RequiredIdeology");
	m_iIdeology =  GC.getInfoTypeForString(szTextVal, true);

	m_bRequiresHolyCity = kResults.GetBool("RequiresHolyCity");
	
	szTextVal = kResults.GetText("RequiredReligion");
	m_iRequiredReligion =  GC.getInfoTypeForString(szTextVal, true);

	m_bRequiredPantheon = kResults.GetBool("RequiresPantheon");

	m_bRequiresIdeology = kResults.GetBool("RequiresIdeology");
	m_bRequiresWar = kResults.GetBool("RequiresWar");
	m_bRequiresWarMinor = kResults.GetBool("RequiresWarMinor");

	szTextVal = kResults.GetText("RequiredAnywhereBuildingClass");
	m_iBuildingRequired =  GC.getInfoTypeForString(szTextVal, true);

	szTextVal = kResults.GetText("RequiredNowhereBuildingClass");
	m_iBuildingLimiter =  GC.getInfoTypeForString(szTextVal, true);

	szTextVal = kResults.GetText("RequiredStateReligion");
	m_iRequiredStateReligion =  GC.getInfoTypeForString(szTextVal, true);

	m_bHasStateReligion = kResults.GetBool("RequiresAnyStateReligion");

	m_bUnhappy = kResults.GetBool("IsUnhappy");
	m_bSuperUnhappy = kResults.GetBool("IsSuperUnhappy");

	m_iCoastal = kResults.GetInt("MinimumNumCoastalCities");
	m_bTradeCapped = kResults.GetBool("LessThanMaximumTradeRoutes");
	
	const char* szEventType = GetType();
	kUtility.SetYields(m_piMinimumYield, "Event_MinimumStartYield", "EventType", szEventType);

	kUtility.PopulateArrayByValue(m_piRequiredResource, "Resources", "Event_MinimumResourceRequired", "ResourceType", "EventType", szEventType, "Quantity");
	kUtility.PopulateArrayByValue(m_piRequiredFeature, "Features", "Event_MinimumFeatureRequired", "FeatureType", "EventType", szEventType, "Quantity");

	{
		//Initialize Linker Table
		const size_t iNumLinkers = kUtility.MaxRows("Notifications");
		m_paLinkerInfo = FNEW(CvEventLinkingInfo[iNumLinkers], c_eCiv5GameplayDLL, 0);
		int idx = 0;

		std::string strEventTypesKey = "Event_EventLinks";
		Database::Results* pEventTypes = kUtility.GetResults(strEventTypesKey);
		if(pEventTypes == NULL)
		{
			pEventTypes = kUtility.PrepareResults(strEventTypesKey, "select EventLinker, EventChoice, CityEvent, CityEventChoice, CheckKnownPlayers, CheckForActive from Event_EventLinks where EventType = ?");
		}

		const size_t lenEventType = strlen(szEventType);
		pEventTypes->Bind(1, szEventType, lenEventType, false);

		while(pEventTypes->Step())
		{

			CvEventLinkingInfo& pEventLinkingInfo= m_paLinkerInfo[idx];
			szTextVal = pEventTypes->GetText("EventLinker");
			pEventLinkingInfo.m_iEvent =  GC.getInfoTypeForString(szTextVal, true);
			szTextVal = pEventTypes->GetText("EventChoice");
			pEventLinkingInfo.m_iEventChoice =  GC.getInfoTypeForString(szTextVal, true);
			szTextVal = pEventTypes->GetText("CityEvent");
			pEventLinkingInfo.m_iCityEvent =  GC.getInfoTypeForString(szTextVal, true);
			szTextVal = pEventTypes->GetText("CityEventChoice");
			pEventLinkingInfo.m_iCityEventChoice =  GC.getInfoTypeForString(szTextVal, true);
			pEventLinkingInfo.m_bCheckWorld = pEventTypes->GetBool("CheckKnownPlayers");
			pEventLinkingInfo.m_bActive = pEventTypes->GetBool("CheckForActive");
			idx++;
		}

		m_iLinkerInfos = idx;
		pEventTypes->Reset();
	}	
	return true;
}
//======================================================================================================
//		CvModEventChoiceInfo
//======================================================================================================
CvModEventChoiceInfo::CvModEventChoiceInfo() :
	 m_iEventPolicy(-1),
	 m_iEventBuilding(-1),
	 m_iEventPromotion(-1),
	 m_iEventTech(-1),
	 m_iEventDuration(0),
	 m_bEventDurationScaling(true),
	 m_iEventChance(0),
	 m_bEraScaling(false),
	 m_bExpires(false),
	 m_iNumFreePolicies(0),
	 m_iNumFreeTechs(0),
	 m_iNumFreeGreatPeople(0),
	 m_iNumGoldenAgeTurns(0),
	 m_iNumWLTKD(0),
	 m_iResistanceTurns(0),
	 m_iRandomBarbs(0),
	 m_piNumFreeUnits(NULL),
	 m_piNumFreeSpecificUnits(NULL),
	 m_iPrereqTech(-1),
	 m_iObsoleteTech(-1),
	 m_iMinimumNationalPopulation(0),
	 m_iMinimumNumberCities(0),
	 m_iRequiredCiv(-1),
	 m_iRequiredEra(-1),
	 m_iObsoleteEra(-1),
	 m_iRequiredPolicy(-1),
	 m_iIdeology(-1),
	 m_iRequiredImprovement(-1),
	 m_iUnitTypeRequired(-1),
	 m_iRequiredReligion(-1),
	 m_bRequiredPantheon(false),
	 m_iRequiredStateReligion(-1),
	 m_bHasStateReligion(false),
	 m_bRequiresHolyCity(false),
	 m_bRequiresIdeology(false),
	 m_bUnhappy(false),
	 m_bSuperUnhappy(false),
	 m_bRequiresWar(false),
	 m_iBuildingRequired(-1),
	 m_iBuildingLimiter(-1),
	 m_piMinimumYield(NULL),
	 m_bRequiresWarMinor(false),
	 m_piRequiredResource(NULL),
	 m_piRequiredFeature(NULL),
	 m_piResourceChange(NULL),
	 m_piFlavor(NULL),
	 m_piEventYield(NULL),
	 m_piPreCheckEventYield(NULL),
	 m_strEventChoiceSoundEffect(""),
	 m_piConvertReligion(NULL),
	 m_piConvertReligionPercent(NULL),
	 m_ppiBuildingClassYield(NULL),
	 m_ppiBuildingClassYieldModifier(NULL),
	 m_piCityYield(NULL),
	 m_pbParentEventIDs(NULL),
	 m_bOneShot(false),
	 m_bMetAnotherCiv(false),
	 m_bInDebt(false),
	 m_bLosingMoney(false),
	 m_iPlayerHappiness(0),
	 m_iCityHappinessGlobal(0),
	 m_iFreeScaledUnits(0),
	 m_iSpecialistsGreatPersonPointsPerTurn(0),
	 m_strDisabledTooltip(""),
	 m_bVassal(false),
	 m_bMaster(false),
	 m_ppiTerrainYield(NULL),
	 m_ppiFeatureYield(NULL),
	 m_ppiImprovementYield(NULL),
	 m_ppiResourceYield(NULL),
	 m_iCoastal(0),
	 m_bCoastalOnly(false),
	 m_bTradeCapped(false),
	 m_bCapitalEffectOnly(false),
	 m_bInstantYieldAllCities(false),
	 m_paLinkerInfo(NULL),
	 m_iLinkerInfos(0),
	 m_iBasicNeedsMedianModifierGlobal(0),
	 m_iGoldMedianModifierGlobal(0),
	 m_iScienceMedianModifierGlobal(0),
	 m_iCultureMedianModifierGlobal(0),
	 m_iReligiousUnrestModifierGlobal(0)
{
}
//------------------------------------------------------------------------------
CvModEventChoiceInfo::~CvModEventChoiceInfo()
{
	SAFE_DELETE_ARRAY(m_piFlavor);
	SAFE_DELETE_ARRAY(m_piResourceChange);
	SAFE_DELETE_ARRAY(m_piEventYield);
	SAFE_DELETE_ARRAY(m_piPreCheckEventYield);
	SAFE_DELETE_ARRAY(m_piNumFreeUnits);
	SAFE_DELETE_ARRAY(m_piNumFreeSpecificUnits);
	SAFE_DELETE_ARRAY(m_piMinimumYield);
	SAFE_DELETE_ARRAY(m_piRequiredResource);
	SAFE_DELETE_ARRAY(m_piRequiredFeature);
	SAFE_DELETE_ARRAY(m_piConvertReligion);
	SAFE_DELETE_ARRAY(m_piConvertReligionPercent);
	SAFE_DELETE_ARRAY(m_piCityYield);
	SAFE_DELETE_ARRAY(m_pbParentEventIDs);
	CvDatabaseUtility::SafeDelete2DArray(m_ppiBuildingClassYield);
	CvDatabaseUtility::SafeDelete2DArray(m_ppiBuildingClassYieldModifier);
	CvDatabaseUtility::SafeDelete2DArray(m_ppiTerrainYield);
	CvDatabaseUtility::SafeDelete2DArray(m_ppiFeatureYield);
	CvDatabaseUtility::SafeDelete2DArray(m_ppiSpecialistYield);
	CvDatabaseUtility::SafeDelete2DArray(m_ppiImprovementYield);
	CvDatabaseUtility::SafeDelete2DArray(m_ppiResourceYield);
	SAFE_DELETE_ARRAY(m_paLinkerInfo);
}
//------------------------------------------------------------------------------
bool CvModEventChoiceInfo::isParentEvent(EventTypes eEvent) const
{
	ASSERT_DEBUG(eEvent < GC.getNumEventInfos(), "Index out of bounds");
	ASSERT_DEBUG(eEvent > -1, "Index out of bounds");
	return m_pbParentEventIDs ? m_pbParentEventIDs[eEvent] : false;
}

//------------------------------------------------------------------------------
int CvModEventChoiceInfo::getEventPromotion() const
{
	return m_iEventPromotion;
}
//------------------------------------------------------------------------------
int CvModEventChoiceInfo::getEventTech() const
{
	return m_iEventTech;
}

//------------------------------------------------------------------------------
int CvModEventChoiceInfo::getEventPolicy() const
{
	return m_iEventPolicy;
}
//------------------------------------------------------------------------------
int CvModEventChoiceInfo::getEventDuration() const
{
	return m_iEventDuration;
}
//------------------------------------------------------------------------------
bool CvModEventChoiceInfo::isEventDurationScaling() const
{
	return m_bEventDurationScaling;
}
//------------------------------------------------------------------------------
int CvModEventChoiceInfo::getEventChance() const
{
	return m_iEventChance;
}
//------------------------------------------------------------------------------
bool CvModEventChoiceInfo::IsEraScaling() const
{
	return m_bEraScaling;
}
//------------------------------------------------------------------------------
bool CvModEventChoiceInfo::Expires() const
{
	return m_bExpires;
}
//------------------------------------------------------------------------------
int CvModEventChoiceInfo::getEventBuilding() const
{
	return m_iEventBuilding;
}
//------------------------------------------------------------------------------
int CvModEventChoiceInfo::getFlavorValue(int i) const
{
	ASSERT_DEBUG(i < GC.getNumFlavorTypes(), "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_piFlavor ? m_piFlavor[i] : 0;
}
//------------------------------------------------------------------------------
int CvModEventChoiceInfo::getEventResourceChange(ResourceTypes eResource) const
{
	ASSERT_DEBUG(eResource < GC.getNumResourceInfos(), "Index out of bounds");
	ASSERT_DEBUG(eResource > -1, "Index out of bounds");
	return m_piResourceChange ? m_piResourceChange[eResource] : 0;
}
//------------------------------------------------------------------------------
int CvModEventChoiceInfo::getEventYield(YieldTypes eYield) const
{
	ASSERT_DEBUG(eYield < NUM_YIELD_TYPES, "Index out of bounds");
	ASSERT_DEBUG(eYield > -1, "Index out of bounds");
	return m_piEventYield ? m_piEventYield[eYield] : -1;
}
//------------------------------------------------------------------------------
int CvModEventChoiceInfo::getPreCheckEventYield(YieldTypes eYield) const
{
	ASSERT_DEBUG(eYield < NUM_YIELD_TYPES, "Index out of bounds");
	ASSERT_DEBUG(eYield > -1, "Index out of bounds");
	return m_piPreCheckEventYield ? m_piPreCheckEventYield[eYield] : -1;
}
CvEventNotificationInfo *CvModEventChoiceInfo::GetNotificationInfo(int i) const
{
//	ASSERT_DEBUG(i < GC.getNumNotificationInfos(), "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");

	if (m_paNotificationInfo[0].GetNotificationString().empty() || m_paNotificationInfo[0].GetNotificationString() == NULL)
	{
		return NULL;
	}
	else
	{
		return &m_paNotificationInfo[i];
	}
}
//------------------------------------------------------------------------------
const char* CvModEventChoiceInfo::getEventChoiceSoundEffect() const
{
	return m_strEventChoiceSoundEffect.c_str();
}
//------------------------------------------------------------------------------
int CvModEventChoiceInfo::getNumFreePolicies() const
{
	return m_iNumFreePolicies;
}
//------------------------------------------------------------------------------
int CvModEventChoiceInfo::getNumFreeTechs() const
{
	return m_iNumFreeTechs;
}
//------------------------------------------------------------------------------
int CvModEventChoiceInfo::getNumFreeGreatPeople() const
{
	return m_iNumFreeGreatPeople;
}
//------------------------------------------------------------------------------
int CvModEventChoiceInfo::getGoldenAgeTurns() const
{
	return m_iNumGoldenAgeTurns;
}
//------------------------------------------------------------------------------
int CvModEventChoiceInfo::getWLTKD() const
{
	return m_iNumWLTKD;
}
//------------------------------------------------------------------------------
int CvModEventChoiceInfo::getResistanceTurns() const
{
	return m_iResistanceTurns;
}
//------------------------------------------------------------------------------
int CvModEventChoiceInfo::getRandomBarbs() const
{
	return m_iRandomBarbs;
}
//------------------------------------------------------------------------------
int CvModEventChoiceInfo::getFreeScaledUnits() const
{
	return m_iFreeScaledUnits;
}
//------------------------------------------------------------------------------
int CvModEventChoiceInfo::getSpecialistsGreatPersonPointsPerTurn() const
{
	return m_iSpecialistsGreatPersonPointsPerTurn;
}
//------------------------------------------------------------------------------
int CvModEventChoiceInfo::getPlayerHappiness() const
{
	return m_iPlayerHappiness;
}
//------------------------------------------------------------------------------
int CvModEventChoiceInfo::getCityHappinessGlobal() const
{
	return m_iCityHappinessGlobal;
}
//------------------------------------------------------------------------------
int CvModEventChoiceInfo::getBasicNeedsMedianModifierGlobal() const
{
	return m_iBasicNeedsMedianModifierGlobal;
}
//------------------------------------------------------------------------------
int CvModEventChoiceInfo::getGoldMedianModifierGlobal() const
{
	return m_iGoldMedianModifierGlobal;
}
//------------------------------------------------------------------------------
int CvModEventChoiceInfo::getScienceMedianModifierGlobal() const
{
	return m_iScienceMedianModifierGlobal;
}
//------------------------------------------------------------------------------
int CvModEventChoiceInfo::getCultureMedianModifierGlobal() const
{
	return m_iCultureMedianModifierGlobal;
}
//------------------------------------------------------------------------------
int CvModEventChoiceInfo::getReligiousUnrestModifierGlobal() const
{
	return m_iReligiousUnrestModifierGlobal;
}
//------------------------------------------------------------------------------
int CvModEventChoiceInfo::getNumFreeSpecificUnits(int i) const
{
	ASSERT_DEBUG(i < GC.getNumUnitInfos(), "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_piNumFreeSpecificUnits ? m_piNumFreeSpecificUnits [i] : -1;
}
//------------------------------------------------------------------------------
int CvModEventChoiceInfo::getNumFreeUnits(int i) const
{
	ASSERT_DEBUG(i < GC.getNumUnitClassInfos(), "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_piNumFreeUnits ? m_piNumFreeUnits[i] : -1;
}
//------------------------------------------------------------------------------
int CvModEventChoiceInfo::getEventConvertReligion(int i) const
{
	ASSERT_DEBUG(i < GC.getNumReligionInfos(), "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_piConvertReligion ? m_piConvertReligion[i] : -1;
}
//------------------------------------------------------------------------------
int CvModEventChoiceInfo::getEventConvertReligionPercent(int i) const
{
	ASSERT_DEBUG(i < GC.getNumReligionInfos(), "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_piConvertReligionPercent ? m_piConvertReligionPercent[i] : -1;
}
//------------------------------------------------------------------------------
int CvModEventChoiceInfo::getCityYield(int i) const
{
	ASSERT_DEBUG(i < NUM_YIELD_TYPES, "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_piCityYield ? m_piCityYield[i] : -1;
}
/// Yield change for a specific BuildingClass by yield type
int CvModEventChoiceInfo::getBuildingClassYield(int i, int j) const
{
	ASSERT_DEBUG(i < GC.getNumBuildingClassInfos(), "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	ASSERT_DEBUG(j < NUM_YIELD_TYPES, "Index out of bounds");
	ASSERT_DEBUG(j > -1, "Index out of bounds");
	return m_ppiBuildingClassYield[i][j];
}
/// Yield modifier change for a specific BuildingClass by yield type
int CvModEventChoiceInfo::getBuildingClassYieldModifier(int i, int j) const
{
	ASSERT_DEBUG(i < GC.getNumBuildingClassInfos(), "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	ASSERT_DEBUG(j < NUM_YIELD_TYPES, "Index out of bounds");
	ASSERT_DEBUG(j > -1, "Index out of bounds");
	return m_ppiBuildingClassYieldModifier[i][j];
}
int CvModEventChoiceInfo::getTerrainYield(int i, int j) const
{
	ASSERT_DEBUG(i < GC.getNumTerrainInfos(), "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	ASSERT_DEBUG(j < NUM_YIELD_TYPES, "Index out of bounds");
	ASSERT_DEBUG(j > -1, "Index out of bounds");
	return m_ppiTerrainYield[i][j];
}
int CvModEventChoiceInfo::getFeatureYield(int i, int j) const
{
	ASSERT_DEBUG(i < GC.getNumFeatureInfos(), "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	ASSERT_DEBUG(j < NUM_YIELD_TYPES, "Index out of bounds");
	ASSERT_DEBUG(j > -1, "Index out of bounds");
	return m_ppiFeatureYield[i][j];
}
int CvModEventChoiceInfo::getImprovementYield(int i, int j) const
{
	ASSERT_DEBUG(i < GC.getNumImprovementInfos(), "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	ASSERT_DEBUG(j < NUM_YIELD_TYPES, "Index out of bounds");
	ASSERT_DEBUG(j > -1, "Index out of bounds");
	return m_ppiImprovementYield[i][j];
}
int CvModEventChoiceInfo::getResourceYield(int i, int j) const
{
	ASSERT_DEBUG(i < GC.getNumResourceInfos(), "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	ASSERT_DEBUG(j < NUM_YIELD_TYPES, "Index out of bounds");
	ASSERT_DEBUG(j > -1, "Index out of bounds");
	return m_ppiResourceYield[i][j];
}
//------------------------------------------------------------------------------
int CvModEventChoiceInfo::getGlobalSpecialistYieldChange(int i, int j) const
{
	ASSERT_DEBUG(i < GC.getNumSpecialistInfos(), "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	ASSERT_DEBUG(j < NUM_YIELD_TYPES, "Index out of bounds");
	ASSERT_DEBUG(j > -1, "Index out of bounds");
	return m_ppiSpecialistYield[i][j];
}
// Filters
//------------------------------------------------------------------------------
int CvModEventChoiceInfo::getPrereqTech() const
{
	return m_iPrereqTech;
}
//------------------------------------------------------------------------------
int CvModEventChoiceInfo::getObsoleteTech() const
{
	return m_iObsoleteTech;
}
//------------------------------------------------------------------------------
int CvModEventChoiceInfo::getMinimumNationalPopulation() const
{
	return m_iMinimumNationalPopulation;
}
//------------------------------------------------------------------------------
int CvModEventChoiceInfo::getMinimumNumberCities() const
{
	return m_iMinimumNumberCities;
}
//------------------------------------------------------------------------------
int CvModEventChoiceInfo::getUnitTypeRequired() const
{
	return m_iUnitTypeRequired;
}
//------------------------------------------------------------------------------
int CvModEventChoiceInfo::getRequiredCiv() const
{
	return m_iRequiredCiv;
}
//------------------------------------------------------------------------------
int CvModEventChoiceInfo::getRequiredEra() const
{
	return m_iRequiredEra;
}
//------------------------------------------------------------------------------
int CvModEventChoiceInfo::getObsoleteEra() const
{
	return m_iObsoleteEra;
}
//------------------------------------------------------------------------------
int CvModEventChoiceInfo::getYieldMinimum(YieldTypes eYield) const
{
	ASSERT_DEBUG(eYield < NUM_YIELD_TYPES, "Index out of bounds");
	ASSERT_DEBUG(eYield > -1, "Index out of bounds");
	return m_piMinimumYield ? m_piMinimumYield[eYield] : -1;
}
//------------------------------------------------------------------------------
int CvModEventChoiceInfo::getRequiredImprovement() const
{
	return m_iRequiredImprovement;
}
//------------------------------------------------------------------------------
int CvModEventChoiceInfo::getRequiredPolicy() const
{
	return m_iRequiredPolicy;
}
//------------------------------------------------------------------------------
int CvModEventChoiceInfo::getRequiredIdeology() const
{
	return m_iIdeology;
}
//------------------------------------------------------------------------------
bool CvModEventChoiceInfo::isRequiresHolyCity() const
{
	return m_bRequiresHolyCity;
}
//------------------------------------------------------------------------------
int CvModEventChoiceInfo::getRequiredReligion() const
{
	return m_iRequiredReligion;
}
//------------------------------------------------------------------------------
bool CvModEventChoiceInfo::hasPantheon() const
{
	return m_bRequiredPantheon;
}
//------------------------------------------------------------------------------
int CvModEventChoiceInfo::getBuildingRequired() const
{
	return m_iBuildingRequired;
}
//------------------------------------------------------------------------------
int CvModEventChoiceInfo::getBuildingLimiter() const
{
	return m_iBuildingLimiter;
}
//------------------------------------------------------------------------------
bool CvModEventChoiceInfo::isRequiresIdeology() const
{
	return m_bRequiresIdeology;
}
//------------------------------------------------------------------------------
bool CvModEventChoiceInfo::isRequiresWar() const
{
	return m_bRequiresWar;
}
//------------------------------------------------------------------------------
bool CvModEventChoiceInfo::isRequiresWarMinor() const
{
	return m_bRequiresWarMinor;
}
//------------------------------------------------------------------------------
int CvModEventChoiceInfo::getResourceRequired(ResourceTypes eResource) const
{
	ASSERT_DEBUG(eResource < GC.getNumResourceInfos(), "Index out of bounds");
	ASSERT_DEBUG(eResource > -1, "Index out of bounds");
	return m_piRequiredResource ? m_piRequiredResource[eResource] : -1;
}
//------------------------------------------------------------------------------
int CvModEventChoiceInfo::getFeatureRequired(FeatureTypes eFeature) const
{
	ASSERT_DEBUG(eFeature < GC.getNumFeatureInfos(), "Index out of bounds");
	ASSERT_DEBUG(eFeature > -1, "Index out of bounds");
	return m_piRequiredFeature ? m_piRequiredFeature[eFeature] : -1;
}

//------------------------------------------------------------------------------
int CvModEventChoiceInfo::getRequiredStateReligion() const
{
	return m_iRequiredStateReligion;
}
//------------------------------------------------------------------------------
bool CvModEventChoiceInfo::hasStateReligion() const
{
	return m_bHasStateReligion;
}
//------------------------------------------------------------------------------
bool CvModEventChoiceInfo::isUnhappy() const
{
	return m_bUnhappy;
}
//------------------------------------------------------------------------------
bool CvModEventChoiceInfo::isSuperUnhappy() const
{
	return m_bSuperUnhappy;
}
//------------------------------------------------------------------------------
bool CvModEventChoiceInfo::isOneShot() const
{
	return m_bOneShot;
}
//------------------------------------------------------------------------------
bool CvModEventChoiceInfo::hasMetAnotherCiv() const
{
	return m_bMetAnotherCiv;
}
//------------------------------------------------------------------------------
bool CvModEventChoiceInfo::isInDebt() const
{
	return m_bInDebt;
}
//------------------------------------------------------------------------------
bool CvModEventChoiceInfo::isLosingMoney() const
{
	return m_bLosingMoney;
}
//------------------------------------------------------------------------------
bool CvModEventChoiceInfo::isVassal() const
{
	return m_bVassal;
}
//------------------------------------------------------------------------------
bool CvModEventChoiceInfo::isMaster() const
{
	return m_bMaster;
}
//------------------------------------------------------------------------------
int CvModEventChoiceInfo::getNumCoastalRequired() const
{
	return m_iCoastal;
}
//------------------------------------------------------------------------------
bool CvModEventChoiceInfo::isCoastalOnly() const
{
	return m_bCoastalOnly;
}
//------------------------------------------------------------------------------
bool CvModEventChoiceInfo::isTradeCapped() const
{
	return m_bTradeCapped;
}
//------------------------------------------------------------------------------
bool CvModEventChoiceInfo::isCapitalEffectOnly() const
{
	return m_bCapitalEffectOnly;
}
//------------------------------------------------------------------------------
bool CvModEventChoiceInfo::isInstantYieldAllCities() const
{
	return m_bInstantYieldAllCities;
}
//------------------------------------------------------------------------------
const char* CvModEventChoiceInfo::getDisabledTooltip() const
{
	return m_strDisabledTooltip;
}
CvEventChoiceLinkingInfo *CvModEventChoiceInfo::GetLinkerInfo(int i) const
{
	ASSERT_DEBUG(i < GetNumLinkers(), "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");

	if (m_paLinkerInfo[0].GetCityLinkingEvent() == -1 && m_paLinkerInfo[0].GetCityLinkingEventChoice() == -1 && m_paLinkerInfo[0].GetLinkingEvent() == -1 && m_paLinkerInfo[0].GetLinkingEventChoice() == -1)
	{
		return NULL;
	}
	else
	{
		return &m_paLinkerInfo[i];
	}
}
//------------------------------------------------------------------------------
bool CvModEventChoiceInfo::CacheResults(Database::Results& kResults, CvDatabaseUtility& kUtility)
{
	if(!CvBaseInfo::CacheResults(kResults, kUtility))
		return false;

	const char* szTextVal = NULL;

	const char* szEventType = GetType();
	kUtility.PopulateArrayByExistence(m_pbParentEventIDs, "Events", "Event_ParentEvents", "EventType", "EventChoiceType", szEventType);

	szTextVal = kResults.GetText("EventPolicy");
	m_iEventPolicy =  GC.getInfoTypeForString(szTextVal, true);

	szTextVal = kResults.GetText("EventTech");
	m_iEventTech =  GC.getInfoTypeForString(szTextVal, true);

	szTextVal = kResults.GetText("EventBuildingClassGlobal");
	m_iEventBuilding =  GC.getInfoTypeForString(szTextVal, true);

	m_iEventDuration = kResults.GetInt("EventDuration");
	m_bEventDurationScaling = kResults.GetBool("EventDurationScaling");

	m_iEventChance = kResults.GetInt("EventChance");

	m_iCoastal = kResults.GetInt("MinimumNumCoastalCities");
	m_bCoastalOnly = kResults.GetBool("AffectsCoastalCitiesOnly");

	m_bTradeCapped = kResults.GetBool("LessThanMaximumTradeRoutes");

	m_bCapitalEffectOnly = kResults.GetBool("CapitalEffectOnly");
	m_bInstantYieldAllCities = kResults.GetBool("YieldBonusAllCities");
	
	m_iNumFreePolicies = kResults.GetInt("EventFreePolicies");
	m_iNumFreeTechs = kResults.GetInt("EventFreeTechs");
	m_iNumFreeGreatPeople = kResults.GetInt("FreeGreatPeople");
	m_iNumGoldenAgeTurns = kResults.GetInt("GoldenAgeTurns");
	m_iNumWLTKD = kResults.GetInt("WLTKDTurns");
	m_iResistanceTurns = kResults.GetInt("ResistanceTurns");
	m_iRandomBarbs = kResults.GetInt("RandomBarbarianSpawn");
	m_iFreeScaledUnits = kResults.GetInt("FreeUnitsTechAppropriate");
	m_iSpecialistsGreatPersonPointsPerTurn = kResults.GetInt("SpecialistsGreatPersonPointsPerTurn");

	m_iPlayerHappiness = kResults.GetInt("PlayerHappiness");
	m_iCityHappinessGlobal = kResults.GetInt("HappinessPerCity");

	m_bEraScaling = kResults.GetBool("EraScaling");
	m_bExpires = kResults.GetBool("Expires");

	m_bOneShot = kResults.GetBool("IsOneShot");

	m_bMetAnotherCiv = kResults.GetBool("HasMetAMajorCiv");
	m_bInDebt = kResults.GetBool("InDebt");
	m_bLosingMoney = kResults.GetBool("LosingMoney");

	szTextVal = kResults.GetText("DisabledTooltip");
	m_strDisabledTooltip = szTextVal;

	szTextVal = kResults.GetText("EventPromotion");
	m_iEventPromotion =  GC.getInfoTypeForString(szTextVal, true);

	kUtility.PopulateArrayByValue(m_piResourceChange, "Resources", "EventChoice_ResourceQuantity", "ResourceType", "EventChoiceType", szEventType, "Quantity");

	kUtility.SetYields(m_piEventYield, "EventChoice_InstantYield", "EventChoiceType", szEventType);
	kUtility.SetYields(m_piPreCheckEventYield, "EventChoice_EventCostYield", "EventChoiceType", szEventType);

	kUtility.SetYields(m_piCityYield, "EventChoice_CityYield", "EventChoiceType", szEventType);

	kUtility.SetFlavors(m_piFlavor, "EventChoiceFlavors", "EventChoiceType", szEventType);

	szTextVal = kResults.GetText("EventChoiceAudio");
	m_strEventChoiceSoundEffect = szTextVal;

	kUtility.PopulateArrayByValue(m_piNumFreeUnits, "UnitClasses", "EventChoice_FreeUnitClasses", "UnitClassType", "EventChoiceType", szEventType, "Quantity");
	kUtility.PopulateArrayByValue(m_piNumFreeSpecificUnits, "Units", "EventChoice_FreeUnits", "UnitType", "EventChoiceType", szEventType, "Quantity");

	kUtility.PopulateArrayByValue(m_piConvertReligion, "Religions", "EventChoice_ConvertNumPopToReligion", "ReligionType", "EventChoiceType", szEventType, "Population");
	kUtility.PopulateArrayByValue(m_piConvertReligionPercent, "Religions", "EventChoice_ConvertPercentPopToReligion", "ReligionType", "EventChoiceType", szEventType, "Percent");

	{
		//Initialize Notifications
		const size_t iNumNotifications = kUtility.MaxRows("Notifications");
		m_paNotificationInfo = FNEW(CvEventNotificationInfo[iNumNotifications], c_eCiv5GameplayDLL, 0);
		int idx = 0;

		std::string strEventChoiceTypesKey = "EventChoice_Notification";
		Database::Results* pEventChoiceTypes = kUtility.GetResults(strEventChoiceTypesKey);
		if(pEventChoiceTypes == NULL)
		{
			pEventChoiceTypes = kUtility.PrepareResults(strEventChoiceTypesKey, "select NotificationType, Description, ShortDescription, IsWorldEvent, NeedPlayerID, Variable1, Variable2 from EventChoice_Notification where EventChoiceType = ?");
		}

		const size_t lenEventChoiceType = strlen(szEventType);
		pEventChoiceTypes->Bind(1, szEventType, lenEventChoiceType, false);

		while(pEventChoiceTypes->Step())
		{
			CvEventNotificationInfo& pEventNotificationInfo = m_paNotificationInfo[idx];
			pEventNotificationInfo.m_strNotificationType = pEventChoiceTypes->GetText("NotificationType");
			pEventNotificationInfo.m_strDescription = pEventChoiceTypes->GetText("Description");
			pEventNotificationInfo.m_strShortDescription = pEventChoiceTypes->GetText("ShortDescription");
			pEventNotificationInfo.m_bWorldEvent = pEventChoiceTypes->GetBool("IsWorldEvent");
			pEventNotificationInfo.m_bNeedPlayerID = pEventChoiceTypes->GetBool("NeedPlayerID");
			pEventNotificationInfo.m_iVariable1 = pEventChoiceTypes->GetInt("Variable1");
			pEventNotificationInfo.m_iVariable2 = pEventChoiceTypes->GetInt("Variable2");
			idx++;
		}

		m_iNotificationInfos = idx;
		pEventChoiceTypes->Reset();
	}
	{
		//Initialize Linker Table
		const size_t iNumLinkers = kUtility.MaxRows("Notifications");
		m_paLinkerInfo = FNEW(CvEventChoiceLinkingInfo[iNumLinkers], c_eCiv5GameplayDLL, 0);
		int idx = 0;

		std::string strEventChoiceTypesKey = "EventChoice_EventLinks";
		Database::Results* pEventChoiceTypes = kUtility.GetResults(strEventChoiceTypesKey);
		if(pEventChoiceTypes == NULL)
		{
			pEventChoiceTypes = kUtility.PrepareResults(strEventChoiceTypesKey, "select Event, EventChoiceLinker, CityEvent, CityEventChoice, CheckKnownPlayers, CheckForActive from EventChoice_EventLinks where EventChoiceType = ?");
		}

		const size_t lenEventChoiceType = strlen(szEventType);
		pEventChoiceTypes->Bind(1, szEventType, lenEventChoiceType, false);

		while(pEventChoiceTypes->Step())
		{
			CvEventChoiceLinkingInfo& pEventChoiceLinkingInfo= m_paLinkerInfo[idx];
			szTextVal = pEventChoiceTypes->GetText("Event");
			pEventChoiceLinkingInfo.m_iEvent =  GC.getInfoTypeForString(szTextVal, true);
			szTextVal = pEventChoiceTypes->GetText("EventChoiceLinker");
			pEventChoiceLinkingInfo.m_iEventChoice =  GC.getInfoTypeForString(szTextVal, true);
			szTextVal = pEventChoiceTypes->GetText("CityEvent");
			pEventChoiceLinkingInfo.m_iCityEvent =  GC.getInfoTypeForString(szTextVal, true);
			szTextVal = pEventChoiceTypes->GetText("CityEventChoice");
			pEventChoiceLinkingInfo.m_iCityEventChoice =  GC.getInfoTypeForString(szTextVal, true);
			pEventChoiceLinkingInfo.m_bCheckWorld = pEventChoiceTypes->GetBool("CheckKnownPlayers");
			pEventChoiceLinkingInfo.m_bActive = pEventChoiceTypes->GetBool("CheckForActive");
			idx++;
		}

		m_iLinkerInfos = idx;
		pEventChoiceTypes->Reset();
	}

	//BuildingYieldChanges
	{
		kUtility.Initialize2DArray(m_ppiBuildingClassYield, "BuildingClasses", "Yields");

		std::string strKey("EventChoice_BuildingClassYieldChange");
		Database::Results* pResults = kUtility.GetResults(strKey);
		if(pResults == NULL)
		{
			pResults = kUtility.PrepareResults(strKey, "select BuildingClasses.ID as BuildingClassID, Yields.ID as YieldID, YieldChange from EventChoice_BuildingClassYieldChange inner join BuildingClasses on BuildingClasses.Type = BuildingClassType inner join Yields on Yields.Type = YieldType where EventChoiceType = ?");
		}

		pResults->Bind(1, szEventType);

		while(pResults->Step())
		{
			const int BuildingClassID = pResults->GetInt(0);
			const int iYieldID = pResults->GetInt(1);
			const int iYieldChange = pResults->GetInt(2);

			m_ppiBuildingClassYield[BuildingClassID][iYieldID] = iYieldChange;
		}
	}
	//BuildingYieldModifierChanges
	{
		kUtility.Initialize2DArray(m_ppiBuildingClassYieldModifier, "BuildingClasses", "Yields");

		std::string strKey("EventChoice_BuildingClassYieldModifier");
		Database::Results* pResults = kUtility.GetResults(strKey);
		if(pResults == NULL)
		{
			pResults = kUtility.PrepareResults(strKey, "select BuildingClasses.ID as BuildingClassID, Yields.ID as YieldID, Modifier from EventChoice_BuildingClassYieldModifier inner join BuildingClasses on BuildingClasses.Type = BuildingClassType inner join Yields on Yields.Type = YieldType where EventChoiceType = ?");
		}

		pResults->Bind(1, szEventType);

		while(pResults->Step())
		{
			const int BuildingClassID = pResults->GetInt(0);
			const int iYieldID = pResults->GetInt(1);
			const int iYieldChange = pResults->GetInt(2);

			m_ppiBuildingClassYieldModifier[BuildingClassID][iYieldID] = iYieldChange;
		}
	}
	
	//TerrainYieldChanges
	{
		kUtility.Initialize2DArray(m_ppiTerrainYield, "Terrains", "Yields");

		std::string strKey("EventChoice_TerrainYieldChange");
		Database::Results* pResults = kUtility.GetResults(strKey);
		if(pResults == NULL)
		{
			pResults = kUtility.PrepareResults(strKey, "select Terrains.ID as TerrainID, Yields.ID as YieldID, YieldChange from EventChoice_TerrainYieldChange inner join Terrains on Terrains.Type = TerrainType inner join Yields on Yields.Type = YieldType where EventChoiceType = ?");
		}

		pResults->Bind(1, szEventType);

		while(pResults->Step())
		{
			const int TerrainID = pResults->GetInt(0);
			const int iYieldID = pResults->GetInt(1);
			const int iYieldChange = pResults->GetInt(2);

			m_ppiTerrainYield[TerrainID][iYieldID] = iYieldChange;
		}
	}
	//ImprovementYieldChanges
	{
		kUtility.Initialize2DArray(m_ppiImprovementYield, "Improvements", "Yields");

		std::string strKey("EventChoice_ImprovementYieldChange");
		Database::Results* pResults = kUtility.GetResults(strKey);
		if(pResults == NULL)
		{
			pResults = kUtility.PrepareResults(strKey, "select Improvements.ID as ImprovementID, Yields.ID as YieldID, YieldChange from EventChoice_ImprovementYieldChange inner join Improvements on Improvements.Type = ImprovementType inner join Yields on Yields.Type = YieldType where EventChoiceType = ?");
		}

		pResults->Bind(1, szEventType);

		while(pResults->Step())
		{
			const int ImprovementID = pResults->GetInt(0);
			const int iYieldID = pResults->GetInt(1);
			const int iYieldChange = pResults->GetInt(2);

			m_ppiImprovementYield[ImprovementID][iYieldID] = iYieldChange;
		}
	}
	//ResourceYieldChanges
	{
		kUtility.Initialize2DArray(m_ppiResourceYield, "Resources", "Yields");

		std::string strKey("EventChoice_ResourceYieldChange");
		Database::Results* pResults = kUtility.GetResults(strKey);
		if(pResults == NULL)
		{
			pResults = kUtility.PrepareResults(strKey, "select Resources.ID as ResourceID, Yields.ID as YieldID, YieldChange from EventChoice_ResourceYieldChange inner join Resources on Resources.Type = ResourceType inner join Yields on Yields.Type = YieldType where EventChoiceType = ?");
		}

		pResults->Bind(1, szEventType);

		while(pResults->Step())
		{
			const int ResourceID = pResults->GetInt(0);
			const int iYieldID = pResults->GetInt(1);
			const int iYieldChange = pResults->GetInt(2);

			m_ppiResourceYield[ResourceID][iYieldID] = iYieldChange;
		}
	}
	//FeatureYieldChanges
	{
		kUtility.Initialize2DArray(m_ppiFeatureYield, "Features", "Yields");

		std::string strKey("EventChoice_FeatureYieldChange");
		Database::Results* pResults = kUtility.GetResults(strKey);
		if(pResults == NULL)
		{
			pResults = kUtility.PrepareResults(strKey, "select Features.ID as FeatureID, Yields.ID as YieldID, YieldChange from EventChoice_FeatureYieldChange inner join Features on Features.Type = FeatureType inner join Yields on Yields.Type = YieldType where EventChoiceType = ?");
		}

		pResults->Bind(1, szEventType);

		while(pResults->Step())
		{
			const int FeatureID = pResults->GetInt(0);
			const int iYieldID = pResults->GetInt(1);
			const int iYieldChange = pResults->GetInt(2);

			m_ppiFeatureYield[FeatureID][iYieldID] = iYieldChange;
		}
	}
	//SpecialistYieldChanges
	{
		kUtility.Initialize2DArray(m_ppiSpecialistYield, "Specialists", "Yields");

		std::string strKey("EventChoice_SpecialistYieldChange");
		Database::Results* pResults = kUtility.GetResults(strKey);
		if(pResults == NULL)
		{
			pResults = kUtility.PrepareResults(strKey, "select Specialists.ID as SpecialistID, Yields.ID as YieldID, YieldChange from EventChoice_SpecialistYieldChange inner join Specialists on Specialists.Type = SpecialistType inner join Yields on Yields.Type = YieldType where EventChoiceType = ?");
		}

		pResults->Bind(1, szEventType);

		while(pResults->Step())
		{
			const int SpecialistID = pResults->GetInt(0);
			const int iYieldID = pResults->GetInt(1);
			const int iYieldChange = pResults->GetInt(2);

			m_ppiSpecialistYield[SpecialistID][iYieldID] = iYieldChange;
		}
	}

	// Filters
	szTextVal = kResults.GetText("PrereqTech");
	m_iPrereqTech =  GC.getInfoTypeForString(szTextVal, true);

	szTextVal = kResults.GetText("ObsoleteTech");
	m_iObsoleteTech =  GC.getInfoTypeForString(szTextVal, true);

	m_bVassal = kResults.GetBool("IsVassalOfSomeone");
	m_bMaster = kResults.GetBool("IsMasterOfSomeone");

	m_iMinimumNationalPopulation = kResults.GetInt("MinimumNationalPopulation");
	m_iMinimumNumberCities = kResults.GetInt("MinimumNumberCities");

	szTextVal = kResults.GetText("UnitClassRequired");
	m_iUnitTypeRequired =  GC.getInfoTypeForString(szTextVal, true);

	szTextVal = kResults.GetText("ImprovementAnywhereRequired");
	m_iRequiredImprovement =  GC.getInfoTypeForString(szTextVal, true);

	szTextVal = kResults.GetText("RequiredCiv");
	m_iRequiredCiv =  GC.getInfoTypeForString(szTextVal, true);

	szTextVal = kResults.GetText("RequiredEra");
	m_iRequiredEra =  GC.getInfoTypeForString(szTextVal, true);

	szTextVal = kResults.GetText("ObsoleteEra");
	m_iObsoleteEra =  GC.getInfoTypeForString(szTextVal, true);

	szTextVal = kResults.GetText("RequiredPolicy");
	m_iRequiredPolicy =  GC.getInfoTypeForString(szTextVal, true);

	szTextVal = kResults.GetText("RequiredIdeology");
	m_iIdeology =  GC.getInfoTypeForString(szTextVal, true);

	m_bRequiresHolyCity = kResults.GetBool("RequiresHolyCity");
	
	szTextVal = kResults.GetText("RequiredReligion");
	m_iRequiredReligion =  GC.getInfoTypeForString(szTextVal, true);

	m_bRequiredPantheon = kResults.GetBool("RequiresPantheon");

	m_bRequiresIdeology = kResults.GetBool("RequiresIdeology");
	m_bRequiresWar = kResults.GetBool("RequiresWar");
	m_bRequiresWarMinor = kResults.GetBool("RequiresWarMinor");

	szTextVal = kResults.GetText("RequiredAnywhereBuildingClass");
	m_iBuildingRequired =  GC.getInfoTypeForString(szTextVal, true);

	szTextVal = kResults.GetText("RequiredNowhereBuildingClass");
	m_iBuildingLimiter =  GC.getInfoTypeForString(szTextVal, true);

	szTextVal = kResults.GetText("RequiredStateReligion");
	m_iRequiredStateReligion =  GC.getInfoTypeForString(szTextVal, true);

	m_bHasStateReligion = kResults.GetBool("RequiresAnyStateReligion");

	m_bUnhappy = kResults.GetBool("IsUnhappy");
	m_bSuperUnhappy = kResults.GetBool("IsSuperUnhappy");

	kUtility.SetYields(m_piMinimumYield, "EventChoice_MinimumStartYield", "EventChoiceType", szEventType);

	kUtility.PopulateArrayByValue(m_piRequiredResource, "Resources", "EventChoice_MinimumResourceRequired", "ResourceType", "EventChoiceType", szEventType, "Quantity");
	kUtility.PopulateArrayByValue(m_piRequiredFeature, "Features", "EventChoice_MinimumFeatureRequired", "FeatureType", "EventChoiceType", szEventType, "Quantity");

	m_iBasicNeedsMedianModifierGlobal = kResults.GetInt("BasicNeedsMedianModifierGlobal");
	m_iGoldMedianModifierGlobal = kResults.GetInt("GoldMedianModifierGlobal");
	m_iScienceMedianModifierGlobal = kResults.GetInt("ScienceMedianModifierGlobal");
	m_iCultureMedianModifierGlobal = kResults.GetInt("CultureMedianModifierGlobal");
	m_iReligiousUnrestModifierGlobal = kResults.GetInt("ReligiousUnrestModifierGlobal");

	return true;
}
//======================================================================================================
//		CvModCityEventInfo
//======================================================================================================
CvModCityEventInfo::CvModCityEventInfo() :
	 m_iEventClass(-1),
	 m_bIgnoresGlobalCooldown(false),
	 m_iPrereqTech(-1),
	 m_iObsoleteTech(-1),
	 m_iMinimumPopulation(0),
	 m_iRandomChance(0),
	 m_iRandomChanceDelta(0),
	 m_iRequiredCiv(-1),
	 m_iRequiredPolicy(-1),
	 m_iIdeology(-1),
	 m_iRequiredEra(-1),
	 m_iObsoleteEra(-1),
	 m_iRequiredReligion(-1),
	 m_iRequiredImprovement(-1),
	 m_bRequiresHolyCity(false),
	 m_bRequiresIdeology(false),
	 m_bRequiresWar(false),
	 m_bCapital(false),
	 m_bCoastal(false),
	 m_bIsRiver(false),
	 m_bEraScaling(false),
	 m_iNumChoices(0),
	 m_iCooldown(0),
	 m_iBuildingRequired(-1),
	 m_iBuildingLimiter(-1),
	 m_piMinimumYield(NULL),
	 m_iLocalResourceRequired(-1),
	 m_bRequiresWarMinor(false),
	 m_iRequiredStateReligion(-1),
	 m_bRequiresGarrison(false),
	 m_bHasStateReligion(false),
	 m_bUnhappy(false),
	 m_bSuperUnhappy(false),
	 m_strSplashArt(""),
	 m_strEventAudio(""),
	 m_bIsResistance(false),
	 m_bIsWLTKD(false),
	 m_bIsOccupied(false),
	 m_bIsRazing(false),
	 m_bHasAnyReligion(false),
	 m_bIsPuppet(false),
	 m_bTradeConnection(false),
	 m_bCityConnection(false),
	 m_iNearbyFeature(-1),
	 m_iNearbyTerrain(-1),
	 m_iMaximumPopulation(0),
	 m_bPantheon(false),
	 m_bNearMountain(false),
	 m_bNearNaturalWonder(false),
	 m_bOneShot(false),
	 m_bMetAnotherCiv(false),
	 m_bInDebt(false),
	 m_bLosingMoney(false),
	 m_bVassal(false),
	 m_bMaster(false),
	 m_bHasPlayerReligion(false),
	 m_bHasPlayerMajority(false),
	 m_bLacksPlayerReligion(false),
	 m_bLacksPlayerMajority(false),
	 m_bEspionageSetup(false),
	 m_bIsCounterSpy(false),
	 m_paCityLinkerInfo(NULL),
	 m_iCityLinkerInfos(0)
{
}
//------------------------------------------------------------------------------
CvModCityEventInfo::~CvModCityEventInfo()
{
	SAFE_DELETE_ARRAY(m_piMinimumYield);
	SAFE_DELETE_ARRAY(m_paCityLinkerInfo);
}
//------------------------------------------------------------------------------
int CvModCityEventInfo::getEventClass() const
{
	return m_iEventClass;
}
bool CvModCityEventInfo::IgnoresGlobalCooldown() const
{
	return m_bIgnoresGlobalCooldown;
} 
//------------------------------------------------------------------------------
int CvModCityEventInfo::getPrereqTech() const
{
	return m_iPrereqTech;
}
//------------------------------------------------------------------------------
int CvModCityEventInfo::getObsoleteTech() const
{
	return m_iObsoleteTech;
}
//------------------------------------------------------------------------------
int CvModCityEventInfo::getMinimumPopulation() const
{
	return m_iMinimumPopulation;
}
//------------------------------------------------------------------------------
int CvModCityEventInfo::getRandomChance() const
{
	return m_iRandomChance;
}
//------------------------------------------------------------------------------
int CvModCityEventInfo::getRandomChanceDelta() const
{
	return m_iRandomChanceDelta;
}
//------------------------------------------------------------------------------
int CvModCityEventInfo::getRequiredCiv() const
{
	return m_iRequiredCiv;
}
//------------------------------------------------------------------------------
int CvModCityEventInfo::getRequiredEra() const
{
	return m_iRequiredEra;
}
//------------------------------------------------------------------------------
int CvModCityEventInfo::getObsoleteEra() const
{
	return m_iObsoleteEra;
}
//------------------------------------------------------------------------------
int CvModCityEventInfo::getYieldMinimum(YieldTypes eYield) const
{
	ASSERT_DEBUG(eYield < NUM_YIELD_TYPES, "Index out of bounds");
	ASSERT_DEBUG(eYield > -1, "Index out of bounds");
	return m_piMinimumYield ? m_piMinimumYield[eYield] : -1;
}
//------------------------------------------------------------------------------
int CvModCityEventInfo::getRequiredImprovement() const
{
	return m_iRequiredImprovement;
}
//------------------------------------------------------------------------------
int CvModCityEventInfo::getRequiredPolicy() const
{
	return m_iRequiredPolicy;
}
//------------------------------------------------------------------------------
int CvModCityEventInfo::getRequiredIdeology() const
{
	return m_iIdeology;
}
//------------------------------------------------------------------------------
bool CvModCityEventInfo::isRequiresHolyCity() const
{
	return m_bRequiresHolyCity;
}
//------------------------------------------------------------------------------
int CvModCityEventInfo::getRequiredReligion() const
{
	return m_iRequiredReligion;
}
//------------------------------------------------------------------------------
int CvModCityEventInfo::getBuildingRequired() const
{
	return m_iBuildingRequired;
}
//------------------------------------------------------------------------------
int CvModCityEventInfo::getBuildingLimiter() const
{
	return m_iBuildingLimiter;
}
//------------------------------------------------------------------------------
int CvModCityEventInfo::getCooldown() const
{
	return m_iCooldown;
}
//------------------------------------------------------------------------------
bool CvModCityEventInfo::isCapital() const
{
	return m_bCapital;
}
//------------------------------------------------------------------------------
bool CvModCityEventInfo::isCoastal() const
{
	return m_bCoastal;
}
//------------------------------------------------------------------------------
bool CvModCityEventInfo::isRiver() const
{
	return m_bIsRiver;
}
//------------------------------------------------------------------------------
bool CvModCityEventInfo::isEraScaling() const
{
	return m_bEraScaling;
}
//------------------------------------------------------------------------------
bool CvModCityEventInfo::isRequiresIdeology() const
{
	return m_bRequiresIdeology;
}
//------------------------------------------------------------------------------
bool CvModCityEventInfo::isRequiresWar() const
{
	return m_bRequiresWar;
}

//------------------------------------------------------------------------------
int CvModCityEventInfo::getNumChoices() const
{
	return m_iNumChoices;
}
//------------------------------------------------------------------------------
bool CvModCityEventInfo::isRequiresWarMinor() const
{
	return m_bRequiresWarMinor;
}
//------------------------------------------------------------------------------
const char* CvModCityEventInfo::getSplashArt() const
{
	return m_strSplashArt.c_str();
}
//------------------------------------------------------------------------------
const char* CvModCityEventInfo::getEventAudio() const
{
	return m_strEventAudio.c_str();
}
//------------------------------------------------------------------------------
int CvModCityEventInfo::getRequiredStateReligion() const
{
	return m_iRequiredStateReligion;
}
//------------------------------------------------------------------------------
bool CvModCityEventInfo::isRequiresGarrison() const
{
	return m_bRequiresGarrison;
}
//------------------------------------------------------------------------------
bool CvModCityEventInfo::hasStateReligion() const
{
	return m_bHasStateReligion;
}
//------------------------------------------------------------------------------
bool CvModCityEventInfo::isUnhappy() const
{
	return m_bUnhappy;
}
//------------------------------------------------------------------------------
bool CvModCityEventInfo::isSuperUnhappy() const
{
	return m_bSuperUnhappy;
}
//------------------------------------------------------------------------------
int CvModCityEventInfo::getLocalResourceRequired() const
{
	return m_iLocalResourceRequired;
}
//------------------------------------------------------------------------------
bool CvModCityEventInfo::isResistance() const
{
	return m_bIsResistance;
}
//------------------------------------------------------------------------------
bool CvModCityEventInfo::isWLTKD() const
{
	return m_bIsWLTKD;
}
//------------------------------------------------------------------------------
bool CvModCityEventInfo::isOccupied() const
{
	return m_bIsOccupied;
}
//------------------------------------------------------------------------------
bool CvModCityEventInfo::isRazing() const
{
	return m_bIsRazing;
}
//------------------------------------------------------------------------------
bool CvModCityEventInfo::hasAnyReligion() const
{
	return m_bHasAnyReligion;
}
//------------------------------------------------------------------------------
bool CvModCityEventInfo::isPuppet() const
{
	return m_bIsPuppet;
}
//------------------------------------------------------------------------------
bool CvModCityEventInfo::hasTradeConnection() const
{
	return m_bTradeConnection;
}
//------------------------------------------------------------------------------
bool CvModCityEventInfo::hasCityConnection() const
{
	return m_bCityConnection;
}
//------------------------------------------------------------------------------
bool CvModCityEventInfo::isNearNaturalWonder() const
{
	return m_bNearNaturalWonder;
}
//------------------------------------------------------------------------------
bool CvModCityEventInfo::isNearMountain() const
{
	return m_bNearMountain;
}
	//------------------------------------------------------------------------------
bool CvModCityEventInfo::hasPantheon() const
{
	return m_bPantheon;
}
//------------------------------------------------------------------------------
int CvModCityEventInfo::hasNearbyFeature() const
{
	return m_iNearbyFeature;
}
//------------------------------------------------------------------------------
int CvModCityEventInfo::hasNearbyTerrain() const
{
	return m_iNearbyTerrain;
}
//------------------------------------------------------------------------------
int CvModCityEventInfo::getMaximumPopulation() const
{
	return m_iMaximumPopulation;
}
//------------------------------------------------------------------------------
bool CvModCityEventInfo::isOneShot() const
{
	return m_bOneShot;
}
//------------------------------------------------------------------------------
bool CvModCityEventInfo::hasMetAnotherCiv() const
{
	return m_bMetAnotherCiv;
}
//------------------------------------------------------------------------------
bool CvModCityEventInfo::isInDebt() const
{
	return m_bInDebt;
}
//------------------------------------------------------------------------------
bool CvModCityEventInfo::isLosingMoney() const
{
	return m_bLosingMoney;
}
//------------------------------------------------------------------------------
bool CvModCityEventInfo::isVassal() const
{
	return m_bVassal;
}
//------------------------------------------------------------------------------
bool CvModCityEventInfo::isMaster() const
{
	return m_bMaster;
}
//------------------------------------------------------------------------------
bool CvModCityEventInfo::hasPlayerReligion() const
{
	return m_bHasPlayerReligion;
}
//------------------------------------------------------------------------------
bool CvModCityEventInfo::lacksPlayerReligion() const
{
	return m_bLacksPlayerReligion;
}
//------------------------------------------------------------------------------
bool CvModCityEventInfo::hasPlayerMajority() const
{
	return m_bHasPlayerMajority;
}
//------------------------------------------------------------------------------
bool CvModCityEventInfo::lacksPlayerMajority() const
{
	return m_bLacksPlayerMajority;
}
bool CvModCityEventInfo::isEspionageSetup() const
{
	return m_bEspionageSetup;
}
bool CvModCityEventInfo::IsCounterSpy() const
{
	return m_bIsCounterSpy;
}
CvCityEventLinkingInfo *CvModCityEventInfo::GetLinkerInfo(int i) const
{
	ASSERT_DEBUG(i < GetNumLinkers(), "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");

	if (m_paCityLinkerInfo[0].GetCityLinkingEvent() == -1 && m_paCityLinkerInfo[0].GetCityLinkingEventChoice() == -1 && m_paCityLinkerInfo[0].GetLinkingEvent() == -1 && m_paCityLinkerInfo[0].GetLinkingEventChoice() == -1)
	{
		return NULL;
	}
	else
	{
		return &m_paCityLinkerInfo[i];
	}
}
//------------------------------------------------------------------------------
bool CvModCityEventInfo::CacheResults(Database::Results& kResults, CvDatabaseUtility& kUtility)
{
	if(!CvBaseInfo::CacheResults(kResults, kUtility))
		return false;

	m_bOneShot = kResults.GetBool("IsOneShot");

	m_bIgnoresGlobalCooldown = kResults.GetBool("IgnoresGlobalCooldown");

	m_bMetAnotherCiv = kResults.GetBool("HasMetAMajorCiv");
	m_bInDebt = kResults.GetBool("InDebt");
	m_bLosingMoney = kResults.GetBool("LosingMoney");

	m_bVassal = kResults.GetBool("IsVassalOfSomeone");
	m_bMaster = kResults.GetBool("IsMasterOfSomeone");

	m_bHasPlayerReligion = kResults.GetBool("CityHasPlayerReligion");
	m_bLacksPlayerReligion = kResults.GetBool("CityLacksPlayerReligion");

	m_bHasPlayerMajority = kResults.GetBool("CityHasPlayerMajorityReligion");
	m_bLacksPlayerMajority = kResults.GetBool("CityLacksPlayerMajorityReligion");

	const char* szTextVal = NULL;

	szTextVal = kResults.GetText("EventClass");
	m_iEventClass = GC.getInfoTypeForString(szTextVal, true);

	szTextVal = kResults.GetText("PrereqTech");
	m_iPrereqTech =  GC.getInfoTypeForString(szTextVal, true);

	szTextVal = kResults.GetText("ObsoleteTech");
	m_iObsoleteTech =  GC.getInfoTypeForString(szTextVal, true);

	m_iMinimumPopulation = kResults.GetInt("MinimumCityPopulation");

	szTextVal = kResults.GetText("ImprovementRequired");
	m_iRequiredImprovement =  GC.getInfoTypeForString(szTextVal, true);

	m_iRandomChance = kResults.GetInt("RandomChance");
	m_iRandomChanceDelta = kResults.GetInt("RandomChanceDelta");

	szTextVal = kResults.GetText("RequiredCiv");
	m_iRequiredCiv =  GC.getInfoTypeForString(szTextVal, true);

	szTextVal = kResults.GetText("RequiredEra");
	m_iRequiredEra =  GC.getInfoTypeForString(szTextVal, true);

	szTextVal = kResults.GetText("ObsoleteEra");
	m_iObsoleteEra =  GC.getInfoTypeForString(szTextVal, true);

	szTextVal = kResults.GetText("RequiredPolicy");
	m_iRequiredPolicy =  GC.getInfoTypeForString(szTextVal, true);

	szTextVal = kResults.GetText("RequiredIdeology");
	m_iIdeology =  GC.getInfoTypeForString(szTextVal, true);

	m_bRequiresHolyCity = kResults.GetBool("RequiresHolyCity");
	
	szTextVal = kResults.GetText("RequiredReligion");
	m_iRequiredReligion =  GC.getInfoTypeForString(szTextVal, true);

	m_bRequiresGarrison = kResults.GetBool("RequiresGarrison");
	m_bHasStateReligion = kResults.GetBool("RequiresAnyStateReligion");
	m_bUnhappy = kResults.GetBool("IsUnhappy");
	m_bSuperUnhappy = kResults.GetBool("IsSuperUnhappy");
	m_bRequiresIdeology = kResults.GetBool("RequiresIdeology");
	m_bRequiresWar = kResults.GetBool("RequiresWar");
	m_bCapital = kResults.GetBool("CapitalOnly");
	m_bCoastal = kResults.GetBool("CoastalOnly");
	m_bIsRiver = kResults.GetBool("RiverOnly");
	m_bEraScaling = kResults.GetBool("EraScaling");
	m_iNumChoices = kResults.GetInt("NumChoices");

	szTextVal = kResults.GetText("RequiredBuildingClass");
	m_iBuildingRequired =  GC.getInfoTypeForString(szTextVal, true);

	szTextVal = kResults.GetText("CannotHaveBuildingClass");
	m_iBuildingLimiter =  GC.getInfoTypeForString(szTextVal, true);

	m_iCooldown = kResults.GetInt("EventCooldown");

	const char* szEventType = GetType();
	kUtility.SetYields(m_piMinimumYield, "CityEvent_MinimumStartYield", "CityEventType", szEventType);

	m_bRequiresWarMinor = kResults.GetBool("RequiresWarMinor");

	m_bIsResistance = kResults.GetBool("RequiresResistance");
	m_bIsWLTKD = kResults.GetBool("RequiresWLTKD");
	m_bIsOccupied = kResults.GetBool("RequiresOccupied");
	m_bIsRazing = kResults.GetBool("RequiresRazing");
	m_bHasAnyReligion = kResults.GetBool("HasAnyReligion");
	m_bIsPuppet = kResults.GetBool("RequiresPuppet");
	m_bTradeConnection = kResults.GetBool("HasTradeConnection");
	m_bCityConnection = kResults.GetBool("HasCityConnection");

	szTextVal = kResults.GetText("RequiredStateReligion");
	m_iRequiredStateReligion =  GC.getInfoTypeForString(szTextVal, true);

	szTextVal = kResults.GetText("CityEventAudio");
	m_strEventAudio = szTextVal;

	szTextVal = kResults.GetText("CityEventArt");
	m_strSplashArt = szTextVal;

	szTextVal = kResults.GetText("LocalResourceRequired");
	m_iLocalResourceRequired =  GC.getInfoTypeForString(szTextVal, true);

	szTextVal = kResults.GetText("NearbyFeature");
	m_iNearbyFeature =  GC.getInfoTypeForString(szTextVal, true);

	szTextVal = kResults.GetText("NearbyTerrain");
	m_iNearbyTerrain =  GC.getInfoTypeForString(szTextVal, true);

	m_iMaximumPopulation = kResults.GetInt("MaximumCityPopulation");
	m_bPantheon = kResults.GetBool("RequiresPantheon");
	m_bNearMountain = kResults.GetBool("RequiresNearbyMountain");
	m_bNearNaturalWonder = kResults.GetBool("RequiresNearbyNaturalWonder");

	m_bEspionageSetup = kResults.GetBool("EspionageSetup");
	m_bIsCounterSpy = kResults.GetBool("IsCounterSpy");

	{
		//Initialize Linker Table
		const size_t iNumLinkers = kUtility.MaxRows("Notifications");
		m_paCityLinkerInfo = FNEW(CvCityEventLinkingInfo[iNumLinkers], c_eCiv5GameplayDLL, 0);
		int idx = 0;

		std::string strCityEventTypesKey = "CityEvent_EventLinks";
		Database::Results* pCityEventTypes = kUtility.GetResults(strCityEventTypesKey);
		if(pCityEventTypes == NULL)
		{
			pCityEventTypes = kUtility.PrepareResults(strCityEventTypesKey, "select Event, EventChoice, CityEventLinker, CityEventChoice, CheckKnownPlayers, CheckOnlyEventCity, CheckForActive from CityEvent_EventLinks where CityEventType = ?");
		}

		const size_t lenCityEventChoiceType = strlen(szEventType);
		pCityEventTypes->Bind(1, szEventType, lenCityEventChoiceType, false);

		while(pCityEventTypes->Step())
		{
			CvCityEventLinkingInfo& pCityEventLinkingInfo= m_paCityLinkerInfo[idx];
			szTextVal = pCityEventTypes->GetText("Event");
			pCityEventLinkingInfo.m_iEvent =  GC.getInfoTypeForString(szTextVal, true);
			szTextVal = pCityEventTypes->GetText("EventChoice");
			pCityEventLinkingInfo.m_iEventChoice =  GC.getInfoTypeForString(szTextVal, true);
			szTextVal = pCityEventTypes->GetText("CityEventLinker");
			pCityEventLinkingInfo.m_iCityEvent =  GC.getInfoTypeForString(szTextVal, true);
			szTextVal = pCityEventTypes->GetText("CityEventChoice");
			pCityEventLinkingInfo.m_iCityEventChoice =  GC.getInfoTypeForString(szTextVal, true);
			pCityEventLinkingInfo.m_bCheckWorld = pCityEventTypes->GetBool("CheckKnownPlayers");
			pCityEventLinkingInfo.m_bOnlyActiveCity = pCityEventTypes->GetBool("CheckOnlyEventCity");
			pCityEventLinkingInfo.m_bActive = pCityEventTypes->GetBool("CheckForActive");
			idx++;
		}

		m_iCityLinkerInfos = idx;
		pCityEventTypes->Reset();
	}
	return true;
}
//======================================================================================================
//		CvModEventCityChoiceInfo
//======================================================================================================
CvModEventCityChoiceInfo::CvModEventCityChoiceInfo() :
	 m_iEventBuilding(-1),
	 m_bEraScaling(false),
	 m_bExpires(false),
	 m_iEventDuration(0),
	 m_bEventDurationScaling(true),
	 m_iEventChance(0),
	 m_iEventBuildingDestruction(-1),
	 m_piEventYield(NULL),
	 m_piPreCheckEventYield(NULL),
	 m_piGPChange(NULL),
	 m_piDestroyImprovement(NULL),
	 m_piFlavor(NULL),
	 m_piNumFreeUnits(NULL),
	 m_piNumFreeSpecificUnits(NULL),
	 m_iNumWLTKD(0),
	 m_iGrowthMod(0),
	 m_iResistanceTurns(0),
	 m_iRandomBarbs(0),
	 m_iRandomBarbsPerEra(0),
	 m_iFreeScaledUnits(0),
	 m_iSpecialistsGreatPersonPointsPerTurn(0),
	 m_iPrereqTech(-1),
	 m_iObsoleteTech(-1),
	 m_iMinimumPopulation(0),
	 m_iRequiredCiv(-1),
	 m_iRequiredEra(-1),
	 m_iObsoleteEra(-1),
	 m_iRequiredImprovement(-1),
	 m_iRequiredPolicy(-1),
	 m_iIdeology(-1),
	 m_iRequiredReligion(-1),
	 m_iBuildingRequired(-1),
	 m_iBuildingLimiter(-1),
	 m_bRequiresHolyCity(false),
	 m_piMinimumYield(NULL),
	 m_bRequiresIdeology(false),
	 m_bRequiresWar(false),
	 m_bCapital(false),
	 m_bCoastal(false),
	 m_bIsRiver(false),
	 m_bRequiresWarMinor(false),
	 m_iRequiredStateReligion(-1),
	 m_bRequiresGarrison(false),
	 m_bHasStateReligion(false),
	 m_bUnhappy(false),
	 m_bSuperUnhappy(false),
	 m_bEnemyUnhappy(false),
	 m_bEnemySuperUnhappy(false),
	 m_strEventChoiceSoundEffect(""),
	 m_piConvertReligion(NULL),
	 m_piConvertReligionPercent(NULL),
	 m_piBuildingDestructionChance(NULL),
	 m_iLocalResourceRequired(-1),
	 m_bIsResistance(false),
	 m_bIsWLTKD(false),
	 m_bIsOccupied(false),
	 m_bIsRazing(false),
	 m_bHasAnyReligion(false),
	 m_bIsPuppet(false),
	 m_bIsNotPuppet(false),
	 m_bTradeConnection(false),
	 m_bCityConnection(false),
	 m_ppiBuildingClassYield(NULL),
	 m_ppiBuildingClassYieldModifier(NULL),
	 m_ppiTerrainYield(NULL),
	 m_ppiFeatureYield(NULL),
	 m_ppiImprovementYield(NULL),
	 m_ppiResourceYield(NULL),
	 m_piCityYield(NULL),
	 m_piCityYieldModifier(NULL),
	 m_piYieldSiphon(NULL),
	 m_piYieldOnSpyIdentified(NULL),
	 m_piYieldOnSpyKilled(NULL),
	 m_iNearbyFeature(-1),
	 m_iNearbyTerrain(-1),
	 m_iMaximumPopulation(0),
	 m_bPantheon(false),
	 m_bNearMountain(false),
	 m_bNearNaturalWonder(false),
	 m_pbParentEventIDs(NULL),
	 m_bOneShot(false),
	 m_bMetAnotherCiv(false),
	 m_bInDebt(false),
	 m_bLosingMoney(false),
	 m_bVassal(false),
	 m_bMaster(false),
	 m_iCityWideDestructionChance(0),
	 m_iCityStrategicResourcePillage(0),
	 m_iPillageResourceTilesChance(0),
	 m_iPillageRoadsChance(0),
	 m_iPillageFortificationsChance(0),
	 m_iMutuallyExclusiveGroup(0),
	 m_iBlockBuildingTurns(0),
	 m_iEventPromotion(0),
	 m_iCityHappiness(0),
	 m_iReligiousPressureModifier(0),
	 m_piResourceChange(NULL),
	 m_strDisabledTooltip(""),
	 m_strSpyMissionEffect(""),
	 m_iConvertsCityToPlayerReligion(0),
	 m_iConvertsCityToPlayerMajorityReligion(0),
	 m_iNetworkPointsNeeded(0),
	 m_bNetworkPointsScaling(false),
	 m_iSpyIdentificationChance(0),
	 m_iSpyKillChance(0),
	 m_iTriggerPlayerEventChoice(NO_EVENT_CHOICE),
	 m_bHasPlayerReligion(false),
	 m_bLacksPlayerReligion(false),
	 m_bHasPlayerMajority(false),
	 m_bLacksPlayerMajority(false),
	 m_iSpyLevelRequired(0),
	 m_iStealTech(0),
	 m_iStealGW(0),
	 m_iSapCityTurns(0),
	 m_iNoTourismTurns(0),
	 m_iStealFromTreasuryPercent(0),
	 m_bIsEspionageMission(false),
	 m_bIsCounterspyMission(false),
	 m_iCounterspyNPRateReduction(0),
	 m_bCounterspyBlockSapCity(false),
	 m_iCityDefenseModifierBase(0),
	 m_iCityDefenseModifier(0),
	 m_paCityLinkerInfo(NULL),
	 m_iCityLinkerInfos(0),
	 m_iBasicNeedsMedianModifier(0),
	 m_iGoldMedianModifier(0),
	 m_iScienceMedianModifier(0),
	 m_iCultureMedianModifier(0),
	 m_iReligiousUnrestModifier(0)
{
}
//------------------------------------------------------------------------------
CvModEventCityChoiceInfo::~CvModEventCityChoiceInfo()
{
	SAFE_DELETE_ARRAY(m_piEventYield);
	SAFE_DELETE_ARRAY(m_piFlavor);
	SAFE_DELETE_ARRAY(m_piGPChange);
	SAFE_DELETE_ARRAY(m_piDestroyImprovement);
	SAFE_DELETE_ARRAY(m_piPreCheckEventYield);
	SAFE_DELETE_ARRAY(m_piNumFreeUnits);
	SAFE_DELETE_ARRAY(m_piNumFreeSpecificUnits);
	SAFE_DELETE_ARRAY(m_piMinimumYield);
	SAFE_DELETE_ARRAY(m_piConvertReligion);
	SAFE_DELETE_ARRAY(m_piConvertReligionPercent);
	SAFE_DELETE_ARRAY(m_piBuildingDestructionChance);
	SAFE_DELETE_ARRAY(m_piCityYield);
	SAFE_DELETE_ARRAY(m_piCityYieldModifier);
	SAFE_DELETE_ARRAY(m_piYieldSiphon);
	SAFE_DELETE_ARRAY(m_piYieldOnSpyIdentified);
	SAFE_DELETE_ARRAY(m_piYieldOnSpyKilled);
	SAFE_DELETE_ARRAY(m_pbParentEventIDs);
	SAFE_DELETE_ARRAY(m_piResourceChange);
	CvDatabaseUtility::SafeDelete2DArray(m_ppiBuildingClassYield);
	CvDatabaseUtility::SafeDelete2DArray(m_ppiBuildingClassYieldModifier);
	CvDatabaseUtility::SafeDelete2DArray(m_ppiTerrainYield);
	CvDatabaseUtility::SafeDelete2DArray(m_ppiFeatureYield);
	CvDatabaseUtility::SafeDelete2DArray(m_ppiImprovementYield);
	CvDatabaseUtility::SafeDelete2DArray(m_ppiResourceYield);
	CvDatabaseUtility::SafeDelete2DArray(m_ppiSpecialistYield);
	SAFE_DELETE_ARRAY(m_paCityLinkerInfo);
}
//------------------------------------------------------------------------------
bool CvModEventCityChoiceInfo::isParentEvent(CityEventTypes eCityEvent) const
{
	ASSERT_DEBUG(eCityEvent < GC.getNumCityEventInfos(), "Index out of bounds");
	ASSERT_DEBUG(eCityEvent > -1, "Index out of bounds");
	return m_pbParentEventIDs ? m_pbParentEventIDs[eCityEvent] : false;
}
//------------------------------------------------------------------------------
int CvModEventCityChoiceInfo::getEventDuration() const
{
	return m_iEventDuration;
}
//------------------------------------------------------------------------------
bool CvModEventCityChoiceInfo::isEventDurationScaling() const
{
	return m_bEventDurationScaling;
}
//------------------------------------------------------------------------------
int CvModEventCityChoiceInfo::getEventChance() const
{
	return m_iEventChance;
}
//------------------------------------------------------------------------------
bool CvModEventCityChoiceInfo::IsEraScaling() const
{
	return m_bEraScaling;
}
//------------------------------------------------------------------------------
bool CvModEventCityChoiceInfo::Expires() const
{
	return m_bExpires;
}
//------------------------------------------------------------------------------
int CvModEventCityChoiceInfo::ConvertsCityToPlayerReligion() const
{
	return m_iConvertsCityToPlayerReligion;
}
//------------------------------------------------------------------------------
int CvModEventCityChoiceInfo::ConvertsCityToPlayerMajorityReligion() const
{
	return m_iConvertsCityToPlayerMajorityReligion;
}

int CvModEventCityChoiceInfo::GetNetworkPointsNeededScaled() const
{
	int iNPScaled = m_iNetworkPointsNeeded;
	if (m_bNetworkPointsScaling)
	{
		iNPScaled *= GC.getGame().getGameSpeedInfo().getTrainPercent();
		iNPScaled /= 100;
	}
	return iNPScaled;
}
int CvModEventCityChoiceInfo::GetSpyIdentificationChance() const
{
	return m_iSpyIdentificationChance;
}
int CvModEventCityChoiceInfo::GetSpyKillChance() const
{
	return m_iSpyKillChance;
}
int CvModEventCityChoiceInfo::GetSpyLevelRequired() const
{
	return m_iSpyLevelRequired;
}

bool CvModEventCityChoiceInfo::isEspionageMission() const
{
	return m_bIsEspionageMission;
}
int CvModEventCityChoiceInfo::getStealTech() const
{
	return m_iStealTech;
}
int CvModEventCityChoiceInfo::getStealGW() const
{
	return m_iStealGW;
}
int CvModEventCityChoiceInfo::getSapCityTurns() const
{
	return m_iSapCityTurns;
}
int CvModEventCityChoiceInfo::getNoTourismTurns() const
{
	return m_iNoTourismTurns;
}
int CvModEventCityChoiceInfo::getStealFromTreasuryPercent() const
{
	return m_iStealFromTreasuryPercent;
}

// Counterspies
bool CvModEventCityChoiceInfo::isCounterspyMission() const
{
	return m_bIsCounterspyMission;
}
int CvModEventCityChoiceInfo::getCounterspyNPRateReduction() const
{
	return m_iCounterspyNPRateReduction;
}
bool CvModEventCityChoiceInfo::isCounterspyBlockSapCity() const
{
	return m_bCounterspyBlockSapCity;
}
int CvModEventCityChoiceInfo::getCityDefenseModifierBase() const
{
	return m_iCityDefenseModifierBase;
}
int CvModEventCityChoiceInfo::getCityDefenseModifier() const
{
	return m_iCityDefenseModifier;
}

EventChoiceTypes CvModEventCityChoiceInfo::GetTriggerPlayerEventChoice() const
{
	return (EventChoiceTypes)m_iTriggerPlayerEventChoice;
}

//------------------------------------------------------------------------------
int CvModEventCityChoiceInfo::getEventBuilding() const
{
	return m_iEventBuilding;
}
//------------------------------------------------------------------------------
int CvModEventCityChoiceInfo::getEventBuildingDestruction() const
{
	return m_iEventBuildingDestruction;
}
//------------------------------------------------------------------------------
int CvModEventCityChoiceInfo::getEventYield(YieldTypes eYield) const
{
	ASSERT_DEBUG(eYield < NUM_YIELD_TYPES, "Index out of bounds");
	ASSERT_DEBUG(eYield > -1, "Index out of bounds");
	return m_piEventYield ? m_piEventYield[eYield] : -1;
}
//------------------------------------------------------------------------------
int CvModEventCityChoiceInfo::getPreCheckEventYield(YieldTypes eYield) const
{
	ASSERT_DEBUG(eYield < NUM_YIELD_TYPES, "Index out of bounds");
	ASSERT_DEBUG(eYield > -1, "Index out of bounds");
	return m_piPreCheckEventYield ? m_piPreCheckEventYield[eYield] : -1;
}
//------------------------------------------------------------------------------
int CvModEventCityChoiceInfo::getFlavorValue(int i) const
{
	ASSERT_DEBUG(i < GC.getNumFlavorTypes(), "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_piFlavor ? m_piFlavor[i] : 0;
}
//------------------------------------------------------------------------------
int CvModEventCityChoiceInfo::getWLTKD() const
{
	return m_iNumWLTKD;
}
//------------------------------------------------------------------------------
int CvModEventCityChoiceInfo::getGrowthMod() const
{
	return m_iGrowthMod;
}
//------------------------------------------------------------------------------
int CvModEventCityChoiceInfo::getResistanceTurns() const
{
	return m_iResistanceTurns;
}
//------------------------------------------------------------------------------
int CvModEventCityChoiceInfo::getRandomBarbs() const
{
	return m_iRandomBarbs;
}
//------------------------------------------------------------------------------
int CvModEventCityChoiceInfo::getRandomBarbsPerEra() const
{
	return m_iRandomBarbsPerEra;
}
//------------------------------------------------------------------------------
int CvModEventCityChoiceInfo::getFreeScaledUnits() const
{
	return m_iFreeScaledUnits;
}
//------------------------------------------------------------------------------
int CvModEventCityChoiceInfo::getSpecialistsGreatPersonPointsPerTurn() const
{
	return m_iSpecialistsGreatPersonPointsPerTurn;
}
//------------------------------------------------------------------------------
int CvModEventCityChoiceInfo::getCityHappiness() const
{
	return m_iCityHappiness;
}
//------------------------------------------------------------------------------
int CvModEventCityChoiceInfo::getReligiousPressureModifier() const
{
	return m_iReligiousPressureModifier;
}
//------------------------------------------------------------------------------
int CvModEventCityChoiceInfo::getBasicNeedsMedianModifier() const
{
	return m_iBasicNeedsMedianModifier;
}
//------------------------------------------------------------------------------
int CvModEventCityChoiceInfo::getGoldMedianModifier() const
{
	return m_iGoldMedianModifier;
}
//------------------------------------------------------------------------------
int CvModEventCityChoiceInfo::getScienceMedianModifier() const
{
	return m_iScienceMedianModifier;
}
//------------------------------------------------------------------------------
int CvModEventCityChoiceInfo::getCultureMedianModifier() const
{
	return m_iCultureMedianModifier;
}
//------------------------------------------------------------------------------
int CvModEventCityChoiceInfo::getReligiousUnrestModifier() const
{
	return m_iReligiousUnrestModifier;
}
//------------------------------------------------------------------------------
const char* CvModEventCityChoiceInfo::getEventChoiceSoundEffect() const
{
	return m_strEventChoiceSoundEffect.c_str();
}
//------------------------------------------------------------------------------
int CvModEventCityChoiceInfo::getEventPromotion() const
{
	return m_iEventPromotion;
}
//------------------------------------------------------------------------------
int CvModEventCityChoiceInfo::getNumFreeUnits(int i) const
{
	ASSERT_DEBUG(i < GC.getNumUnitClassInfos(), "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_piNumFreeUnits ? m_piNumFreeUnits[i] : -1;
}
//------------------------------------------------------------------------------
int CvModEventCityChoiceInfo::getNumFreeSpecificUnits(int i) const
{
	ASSERT_DEBUG(i < GC.getNumUnitInfos(), "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_piNumFreeSpecificUnits ? m_piNumFreeSpecificUnits[i] : -1;
}
//------------------------------------------------------------------------------
int CvModEventCityChoiceInfo::getEventConvertReligion(int i) const
{
	ASSERT_DEBUG(i < GC.getNumReligionInfos(), "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_piConvertReligion ? m_piConvertReligion[i] : -1;
}
//------------------------------------------------------------------------------
int CvModEventCityChoiceInfo::getEventConvertReligionPercent(int i) const
{
	ASSERT_DEBUG(i < GC.getNumReligionInfos(), "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_piConvertReligionPercent ? m_piConvertReligionPercent[i] : -1;
}
//------------------------------------------------------------------------------
int CvModEventCityChoiceInfo::getEventGPChange(SpecialistTypes eSpecialist) const
{
	ASSERT_DEBUG(eSpecialist < GC.getNumSpecialistInfos(), "Index out of bounds");
	ASSERT_DEBUG(eSpecialist > -1, "Index out of bounds");
	return m_piGPChange ? m_piGPChange[eSpecialist] : 0;
}
//------------------------------------------------------------------------------
int CvModEventCityChoiceInfo::getImprovementDestruction(ImprovementTypes eImprovement) const
{
	ASSERT_DEBUG(eImprovement < GC.getNumImprovementInfos(), "Index out of bounds");
	ASSERT_DEBUG(eImprovement > -1, "Index out of bounds");
	return m_piDestroyImprovement ? m_piDestroyImprovement[eImprovement] : 0;
}
//------------------------------------------------------------------------------
int CvModEventCityChoiceInfo::getBuildingDestructionChance(int i) const
{
	ASSERT_DEBUG(i < GC.getNumBuildingClassInfos(), "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_piBuildingDestructionChance ? m_piBuildingDestructionChance[i] : -1;
}
//------------------------------------------------------------------------------
int CvModEventCityChoiceInfo::getCityWideDestructionChance() const
{
	return m_iCityWideDestructionChance;
}

int CvModEventCityChoiceInfo::getCityStrategicResourcePillage() const
{
	return m_iCityStrategicResourcePillage;
}

int CvModEventCityChoiceInfo::getPillageResourceTilesChance() const
{
	return m_iPillageResourceTilesChance;
}

int CvModEventCityChoiceInfo::getPillageRoadsChance() const
{
	return m_iPillageRoadsChance;
}

int CvModEventCityChoiceInfo::getPillageFortificationsChance() const
{
	return m_iPillageFortificationsChance;
}

int CvModEventCityChoiceInfo::getMutuallyExclusiveGroup() const
{
	return m_iMutuallyExclusiveGroup;
}

int CvModEventCityChoiceInfo::getBlockBuildingTurns() const
{
	return m_iBlockBuildingTurns;
}

CvCityEventNotificationInfo *CvModEventCityChoiceInfo::GetNotificationInfo(int i) const
{
//	ASSERT_DEBUG(i < GC.getNumNotificationInfos(), "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");

	if (m_paCityNotificationInfo[0].GetNotificationString().empty() || m_paCityNotificationInfo[0].GetNotificationString() == NULL)
	{
		return NULL;
	}
	else
	{
		return &m_paCityNotificationInfo[i];
	}
}

CvCityEventChoiceLinkingInfo *CvModEventCityChoiceInfo::GetLinkerInfo(int i) const
{
	ASSERT_DEBUG(i < GetNumLinkers(), "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");

	if (m_paCityLinkerInfo[0].GetCityLinkingEvent() == -1 && m_paCityLinkerInfo[0].GetCityLinkingEventChoice() == -1 && m_paCityLinkerInfo[0].GetLinkingEvent() == -1 && m_paCityLinkerInfo[0].GetLinkingEventChoice() == -1)
	{
		return NULL;
	}
	else
	{
		return &m_paCityLinkerInfo[i];
	}
}
//------------------------------------------------------------------------------
int CvModEventCityChoiceInfo::getCityYield(int i) const
{
	ASSERT_DEBUG(i < NUM_YIELD_TYPES, "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_piCityYield ? m_piCityYield[i] : -1;
}

int CvModEventCityChoiceInfo::getCityYieldModifier(int i) const
{
	ASSERT_DEBUG(i < NUM_YIELD_TYPES, "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_piCityYieldModifier ? m_piCityYieldModifier[i] : -1;
}

int CvModEventCityChoiceInfo::getYieldSiphon(int i) const
{
	ASSERT_DEBUG(i < NUM_YIELD_TYPES, "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_piYieldSiphon ? m_piYieldSiphon[i] : -1;
}

int CvModEventCityChoiceInfo::getYieldOnSpyIdentified(int i) const
{
	ASSERT_DEBUG(i < NUM_YIELD_TYPES, "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_piYieldOnSpyIdentified ? m_piYieldOnSpyIdentified[i] : -1;
}
int CvModEventCityChoiceInfo::getYieldOnSpyKilled(int i) const
{
	ASSERT_DEBUG(i < NUM_YIELD_TYPES, "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	return m_piYieldOnSpyKilled ? m_piYieldOnSpyKilled[i] : -1;
}

/// Yield change for a specific BuildingClass by yield type
int CvModEventCityChoiceInfo::getBuildingClassYield(int i, int j) const
{
	ASSERT_DEBUG(i < GC.getNumBuildingClassInfos(), "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	ASSERT_DEBUG(j < NUM_YIELD_TYPES, "Index out of bounds");
	ASSERT_DEBUG(j > -1, "Index out of bounds");
	return m_ppiBuildingClassYield[i][j];
}
/// Yield modifier change for a specific BuildingClass by yield type
int CvModEventCityChoiceInfo::getBuildingClassYieldModifier(int i, int j) const
{
	ASSERT_DEBUG(i < GC.getNumBuildingClassInfos(), "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	ASSERT_DEBUG(j < NUM_YIELD_TYPES, "Index out of bounds");
	ASSERT_DEBUG(j > -1, "Index out of bounds");
	return m_ppiBuildingClassYieldModifier[i][j];
}
int CvModEventCityChoiceInfo::getTerrainYield(int i, int j) const
{
	ASSERT_DEBUG(i < GC.getNumTerrainInfos(), "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	ASSERT_DEBUG(j < NUM_YIELD_TYPES, "Index out of bounds");
	ASSERT_DEBUG(j > -1, "Index out of bounds");
	return m_ppiTerrainYield[i][j];
}
int CvModEventCityChoiceInfo::getFeatureYield(int i, int j) const
{
	ASSERT_DEBUG(i < GC.getNumFeatureInfos(), "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	ASSERT_DEBUG(j < NUM_YIELD_TYPES, "Index out of bounds");
	ASSERT_DEBUG(j > -1, "Index out of bounds");
	return m_ppiFeatureYield[i][j];
}
int CvModEventCityChoiceInfo::getImprovementYield(int i, int j) const
{
	ASSERT_DEBUG(i < GC.getNumImprovementInfos(), "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	ASSERT_DEBUG(j < NUM_YIELD_TYPES, "Index out of bounds");
	ASSERT_DEBUG(j > -1, "Index out of bounds");
	return m_ppiImprovementYield[i][j];
}
int CvModEventCityChoiceInfo::getResourceYield(int i, int j) const
{
	ASSERT_DEBUG(i < GC.getNumResourceInfos(), "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	ASSERT_DEBUG(j < NUM_YIELD_TYPES, "Index out of bounds");
	ASSERT_DEBUG(j > -1, "Index out of bounds");
	return m_ppiResourceYield[i][j];
}
//------------------------------------------------------------------------------
int CvModEventCityChoiceInfo::getCitySpecialistYieldChange(int i, int j) const
{
	ASSERT_DEBUG(i < GC.getNumSpecialistInfos(), "Index out of bounds");
	ASSERT_DEBUG(i > -1, "Index out of bounds");
	ASSERT_DEBUG(j < NUM_YIELD_TYPES, "Index out of bounds");
	ASSERT_DEBUG(j > -1, "Index out of bounds");
	return m_ppiSpecialistYield[i][j];
}
//------------------------------------------------------------------------------
int CvModEventCityChoiceInfo::getEventResourceChange(ResourceTypes eResource) const
{
	ASSERT_DEBUG(eResource < GC.getNumResourceInfos(), "Index out of bounds");
	ASSERT_DEBUG(eResource > -1, "Index out of bounds");
	return m_piResourceChange ? m_piResourceChange[eResource] : 0;
}
// Filters
//------------------------------------------------------------------------------
int CvModEventCityChoiceInfo::getPrereqTech() const
{
	return m_iPrereqTech;
}
//------------------------------------------------------------------------------
int CvModEventCityChoiceInfo::getObsoleteTech() const
{
	return m_iObsoleteTech;
}
//------------------------------------------------------------------------------
int CvModEventCityChoiceInfo::getMinimumPopulation() const
{
	return m_iMinimumPopulation;
}
//------------------------------------------------------------------------------
int CvModEventCityChoiceInfo::getRequiredCiv() const
{
	return m_iRequiredCiv;
}
//------------------------------------------------------------------------------
int CvModEventCityChoiceInfo::getRequiredEra() const
{
	return m_iRequiredEra;
}
//------------------------------------------------------------------------------
int CvModEventCityChoiceInfo::getObsoleteEra() const
{
	return m_iObsoleteEra;
}
//------------------------------------------------------------------------------
int CvModEventCityChoiceInfo::getYieldMinimum(YieldTypes eYield) const
{
	ASSERT_DEBUG(eYield < NUM_YIELD_TYPES, "Index out of bounds");
	ASSERT_DEBUG(eYield > -1, "Index out of bounds");
	return m_piMinimumYield ? m_piMinimumYield[eYield] : -1;
}
//------------------------------------------------------------------------------
int CvModEventCityChoiceInfo::getRequiredImprovement() const
{
	return m_iRequiredImprovement;
}
//------------------------------------------------------------------------------
int CvModEventCityChoiceInfo::getRequiredPolicy() const
{
	return m_iRequiredPolicy;
}
//------------------------------------------------------------------------------
int CvModEventCityChoiceInfo::getRequiredIdeology() const
{
	return m_iIdeology;
}
//------------------------------------------------------------------------------
bool CvModEventCityChoiceInfo::isRequiresHolyCity() const
{
	return m_bRequiresHolyCity;
}
//------------------------------------------------------------------------------
int CvModEventCityChoiceInfo::getRequiredReligion() const
{
	return m_iRequiredReligion;
}
//------------------------------------------------------------------------------
int CvModEventCityChoiceInfo::getBuildingRequired() const
{
	return m_iBuildingRequired;
}
//------------------------------------------------------------------------------
int CvModEventCityChoiceInfo::getBuildingLimiter() const
{
	return m_iBuildingLimiter;
}
//------------------------------------------------------------------------------
bool CvModEventCityChoiceInfo::isCapital() const
{
	return m_bCapital;
}
//------------------------------------------------------------------------------
bool CvModEventCityChoiceInfo::isCoastal() const
{
	return m_bCoastal;
}
//------------------------------------------------------------------------------
bool CvModEventCityChoiceInfo::isRiver() const
{
	return m_bIsRiver;
}
//------------------------------------------------------------------------------
bool CvModEventCityChoiceInfo::isRequiresIdeology() const
{
	return m_bRequiresIdeology;
}
//------------------------------------------------------------------------------
bool CvModEventCityChoiceInfo::isRequiresWar() const
{
	return m_bRequiresWar;
}

//------------------------------------------------------------------------------
bool CvModEventCityChoiceInfo::isRequiresWarMinor() const
{
	return m_bRequiresWarMinor;
}
//---------------------------------------------------------------------------
int CvModEventCityChoiceInfo::getRequiredStateReligion() const
{
	return m_iRequiredStateReligion;
}
//------------------------------------------------------------------------------
bool CvModEventCityChoiceInfo::isRequiresGarrison() const
{
	return m_bRequiresGarrison;
}
//------------------------------------------------------------------------------
bool CvModEventCityChoiceInfo::isResistance() const
{
	return m_bIsResistance;
}
//------------------------------------------------------------------------------
bool CvModEventCityChoiceInfo::isWLTKD() const
{
	return m_bIsWLTKD;
}
//------------------------------------------------------------------------------
bool CvModEventCityChoiceInfo::isOccupied() const
{
	return m_bIsOccupied;
}
//------------------------------------------------------------------------------
bool CvModEventCityChoiceInfo::isRazing() const
{
	return m_bIsRazing;
}
//------------------------------------------------------------------------------
bool CvModEventCityChoiceInfo::hasAnyReligion() const
{
	return m_bHasAnyReligion;
}
//------------------------------------------------------------------------------
bool CvModEventCityChoiceInfo::isPuppet() const
{
	return m_bIsPuppet;
}
//------------------------------------------------------------------------------
bool CvModEventCityChoiceInfo::isNotPuppet() const
{
	return m_bIsNotPuppet;
}
//------------------------------------------------------------------------------
bool CvModEventCityChoiceInfo::hasTradeConnection() const
{
	return m_bTradeConnection;
}
//------------------------------------------------------------------------------
bool CvModEventCityChoiceInfo::hasCityConnection() const
{
	return m_bCityConnection;
}
//------------------------------------------------------------------------------
bool CvModEventCityChoiceInfo::hasStateReligion() const
{
	return m_bHasStateReligion;
}
//------------------------------------------------------------------------------
bool CvModEventCityChoiceInfo::isUnhappy() const
{
	return m_bUnhappy;
}
//------------------------------------------------------------------------------
bool CvModEventCityChoiceInfo::isSuperUnhappy() const
{
	return m_bSuperUnhappy;
}
//------------------------------------------------------------------------------
bool CvModEventCityChoiceInfo::isEnemyUnhappy() const
{
	return m_bEnemyUnhappy;
}
//------------------------------------------------------------------------------
bool CvModEventCityChoiceInfo::isEnemySuperUnhappy() const
{
	return m_bEnemySuperUnhappy;
}
//------------------------------------------------------------------------------
int CvModEventCityChoiceInfo::getLocalResourceRequired() const
{
	return m_iLocalResourceRequired;
}
//------------------------------------------------------------------------------
bool CvModEventCityChoiceInfo::isNearNaturalWonder() const
{
	return m_bNearNaturalWonder;
}
//------------------------------------------------------------------------------
bool CvModEventCityChoiceInfo::isNearMountain() const
{
	return m_bNearMountain;
}
	//------------------------------------------------------------------------------
bool CvModEventCityChoiceInfo::hasPantheon() const
{
	return m_bPantheon;
}
//------------------------------------------------------------------------------
int CvModEventCityChoiceInfo::hasNearbyFeature() const
{
	return m_iNearbyFeature;
}
//------------------------------------------------------------------------------
int CvModEventCityChoiceInfo::hasNearbyTerrain() const
{
	return m_iNearbyTerrain;
}
//------------------------------------------------------------------------------
int CvModEventCityChoiceInfo::getMaximumPopulation() const
{
	return m_iMaximumPopulation;
}
//------------------------------------------------------------------------------
bool CvModEventCityChoiceInfo::isOneShot() const
{
	return m_bOneShot;
}
//------------------------------------------------------------------------------
bool CvModEventCityChoiceInfo::hasMetAnotherCiv() const
{
	return m_bMetAnotherCiv;
}
//------------------------------------------------------------------------------
bool CvModEventCityChoiceInfo::isInDebt() const
{
	return m_bInDebt;
}
//------------------------------------------------------------------------------
bool CvModEventCityChoiceInfo::isLosingMoney() const
{
	return m_bLosingMoney;
}
//------------------------------------------------------------------------------
bool CvModEventCityChoiceInfo::isVassal() const
{
	return m_bVassal;
}
//------------------------------------------------------------------------------
bool CvModEventCityChoiceInfo::isMaster() const
{
	return m_bMaster;
}
//------------------------------------------------------------------------------
bool CvModEventCityChoiceInfo::hasPlayerReligion() const
{
	return m_bHasPlayerReligion;
}
//------------------------------------------------------------------------------
bool CvModEventCityChoiceInfo::lacksPlayerReligion() const
{
	return m_bLacksPlayerReligion;
}
//------------------------------------------------------------------------------
bool CvModEventCityChoiceInfo::hasPlayerMajority() const
{
	return m_bHasPlayerMajority;
}
//------------------------------------------------------------------------------
bool CvModEventCityChoiceInfo::lacksPlayerMajority() const
{
	return m_bLacksPlayerMajority;
}
//------------------------------------------------------------------------------
const char* CvModEventCityChoiceInfo::getDisabledTooltip() const
{
	return m_strDisabledTooltip;
}
//------------------------------------------------------------------------------
const char* CvModEventCityChoiceInfo::getSpyMissionEffect() const
{
	return m_strSpyMissionEffect;
}
//------------------------------------------------------------------------------
bool CvModEventCityChoiceInfo::CacheResults(Database::Results& kResults, CvDatabaseUtility& kUtility)
{
	if(!CvBaseInfo::CacheResults(kResults, kUtility))
		return false;

	const char* szTextVal = NULL;

	const char* szEventType = GetType();
	kUtility.PopulateArrayByExistence(m_pbParentEventIDs, "CityEvents", "CityEvent_ParentEvents", "CityEventType", "CityEventChoiceType", szEventType);

	szTextVal = kResults.GetText("EventBuildingClass");
	m_iEventBuilding =  GC.getInfoTypeForString(szTextVal, true);

	m_iEventDuration = kResults.GetInt("EventDuration");
	m_bEventDurationScaling = kResults.GetBool("EventDurationScaling");
	m_iEventChance = kResults.GetInt("EventChance");

	m_bEraScaling = kResults.GetBool("EraScaling");
	m_bExpires = kResults.GetBool("Expires");

	m_bOneShot = kResults.GetBool("IsOneShot");

	szTextVal = kResults.GetText("TriggerPlayerEventChoice");
	m_iTriggerPlayerEventChoice = GC.getInfoTypeForString(szTextVal, true);

	//espionage!
	m_bIsCounterspyMission = kResults.GetBool("IsCounterSpyMission");
	m_bIsEspionageMission = kResults.GetBool("IsEspionageMission");
	m_iNetworkPointsNeeded = kResults.GetInt("NetworkPointsNeeded");
	m_bNetworkPointsScaling = kResults.GetBool("NetworkPointsScaling");
	m_iSpyIdentificationChance = kResults.GetInt("SpyIDChance");
	m_iSpyKillChance = kResults.GetInt("SpyKillChance");
	m_iSpyLevelRequired = kResults.GetInt("SpyLevelRequired");
	m_iStealTech = kResults.GetInt("StealNumTechs");
	m_iStealGW = kResults.GetInt("StealNumGW");
	m_iSapCityTurns = kResults.GetInt("SapCityTurns");
	m_iNoTourismTurns = kResults.GetInt("NoTourismTurns");
	m_iStealFromTreasuryPercent = kResults.GetInt("StealFromTreasuryPercent");
	// Counterspies
	m_iCounterspyNPRateReduction = kResults.GetInt("CounterspyNPRateReduction");
	m_bCounterspyBlockSapCity = kResults.GetBool("CounterspyBlockSapCity");
	m_iCityDefenseModifierBase = kResults.GetInt("CityDefenseModifierBase");
	m_iCityDefenseModifier = kResults.GetInt("CityDefenseModifier");

	m_iConvertsCityToPlayerReligion = kResults.GetBool("ConvertToPlayerReligionPercent");
	m_iConvertsCityToPlayerMajorityReligion = kResults.GetBool("ConvertToPlayerMajorityReligionPercent");
	m_bHasPlayerReligion = kResults.GetBool("CityHasPlayerReligion");
	m_bLacksPlayerReligion = kResults.GetBool("CityLacksPlayerReligion");

	m_bHasPlayerMajority = kResults.GetBool("CityHasPlayerMajorityReligion");
	m_bLacksPlayerMajority = kResults.GetBool("CityLacksPlayerMajorityReligion");

	m_bMetAnotherCiv = kResults.GetBool("HasMetAMajorCiv");
	m_bInDebt = kResults.GetBool("InDebt");
	m_bLosingMoney = kResults.GetBool("LosingMoney");

	m_bVassal = kResults.GetBool("IsVassalOfSomeone");
	m_bMaster = kResults.GetBool("IsMasterOfSomeone");

	szTextVal = kResults.GetText("EventBuildingClassDestruction");
	m_iEventBuildingDestruction =  GC.getInfoTypeForString(szTextVal, true);

	szTextVal = kResults.GetText("DisabledTooltip");
	m_strDisabledTooltip = szTextVal;

	szTextVal = kResults.GetText("SpyMissionEffect");
	m_strSpyMissionEffect = szTextVal;

	kUtility.PopulateArrayByValue(m_piResourceChange, "Resources", "CityEventChoice_ResourceQuantity", "ResourceType", "CityEventChoiceType", szEventType, "Quantity");

	kUtility.SetYields(m_piEventYield, "CityEventChoice_InstantYield", "CityEventChoiceType", szEventType);
	kUtility.SetYields(m_piPreCheckEventYield, "CityEventChoice_EventCostYield", "CityEventChoiceType", szEventType);

	kUtility.SetYields(m_piCityYield, "CityEventChoice_CityYield", "CityEventChoiceType", szEventType);
	kUtility.SetYields(m_piCityYieldModifier, "CityEventChoice_CityYieldModifier", "CityEventChoiceType", szEventType);

	kUtility.SetYields(m_piYieldSiphon, "CityEventChoice_YieldSiphon", "CityEventChoiceType", szEventType);
	kUtility.SetYields(m_piYieldOnSpyIdentified, "CityEventChoice_YieldOnSpyIdentified", "CityEventChoiceType", szEventType);
	kUtility.SetYields(m_piYieldOnSpyKilled, "CityEventChoice_YieldOnSpyKilled", "CityEventChoiceType", szEventType);

	kUtility.PopulateArrayByValue(m_piGPChange, "Specialists", "CityEventChoice_GreatPersonPoints", "SpecialistType", "CityEventChoiceType", szEventType, "Points");
	kUtility.PopulateArrayByValue(m_piDestroyImprovement, "Improvements", "CityEventChoice_ImprovementDestructionRandom", "ImprovementType", "CityEventChoiceType", szEventType, "Number");

	kUtility.SetFlavors(m_piFlavor, "CityEventChoiceFlavors", "CityEventChoiceType", szEventType);

	m_iGrowthMod = kResults.GetInt("GrowthMod");
	m_iNumWLTKD = kResults.GetInt("WLTKDTurns");
	m_iResistanceTurns = kResults.GetInt("ResistanceTurns");
	m_iRandomBarbs = kResults.GetInt("RandomBarbarianSpawn");
	m_iRandomBarbsPerEra = kResults.GetInt("RandomBarbarianSpawnPerEra");
	m_iFreeScaledUnits = kResults.GetInt("FreeUnitsTechAppropriate");
	m_iSpecialistsGreatPersonPointsPerTurn = kResults.GetInt("SpecialistsGreatPersonPointsPerTurn");
	m_iCityWideDestructionChance = kResults.GetInt("CityWideBuildingDestructionChance");
	m_iCityStrategicResourcePillage = kResults.GetInt("PillageCityStrategicNum");
	m_iPillageResourceTilesChance = kResults.GetInt("PillageResourceTilesChance");
	m_iPillageRoadsChance = kResults.GetInt("PillageRoadsChance");
	m_iPillageFortificationsChance = kResults.GetInt("PillageFortificationsChance");
	m_iMutuallyExclusiveGroup = kResults.GetInt("MutuallyExclusiveGroup");
	m_iBlockBuildingTurns = kResults.GetInt("BlockBuildingTurns");

	m_iCityHappiness = kResults.GetInt("CityHappiness");
	m_iReligiousPressureModifier = kResults.GetInt("ReligiousPressureModifier");

	szTextVal = kResults.GetText("FreePromotionCity");
	m_iEventPromotion =  GC.getInfoTypeForString(szTextVal, true);

	szTextVal = kResults.GetText("EventChoiceAudio");
	m_strEventChoiceSoundEffect = szTextVal;

	kUtility.PopulateArrayByValue(m_piNumFreeUnits, "UnitClasses", "CityEventChoice_FreeUnitClasses", "UnitClassType", "CityEventChoiceType", szEventType, "Quantity");
	kUtility.PopulateArrayByValue(m_piNumFreeSpecificUnits, "Units", "CityEventChoice_FreeUnits", "UnitType", "CityEventChoiceType", szEventType, "Quantity");
	kUtility.PopulateArrayByValue(m_piConvertReligion, "Religions", "CityEventChoice_ConvertNumPopToReligion", "ReligionType", "CityEventChoiceType", szEventType, "Population");
	kUtility.PopulateArrayByValue(m_piConvertReligionPercent, "Religions", "CityEventChoice_ConvertPercentPopToReligion", "ReligionType", "CityEventChoiceType", szEventType, "Percent");
	kUtility.PopulateArrayByValue(m_piBuildingDestructionChance, "BuildingClasses", "CityEventChoice_BuildingClassDestructionChance", "BuildingClassType", "CityEventChoiceType", szEventType, "Chance");

	szTextVal = kResults.GetText("LocalResourceRequired");
	m_iLocalResourceRequired =  GC.getInfoTypeForString(szTextVal, true);

	{
		//Initialize Notifications
		const size_t iNumNotifications = kUtility.MaxRows("Notifications");
		m_paCityNotificationInfo = FNEW(CvCityEventNotificationInfo[iNumNotifications], c_eCiv5GameplayDLL, 0);
		int idx = 0;

		std::string strCityEventChoiceTypesKey = "CityEventChoice_Notification";
		Database::Results* pCityEventChoiceTypes = kUtility.GetResults(strCityEventChoiceTypesKey);
		if(pCityEventChoiceTypes == NULL)
		{
			pCityEventChoiceTypes = kUtility.PrepareResults(strCityEventChoiceTypesKey, "select NotificationType, Description, ShortDescription, IsWorldEvent, EspionageEvent, NeedCityCoordinates, NeedPlayerID, ExtraVariable from CityEventChoice_Notification where CityEventChoiceType = ?");
		}

		const size_t lenCityEventChoiceType = strlen(szEventType);
		pCityEventChoiceTypes->Bind(1, szEventType, lenCityEventChoiceType, false);

		while(pCityEventChoiceTypes->Step())
		{
			CvCityEventNotificationInfo& pCityEventNotificationInfo = m_paCityNotificationInfo[idx];
			pCityEventNotificationInfo.m_strNotificationType = pCityEventChoiceTypes->GetText("NotificationType");
			pCityEventNotificationInfo.m_strDescription = pCityEventChoiceTypes->GetText("Description");
			pCityEventNotificationInfo.m_strShortDescription = pCityEventChoiceTypes->GetText("ShortDescription");
			pCityEventNotificationInfo.m_bWorldEvent = pCityEventChoiceTypes->GetBool("IsWorldEvent");
			pCityEventNotificationInfo.m_bEspionageEvent = pCityEventChoiceTypes->GetBool("EspionageEvent");
			pCityEventNotificationInfo.m_bNeedCityCoordinates = pCityEventChoiceTypes->GetInt("NeedCityCoordinates")>0;
			pCityEventNotificationInfo.m_bNeedPlayerID = pCityEventChoiceTypes->GetBool("NeedPlayerID");
			pCityEventNotificationInfo.m_iVariable = pCityEventChoiceTypes->GetInt("ExtraVariable");
			idx++;
		}

		m_iCityNotificationInfos = idx;
		pCityEventChoiceTypes->Reset();
	}
	{
		//Initialize Linker Table
		const size_t iNumLinkers = kUtility.MaxRows("Notifications");
		m_paCityLinkerInfo = FNEW(CvCityEventChoiceLinkingInfo[iNumLinkers], c_eCiv5GameplayDLL, 0);
		int idx = 0;

		std::string strCityEventChoiceTypesKey = "CityEventChoice_EventLinks";
		Database::Results* pCityEventChoiceTypes = kUtility.GetResults(strCityEventChoiceTypesKey);
		if(pCityEventChoiceTypes == NULL)
		{
			pCityEventChoiceTypes = kUtility.PrepareResults(strCityEventChoiceTypesKey, "select Event, EventChoice, CityEvent, CityEventChoiceLinker, CheckKnownPlayers, CheckOnlyEventCity, CheckForActive from CityEventChoice_EventLinks where CityEventChoiceType = ?");
		}

		const size_t lenCityEventChoiceType = strlen(szEventType);
		pCityEventChoiceTypes->Bind(1, szEventType, lenCityEventChoiceType, false);

		while(pCityEventChoiceTypes->Step())
		{
			CvCityEventChoiceLinkingInfo& pCityEventLinkingInfo= m_paCityLinkerInfo[idx];
			szTextVal = pCityEventChoiceTypes->GetText("Event");
			pCityEventLinkingInfo.m_iEvent =  GC.getInfoTypeForString(szTextVal, true);
			szTextVal = pCityEventChoiceTypes->GetText("EventChoice");
			pCityEventLinkingInfo.m_iEventChoice =  GC.getInfoTypeForString(szTextVal, true);
			szTextVal = pCityEventChoiceTypes->GetText("CityEvent");
			pCityEventLinkingInfo.m_iCityEvent =  GC.getInfoTypeForString(szTextVal, true);
			szTextVal = pCityEventChoiceTypes->GetText("CityEventChoiceLinker");
			pCityEventLinkingInfo.m_iCityEventChoice =  GC.getInfoTypeForString(szTextVal, true);
			pCityEventLinkingInfo.m_bOnlyActiveCity = pCityEventChoiceTypes->GetBool("CheckOnlyEventCity");
			pCityEventLinkingInfo.m_bActive = pCityEventChoiceTypes->GetBool("CheckForActive");
			idx++;
		}

		m_iCityLinkerInfos = idx;
		pCityEventChoiceTypes->Reset();
	}
	
	//BuildingYieldChanges
	{
		kUtility.Initialize2DArray(m_ppiBuildingClassYield, "BuildingClasses", "Yields");

		std::string strKey("CityEventChoice_BuildingClassYieldChange");
		Database::Results* pResults = kUtility.GetResults(strKey);
		if(pResults == NULL)
		{
			pResults = kUtility.PrepareResults(strKey, "select BuildingClasses.ID as BuildingClassID, Yields.ID as YieldID, YieldChange from CityEventChoice_BuildingClassYieldChange inner join BuildingClasses on BuildingClasses.Type = BuildingClassType inner join Yields on Yields.Type = YieldType where CityEventChoiceType = ?");
		}

		pResults->Bind(1, szEventType);

		while(pResults->Step())
		{
			const int BuildingClassID = pResults->GetInt(0);
			const int iYieldID = pResults->GetInt(1);
			const int iYieldChange = pResults->GetInt(2);

			m_ppiBuildingClassYield[BuildingClassID][iYieldID] = iYieldChange;
		}
	}
	//BuildingYieldModifierChanges
	{
		kUtility.Initialize2DArray(m_ppiBuildingClassYieldModifier, "BuildingClasses", "Yields");

		std::string strKey("CityEventChoice_BuildingClassYieldModifier");
		Database::Results* pResults = kUtility.GetResults(strKey);
		if(pResults == NULL)
		{
			pResults = kUtility.PrepareResults(strKey, "select BuildingClasses.ID as BuildingClassID, Yields.ID as YieldID, Modifier from CityEventChoice_BuildingClassYieldModifier inner join BuildingClasses on BuildingClasses.Type = BuildingClassType inner join Yields on Yields.Type = YieldType where CityEventChoiceType = ?");
		}

		pResults->Bind(1, szEventType);

		while(pResults->Step())
		{
			const int BuildingClassID = pResults->GetInt(0);
			const int iYieldID = pResults->GetInt(1);
			const int iYieldChange = pResults->GetInt(2);

			m_ppiBuildingClassYieldModifier[BuildingClassID][iYieldID] = iYieldChange;
		}
	}
	
	//TerrainYieldChanges
	{
		kUtility.Initialize2DArray(m_ppiTerrainYield, "Terrains", "Yields");

		std::string strKey("CityEventChoice_TerrainYieldChange");
		Database::Results* pResults = kUtility.GetResults(strKey);
		if(pResults == NULL)
		{
			pResults = kUtility.PrepareResults(strKey, "select Terrains.ID as TerrainID, Yields.ID as YieldID, YieldChange from CityEventChoice_TerrainYieldChange inner join Terrains on Terrains.Type = TerrainType inner join Yields on Yields.Type = YieldType where CityEventChoiceType = ?");
		}

		pResults->Bind(1, szEventType);

		while(pResults->Step())
		{
			const int TerrainID = pResults->GetInt(0);
			const int iYieldID = pResults->GetInt(1);
			const int iYieldChange = pResults->GetInt(2);

			m_ppiTerrainYield[TerrainID][iYieldID] = iYieldChange;
		}
	}
	//ImprovementYieldChanges
	{
		kUtility.Initialize2DArray(m_ppiImprovementYield, "Improvements", "Yields");

		std::string strKey("CityEventChoice_ImprovementYieldChange");
		Database::Results* pResults = kUtility.GetResults(strKey);
		if(pResults == NULL)
		{
			pResults = kUtility.PrepareResults(strKey, "select Improvements.ID as ImprovementID, Yields.ID as YieldID, YieldChange from CityEventChoice_ImprovementYieldChange inner join Improvements on Improvements.Type = ImprovementType inner join Yields on Yields.Type = YieldType where CityEventChoiceType = ?");
		}

		pResults->Bind(1, szEventType);

		while(pResults->Step())
		{
			const int ImprovementID = pResults->GetInt(0);
			const int iYieldID = pResults->GetInt(1);
			const int iYieldChange = pResults->GetInt(2);

			m_ppiImprovementYield[ImprovementID][iYieldID] = iYieldChange;
		}
	}
	//ResourceYieldChanges
	{
		kUtility.Initialize2DArray(m_ppiResourceYield, "Resources", "Yields");

		std::string strKey("CityEventChoice_ResourceYieldChange");
		Database::Results* pResults = kUtility.GetResults(strKey);
		if(pResults == NULL)
		{
			pResults = kUtility.PrepareResults(strKey, "select Resources.ID as ResourceID, Yields.ID as YieldID, YieldChange from CityEventChoice_ResourceYieldChange inner join Resources on Resources.Type = ResourceType inner join Yields on Yields.Type = YieldType where CityEventChoiceType = ?");
		}

		pResults->Bind(1, szEventType);

		while(pResults->Step())
		{
			const int ResourceID = pResults->GetInt(0);
			const int iYieldID = pResults->GetInt(1);
			const int iYieldChange = pResults->GetInt(2);

			m_ppiResourceYield[ResourceID][iYieldID] = iYieldChange;
		}
	}
	//FeatureYieldChanges
	{
		kUtility.Initialize2DArray(m_ppiFeatureYield, "Features", "Yields");

		std::string strKey("CityEventChoice_FeatureYieldChange");
		Database::Results* pResults = kUtility.GetResults(strKey);
		if(pResults == NULL)
		{
			pResults = kUtility.PrepareResults(strKey, "select Features.ID as FeatureID, Yields.ID as YieldID, YieldChange from CityEventChoice_FeatureYieldChange inner join Features on Features.Type = FeatureType inner join Yields on Yields.Type = YieldType where CityEventChoiceType = ?");
		}

		pResults->Bind(1, szEventType);

		while(pResults->Step())
		{
			const int FeatureID = pResults->GetInt(0);
			const int iYieldID = pResults->GetInt(1);
			const int iYieldChange = pResults->GetInt(2);

			m_ppiFeatureYield[FeatureID][iYieldID] = iYieldChange;
		}
	}
	//SpecialistYieldChanges
	{
		kUtility.Initialize2DArray(m_ppiSpecialistYield, "Specialists", "Yields");

		std::string strKey("CityEventChoice_SpecialistYieldChange");
		Database::Results* pResults = kUtility.GetResults(strKey);
		if(pResults == NULL)
		{
			pResults = kUtility.PrepareResults(strKey, "select Specialists.ID as SpecialistID, Yields.ID as YieldID, YieldChange from CityEventChoice_SpecialistYieldChange inner join Specialists on Specialists.Type = SpecialistType inner join Yields on Yields.Type = YieldType where CityEventChoiceType = ?");
		}

		pResults->Bind(1, szEventType);

		while(pResults->Step())
		{
			const int SpecialistID = pResults->GetInt(0);
			const int iYieldID = pResults->GetInt(1);
			const int iYieldChange = pResults->GetInt(2);

			m_ppiSpecialistYield[SpecialistID][iYieldID] = iYieldChange;
		}
	}
	

	//Filters
	szTextVal = kResults.GetText("PrereqTech");
	m_iPrereqTech =  GC.getInfoTypeForString(szTextVal, true);

	szTextVal = kResults.GetText("ObsoleteTech");
	m_iObsoleteTech =  GC.getInfoTypeForString(szTextVal, true);

	m_iMinimumPopulation = kResults.GetInt("MinimumCityPopulation");

	szTextVal = kResults.GetText("ImprovementRequired");
	m_iRequiredImprovement =  GC.getInfoTypeForString(szTextVal, true);

	szTextVal = kResults.GetText("RequiredCiv");
	m_iRequiredCiv =  GC.getInfoTypeForString(szTextVal, true);

	szTextVal = kResults.GetText("RequiredEra");
	m_iRequiredEra =  GC.getInfoTypeForString(szTextVal, true);

	szTextVal = kResults.GetText("ObsoleteEra");
	m_iObsoleteEra =  GC.getInfoTypeForString(szTextVal, true);

	szTextVal = kResults.GetText("RequiredPolicy");
	m_iRequiredPolicy =  GC.getInfoTypeForString(szTextVal, true);

	szTextVal = kResults.GetText("RequiredIdeology");
	m_iIdeology =  GC.getInfoTypeForString(szTextVal, true);

	m_bRequiresHolyCity = kResults.GetBool("RequiresHolyCity");
	
	szTextVal = kResults.GetText("RequiredReligion");
	m_iRequiredReligion =  GC.getInfoTypeForString(szTextVal, true);

	m_bRequiresGarrison = kResults.GetBool("RequiresGarrison");
	m_bHasStateReligion = kResults.GetBool("RequiresAnyStateReligion");
	m_bEnemyUnhappy = kResults.GetBool("IsEnemyUnhappy");
	m_bEnemySuperUnhappy = kResults.GetBool("IsEnemySuperUnhappy");
	m_bUnhappy = kResults.GetBool("IsUnhappy");
	m_bSuperUnhappy = kResults.GetBool("IsSuperUnhappy");
	m_bRequiresIdeology = kResults.GetBool("RequiresIdeology");
	m_bRequiresWar = kResults.GetBool("RequiresWar");
	m_bCapital = kResults.GetBool("CapitalOnly");
	m_bCoastal = kResults.GetBool("CoastalOnly");
	m_bIsRiver = kResults.GetBool("RiverOnly");

	szTextVal = kResults.GetText("RequiredBuildingClass");
	m_iBuildingRequired =  GC.getInfoTypeForString(szTextVal, true);

	szTextVal = kResults.GetText("CannotHaveBuildingClass");
	m_iBuildingLimiter =  GC.getInfoTypeForString(szTextVal, true);

	kUtility.SetYields(m_piMinimumYield, "CityEventChoice_MinimumStartYield", "CityEventChoiceType", szEventType);

	m_bRequiresWarMinor = kResults.GetBool("RequiresWarMinor");

	m_bIsResistance = kResults.GetBool("RequiresResistance");
	m_bIsWLTKD = kResults.GetBool("RequiresWLTKD");
	m_bIsOccupied = kResults.GetBool("RequiresOccupied");
	m_bIsRazing = kResults.GetBool("RequiresRazing");
	m_bHasAnyReligion = kResults.GetBool("HasAnyReligion");
	m_bIsPuppet = kResults.GetBool("RequiresPuppet");
	m_bIsNotPuppet = kResults.GetBool("RequiresNotPuppet");
	m_bTradeConnection = kResults.GetBool("HasTradeConnection");
	m_bCityConnection = kResults.GetBool("HasCityConnection");

	szTextVal = kResults.GetText("RequiredStateReligion");
	m_iRequiredStateReligion =  GC.getInfoTypeForString(szTextVal, true);

	szTextVal = kResults.GetText("NearbyFeature");
	m_iNearbyFeature =  GC.getInfoTypeForString(szTextVal, true);

	szTextVal = kResults.GetText("NearbyTerrain");
	m_iNearbyTerrain =  GC.getInfoTypeForString(szTextVal, true);

	m_iMaximumPopulation = kResults.GetInt("MaximumCityPopulation");
	m_bPantheon = kResults.GetBool("RequiresPantheon");
	m_bNearMountain = kResults.GetBool("RequiresNearbyMountain");
	m_bNearNaturalWonder = kResults.GetBool("RequiresNearbyNaturalWonder");

	m_iBasicNeedsMedianModifier = kResults.GetInt("BasicNeedsMedianModifier");
	m_iGoldMedianModifier = kResults.GetInt("GoldMedianModifier");
	m_iScienceMedianModifier = kResults.GetInt("ScienceMedianModifier");
	m_iCultureMedianModifier = kResults.GetInt("CultureMedianModifier");
	m_iReligiousUnrestModifier = kResults.GetInt("ReligiousUnrestModifier");

	return true;
}
#endif
//------------------------------------------------------------------------------
bool CvEraInfo::getVassalageEnabled() const
{
	return m_bVassalageEnabled;
}
//------------------------------------------------------------------------------
int CvGameSpeedInfo::getTechCostPerTurnMultiplier() const
{
	return m_iTechCostPerTurnMultiplier;
}
//------------------------------------------------------------------------------
int CvGameSpeedInfo::getMinimumVoluntaryVassalTurns() const
{
	return m_iMinimumVoluntaryVassalTurns;
}
//------------------------------------------------------------------------------
int CvGameSpeedInfo::getMinimumVassalTurns() const
{
	return m_iMinimumVassalTurns;
}
//------------------------------------------------------------------------------
int CvGameSpeedInfo::getMinimumVassalTaxTurns() const
{
	return m_iMinimumVassalTaxTurns;
}
//------------------------------------------------------------------------------
int CvGameSpeedInfo::getMinimumVassalLiberateTurns() const
{
	return m_iMinimumVassalLiberateTurns;
}
//------------------------------------------------------------------------------
int CvGameSpeedInfo::getNumTurnsBetweenVassals() const
{
	return m_iNumTurnsBetweenVassals;
}

/// Helper function to read in an integer array of data sized according to number of building types
void FeatureArrayHelpers::Read(FDataStream& kStream, int* paiFeatureArray)
{
	int iNumEntries = 0;

	kStream >> iNumEntries;

	int iArraySize = GC.getNumFeatureInfos();
	for(int iI = 0; iI < iNumEntries; iI++)
	{
		uint uiHash = 0;
		kStream >> uiHash;
		if (uiHash != 0 && uiHash != (uint)NO_FEATURE)
		{
			int iType = GC.getInfoTypeForHash(uiHash);
			if(iType != -1 && iType < iArraySize)
			{
				kStream >> paiFeatureArray[iType];
			}
			else
			{
				CvString szError;
				szError.Format("LOAD ERROR: Feature Type not found");
				GC.LogMessage(szError.GetCString());
				ASSERT_DEBUG(false, szError);

				int iDummy = 0;
				kStream >> iDummy;
			}
		}
	}
}

/// Helper function to write out an integer array of data sized according to number of feature types
void FeatureArrayHelpers::Write(FDataStream& kStream, int* paiFeatureArray, int iArraySize)
{
	kStream << iArraySize;

	for(int iI = 0; iI < iArraySize; iI++)
	{
		const FeatureTypes eFeature = static_cast<FeatureTypes>(iI);
		CvFeatureInfo* pkFeatureInfo = GC.getFeatureInfo(eFeature);
		if(pkFeatureInfo)
		{
			CvInfosSerializationHelper::WriteHashed(kStream, pkFeatureInfo);
			kStream << paiFeatureArray[iI];
		}
		else
		{
			kStream << (int)NO_FEATURE;
		}
	}
}

/// Helper function to read in an integer array of data sized according to number of building types
void FeatureArrayHelpers::ReadYieldArray(FDataStream& kStream, int** ppaaiFeatureYieldArray, int iNumYields)
{
	int iNumEntries = 0;

	kStream >> iNumEntries;

	for(int iI = 0; iI < iNumEntries; iI++)
	{
		int iHash = 0;
		kStream >> iHash;
		if(iHash != 0)
		{
			int iType = GC.getInfoTypeForHash(iHash);
			if(iType != -1)
			{
				for(int jJ = 0; jJ < iNumYields; jJ++)
				{
					kStream >> ppaaiFeatureYieldArray[iType][jJ];
				}
			}
			else
			{
				CvString szError;
				szError.Format("LOAD ERROR: Feature Type not found: %08x", iHash);
				GC.LogMessage(szError.GetCString());
				ASSERT_DEBUG(false, szError);

				for(int jJ = 0; jJ < iNumYields; jJ++)
				{
					int iDummy = 0;
					kStream >> iDummy;
				}
			}
		}
	}
}

/// Helper function to write out an integer array of data sized according to number of feature types
void FeatureArrayHelpers::WriteYieldArray(FDataStream& kStream, int** ppaaiFeatureYieldArray, int iArraySize)
{
	kStream << iArraySize;

	for(int iI = 0; iI < iArraySize; iI++)
	{
		const FeatureTypes eFeature = static_cast<FeatureTypes>(iI);
		CvFeatureInfo* pkFeatureInfo = GC.getFeatureInfo(eFeature);
		if(pkFeatureInfo)
		{
			CvInfosSerializationHelper::WriteHashed(kStream, pkFeatureInfo);
			for(int jJ = 0; jJ < NUM_YIELD_TYPES; jJ++)
			{
				kStream << ppaaiFeatureYieldArray[iI][jJ];
			}
		}
		else
		{
			kStream << 0;
		}
	}
}


/// Helper function to read in an integer array of data sized according to number of building types
void TerrainArrayHelpers::Read(FDataStream& kStream, int* paiTerrainArray)
{
	int iNumEntries = 0;

	kStream >> iNumEntries;

	int iArraySize = GC.getNumTerrainInfos();
	for(int iI = 0; iI < iNumEntries; iI++)
	{
		uint uiHash = 0;
		kStream >> uiHash;
		if (uiHash != 0 && uiHash != (uint)NO_TERRAIN)
		{
			int iType = GC.getInfoTypeForHash(uiHash);
			if(iType != -1 && iType < iArraySize)
			{
				kStream >> paiTerrainArray[iType];
			}
			else
			{
				CvString szError;
				szError.Format("LOAD ERROR: Terrain Type not found");
				GC.LogMessage(szError.GetCString());
				ASSERT_DEBUG(false, szError);

				int iDummy = 0;
				kStream >> iDummy;
			}
		}
	}
}

/// Helper function to write out an integer array of data sized according to number of terrain types
void TerrainArrayHelpers::Write(FDataStream& kStream, int* paiTerrainArray, int iArraySize)
{
	kStream << iArraySize;

	for(int iI = 0; iI < iArraySize; iI++)
	{
		const TerrainTypes eTerrain = static_cast<TerrainTypes>(iI);
		CvTerrainInfo* pkTerrainInfo = GC.getTerrainInfo(eTerrain);
		if(pkTerrainInfo)
		{
			CvInfosSerializationHelper::WriteHashed(kStream, pkTerrainInfo);
			kStream << paiTerrainArray[iI];
		}
		else
		{
			kStream << (int)NO_TERRAIN;
		}
	}
}

/// Helper function to read in an integer array of data sized according to number of building types
void TerrainArrayHelpers::ReadYieldArray(FDataStream& kStream, int** ppaaiTerrainYieldArray, int iNumYields)
{
	int iNumEntries = 0;

	kStream >> iNumEntries;

	for(int iI = 0; iI < iNumEntries; iI++)
	{
		int iHash = 0;
		kStream >> iHash;
		if(iHash != 0)
		{
			int iType = GC.getInfoTypeForHash(iHash);
			if(iType != -1)
			{
				for(int jJ = 0; jJ < iNumYields; jJ++)
				{
					kStream >> ppaaiTerrainYieldArray[iType][jJ];
				}
			}
			else
			{
				CvString szError;
				szError.Format("LOAD ERROR: Terrain Type not found: %08x", iHash);
				GC.LogMessage(szError.GetCString());
				ASSERT_DEBUG(false, szError);

				for(int jJ = 0; jJ < iNumYields; jJ++)
				{
					int iDummy = 0;
					kStream >> iDummy;
				}
			}
		}
	}
}

/// Helper function to write out an integer array of data sized according to number of terrain types
void TerrainArrayHelpers::WriteYieldArray(FDataStream& kStream, int** ppaaiTerrainYieldArray, int iArraySize)
{
	kStream << iArraySize;

	for(int iI = 0; iI < iArraySize; iI++)
	{
		const TerrainTypes eTerrain = static_cast<TerrainTypes>(iI);
		CvTerrainInfo* pkTerrainInfo = GC.getTerrainInfo(eTerrain);
		if(pkTerrainInfo)
		{
			CvInfosSerializationHelper::WriteHashed(kStream, pkTerrainInfo);
			for(int jJ = 0; jJ < NUM_YIELD_TYPES; jJ++)
			{
				kStream << ppaaiTerrainYieldArray[iI][jJ];
			}
		}
		else
		{
			kStream << 0;
		}
	}
}