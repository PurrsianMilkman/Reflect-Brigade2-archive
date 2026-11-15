/* Launch under debug; on a fatal fault dump context+stack, freeze ONLY that
   thread, and keep the process alive to see whether the engine runs. */
#include <windows.h>
#include <stdio.h>
#define MAXM 512
static struct { DWORD base,size; char name[64]; } M[MAXM]; static int nm;
static HANDLE g_proc;
static void addmod(HANDLE hf,LPVOID base,int quiet){
    if(nm>=MAXM) return; char path[MAX_PATH]="<unknown>";
    if(hf&&hf!=INVALID_HANDLE_VALUE) GetFinalPathNameByHandleA(hf,path,MAX_PATH,0);
    const char*s=path; for(const char*q=path;*q;q++) if(*q=='/'||*q==92) s=q+1;
    DWORD sz=0x10000; SIZE_T got; BYTE hdr[0x400];
    if(ReadProcessMemory(g_proc,base,hdr,sizeof hdr,&got)&&got>=0x200){
        DWORD e=*(DWORD*)(hdr+0x3c); if(e<0x300) sz=*(DWORD*)(hdr+e+24+56); }
    M[nm].base=(DWORD)base; M[nm].size=sz; lstrcpynA(M[nm].name,s,63);
    if(!quiet) printf("  LOAD %08lX %s\n",(DWORD)base,s);
    nm++;
}
static int resolve(DWORD a,char*o){
    for(int i=0;i<nm;i++) if(a>=M[i].base&&a<M[i].base+M[i].size){wsprintfA(o,"%s+0x%lX",M[i].name,a-M[i].base);return 1;}
    wsprintfA(o,"0x%08lX",a); return 0; }


static void dumpmem(HANDLE proc, DWORD addr){
    MEMORY_BASIC_INFORMATION mbi;
    const char* st;
    printf("    --- memory map around destination ---\n");
    DWORD probes[3]; probes[0]=addr-0x1000; probes[1]=addr; probes[2]=addr+0x1000;
    for(int i=0;i<3;i++){
        if(VirtualQueryEx(proc,(void*)probes[i],&mbi,sizeof mbi)){
            st = mbi.State==MEM_COMMIT?"COMMIT":(mbi.State==MEM_FREE?"FREE":"RESERVE");
            printf("      probe %08lX -> base=%08lX allocBase=%08lX size=%-9lu %s prot=0x%lX\n",
                probes[i],(DWORD)mbi.BaseAddress,(DWORD)mbi.AllocationBase,
                (DWORD)mbi.RegionSize, st, mbi.Protect);
        } else printf("      probe %08lX -> VirtualQueryEx failed\n",probes[i]);
    }
}

static void dumpfault(HANDLE proc, DEBUG_EVENT*de){
    HANDLE t=OpenThread(THREAD_ALL_ACCESS,FALSE,de->dwThreadId);
    if(!t) return;
    CONTEXT c; c.ContextFlags=CONTEXT_FULL;
    if(GetThreadContext(t,&c)){
        printf("    %s address 0x%08lX\n",
            de->u.Exception.ExceptionRecord.ExceptionInformation[0]?"WROTE":"READ",
            (DWORD)de->u.Exception.ExceptionRecord.ExceptionInformation[1]);
        printf("    eax=%08lX ebx=%08lX ecx=%08lX edx=%08lX esi=%08lX edi=%08lX esp=%08lX\n",
            c.Eax,c.Ebx,c.Ecx,c.Edx,c.Esi,c.Edi,c.Esp);
        printf("    stack scan (callers):\n");
        char l2[512]; int sh=0;
        for(DWORD a=c.Esp;a<c.Esp+0x400 && sh<18;a+=4){
            DWORD v; SIZE_T g;
            if(!ReadProcessMemory(proc,(void*)a,&v,4,&g)||g!=4) break;
            if(resolve(v,l2)){ printf("      [%08lX] %s\n",a,l2); sh++; }
        }
        dumpmem(proc,(DWORD)de->u.Exception.ExceptionRecord.ExceptionInformation[1]);
    }
    CloseHandle(t);
}

int main(int argc,char**argv){
    STARTUPINFOA si; PROCESS_INFORMATION pi; ZeroMemory(&si,sizeof si); si.cb=sizeof si;
    char cmd[MAX_PATH]; lstrcpyA(cmd,argv[1]);
    if(!CreateProcessA(NULL,cmd,NULL,NULL,FALSE,DEBUG_ONLY_THIS_PROCESS,NULL,argv[2],&si,&pi)){
        printf("CreateProcess failed %lu\n",GetLastError()); return 1; }
    g_proc=pi.hProcess; printf("pid=%lu\n",pi.dwProcessId);
    DWORD start=GetTickCount(), limit=(DWORD)(argc>3?atoi(argv[3]):40000), lastTick=99;
    DEBUG_EVENT de; int firstBP=1; long frozen=0,ncpp=0;
    for(;;){
        DWORD el=GetTickCount()-start;
        if(el/5000!=lastTick){ lastTick=el/5000; printf("  [t=%lus alive, %ld frozen]\n",el/1000,frozen); fflush(stdout); }
        if(!WaitForDebugEvent(&de,200)){
            if(el>limit){ printf("\n[timeout: PROCESS STILL ALIVE after %lums]\n",limit); TerminateProcess(pi.hProcess,0); break; }
            continue; }
        DWORD cont=DBG_CONTINUE;
        switch(de.dwDebugEventCode){
        case CREATE_PROCESS_DEBUG_EVENT: addmod(de.u.CreateProcessInfo.hFile,de.u.CreateProcessInfo.lpBaseOfImage,0); break;
        case LOAD_DLL_DEBUG_EVENT: { char p[MAX_PATH]="";
            if(de.u.LoadDll.hFile) GetFinalPathNameByHandleA(de.u.LoadDll.hFile,p,MAX_PATH,0);
            addmod(de.u.LoadDll.hFile,de.u.LoadDll.lpBaseOfDll, strstr(p,"indows")?1:0); break; }
        case OUTPUT_DEBUG_STRING_EVENT: { char b[1024]={0}; SIZE_T g;
            if(de.u.DebugString.nDebugStringLength<sizeof b)
                ReadProcessMemory(pi.hProcess,de.u.DebugString.lpDebugStringData,b,de.u.DebugString.nDebugStringLength,&g);
            printf("  DBGSTR: %s\n",b); break; }
        case EXCEPTION_DEBUG_EVENT: {
            DWORD code=de.u.Exception.ExceptionRecord.ExceptionCode; int first=de.u.Exception.dwFirstChance;
            if(code==EXCEPTION_BREAKPOINT&&firstBP){ firstBP=0; break; }
            if(code==0x406D1388||code==0x80000003) break;
            if(code==0xE06D7363&&first){ ncpp++; break; }
            if(first){ cont=DBG_EXCEPTION_NOT_HANDLED; break; }
            char loc[512]; resolve((DWORD)de.u.Exception.ExceptionRecord.ExceptionAddress,loc);
            printf("\n*** FATAL 0x%08lX at %s on tid=%lu -> freezing thread\n",code,loc,de.dwThreadId);
            dumpfault(pi.hProcess,&de);
            HANDLE th=OpenThread(THREAD_ALL_ACCESS,FALSE,de.dwThreadId);
            if(th){ SuspendThread(th); CloseHandle(th); frozen++; }
            cont=DBG_CONTINUE; break; }
        case EXIT_PROCESS_DEBUG_EVENT:
            printf("\n[process EXITED code=%lu at t=%lums] (%ld C++ throws, %ld frozen)\n",
                de.u.ExitProcess.dwExitCode,el,ncpp,frozen);
            ContinueDebugEvent(de.dwProcessId,de.dwThreadId,cont); return 0; }
        ContinueDebugEvent(de.dwProcessId,de.dwThreadId,cont);
    }
    return 0;
}
