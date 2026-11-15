/* Full-visibility 32-bit nvcuda.dll shim: logs every driver call TracerCUDA.dll makes. */
#include <windows.h>
#include <stdio.h>

typedef unsigned int CUdeviceptr32;
static HMODULE g_real; static FILE* g_log; static CRITICAL_SECTION g_cs; static long g_n;

static void LG(const char*fmt,...){
    if(!g_log) return;
    EnterCriticalSection(&g_cs);
    if(++g_n < 4000){ va_list a; va_start(a,fmt); vfprintf(g_log,fmt,a); va_end(a); fputc(10,g_log); fflush(g_log); }
    LeaveCriticalSection(&g_cs);
}

#define R(name,ret,params,args,fmt,...) \
  static ret (__stdcall *p_##name) params; \
  ret __stdcall name params { ret r = p_##name args; LG(fmt, __VA_ARGS__); return r; }

R(cuInit,int,(unsigned f),(f),"cuInit(%u) -> %d",f,r)
R(cuDriverGetVersion,int,(int*v),(v),"cuDriverGetVersion -> %d (ver=%d)",r,v?*v:-1)
R(cuDeviceGetCount,int,(int*c),(c),"cuDeviceGetCount -> %d (count=%d)",r,c?*c:-1)
R(cuDeviceGet,int,(int*d,int o),(d,o),"cuDeviceGet(ord=%d) -> %d",o,r)
R(cuDeviceGetName,int,(char*n,int l,int d),(n,l,d),"cuDeviceGetName(dev=%d) -> %d name=%s",d,r,(r==0&&n)?n:"?")
R(cuCtxCreate_v2,int,(void**c,unsigned f,int d),(c,f,d),"cuCtxCreate(flags=0x%x dev=%d) -> %d",f,d,r)
R(cuCtxSynchronize,int,(void),(),"cuCtxSynchronize -> %d",r)
R(cuModuleLoad,int,(void**m,const char*fn),(m,fn),"cuModuleLoad(\"%s\") -> %d",fn,r)
R(cuModuleGetFunction,int,(void**f,void*m,const char*n),(f,m,n),"  GetFunction(\"%s\") -> %d",n,r)
R(cuModuleGetGlobal_v2,int,(CUdeviceptr32*d,unsigned*s,void*m,const char*n),(d,s,m,n),"  GetGlobal(\"%s\") -> %d size=%u",n,r,(r==0&&s)?*s:0)
R(cuModuleGetTexRef,int,(void**t,void*m,const char*n),(t,m,n),"  GetTexRef(\"%s\") -> %d",n,r)
R(cuFuncSetBlockShape,int,(void*f,int x,int y,int z),(f,x,y,z),"  SetBlockShape(%d,%d,%d) -> %d",x,y,z,r)
R(cuFuncSetSharedSize,int,(void*f,unsigned b),(f,b),"  SetSharedSize(%u) -> %d",b,r)
R(cuFuncSetCacheConfig,int,(void*f,int c),(f,c),"  SetCacheConfig(%d) -> %d",c,r)
R(cuLaunchGrid,int,(void*f,int w,int h),(f,w,h),"LAUNCH grid(%d,%d) -> %d",w,h,r)
R(cuMemAlloc_v2,int,(CUdeviceptr32*d,unsigned n),(d,n),"  MemAlloc(%u B) -> %d",n,r)
R(cuMemFree_v2,int,(CUdeviceptr32 d),(d),"  MemFree -> %d",r)
R(cuMemFreeHost,int,(void*p),(p),"  MemFreeHost -> %d",r)
R(cuMemHostAlloc,int,(void**p,unsigned n,unsigned f),(p,n,f),"  MemHostAlloc(%u B,flags=0x%x) -> %d",n,f,r)
R(cuMemHostGetDevicePointer_v2,int,(CUdeviceptr32*d,void*h,unsigned f),(d,h,f),"  HostGetDevPtr -> %d",r)
R(cuMemcpyHtoD_v2,int,(CUdeviceptr32 d,const void*s,unsigned n),(d,s,n),"  HtoD(%u B) -> %d",n,r)
R(cuTexRefSetAddress_v2,int,(unsigned*o,void*t,CUdeviceptr32 d,unsigned n),(o,t,d,n),"  TexRefSetAddress(%u B) -> %d",n,r)
R(cuTexRefSetFormat,int,(void*t,int fmt,int nc),(t,fmt,nc),"  TexRefSetFormat(fmt=0x%x,nc=%d) -> %d",fmt,nc,r)

#define BIND(n) *(FARPROC*)&p_##n = GetProcAddress(g_real,#n); if(!p_##n) LG("!! missing %s",#n);

BOOL WINAPI DllMain(HINSTANCE h,DWORD reason,LPVOID res){
    if(reason==DLL_PROCESS_ATTACH){
        char sysdir[MAX_PATH], real[MAX_PATH]; char sep[2];
        InitializeCriticalSection(&g_cs);
        g_log=fopen("nvcuda_shim.log","w");
        GetSystemDirectoryA(sysdir,MAX_PATH);
        sep[0]=(char)92; sep[1]=0;
        lstrcpyA(real,sysdir); lstrcatA(real,sep); lstrcatA(real,"nvcuda.dll");
        g_real=LoadLibraryA(real);
        LG("== shim attached, real=%s (%p) ==",real,(void*)g_real);
        if(!g_real) return TRUE;
        BIND(cuInit) BIND(cuDriverGetVersion) BIND(cuDeviceGetCount) BIND(cuDeviceGet)
        BIND(cuDeviceGetName) BIND(cuCtxCreate_v2) BIND(cuCtxSynchronize) BIND(cuModuleLoad)
        BIND(cuModuleGetFunction) BIND(cuModuleGetGlobal_v2) BIND(cuModuleGetTexRef)
        BIND(cuFuncSetBlockShape) BIND(cuFuncSetSharedSize) BIND(cuFuncSetCacheConfig)
        BIND(cuLaunchGrid) BIND(cuMemAlloc_v2) BIND(cuMemFree_v2) BIND(cuMemFreeHost)
        BIND(cuMemHostAlloc) BIND(cuMemHostGetDevicePointer_v2) BIND(cuMemcpyHtoD_v2)
        BIND(cuTexRefSetAddress_v2) BIND(cuTexRefSetFormat)
        LG("== all entry points bound ==");
    }
    return TRUE;
}
