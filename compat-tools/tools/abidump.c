/* nvcud0.dll - logging proxy for TracerCUDA.dll (string-patched to load us).
   Recovers the kernel ABI and dumps the scene buffers for offline analysis. */
#include <windows.h>
#include <stdio.h>

static HMODULE g_real; static FILE* L; static CRITICAL_SECTION CS;

/* ---------- allocation tracking ---------- */
#define MAXA 1024
static struct { unsigned ptr, size; char kind[24]; } A[MAXA];
static int na;
static void note(unsigned p, unsigned s, const char* k){
    if(na<MAXA){ A[na].ptr=p; A[na].size=s; lstrcpynA(A[na].kind,k,23); na++; }
}
static int findalloc(unsigned v){
    for(int i=0;i<na;i++){
        if(A[i].size && v>=A[i].ptr && v<A[i].ptr+A[i].size) return i;
        if(v==A[i].ptr) return i;
    }
    return -1;
}
static void describe(unsigned v, char* out){
    int i=findalloc(v); out[0]=0;
    if(i>=0) wsprintfA(out,"  <- %s#%d base=%08X size=%u off=+%u",A[i].kind,i,A[i].ptr,A[i].size,v-A[i].ptr);
}
static void LG(const char*f,...){ if(!L)return; EnterCriticalSection(&CS);
    va_list a; va_start(a,f); vfprintf(L,f,a); va_end(a); fputc(10,L); fflush(L); LeaveCriticalSection(&CS); }

static unsigned g_ctxPtr=0, g_ctxSize=0;
static void* g_host=0; static unsigned g_hostSize=0; static long g_syncs=0;
static int g_ctxDumps=0;
static unsigned char g_lastCtx[1024];
static int g_haveLast=0;

static void dumpContextFull(const unsigned char* p, unsigned n){
    LG(" ");
    LG("================ context FULL dump #%d  (%u bytes) ================",++g_ctxDumps,n);
    LG("  off    hex        int32         float           note");
    for(unsigned o=0;o<n;o+=4){
        unsigned v; int iv; float fv;
        memcpy(&v,p+o,4); memcpy(&iv,p+o,4); memcpy(&fv,p+o,4);
        if(v==0xBAADF00D) continue;
        char d[200]; describe(v,d);
        char fs[64];
        if(fv!=fv) lstrcpyA(fs,"nan");
        else if(fv>-1e12f && fv<1e12f) sprintf(fs,"%.6g",fv); else lstrcpyA(fs,"~big");
        LG("  +%04X  0x%08X %-13d %-15s%s",o,v,iv,fs,d);
    }
    LG("================ end context ================");
    LG(" ");
}

/* dump a payload to its own file, once per (allocation,offset) */
#define MAXDUMPED 64
static struct { unsigned dst, size; } dumped[MAXDUMPED]; static int ndumped;
static int alreadyDumped(unsigned dst, unsigned size){
    for(int i=0;i<ndumped;i++) if(dumped[i].dst==dst && dumped[i].size==size) return 1;
    if(ndumped<MAXDUMPED){ dumped[ndumped].dst=dst; dumped[ndumped].size=size; ndumped++; }
    return 0;
}
static void dumpPayload(const char* kind, unsigned dst, unsigned base, const void* src, unsigned n){
    char fn[256];
    wsprintfA(fn,"dump_%s_off%u_sz%u.bin",kind,dst-base,n);
    FILE* f=fopen(fn,"wb");
    if(f){ fwrite(src,1,n,f); fclose(f); LG("    >>> wrote %s (%u bytes)",fn,n); }
}

/* ---------- simple forwarders with logging ---------- */
#define R(name,ret,params,args,LOGSTMT) \
  static ret (__stdcall *p_##name) params; \
  ret __stdcall name params { ret r = p_##name args; LOGSTMT; return r; }

R(cuInit,int,(unsigned f),(f), LG("cuInit(%u) -> %d",f,r))
R(cuDriverGetVersion,int,(int*v),(v), LG("cuDriverGetVersion -> %d ver=%d",r,v?*v:-1))
R(cuDeviceGetCount,int,(int*c),(c), LG("cuDeviceGetCount -> %d count=%d",r,c?*c:-1))
R(cuDeviceGet,int,(int*d,int o),(d,o), LG("cuDeviceGet(ord=%d) -> %d",o,r))
R(cuDeviceGetName,int,(char*n,int l,int d),(n,l,d), LG("cuDeviceGetName -> %d name=%s",r,(r==0&&n)?n:"?"))
R(cuCtxCreate_v2,int,(void**c,unsigned f,int d),(c,f,d), LG("cuCtxCreate(flags=0x%X dev=%d) -> %d",f,d,r))

R(cuModuleGetFunction,int,(void**fn,void*m,const char*n),(fn,m,n),
    LG("cuModuleGetFunction(%s) -> %d handle=%p",n,r,fn?*fn:0))
R(cuModuleGetTexRef,int,(void**t,void*m,const char*n),(t,m,n),
    LG("cuModuleGetTexRef(%s) -> %d handle=%p",n,r,t?*t:0))
R(cuFuncSetBlockShape,int,(void*f,int x,int y,int z),(f,x,y,z),
    LG("  SetBlockShape(func=%p, %d,%d,%d)",f,x,y,z))
R(cuFuncSetSharedSize,int,(void*f,unsigned b),(f,b), LG("  SetSharedSize(%u bytes)",b))
R(cuFuncSetCacheConfig,int,(void*f,int c),(f,c), LG("  SetCacheConfig(%d)",c))
R(cuMemFree_v2,int,(unsigned d),(d), (void)r)
R(cuMemFreeHost,int,(void*p),(p), (void)r)
R(cuTexRefSetFormat,int,(void*t,int fmt,int nc),(t,fmt,nc),
    LG("  TexRefSetFormat(tex=%p, fmt=0x%X, channels=%d)",t,fmt,nc))

/* ---------- hand-written wrappers ---------- */
static int (__stdcall *p_cuModuleLoad)(void**,const char*);
static int (__stdcall *p_cuModuleGetGlobal_v2)(unsigned*,unsigned*,void*,const char*);
static int (__stdcall *p_cuMemAlloc_v2)(unsigned*,unsigned);
static int (__stdcall *p_cuMemHostAlloc)(void**,unsigned,unsigned);
static int (__stdcall *p_cuMemHostGetDevicePointer_v2)(unsigned*,void*,unsigned);
static int (__stdcall *p_cuMemcpyHtoD_v2)(unsigned,const void*,unsigned);
static int (__stdcall *p_cuTexRefSetAddress_v2)(unsigned*,void*,unsigned,unsigned);
static int (__stdcall *p_cuLaunchGrid)(void*,int,int);

static int (__stdcall *p_cuCtxSynchronize)(void);
int __stdcall cuCtxSynchronize(void){
    int r=p_cuCtxSynchronize();
    if(++g_syncs==12 && g_host && g_hostSize){
        FILE*f=fopen("dump_output_hostmapped.bin","wb");
        if(f){ fwrite(g_host,1,g_hostSize,f); fclose(f);
               LG("    >>> wrote dump_output_hostmapped.bin (%u bytes) after %ld syncs",g_hostSize,g_syncs); }
    }
    return r;
}
int __stdcall cuModuleLoad(void**m,const char*fn){
    int r=p_cuModuleLoad(m,fn);
    LG("cuModuleLoad(%s) -> %d %s",fn,r,r?"FAILED":"OK");
    return r;
}
int __stdcall cuModuleGetGlobal_v2(unsigned*dp,unsigned*sz,void*m,const char*n){
    int r=p_cuModuleGetGlobal_v2(dp,sz,m,n);
    LG("cuModuleGetGlobal(%s) -> %d devptr=0x%08X size=%u",n,r,dp?*dp:0,sz?*sz:0);
    if(r==0 && n && lstrcmpA(n,"context")==0){ g_ctxPtr=*dp; g_ctxSize=*sz; note(*dp,*sz,"context"); }
    return r;
}
int __stdcall cuMemAlloc_v2(unsigned*d,unsigned n){
    int r=p_cuMemAlloc_v2(d,n);
    if(r==0){ note(*d,n,"devbuf"); LG("cuMemAlloc(%u bytes) -> devptr=0x%08X   [devbuf#%d]",n,*d,na-1); }
    else LG("cuMemAlloc(%u) FAILED %d",n,r);
    return r;
}
int __stdcall cuMemHostAlloc(void**p,unsigned n,unsigned f){
    int r=p_cuMemHostAlloc(p,n,f);
    LG("cuMemHostAlloc(%u bytes, flags=0x%X) -> host=%p",n,f,p?*p:0);
    if(r==0 && p && n>=2000000){ g_host=*p; g_hostSize=n; }
    return r;
}
int __stdcall cuMemHostGetDevicePointer_v2(unsigned*d,void*h,unsigned f){
    int r=p_cuMemHostGetDevicePointer_v2(d,h,f);
    if(r==0){ note(*d,0,"hostmapped"); LG("cuMemHostGetDevicePointer(host=%p) -> devptr=0x%08X",h,*d); }
    return r;
}
int __stdcall cuMemcpyHtoD_v2(unsigned dst,const void*src,unsigned n){
    int r=p_cuMemcpyHtoD_v2(dst,src,n);
    if(g_ctxPtr && dst>=g_ctxPtr && dst<g_ctxPtr+g_ctxSize){
        if(g_ctxDumps<2){ dumpContextFull((const unsigned char*)src,n); memcpy(g_lastCtx,src,n<sizeof g_lastCtx?n:sizeof g_lastCtx); g_haveLast=1; }
        else if(g_haveLast && n<=sizeof g_lastCtx){
            char buf[900]; buf[0]=0; int cnt=0;
            for(unsigned o=0;o<n;o+=4){
                if(memcmp(g_lastCtx+o,(const unsigned char*)src+o,4)!=0){
                    if(cnt<20){ char t[32]; wsprintfA(t,"+%03X ",o); lstrcatA(buf,t); }
                    cnt++;
                }
            }
            if(cnt) LG("context delta (%d fields): %s",cnt,buf);
            memcpy(g_lastCtx,src,n);
        }
        return r;
    }
    int ai=findalloc(dst);
    unsigned asz = ai>=0 ? A[ai].size : 0;
    char d[200]; describe(dst,d);
    LG("cuMemcpyHtoD(dst=0x%08X, %u bytes)%s",dst,n,d);
    /* dump the small, structurally interesting buffers (skip 32MB atlases) */
    if(ai>=0 && !alreadyDumped(dst,n)){
        if(asz==14680064)      dumpPayload("bvh", dst, A[ai].ptr, src, n);
        else if(asz==6800)     dumpPayload("mat", dst, A[ai].ptr, src, n);
        else if(asz==32768)    dumpPayload("light", dst, A[ai].ptr, src, n);
    }
    return r;
}
int __stdcall cuTexRefSetAddress_v2(unsigned*off,void*tex,unsigned dptr,unsigned bytes){
    int r=p_cuTexRefSetAddress_v2(off,tex,dptr,bytes);
    char d[200]; describe(dptr,d);
    LG("  TexRefSetAddress(tex=%p, devptr=0x%08X, %u bytes) byteoffset=%u%s",tex,dptr,bytes,off?*off:0,d);
    return r;
}
int __stdcall cuLaunchGrid(void*f,int w,int h){
    int r=p_cuLaunchGrid(f,w,h);
    static int nl=0;
    if(nl<5) LG("LAUNCH func=%p grid=(%d,%d) -> %d",f,w,h,r);
    nl++;
    return r;
}

#define B(n) *(FARPROC*)&p_##n=GetProcAddress(g_real,#n); if(!p_##n) LG("!! missing %s",#n);
BOOL WINAPI DllMain(HINSTANCE h,DWORD reason,LPVOID res){
    if(reason==DLL_PROCESS_ATTACH){
        InitializeCriticalSection(&CS);
        L=fopen("abi_dump.log","w");
        g_real=LoadLibraryA("nvcuda.dll");
        LG("== nvcud0.dll proxy attached; real nvcuda.dll = %p ==",(void*)g_real);
        if(!g_real) return TRUE;
        B(cuInit) B(cuDriverGetVersion) B(cuDeviceGetCount) B(cuDeviceGet) B(cuDeviceGetName)
        B(cuCtxCreate_v2) B(cuCtxSynchronize) B(cuModuleLoad) B(cuModuleGetFunction)
        B(cuModuleGetGlobal_v2) B(cuModuleGetTexRef) B(cuFuncSetBlockShape) B(cuFuncSetSharedSize)
        B(cuFuncSetCacheConfig) B(cuLaunchGrid) B(cuMemAlloc_v2) B(cuMemFree_v2) B(cuMemFreeHost)
        B(cuMemHostAlloc) B(cuMemHostGetDevicePointer_v2) B(cuMemcpyHtoD_v2)
        B(cuTexRefSetAddress_v2) B(cuTexRefSetFormat)
        LG("== bound ==");
    }
    return TRUE;
}
