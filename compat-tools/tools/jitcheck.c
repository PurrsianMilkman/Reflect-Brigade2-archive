#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#define API __stdcall
int main(int argc,char**argv){
    HMODULE h=LoadLibraryA("nvcuda.dll");
    int (API*cuInit)(unsigned)=(void*)GetProcAddress(h,"cuInit");
    int (API*cuDeviceGet)(int*,int)=(void*)GetProcAddress(h,"cuDeviceGet");
    int (API*cuCtxCreate)(void**,unsigned,int)=(void*)GetProcAddress(h,"cuCtxCreate_v2");
    int (API*cuModuleLoadDataEx)(void**,const void*,unsigned,int*,void**)=(void*)GetProcAddress(h,"cuModuleLoadDataEx");
    int (API*cuModuleGetFunction)(void**,void*,const char*)=(void*)GetProcAddress(h,"cuModuleGetFunction");
    FILE*f=fopen(argv[1],"rb"); fseek(f,0,SEEK_END); long n=ftell(f); fseek(f,0,SEEK_SET);
    char*buf=calloc(n+1,1); fread(buf,1,n,f); fclose(f);
    int dev; void*ctx; cuInit(0); cuDeviceGet(&dev,0); cuCtxCreate(&ctx,0,dev);
    static char elog[16384], ilog[16384];
    int opts[]={5/*ERROR_LOG_BUFFER*/,6/*ERROR_LOG_BUFFER_SIZE_BYTES*/,3/*INFO_LOG_BUFFER*/,4/*INFO_LOG_BUFFER_SIZE*/,12/*LOG_VERBOSE*/};
    void*vals[]={elog,(void*)sizeof(elog),ilog,(void*)sizeof(ilog),(void*)1};
    void*m=0; int r=cuModuleLoadDataEx(&m,buf,5,opts,vals);
    printf("cuModuleLoadDataEx -> %d\n",r);
    if(elog[0]) printf("ERROR LOG:\n%s\n",elog);
    if(argc>2 && ilog[0]) printf("INFO LOG:\n%s\n",ilog);
    return r;
}
