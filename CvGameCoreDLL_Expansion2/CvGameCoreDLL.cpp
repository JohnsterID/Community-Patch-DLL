/*	-------------------------------------------------------------------------------------------------------
	© 1991-2012 Take-Two Interactive Software and its subsidiaries.  Developed by Firaxis Games.  
	Sid Meier's Civilization V, Civ, Civilization, 2K Games, Firaxis Games, Take-Two Interactive Software 
	and their respective logos are all trademarks of Take-Two interactive Software, Inc.  
	All other marks and trademarks are the property of their respective owners.  
	All rights reserved. 
	------------------------------------------------------------------------------------------------------- */
#include "CvGameCoreDLLPCH.h"
#include "CvGlobals.h"
#include "ICvDLLUserInterface.h"
#include "Win32/FDebugHelper.h"
#include "CvDllContext.h"

// must be included after all other headers
#include "LintFree.h"

//------------------------------------------------------------------------------
extern "C" ICvGameContext1* DllGetGameContext()
{
	return CvDllGameContext::GetSingleton();
}
//------------------------------------------------------------------------------
BOOL APIENTRY DllMain(HANDLE hModule,
                      DWORD  ul_reason_for_call,
                      LPVOID)
{
	// CRITICAL DLL LIFECYCLE DEBUGGING: Track load/unload cycles causing infinite constructor loop
	FILE* dllLifecycleFile = NULL;
	if (fopen_s(&dllLifecycleFile, "DLL_LIFECYCLE_DEBUG.txt", "a") == 0 && dllLifecycleFile != NULL) {
		DWORD currentTime = GetTickCount();
		DWORD threadId = GetCurrentThreadId();
		DWORD processId = GetCurrentProcessId();
		
		static DWORD firstCallTime = 0;
		static int attachCount = 0;
		static int detachCount = 0;
		static DWORD lastAttachTime = 0;
		
		if (firstCallTime == 0) {
			firstCallTime = currentTime;
		}
		
		switch(ul_reason_for_call) {
		case DLL_PROCESS_ATTACH:
			attachCount++;
			fprintf(dllLifecycleFile, "\n=== DLL_PROCESS_ATTACH #%d ===\n", attachCount);
			fprintf(dllLifecycleFile, "Timestamp: %lu (Time since first: %lu ms)\n", currentTime, currentTime - firstCallTime);
			fprintf(dllLifecycleFile, "Thread ID: %lu\n", threadId);
			fprintf(dllLifecycleFile, "Process ID: %lu\n", processId);
			fprintf(dllLifecycleFile, "Module Handle: %p\n", hModule);
			if (attachCount > 1) {
				fprintf(dllLifecycleFile, "WARNING: Multiple DLL_PROCESS_ATTACH calls detected!\n");
				fprintf(dllLifecycleFile, "Time since last attach: %lu ms\n", currentTime - lastAttachTime);
			}
			lastAttachTime = currentTime;
			break;
			
		case DLL_PROCESS_DETACH:
			detachCount++;
			fprintf(dllLifecycleFile, "\n=== DLL_PROCESS_DETACH #%d ===\n", detachCount);
			fprintf(dllLifecycleFile, "Timestamp: %lu (Time since first: %lu ms)\n", currentTime, currentTime - firstCallTime);
			fprintf(dllLifecycleFile, "Thread ID: %lu\n", threadId);
			fprintf(dllLifecycleFile, "Process ID: %lu\n", processId);
			fprintf(dllLifecycleFile, "Module Handle: %p\n", hModule);
			fprintf(dllLifecycleFile, "Attach/Detach ratio: %d/%d\n", attachCount, detachCount);
			break;
			
		case DLL_THREAD_ATTACH:
			fprintf(dllLifecycleFile, "DLL_THREAD_ATTACH - Thread: %lu, Time: %lu ms\n", threadId, currentTime - firstCallTime);
			break;
			
		case DLL_THREAD_DETACH:
			fprintf(dllLifecycleFile, "DLL_THREAD_DETACH - Thread: %lu, Time: %lu ms\n", threadId, currentTime - firstCallTime);
			break;
		}
		
		fclose(dllLifecycleFile);
	}

	switch(ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		// The DLL is being loaded into the virtual address space of the current process as a result of the process starting up
		OutputDebugString("DLL_PROCESS_ATTACH\n");
		FDebugHelper::GetInstance().LoadSymbols((HMODULE)hModule);
		// set timer precision
		MMRESULT iTimeSet = timeBeginPeriod(1);		// set timeGetTime and sleep resolution to 1 ms, otherwise it's 10-16ms
		DEBUG_VARIABLE(iTimeSet);
		ASSERT(iTimeSet==TIMERR_NOERROR, "failed setting timer resolution to 1 ms");
		CvDllGameContext::InitializeSingleton();
	}
	break;
	case DLL_THREAD_ATTACH:
		OutputDebugString("DLL_THREAD_ATTACH\n");
		break;
	case DLL_THREAD_DETACH:
		OutputDebugString("DLL_THREAD_DETACH\n");
		break;
	case DLL_PROCESS_DETACH:
		OutputDebugString("DLL_PROCESS_DETACH\n");
		timeEndPeriod(1);
		CvDllGameContext::DestroySingleton();
		GC.setDLLIFace(NULL);
		break;
	}

	return TRUE;	// success
}
