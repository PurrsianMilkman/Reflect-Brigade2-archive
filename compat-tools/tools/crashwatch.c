/* Win32 debugger for 32-bit targets: accurate module map, ignores benign
   first-chance exceptions, reports the real fault with a stack scan. */
#include <windows.h>
#include <stdio.h>

#define MAXM 512
static struct { DWORD base, size; char name[64]; } M[MAXM];
static int nm;
static HANDLE g_proc;

static void addmod(HANDLE hf, LPVOID base, int quiet){
    if(nm>=MAXM) return;
    char path[MAX_PATH]="<unknown>";
    if(hf && hf!=INVALID_HANDLE_VALUE) GetFinalPathNameByHandleA(hf,path,MAX_PATH,0);
    const char*s=path; for(const char*q=path;*q;q++) if(*q=='/'||*q==92) s=q+1;
    DWORD sz=0x10000; SIZE_T got; BYTE hdr[0x400];
    if(ReadProcessMemory(g_proc,base,hdr,sizeof hdr,&got) && got>=0x200){
        DWORD e=*(DWORD*)(hdr+0x3c);
        if(e<0x400-0x100) sz=*(DWORD*)(hdr+e+24+56);   /* SizeOfImage */
    }
    M[nm].base=(DWORD)base; M[nm].size=sz; lstrcpynA(M[nm].name,s,63);
    if(!quiet) printf("  LOAD %08lX  size=%-9lu %s\n",(DWORD)base,sz,path);
    nm++;
}
static int resolve(DWORD a,char*out){
    for(int i=0;i<nm;i++) if(a>=M[i].base && a<M[i].base+M[i].size){
        wsprintfA(out,"%s+0x%lX",M[i].name,a-M[i].base); return 1; }
    wsprintfA(out,"0x%08lX",a); return 0;
}
int main(int argc,char**argv){
    STARTUPINFOA si; PROCESS_INFORMATION pi;
    ZeroMemory(&si,sizeof si); si.cb=sizeof si;
    char cmd[MAX_PATH]; lstrcpyA(cmd,argv[1]);
    if(!CreateProcessA(NULL,cmd,NULL,NULL,FALSE,DEBUG_ONLY_THIS_PROCESS,NULL,argv[2],&si,&pi)){
        printf("CreateProcess failed %lu\n",GetLastError()); return 1; }
    g_proc=pi.hProcess;
    printf("launched pid=%lu\n",pi.dwProcessId);
    DWORD start=GetTickCount(), limit=(DWORD)(argc>3?atoi(argv[3]):30000);
    DEBUG_EVENT de; int firstBP=1; long ncpp=0, nname=0;
    for(;;){
        if(!WaitForDebugEvent(&de,300)){
            if(GetTickCount()-start>limit){
                printf("\n[timeout %lums - STILL RUNNING]  (benign: %ld C++ throws, %ld thread-names)\n",limit,ncpp,nname);
                TerminateProcess(pi.hProcess,1); break; }
            continue; }
        DWORD cont=DBG_CONTINUE;
        switch(de.dwDebugEventCode){
        case CREATE_PROCESS_DEBUG_EVENT: addmod(de.u.CreateProcessInfo.hFile,de.u.CreateProcessInfo.lpBaseOfImage,0); break;
        case LOAD_DLL_DEBUG_EVENT: {
            char p[MAX_PATH]="";
            if(de.u.LoadDll.hFile) GetFinalPathNameByHandleA(de.u.LoadDll.hFile,p,MAX_PATH,0);
            int sys = (strstr(p,"\WINDOWS\\")||strstr(p,"\Windows\\"))?1:0;
            addmod(de.u.LoadDll.hFile,de.u.LoadDll.lpBaseOfDll,sys);
            if(strstr(p,"nvcuda")) printf("  >>> nvcuda module: %s @ %08lX\n",p,(DWORD)de.u.LoadDll.lpBaseOfDll);
            break; }
        case OUTPUT_DEBUG_STRING_EVENT: {
            char b[1024]={0}; SIZE_T got;
            if(de.u.DebugString.nDebugStringLength<sizeof b)
                ReadProcessMemory(pi.hProcess,de.u.DebugString.lpDebugStringData,b,de.u.DebugString.nDebugStringLength,&got);
            printf("  DBGSTR: %s",b); break; }
        case EXCEPTION_DEBUG_EVENT: {
            DWORD code=de.u.Exception.ExceptionRecord.ExceptionCode;
            int first=de.u.Exception.dwFirstChance;
            if(code==EXCEPTION_BREAKPOINT && firstBP){ firstBP=0; break; }
            if(code==0x406D1388){ nname++; break; }                 /* SetThreadName */
            if(code==0xE06D7363 && first){ ncpp++; break; }          /* C++ throw, handled */
            if(code==0x80000003){ break; }   /* UIA/debugger breakpoint - benign */
            char loc[512]; resolve((DWORD)de.u.Exception.ExceptionRecord.ExceptionAddress,loc);
            printf("\n*** EXCEPTION 0x%08lX at %s  firstChance=%d\n",code,loc,first);
            if(code==EXCEPTION_ACCESS_VIOLATION)
                printf("    %s address 0x%08lX\n",
                    de.u.Exception.ExceptionRecord.ExceptionInformation[0]?"WROTE":"READ",
                    (DWORD)de.u.Exception.ExceptionRecord.ExceptionInformation[1]);
            HANDLE th=OpenThread(THREAD_ALL_ACCESS,FALSE,de.dwThreadId);
            if(th){ CONTEXT c; c.ContextFlags=CONTEXT_FULL;
                if(GetThreadContext(th,&c)){
                    printf("    eax=%08lX ebx=%08lX ecx=%08lX edx=%08lX esi=%08lX edi=%08lX\n",c.Eax,c.Ebx,c.Ecx,c.Edx,c.Esi,c.Edi);
                    printf("    call-stack candidates:\n");
                    char l2[512]; int shown=0;
                    for(DWORD a=c.Esp;a<c.Esp+0x600 && shown<16;a+=4){
                        DWORD v; SIZE_T g;
                        if(!ReadProcessMemory(pi.hProcess,(void*)a,&v,4,&g)||g!=4) break;
                        if(resolve(v,l2)) { printf("      [%08lX] %s\n",a,l2); shown++; } } }
                CloseHandle(th); }
            if(!first){
                printf("[fatal - terminating]\n"); TerminateProcess(pi.hProcess,1); }
            cont=DBG_EXCEPTION_NOT_HANDLED; break; }
        case EXIT_PROCESS_DEBUG_EVENT:
            printf("\n[process exited, code=%lu]  (benign: %ld C++ throws, %ld thread-names)\n",
                   de.u.ExitProcess.dwExitCode,ncpp,nname);
            ContinueDebugEvent(de.dwProcessId,de.dwThreadId,cont); return 0;
        }
        ContinueDebugEvent(de.dwProcessId,de.dwThreadId,cont);
    }
    return 0;
}
