//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//!	 \file		CvAssert.h
//!  \brief     Public header of the CvAssert suite.
//!
//!		This file includes assertion macros used by Civilization.
//!
//!  Key Macros:
//!  - CvAssert(expr)				- assertions w/ no message
//!	 - CvAssertFmt(expr, fmt, ...)  - assertions w/ printf style messages
//!  - CvAssertMsg(expr, msg)		- assertions w/ constant messages
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
#ifndef _CVASSERT_H
#define _CVASSERT_H
#pragma once

#include <string>
#include <set>

#ifndef _WIN32
#include <cstdio>
#endif

bool CvAssertDlg(const char* expr, const char* szFile, unsigned int uiLine, bool& bIgnoreAlways, const char* msg) {
#ifdef _WIN32
    int nRet = MessageBoxA(NULL, msg, "Assertion Failed", MB_ICONERROR | MB_ABORTRETRYIGNORE | MB_SETFOREGROUND);
#else
    printf("%s\n%s\n%s\n", msg, "Abort (A)", "Ignore (I)");
    char response[16];
    fgets(response, 16, stdin);
    int nRet = (response[0] == 'a' || response[0] == 'A') ? 1 : 2;
#endif

    switch (nRet) {
    case 1:
        return true;
    case 2:
        return false;
    case 3:
        bIgnoreAlways = true;
        return false;
    default:
        return false;
    }
}

// Set breakpoint creation macros
#if defined (_WIN32)
#define CVASSERT_BREAKPOINT __debugbreak()
#else
#define CVASSERT_BREAKPOINT assert(0)
#endif

// Only compile in FAssert's if CVASSERT_ENABLE is defined.  By default, however, let's key off of
// _DEBUG.  Sometimes, however, it's useful to enable asserts in release builds, and you can do that
// simply by changing the following lines to define CVASSERT_ENABLE or using project settings to override
#if !defined(FINAL_RELEASE) || defined(VPDEBUG)
#define  CVASSERT_ENABLE
#endif  // FINAL_RELEASE


#ifdef CVASSERT_ENABLE 

#define CvAssertMsg(expr, msg) CvAssertMsg(expr, msg, __FILE__, __LINE__)
{                                                                                           \
    static bool bIgnoreAlways = false;                                                      \
    if (!bIgnoreAlways && !(expr))                                                          \
    {                                                                                       \
        std::string assertMsg = #expr;                                                       \
        std::string fileMsg = szFile;                                                        \
        unsigned int lineMsg = uiLine;                                                       \
        std::string message = (msg != NULL) ? msg : "";                                      \
        if (CvAssertDlg(assertMsg.c_str(), fileMsg.c_str(), lineMsg, bIgnoreAlways, message.c_str()))  \
        {                                                                                   \
            CVASSERT_BREAKPOINT;                                                            \
        }                                                                                   \
    }                                                                                       \
}

#define CvAssertFmt(expr, fmt, ...) CvAssertFmt(expr, fmt, __FILE__, __LINE__, __VA_ARGS__)
{                                                                                           \
    static bool bIgnoreAlways = false;                                                      \
    if (!bIgnoreAlways && !(expr))                                                          \
    {                                                                                       \
        char str[1024];                                                                     \
        sprintf(str, fmt, __VA_ARGS__);                                                     \
        std::string assertMsg = #expr;                                                       \
        std::string fileMsg = szFile;                                                        \
        unsigned int lineMsg = uiLine;                                                       \
        std::string message = str;                                                           \
        if (CvAssertDlg(assertMsg.c_str(), fileMsg.c_str(), lineMsg, bIgnoreAlways, message.c_str()))  \
        {                                                                                   \
            CVASSERT_BREAKPOINT;                                                            \
        }                                                                                   \
    }                                                                                       \
}

#define CvAssert(expr) CvAssertMsg(expr, "", __FILE__, __LINE__)

// An assert that only happens in the when CVASSERT_ENABLE is true AND it is a debug build
#ifdef _DEBUG
#define CvAssert_Debug( expr ) CvAssertMsg(expr, "")
#define CvAssertMsg_Debug(expr, msg) CvAssertMsg(expr, msg)
#else
#define CvAssert_Debug(expr)
#define CvAssertMsg_Debug(expr, msg)
#endif

#if 0 // disabling Object Validation
#include "FSTLBlockAllocator.h"

template<typename ObjectType, unsigned int pool_type>
struct ObjectValidator
{
	std::set<const ObjectType *, std::less<const ObjectType *>, FSTLBlockAllocator<ObjectType,1024,pool_type> > m_deletedObjects;
	void objectAllocated(const ObjectType * obj)
	{
		m_deletedObjects.erase(obj);
	}
	void objectDestroyed(const ObjectType * obj)
	{
		m_deletedObjects.insert(obj);
	}
	bool isObjectValid(const ObjectType * obj)
	{
		bool result = (obj != 0);
		if(result) 
			result = (m_deletedObjects.find(obj) == m_deletedObjects.end());
		CvAssertMsg(result, "Object is invalid in this context");
		return result;
	}
};

#define OBJECT_VALIDATE_DEFINITION(ObjectType) ObjectValidator<ObjectType, c_eDebugSafetyNet> s_objectValidator;

#define OBJECT_ALLOCATED s_objectValidator.objectAllocated(this);;
#define OBJECT_DESTROYED s_objectValidator.objectDestroyed(this);
#define VALIDATE_OBJECT s_objectValidator.isObjectValid(this);
#else
#define OBJECT_VALIDATE_DEFINITION(ObjectType)
#define OBJECT_ALLOCATED
#define OBJECT_DESTROYED
#define VALIDATE_OBJECT
#endif

#else	//CVASSERT_ENABLE == FALSE

#define CvAssertFmt(expr, fmt, ...)
#define CvAssertMsg(expr, msg)
#define CvAssert(expr)
#define CvAssert_Debug(expr)
#define CvAssertMsg_Debug(expr, msg)

#define OBJECT_VALIDATE_DEFINITION(ObjectType)
#define OBJECT_ALLOCATED
#define OBJECT_DESTROYED
#define VALIDATE_OBJECT

#endif


#endif // _CVASSERT_H
