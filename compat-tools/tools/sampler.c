/* 32-bit thread sampler: EIP + EBP-chain backtrace, resolved to module+offset. */
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>

#define MAXMOD 256
static struct { DWORD base, size; char name[64]; } mods[MAXMOD];
static int nmod;

static void loadmods(DWORD pid){
    HANDLE s=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,pid);
    MODULEENTRY32 me; me.dwSize=sizeof me; nmod=0;
    if(s==INVALID_HANDLE_VALUE){ printf("  (module snapshot failed %lu)\n",GetLastError()); return; }
    if(Module32First(s,&me)) do {
        if(nmod<MAXMOD){ mods[nmod].base=(DWORD)me.modBaseAddr; mods[nmod].size=me.modBaseSize;
            lstrcpynA(mods[nmod].name,me.szModule,63); nmod++; }
    } while(Module32Next(s,&me));
    CloseHandle(s);
}
static const char* resolve(DWORD a, DWORD*off){
    for(int i=0;i<nmod;i++) if(a>=mods[i].base && a<mods[i].base+mods[i].size){ *off=a-mods[i].base; return mods[i].name; }
    *off=a; return "?";
}
static void show(DWORD a){
    DWORD off; const char*m=resolve(a,&off);
    printf("%s+0x%lX", m, off);
}

int main(int argc,char**argv){
    DWORD pid=(DWORD)atoi(argv[1]);
    int rounds = argc>2?atoi(argv[2]):1;
    HANDLE ph=OpenProcess(PROCESS_ALL_ACCESS,FALSE,pid);
    if(!ph){ printf("OpenProcess failed %lu\n",GetLastError()); return 1; }
    loadmods(pid);
    printf("=== %d modules ===\n",nmod);
    for(int i=0;i<nmod;i++) printf("  %-24s base=%08lX size=%lu\n",mods[i].name,mods[i].base,mods[i].size);

    for(int r=0;r<rounds;r++){
        printf("\n======== SAMPLE %d ========\n",r+1);
        HANDLE ts=CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD,0);
        THREADENTRY32 te; te.dwSize=sizeof te;
        if(Thread32First(ts,&te)) do {
            if(te.th32OwnerProcessID!=pid) continue;
            HANDLE th=OpenThread(THREAD_ALL_ACCESS,FALSE,te.th32ThreadID);
            if(!th) continue;
            DWORD susp=SuspendThread(th);
            CONTEXT c; c.ContextFlags=CONTEXT_FULL;
            if(GetThreadContext(th,&c)){
                DWORD off; const char*m=resolve(c.Eip,&off);
                /* only print threads inside the app's own code, or all if unresolved */
                printf("tid=%-6lu susp=%-2ld eip=", te.th32ThreadID, (long)susp);
                show(c.Eip);
                printf("  esp=%08lX ebp=%08lX\n", c.Esp, c.Ebp);
                /* EBP chain walk */
                DWORD ebp=c.Ebp, ret, next; SIZE_T got;
                for(int f=0; f<12 && ebp; f++){
                    if(!ReadProcessMemory(ph,(void*)(ebp+4),&ret,4,&got)||got!=4) break;
                    if(!ReadProcessMemory(ph,(void*)ebp,&next,4,&got)||got!=4) break;
                    if(ret<0x10000) break;
                    printf("      #%-2d ",f); show(ret); printf("\n");
                    if(next<=ebp) break;
                    ebp=next;
                }
            }
            ResumeThread(th);
            CloseHandle(th);
        } while(Thread32Next(ts,&te));
        CloseHandle(ts);
        if(r+1<rounds) Sleep(1500);
    }
    return 0;
}
