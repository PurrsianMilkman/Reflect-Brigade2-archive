#include <windows.h>
#include <stdio.h>
#define API __stdcall
static int (API *cuInit)(unsigned);
static int (API *cuDeviceGet)(int*,int);
static int (API *cuCtxCreate)(void**,unsigned,int);
static int (API *cuModuleLoad)(void**,const char*);
static int (API *cuModuleGetFunction)(void**,void*,const char*);
static int (API *cuModuleGetGlobal)(unsigned*,unsigned*,void*,const char*);
static int (API *cuModuleGetTexRef)(void**,void*,const char*);
static int (API *cuGetErrorName)(int,const char**);
static const char* E(int r){const char*s="?";if(cuGetErrorName)cuGetErrorName(r,&s);return s;}
int main(int argc,char**argv){
    HMODULE h=LoadLibraryA("nvcuda.dll");
    if(!h){printf("no driver\n");return 1;}
#define G(n,s) *(FARPROC*)&n=GetProcAddress(h,s);
    G(cuInit,"cuInit") G(cuDeviceGet,"cuDeviceGet") G(cuCtxCreate,"cuCtxCreate_v2")
    G(cuModuleLoad,"cuModuleLoad") G(cuModuleGetFunction,"cuModuleGetFunction")
    G(cuModuleGetGlobal,"cuModuleGetGlobal_v2") G(cuModuleGetTexRef,"cuModuleGetTexRef")
    G(cuGetErrorName,"cuGetErrorName")
    int dev; void*ctx; cuInit(0); cuDeviceGet(&dev,0); cuCtxCreate(&ctx,0,dev);
    void*m=0; int r=cuModuleLoad(&m,argv[1]);
    printf("cuModuleLoad(\"%s\") -> %d (%s)\n",argv[1],r,E(r));
    if(r) return 1;
    void*f; 
    r=cuModuleGetFunction(&f,m,"TracePixelMIS");     printf("  GetFunction(TracePixelMIS)     -> %d (%s)\n",r,E(r));
    r=cuModuleGetFunction(&f,m,"TracePixelReflect"); printf("  GetFunction(TracePixelReflect) -> %d (%s)\n",r,E(r));
    unsigned dp=0,sz=0;
    r=cuModuleGetGlobal(&dp,&sz,m,"context");        printf("  GetGlobal(context)             -> %d (%s)  size=%u  devptr=0x%08X\n",r,E(r),sz,dp);
    void*t;
    r=cuModuleGetTexRef(&t,m,"t_bvh");               printf("  GetTexRef(t_bvh)               -> %d (%s)\n",r,E(r));
    return 0;
}
