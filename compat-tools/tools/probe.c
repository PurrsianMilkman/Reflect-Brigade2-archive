/* 32-bit CUDA driver-API probe: replicates exactly what TracerCUDA.dll does. */
#include <windows.h>
#include <stdio.h>

typedef int CUresult;
typedef int CUdevice;
typedef void* CUcontext;
typedef void* CUmodule;
typedef void* CUfunction;
typedef unsigned int CUdeviceptr;   /* 32-bit host => 32-bit device ptr */

#define API __stdcall
static CUresult (API *cuInit)(unsigned);
static CUresult (API *cuDriverGetVersion)(int*);
static CUresult (API *cuDeviceGetCount)(int*);
static CUresult (API *cuDeviceGet)(CUdevice*,int);
static CUresult (API *cuDeviceGetName)(char*,int,CUdevice);
static CUresult (API *cuDeviceGetAttribute)(int*,int,CUdevice);
static CUresult (API *cuCtxCreate)(CUcontext*,unsigned,CUdevice);
static CUresult (API *cuModuleLoad)(CUmodule*,const char*);
static CUresult (API *cuModuleGetFunction)(CUfunction*,CUmodule,const char*);
static CUresult (API *cuMemAlloc)(CUdeviceptr*,size_t);
static CUresult (API *cuMemcpyDtoH)(void*,CUdeviceptr,size_t);
static CUresult (API *cuFuncSetBlockShape)(CUfunction,int,int,int);
static CUresult (API *cuParamSetv)(CUfunction,int,void*,unsigned);
static CUresult (API *cuParamSetSize)(CUfunction,unsigned);
static CUresult (API *cuLaunchGrid)(CUfunction,int,int);
static CUresult (API *cuCtxSynchronize)(void);
static CUresult (API *cuGetErrorName)(CUresult,const char**);

static const char* err(CUresult r){
    const char*s="?"; if(cuGetErrorName && cuGetErrorName(r,&s)==0) return s; return "?";
}
#define CK(call) do{ CUresult _r=(call); \
    printf("  %-42s -> %d (%s)\n", #call, _r, err(_r)); }while(0)

int main(void){
    HMODULE h = LoadLibraryA("nvcuda.dll");
    printf("LoadLibrary(nvcuda.dll) [32-bit process] = %p\n", (void*)h);
    if(!h){ printf("FATAL: no 32-bit CUDA driver\n"); return 1; }
    char path[MAX_PATH]; GetModuleFileNameA(h,path,MAX_PATH);
    printf("  resolved to: %s\n\n", path);

#define G(n) *(FARPROC*)&n = GetProcAddress(h,#n); if(!n) printf("  MISSING EXPORT: %s\n", #n);
    G(cuInit) G(cuDriverGetVersion) G(cuDeviceGetCount) G(cuDeviceGet) G(cuDeviceGetName)
    G(cuDeviceGetAttribute) G(cuModuleLoad) G(cuModuleGetFunction)
    G(cuMemAlloc) G(cuMemcpyDtoH) G(cuFuncSetBlockShape) G(cuParamSetv)
    G(cuParamSetSize) G(cuLaunchGrid) G(cuCtxSynchronize) G(cuGetErrorName)
    *(FARPROC*)&cuCtxCreate = GetProcAddress(h,"cuCtxCreate_v2");
    if(!cuCtxCreate) printf("  MISSING EXPORT: cuCtxCreate_v2\n");
    *(FARPROC*)&cuMemAlloc  = GetProcAddress(h,"cuMemAlloc_v2");
    *(FARPROC*)&cuMemcpyDtoH= GetProcAddress(h,"cuMemcpyDtoH_v2");

    int ver=0,cnt=0,maj=0,min=0; CUdevice dev; char name[256]={0};
    CUresult r = cuInit(0);
    printf("cuInit                 -> %d (%s)\n", r, err(r));
    if(r) return 1;
    cuDriverGetVersion(&ver); printf("driver CUDA version    -> %d.%d\n", ver/1000,(ver%1000)/10);
    cuDeviceGetCount(&cnt);   printf("CUDA device count      -> %d\n", cnt);
    if(cnt<1){ printf("FATAL: 32-bit driver sees no devices\n"); return 1; }
    cuDeviceGet(&dev,0);
    cuDeviceGetName(name,sizeof name,dev);
    cuDeviceGetAttribute(&maj,75,dev); cuDeviceGetAttribute(&min,76,dev);
    printf("device 0               -> %s (sm_%d%d)\n\n", name, maj, min);

    CUcontext ctx;
    r = cuCtxCreate(&ctx,0,dev);
    printf("cuCtxCreate            -> %d (%s)\n\n", r, err(r));
    if(r) return 1;

    /* --- TEST A: the shipped sm_20 cubin --- */
    CUmodule m=0;
    r = cuModuleLoad(&m, "TracerCUDA_sm20.cubin");
    printf("TEST A  load sm_20 cubin (as shipped)   -> %d (%s)\n", r, err(r));

    /* --- TEST B: hand-written 32-bit PTX, compute_50, JIT to sm_86 --- */
    CUmodule m2=0;
    r = cuModuleLoad(&m2, "test32.ptx");
    printf("TEST B  load 32-bit PTX (.address_size 32, .target sm_50)\n");
    printf("        cuModuleLoad                    -> %d (%s)\n", r, err(r));
    if(r==0){
        CUfunction f; CUdeviceptr d; unsigned host[8]={0};
        r=cuModuleGetFunction(&f,m2,"writeTid");
        printf("        cuModuleGetFunction             -> %d (%s)\n", r, err(r));
        r=cuMemAlloc(&d,sizeof host);
        printf("        cuMemAlloc                      -> %d (%s) devptr=0x%08X\n", r, err(r), d);
        cuFuncSetBlockShape(f,8,1,1);
        cuParamSetv(f,0,&d,4);
        cuParamSetSize(f,4);
        r=cuLaunchGrid(f,1,1);
        printf("        cuLaunchGrid (legacy API)       -> %d (%s)\n", r, err(r));
        r=cuCtxSynchronize();
        printf("        cuCtxSynchronize                -> %d (%s)\n", r, err(r));
        r=cuMemcpyDtoH(host,d,sizeof host);
        printf("        cuMemcpyDtoH                    -> %d (%s)\n", r, err(r));
        printf("        kernel output (expect 1000..1007): ");
        for(int i=0;i<8;i++) printf("%u ",host[i]);
        printf("\n");
    }
    return 0;
}
