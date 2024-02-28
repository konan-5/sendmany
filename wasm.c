    


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
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <emscripten.h>

#include "K12AndKeyUtil.h"

#include "qdefines.h"
#include "qstructs.h"
#include "qkeys.c"
#include "qhelpers.c"
int32_t FANDEPTH,LATEST_TICK = 1;    // start at nonzero value to trigger initial requests
pthread_mutex_t txq_mutex,txq_sendmutex,conn_mutex;

// C code "linked" in by #include
#include "qtime.c"
#include "qconn.c"
#include "qcmds.c"
#include "qtx.c"
#include "qthreads.c"
#include "qpeers.c"
#include "qfanout.c"
#include "qsendmany.c"
#include "qtests.c"


char *wasm_result(int32_t retval,char *displaystr,int32_t seedpage)
{
    static char json[65536];
    sprintf(json,"{\"result\":%d,\"display\":\"%s\",\"seedpage\":%d}",retval,displaystr,seedpage);
    return(json);
}

int32_t genaddr(char *origseed,char *extrastr,uint8_t subseed[32],uint8_t privatekey[32],uint8_t publickey[32],char *addr)
{
    uint8_t extraseed[64];
    if ( extrastr != 0 && extrastr[0] != 0 )
    {
        getSubseedFromSeed((uint8_t *)origseed,extraseed);
        KangarooTwelve((uint8_t *)extrastr,(int32_t)strlen(extrastr),&extraseed[32],32);
        KangarooTwelve(extraseed,sizeof(extraseed),subseed,32);
    } else getSubseedFromSeed((uint8_t *)origseed,subseed);
    getPrivateKeyFromSubSeed(subseed,privatekey);
    getPublicKeyFromPrivateKey(privatekey,publickey);
    getIdentityFromPublicKey(publickey,addr,false);
    addr[60] = 0;
    //printf("genaddr got (%s) extra.(%s) -> %s\n",origseed,extrastr,addr);
    return(0);
}

char *addseedfunc(char **argv,int32_t argc)
{
    int32_t i,retval;
    uint8_t privatekey[32],publickey[32],subseed[32];
    char addr[64],*origseed,*extrastr;
    if ( argc == 0 )
        return(wasm_result(-2,"addseed needs seed",0));
    origseed = argv[0];
    extrastr = argv[1];
    retval = genaddr(origseed,extrastr,subseed,privatekey,publickey,addr);
    return(wasm_result(retval,addr,0));
}

int32_t accountcodec(char *rw,char *password,uint8_t subseed[32])
{
    FILE *fp;
    uint8_t salt[32];
    char fname[512];
    int32_t i,retval = -1;
    KangarooTwelve((uint8_t *)password,(int32_t)strlen(password),salt,32);
    sprintf(fname,"%cqwallet%c%s",dir_delim(),dir_delim(),password);
    //sprintf(fname,"%s",password);
    printf("check (%s) %s\n",fname,rw);
    if ( (fp= fopen(fname,rw)) != 0 )
    {
        printf("opened (%s) %s\n",fname,rw);
        if ( strcmp(rw,"wb") == 0 )
        {
            for (i=0; i<sizeof(salt); i++)
                subseed[i] ^= salt[i];
            retval = fwrite(subseed,1,32,fp);
        }
        else
        {
            retval = fread(subseed,1,32,fp);
            for (i=0; i<sizeof(salt); i++)
                subseed[i] ^= salt[i];
        }
        fclose(fp);
    }
    printf("%s -> retval %d\n",rw,retval);
    if ( retval == 32 )
        return(0);
    return(retval);
}

char *loginfunc(char **argv,int32_t argc)
{
    int32_t i,retval;
    uint16_t bipi;
    uint64_t tmp;
    uint8_t privatekey[32],publickey[32],subseed[32];
    char addr[64],seed[512],*password,bipwords[24][16],*retstr,*extrastr = (char *)"";
    if ( argc == 0 )
        return(wasm_result(-3,"login needs password",0));
    password = argv[0];
    devurandom(subseed,32);
    if ( accountcodec("rb",password,subseed) == 0 )
    {
        getPrivateKeyFromSubSeed(subseed,privatekey);
        memset(subseed,0xff,sizeof(subseed));
        getPublicKeyFromPrivateKey(privatekey,publickey);
        memset(privatekey,0xff,sizeof(privatekey));
        pubkey2addr(publickey,addr);
        printf("found encrpted file for (%s) -> %s\n",password,addr);
        return(wasm_result(0,addr,0));
    }
    printf("create encrypted file for %s\n",password);
    if ( argc == 1 )
    {
        for (i=0; i<55; i++)
        {
            devurandom((uint8_t *)&tmp,sizeof(tmp));
            seed[i] = (tmp % 26) + 'a';
        }
        seed[55] = 0;
        getSubseedFromSeed((uint8_t *)seed,subseed);
    }
    else if ( strcmp(argv[1],"bip39") == 0 )
    {
        seed[0] = 0;
        for (i=0; i<sizeof(bipwords)/sizeof(*bipwords); i++)
        {
            devurandom((uint8_t *)&bipi,sizeof(bipi));
            sprintf(seed+strlen(seed),"%s ",BIP39[bipi % (sizeof(BIP39)/sizeof(*BIP39))]);
        }
        seed[strlen(seed)-1] = 0;
        KangarooTwelve((uint8_t *)seed,strlen(seed),subseed,32);
    }
    else
        return(wasm_result(-4,"login unknown second arg",0));
    getPrivateKeyFromSubSeed(subseed,privatekey);
    getPublicKeyFromPrivateKey(privatekey,publickey);
    memset(privatekey,0xff,sizeof(privatekey));
    retval = accountcodec("wb",password,subseed);
    memset(subseed,0xff,sizeof(subseed));
    if ( retval < 0 )
    {
        memset(seed,0xff,sizeof(seed));
        printf("error -5\n");
        return(wasm_result(-5,"error creating encrypted account",0));
    }
    getIdentityFromPublicKey(publickey,addr,false);
    addr[60] = 0;
    printf("loginfunc got (%s) -> seed {%s} %s\n",password,seed,addr);
    return(wasm_result(retval,seed,1));
}

struct qcommands
{
    const char *command;
    char *(*func)(char **,int32_t);
} QCMDS[] =
{
    { "addseed", addseedfunc },
    { "login", loginfunc },
};

char *qwallet(char *_args)
{
    int32_t i,j,len,argc = 0;
    char *argv[16],cmd[64],args[1024];
    args[sizeof(args)-1] = 0;
    strncpy(args,_args,sizeof(args)-1);
    for (i=0; args[i]!=0&&i<sizeof(cmd)-1; i++)
    {
        cmd[i] = args[i];
        if ( args[i] == ' ' )
            break;
    }
    cmd[i] = 0;
    printf("args.(%s) -> cmd [%s]\n",args,cmd);
    for (i=0; i<sizeof(QCMDS)/sizeof(*QCMDS); i++)
    {
        if ( strcmp(cmd,QCMDS[i].command) == 0 )
        {
            len = (int32_t)strlen(cmd);
            while ( args[len] == ' ' || args[len] == '\t' || args[len] == '\r' || args[len] == '\n' )
                len++;
            argv[argc++] = &args[len];
            while ( args[len] != 0 )
            {
                if ( args[len] == ',' )
                {
                    args[len++] = 0;
                    argv[argc++] = &args[len];
                    if ( argc >= (sizeof(argv)/sizeof(*argv)) )
                        return(wasm_result(-5,"too many arguments",0));
                } else len++;
            }
            argv[argc] = (char *)"";
            for (j=0; j<argc; j++)
                printf("{%s} ",argv[j]);
            printf("argc.%d %s\n",argc,cmd);
            return((*QCMDS[i].func)(argv,argc));
        }
    }
    return(wasm_result(-1,"unknown command",0));
}

#ifdef EMSCRIPTEN

EM_JS(void, start_timer, (),
    {
        Module.timer = false;
        setTimeout(function() { Module.timer = true; }, 1000);
    }
);
EM_JS(bool, check_timer, (), { return Module.timer; });

int main()
{
    MAIN_THREAD_EM_ASM(
           FS.mkdir('/qwallet');
           // FS.mount(IDBFS, {}, '/qwallet');
           FS.mount(NODEFS, { root: '.' }, '/qwallet');
           FS.syncfs(true, function (err) {
             assert(!err); });
    );

    start_timer();
    while ( 1 )
    {
      if ( check_timer() )
      {
          //printf("timer happened!\n");
            //   qwallet((char *)"login passwordB,bip39");
        /*char *retstr = qwallet((char *)"login password");
          printf("got retstr.(%s)\n",retstr);
          EM_ASM(
                 FS.syncfs(function (err) {
                assert(!err);
              });
          );*/
           fflush(stdout);
          start_timer();
          //return 0;
      }
      //printf("sleeping...\n");
      emscripten_sleep(1000);
    }
}
#else
int main()
{
    makedir((char *)"qwallet");
    qwallet((char *)"login password");
    return(0);
}
#endif

    
