/* Run the translated TracePixelMIS on the captured Reflect scene, offline.
   usage: runscene <ptx> <kernel> <gridx> <out.raw> [timeout_ms] */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define API __stdcall
typedef unsigned int u32;
static HMODULE h;
#define FN(ret,name,args,sym) static ret (API *name)args;
FN(int,cuInit,(unsigned),0) FN(int,cuDeviceGet,(int*,int),0) FN(int,cuCtxCreate,(void**,unsigned,int),0)
FN(int,cuModuleLoad,(void**,const char*),0) FN(int,cuModuleGetFunction,(void**,void*,const char*),0)
FN(int,cuModuleGetGlobal,(u32*,u32*,void*,const char*),0) FN(int,cuModuleGetTexRef,(void**,void*,const char*),0)
FN(int,cuMemAlloc,(u32*,u32),0) FN(int,cuMemcpyHtoD,(u32,const void*,u32),0) FN(int,cuMemcpyDtoH,(void*,u32,u32),0)
FN(int,cuMemsetD32,(u32,unsigned,u32),0)
FN(int,cuFuncSetBlockShape,(void*,int,int,int),0) FN(int,cuFuncSetSharedSize,(void*,unsigned),0)
FN(int,cuFuncSetCacheConfig,(void*,int),0)
FN(int,cuLaunchGrid,(void*,int,int),0) FN(int,cuCtxSynchronize,(void),0) FN(int,cuGetErrorName,(int,const char**),0)
FN(int,cuTexRefSetFormat,(void*,int,int),0) FN(int,cuTexRefSetAddress,(u32*,void*,u32,u32),0)
FN(int,cuEventCreate,(void**,unsigned),0) FN(int,cuEventRecord,(void*,void*),0) FN(int,cuEventQuery,(void*),0)
static const char* E(int r){const char*s="?";if(cuGetErrorName)cuGetErrorName(r,&s);return s;}
#define CK(x) do{int _r=(x); if(_r){printf("  FAIL %-34s -> %d (%s)\n",#x,_r,E(_r)); fflush(stdout); return 1;} }while(0)
static void* readf(const char*p,size_t*n){FILE*f=fopen(p,"rb");if(!f){printf("cannot open %s\n",p);exit(2);}fseek(f,0,SEEK_END);*n=ftell(f);fseek(f,0,SEEK_SET);void*b=malloc(*n);fread(b,1,*n,f);fclose(f);return b;}
int main(int argc,char**argv){
    const char*ptx=argv[1],*kname=argv[2]; int grid=atoi(argv[3]); const char*outp=argv[4];
    int tmo = argc>5?atoi(argv[5]):5000;
    h=LoadLibraryA("nvcuda.dll");
#define G(n,s) *(FARPROC*)&n=GetProcAddress(h,s);
    G(cuInit,"cuInit") G(cuDeviceGet,"cuDeviceGet") G(cuCtxCreate,"cuCtxCreate_v2")
    G(cuModuleLoad,"cuModuleLoad") G(cuModuleGetFunction,"cuModuleGetFunction")
    G(cuModuleGetGlobal,"cuModuleGetGlobal_v2") G(cuModuleGetTexRef,"cuModuleGetTexRef")
    G(cuMemAlloc,"cuMemAlloc_v2") G(cuMemcpyHtoD,"cuMemcpyHtoD_v2") G(cuMemcpyDtoH,"cuMemcpyDtoH_v2")
    G(cuMemsetD32,"cuMemsetD32_v2") G(cuFuncSetCacheConfig,"cuFuncSetCacheConfig")
    G(cuFuncSetBlockShape,"cuFuncSetBlockShape") G(cuFuncSetSharedSize,"cuFuncSetSharedSize")
    G(cuLaunchGrid,"cuLaunchGrid") G(cuCtxSynchronize,"cuCtxSynchronize") G(cuGetErrorName,"cuGetErrorName")
    G(cuTexRefSetFormat,"cuTexRefSetFormat") G(cuTexRefSetAddress,"cuTexRefSetAddress_v2")
    G(cuEventCreate,"cuEventCreate") G(cuEventRecord,"cuEventRecord") G(cuEventQuery,"cuEventQuery")
    int dev; void*ctx;
    CK(cuInit(0)); CK(cuDeviceGet(&dev,0)); CK(cuCtxCreate(&ctx,0xC,dev));
    void*mod; CK(cuModuleLoad(&mod,ptx));
    void*fn; CK(cuModuleGetFunction(&fn,mod,kname));
    u32 cptr=0,csz=0; CK(cuModuleGetGlobal(&cptr,&csz,mod,"context"));
    size_t n; unsigned char*c=readf("../work/context.bin",&n);
    u32 out,bvh,mat,lit,a0,a1;
    const int W=512,H=300;
    CK(cuMemAlloc(&out,W*H*16)); CK(cuMemsetD32(out,0xDEADBEEF,W*H*4));
    void*b=readf("../work/bvh_full.bin",&n); CK(cuMemAlloc(&bvh,(u32)n)); CK(cuMemcpyHtoD(bvh,b,(u32)n)); u32 bvhsz=(u32)n;
    b=readf("../work/dump_mat_off0_sz6800.bin",&n); CK(cuMemAlloc(&mat,(u32)n)); CK(cuMemcpyHtoD(mat,b,(u32)n));
    b=readf("../work/dump_light_off0_sz32768.bin",&n); CK(cuMemAlloc(&lit,(u32)n)); CK(cuMemcpyHtoD(lit,b,(u32)n));
    CK(cuMemAlloc(&a0,33554432)); CK(cuMemsetD32(a0,0xFF808080,8388608));
    CK(cuMemAlloc(&a1,33554432)); CK(cuMemsetD32(a1,0xFF808080,8388608));
    b=readf("../work/sky.rgba",&n); CK(cuMemcpyHtoD(a1+3145728,b,(u32)n));
    if(argc>6) *(int*)(c+0x20C)=atoi(argv[6]);
    if(argc>7){ double a=atof(argv[7])*3.14159265358979/180.0; float d[3]={(float)sin(a),0,(float)-cos(a)}, u[3]={0,1,0}, r[3]={(float)-cos(a),0,(float)-sin(a)};
        memcpy(c+0x250,d,12); memcpy(c+0x25C,u,12); memcpy(c+0x268,r,12); }
    *(u32*)(c+0)=a0; *(u32*)(c+4)=a1; *(u32*)(c+0x220)=out; *(u32*)(c+0x224)=bvh; *(u32*)(c+0x228)=mat; *(u32*)(c+0x22C)=lit;
    CK(cuMemcpyHtoD(cptr,c,656));
    void*tex; CK(cuModuleGetTexRef(&tex,mod,"t_bvh")); CK(cuTexRefSetFormat(tex,0x20,4));
    u32 off=0; CK(cuTexRefSetAddress(&off,tex,bvh,bvhsz));
    CK(cuFuncSetCacheConfig(fn,2)); CK(cuFuncSetBlockShape(fn,32,2,1)); CK(cuFuncSetSharedSize(fn,0));
    printf("launching %s grid=%d ...\n",kname,grid); fflush(stdout);
    void*ev; CK(cuEventCreate(&ev,0));
    DWORD t0=GetTickCount();
    CK(cuLaunchGrid(fn,grid,1)); CK(cuEventRecord(ev,0));
    int r; while((r=cuEventQuery(ev))==600 /*NOT_READY*/){ if(GetTickCount()-t0>(DWORD)tmo){printf("TIMEOUT after %d ms (kernel still running)\n",tmo); fflush(stdout); ExitProcess(3);} Sleep(5);}
    printf("kernel done in %lu ms, event -> %d (%s)\n",GetTickCount()-t0,r,E(r));
    if(r) return 1;
    float*host=malloc(W*H*16); CK(cuMemcpyDtoH(host,out,W*H*16));
    FILE*f=fopen(outp,"wb"); fwrite(host,1,W*H*16,f); fclose(f);
    int written=0,nan=0; double sum=0; for(int i=0;i<W*H*4;i++){ unsigned u=((unsigned*)host)[i]; if(u!=0xDEADBEEF)written++; if(host[i]!=host[i])nan++; else sum+=host[i]; }
    printf("floats written: %d / %d, nan=%d, mean=%.4f\n",written,W*H*4,nan,sum/(W*H*4));
    return 0;
}
