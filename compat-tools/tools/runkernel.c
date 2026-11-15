/* Standalone harness: set up a `context` exactly like TracerCUDA.dll does,
   launch the kernel with the real launch config, read back the output buffer. */
#include <windows.h>
#include <stdio.h>
#define API __stdcall
typedef unsigned int u32;
static int (API *cuInit)(unsigned);
static int (API *cuDeviceGet)(int*,int);
static int (API *cuCtxCreate)(void**,unsigned,int);
static int (API *cuModuleLoad)(void**,const char*);
static int (API *cuModuleGetFunction)(void**,void*,const char*);
static int (API *cuModuleGetGlobal)(u32*,u32*,void*,const char*);
static int (API *cuModuleGetTexRef)(void**,void*,const char*);
static int (API *cuMemAlloc)(u32*,u32);
static int (API *cuMemcpyHtoD)(u32,const void*,u32);
static int (API *cuMemcpyDtoH)(void*,u32,u32);
static int (API *cuFuncSetBlockShape)(void*,int,int,int);
static int (API *cuFuncSetSharedSize)(void*,unsigned);
static int (API *cuLaunchGrid)(void*,int,int);
static int (API *cuCtxSynchronize)(void);
static int (API *cuGetErrorName)(int,const char**);
static const char* E(int r){const char*s="?";if(cuGetErrorName)cuGetErrorName(r,&s);return s;}
#define CK(x) do{int _r=(x); if(_r){printf("  FAIL %-34s -> %d (%s)\n",#x,_r,E(_r)); return 1;} }while(0)

int main(int argc,char**argv){
    const char* ptx = argc>1?argv[1]:"testpattern.ptx";
    HMODULE h=LoadLibraryA("nvcuda.dll");
    if(!h){printf("no driver\n");return 1;}
#define G(n,s) *(FARPROC*)&n=GetProcAddress(h,s);
    G(cuInit,"cuInit") G(cuDeviceGet,"cuDeviceGet") G(cuCtxCreate,"cuCtxCreate_v2")
    G(cuModuleLoad,"cuModuleLoad") G(cuModuleGetFunction,"cuModuleGetFunction")
    G(cuModuleGetGlobal,"cuModuleGetGlobal_v2") G(cuModuleGetTexRef,"cuModuleGetTexRef")
    G(cuMemAlloc,"cuMemAlloc_v2") G(cuMemcpyHtoD,"cuMemcpyHtoD_v2") G(cuMemcpyDtoH,"cuMemcpyDtoH_v2")
    G(cuFuncSetBlockShape,"cuFuncSetBlockShape") G(cuFuncSetSharedSize,"cuFuncSetSharedSize")
    G(cuLaunchGrid,"cuLaunchGrid") G(cuCtxSynchronize,"cuCtxSynchronize") G(cuGetErrorName,"cuGetErrorName")

    int dev; void*ctx;
    CK(cuInit(0)); CK(cuDeviceGet(&dev,0)); CK(cuCtxCreate(&ctx,0,dev));
    void*mod; int r=cuModuleLoad(&mod,ptx);
    printf("cuModuleLoad(%s) -> %d (%s)\n",ptx,r,E(r));
    if(r) return 1;
    void*fn; CK(cuModuleGetFunction(&fn,mod,"TracePixelMIS"));
    u32 cptr=0,csize=0; CK(cuModuleGetGlobal(&cptr,&csize,mod,"context"));
    printf("context devptr=0x%08X size=%u\n",cptr,csize);

    const int W=512,H=300;
    u32 out=0; CK(cuMemAlloc(&out, W*H*16));
    printf("output buffer devptr=0x%08X (%d bytes)\n",out,W*H*16);

    /* build the context image exactly as observed */
    unsigned char c[656]; memset(c,0,sizeof c);
    *(int*)(c+0x200)=W;
    *(int*)(c+0x204)=H;
    *(int*)(c+0x208)=0;
    *(int*)(c+0x20C)=4;
    *(u32*)(c+0x220)=out;
    CK(cuMemcpyHtoD(cptr,c,656));

    CK(cuFuncSetBlockShape(fn,32,2,1));
    CK(cuFuncSetSharedSize(fn,0));
    r=cuLaunchGrid(fn,2400,1);
    printf("cuLaunchGrid(2400,1) -> %d (%s)\n",r,E(r));
    r=cuCtxSynchronize();
    printf("cuCtxSynchronize      -> %d (%s)\n",r,E(r));
    if(r) return 1;

    float* host=(float*)malloc(W*H*16);
    CK(cuMemcpyDtoH(host,out,W*H*16));
    printf("\nsampled pixels (expect r = x/W, g = y/H, b = 0.2):\n");
    int pts[6][2]={{0,0},{511,0},{0,299},{511,299},{256,150},{128,75}};
    for(int i=0;i<6;i++){
        int x=pts[i][0], y=pts[i][1];
        float*p=host+((size_t)y*W+x)*4;
        printf("  (%3d,%3d) = (%.4f, %.4f, %.4f, %.4f)   expect (%.4f, %.4f, 0.2000, 1.0000)\n",
               x,y,p[0],p[1],p[2],p[3],(float)x/W,(float)y/H);
    }
    int nz=0; for(int i=0;i<W*H*4;i++) if(host[i]!=0.0f) nz++;
    printf("\nnon-zero floats: %d / %d\n",nz,W*H*4);
    return 0;
}
