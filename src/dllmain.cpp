#include "expander.h"
#include "lib/datetime.h"
#include "lib/terminal.h"
#include "lib/lock/Lock.h"
#include "struct/rsf/ExecutionContext.h"


extern uint               g_terminalBuild;                     // terminal build number
extern CRITICAL_SECTION   g_terminalMutex;                     // mutex for application-wide locking
extern Locks              g_locks;                             // a map holding pointers to fine-granular locks

extern MqlProgramList     g_mqlPrograms;                       // all MQL programs: vector<ContextChain> with index = program id
extern std::vector<DWORD> g_threads;                           // all known threads executing MQL programs
extern std::vector<uint>  g_threadsPrograms;                   // the last MQL program executed by a thread


//
// Win32 Mutex            = system-wide mutex
// Win32 CRITICAL_SECTION = process-wide mutex
// https://docs.microsoft.com/en-us/windows/desktop/Sync/critical-section-objects
//
//
// Speed benchmark and especially: ...if the spin count expires, the mutex for the critical section will be allocated.
// https://stackoverflow.com/questions/800383/what-is-the-difference-between-mutex-and-critical-section
//
//
// Exception safety:
// http://progblogs.blogspot.com/2012/02/critical-section-vs-mutex.html
//


// forward declarations
void WINAPI onProcessAttach();
void WINAPI onProcessDetach(BOOL isTerminating);


/**
 * DLL entry point.
 *
 * @param  HMODULE hModule
 * @param  DWORD   reason
 * @param  LPVOID  reserved
 *
 * @return BOOL
 */
BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID reserved) {
   switch (reason) {
      case DLL_PROCESS_ATTACH: onProcessAttach();               break;
      case DLL_THREAD_ATTACH :                                  break;
      case DLL_THREAD_DETACH :                                  break;
      case DLL_PROCESS_DETACH: onProcessDetach((BOOL)reserved); break;
   }
   return(TRUE);
}


/**
 * Handler for DLL_PROCESS_ATTACH events.
 */
void WINAPI onProcessAttach() {
   g_terminalBuild = GetTerminalBuild();

   g_mqlPrograms    .reserve(128);
   g_threads        .reserve(512);
   g_threadsPrograms.reserve(512);

   InitializeCriticalSection(&g_terminalMutex);
}


/**
 * Handler for DLL_PROCESS_DETACH events.
 *
 * @param  BOOL isTerminating - whether the DLL is detached because the process is terminating
 */
void WINAPI onProcessDetach(BOOL isTerminating) {
   if (isTerminating)
      return;

   DeleteCriticalSection(&g_terminalMutex);
   RemoveTickTimers();

   for (Locks::iterator it=g_locks.begin(), end=g_locks.end(); it != end; ++it) {
      delete it->second;
   }
   g_locks.clear();
}
