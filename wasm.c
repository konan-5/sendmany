    
// emcc -O3 -sFETCH -lwebsocket.js -sEXPORTED_FUNCTIONS=_qwallet,_main -sEXPORTED_RUNTIME_METHODS=ccall -pthread -sPROXY_TO_PTHREAD -s PTHREAD_POOL_SIZE=8 -s INITIAL_MEMORY=33554432 -s WASM_ASYNC_COMPILATION=0 -s SINGLE_FILE=1 -s -D_LARGEFILE64_SOURCE=1 -fPIC -Wno-implicit-function-declaration -msse2 -msse3 -msse4.1 -msimd128 -msse4.2 -mavx -sASYNCIFY wasm.c
// node a.out.js


#define TESTNET

#define MAX_INPUT_SIZE 1024
#ifdef TESTNET
#define DEFAULT_NODE_PORT 31841
#define DEFAULT_NODE_IP ((char *)"57.129.19.155") //193.135.9.63") //
#define TICKOFFSET 3
#else
#define DEFAULT_NODE_PORT 21841
#define DEFAULT_NODE_IP ((char *)"148.251.185.19")
#define TICKOFFSET 3
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

int32_t LATEST_TICK;

#include "K12AndKeyUtil.h"
#include "qdefines.h"
#include "qstructs.h"
#include "qhelpers.c"
//#include "qkeys.c"
//#include "qtx.c"


char *qwallet(char *args)
{
    int i;
    printf("args.(%s)\n",args);

    return("{ \"result\":0, \"display\":\"this is the result\"}");
}


/*int oldmain(int argc, const char * argv[])
{
    int32_t i;
    uint8_t digest[32],subseed[32],privatekey[32],publickey[32];
    char rawhex[4096],txhash[64],dest[64],firstaddr[64],*origseed;
    if ( argc == 1 )
        origseed = (char *)"sdebfreexvihhpcntgkhvkeynlejwhiufpvzzvqfgovfadywgrflbol";
    else origseed = (char *)argv[1];
    deriveaddr(origseed,(char *)"",subseed,privatekey,publickey,dest);
    deriveaddr(origseed,(char *)"Z",subseed,privatekey,publickey,firstaddr);
    printf("(%s) -> orig %s, firstaddr is %s\n",origseed,dest,firstaddr);
    create_rawtransaction(rawhex,txhash,digest,subseed,dest,1,13000000,"");
    printf("reclaim %s -> %s %s\n",dest,firstaddr,rawhex);
    while ( 1 )
    {
        devurandom(subseed,sizeof(subseed));
        getPrivateKeyFromSubSeed(subseed,privatekey);
        getPublicKeyFromPrivateKey(privatekey,publickey);
        getIdentityFromPublicKey(publickey,dest,false);
        dest[60] = 0;
        byteToHex(subseed,rawhex,32);
        printf("0x%s %s\n",rawhex,dest);
    }
    return(0);
 }*/


#include <emscripten.h>
#include <stdio.h>

EM_JS(void, start_timer, (), { Module.timer = false; setTimeout(function() { Module.timer = true; }, 500); });
EM_JS(bool, check_timer, (), { return Module.timer; });

int main()
{
    start_timer();
    while ( 1 )
    {
      if ( check_timer() )
      {
          printf("timer happened!\n");
          start_timer();
          //return 0;
      }
      printf("sleeping...\n");
      emscripten_sleep(100);
    }
}

    
