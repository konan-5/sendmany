    


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
#include <netdb.h>
#include <assert.h>
#include <sys/socket.h>
#include <arpa/inet.h>
//#include <SDL.h>
#ifdef EMSCRIPTEN
#include <emscripten.h>
#include <emscripten/websocket.h>
#include <emscripten/fetch.h>
#include <emscripten/threading.h>
#include <emscripten/posix_socket.h>
#endif

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

#define JSON_BUFSIZE 4096

char *wasm_result(int32_t retval,char *displaystr,int32_t seedpage)
{
    static char json[JSON_BUFSIZE];
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
    char fname[512],saltstr[65];
    int32_t i,retval = -1;
    KangarooTwelve((uint8_t *)password,(int32_t)strlen(password),salt,32);
    byteToHex(salt,saltstr,sizeof(salt));
    sprintf(fname,"%cqwallet%c%s",dir_delim(),dir_delim(),saltstr+48);
    //sprintf(fname,"%s",password);
    printf("check (%s) %s\n",fname,rw);
    if ( (fp= fopen(fname,rw)) != 0 )
    {
        //printf("opened (%s) %s\n",fname,rw);
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
    //printf("%s -> retval %d\n",rw,retval);
    if ( retval == 32 )
        return(0);
    return(retval);
}

char *sendfunc(char **argv,int32_t argc)
{
    static char str[4096+1024];
    char *password,*dest,txid[64],addr[64],rawhex[4096];
    uint8_t txdigest[32],subseed[32],privatekey[32],publickey[32],destpub[32],extradata[MAX_INPUT_SIZE];
    int64_t amount;
    int32_t txtick;
    if ( argc != 3 )
        return(wasm_result(-7,"sendfunc needs password dest amount",0));
    password = argv[0];
    dest = argv[1];
    if ( addr2pubkey(dest,destpub) == 0 )
    {
        char checkaddr[64];
        for (int i=0; dest[i]!=0; i++)
        {
            strcpy(checkaddr,dest);
            for (int j='A'; j<='Z'; j++)
            {
                checkaddr[i] = j;
                if ( checkSumIdentity(checkaddr) != 0 )
                {
                    sprintf(str,"send illegal dest: changing %dth to %c to %s passes checksum",i,j,checkaddr);
                    return(wasm_result(-8,str,0));
                }
            }
        }
        return(wasm_result(-9,"illegal destination address, bad checksum",0));
    }
    amount = atoll(argv[2]);
    txid[0] = 0;
    if ( accountcodec("rb",password,subseed) == 0 )
    {
        getPrivateKeyFromSubSeed(subseed,privatekey);
        getPublicKeyFromPrivateKey(privatekey,publickey);
        pubkey2addr(publickey,addr);
        create_rawtxhex(rawhex,txid,txdigest,subseed,0,publickey,destpub,amount,extradata,0,txtick);
        txid[60] = 0;
        sprintf(str,"%s %s -> %s, %s, %s",addr,amountstr(amount),dest,txid,rawhex);
        printf("%s\n",str);
        memset(subseed,0xff,sizeof(subseed));
        memset(privatekey,0xff,sizeof(privatekey));
        return(wasm_result(0,str,0));
    }
    return(wasm_result(-10,"unknown user account password file not found",0));
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
        //printf("found encrypted file for (%s) -> %s\n",password,addr);
        return(wasm_result(0,addr,0));
    }
    //printf("create encrypted file for %s\n",password);
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
        return(wasm_result(-5,"error creating encrypted account",0));
    }
    getIdentityFromPublicKey(publickey,addr,false);
    addr[60] = 0;
    printf("loginfunc got (%s) -> seed {%s} %s\n",password,seed,addr);
    return(wasm_result(retval,seed,1));
}

char QWALLET_ARGS[64][JSON_BUFSIZE],QWALLET_RESULTS[(sizeof(QWALLET_ARGS)/sizeof(*QWALLET_ARGS))][JSON_BUFSIZE],QWALLET_lasti;

struct qcommands
{
    const char *command;
    char *(*func)(char **,int32_t);
} QCMDS[] =
{
    { "addseed", addseedfunc },
    { "login", loginfunc },
    { "send", sendfunc },
};

char *qwallet(char *_args)
{
    int32_t i,pendingid;
    static char retbuf[JSON_BUFSIZE];
    printf("qwallet %s\n",_args);
    if ( strncmp(_args,(char *)"status",6) == 0 )
    {
        pendingid = atoi(_args+7) % (sizeof(QWALLET_RESULTS)/sizeof(*QWALLET_RESULTS));
        if ( QWALLET_RESULTS[pendingid][0] == 0 )
        {
            if ( QWALLET_ARGS[pendingid][0] == 0 )
                return(wasm_result(pendingid,"no command pending",0));
            else return(wasm_result(pendingid,"command pending",0));
        }
        else
        {
            strcpy(retbuf,QWALLET_RESULTS[pendingid]);
            memset(QWALLET_RESULTS[pendingid],0,sizeof(QWALLET_RESULTS[pendingid]));
            memset(QWALLET_ARGS[pendingid],0,sizeof(QWALLET_ARGS[pendingid]));
            return(retbuf);
        }
    }
    else if ( strcmp(_args,(char *)"v1request") == 0 )
    {
        if ( (rand() % 4) == 0 )
            return(wasm_result(0,(char *)"tick-data/12050984",0));
        else if ( (rand() % 5) == 0 )
            return(wasm_result(0,(char *)"quorum/12050984",0));
        else if ( (rand() % 6) == 0 )
            return(wasm_result(0,(char *)"tx/pymrxtprfqltdcvyqesztatnuwlaqqaswdrlzxvjgdxhzgqwhaygjiudrnul",0));
        return(wasm_result(-13,(char *)"no v1requests available",0));
    }
    else if ( strncmp(_args,(char *)"v1status",8) == 0 )
    {
        printf("%s\n",_args);
        return(wasm_result(0,(char *)"thank you for status!",0));
    }
    else if ( strncmp(_args,(char *)"v1tick-data",11) == 0 )
    {
        printf("tick-data %s\n",_args);
        return(wasm_result(0,(char *)"thank you for tick-data!",0));
    }
    else if ( strncmp(_args,(char *)"v1quorum",8) == 0 )
    {
        printf("quorum data %s\n",_args);
        return(wasm_result(0,(char *)"thank you for quorum data!",0));
    }
    else if ( strncmp(_args,(char *)"v1tx",4) == 0 )
    {
        printf("txid %s\n",_args);
        return(wasm_result(0,(char *)"thank you for txid!",0));
    }
    for (i=0; i<(sizeof(QWALLET_ARGS)/sizeof(*QWALLET_ARGS)); i++)
    {
        pendingid = (QWALLET_lasti + i + 1) % (sizeof(QWALLET_ARGS)/sizeof(*QWALLET_ARGS));
        if ( QWALLET_ARGS[pendingid][0] == 0 )
        {
            strcpy(QWALLET_ARGS[pendingid],_args);
            memset(QWALLET_RESULTS[i],0,sizeof(QWALLET_RESULTS[i]));
            QWALLET_lasti = pendingid;
            return(wasm_result(pendingid,"command queued",0));
        }
    }
    return(wasm_result(-6,"command queue full",0));
}

char *_qwallet(char *_args)
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
    //printf("args.(%s) -> cmd [%s]\n",args,cmd);
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
            //for (j=0; j<argc; j++)
            //    printf("{%s} ",argv[j]);
            //printf("argc.%d %s\n",argc,cmd);
            return((*QCMDS[i].func)(argv,argc));
        }
    }
    return(wasm_result(-1,"unknown command",0));
}

#ifdef EMSCRIPTEN
static EMSCRIPTEN_WEBSOCKET_T bridgeSocket = 0;

EM_JS(void, start_timer, (),
    {
        Module.timer = false;
        setTimeout(function() { Module.timer = true; }, 500);
    }
);
EM_JS(bool, check_timer, (), { return Module.timer; });


int32_t MAIN_count;

int main()
{
    int32_t i;
    printf("MAIN CALLED.%d\n",MAIN_count);
    MAIN_count++;
    MAIN_THREAD_EM_ASM(
           FS.mkdir('/qwallet');
           // FS.mount(IDBFS, {}, '/qwallet');
           FS.mount(NODEFS, { root: '.' }, '/qwallet');
           FS.syncfs(true, function (err) {
             assert(!err); });
    );
    //pthread_t mainloop_thread;
    //pthread_create(&mainloop_thread,NULL,&mainloop,0);
    start_timer();
    while ( 1 )
    {
        if ( check_timer() )
        {
            start_timer();
        }
        for (i=0; i<(sizeof(QWALLET_ARGS)/sizeof(*QWALLET_ARGS)); i++)
        {
            if ( QWALLET_ARGS[i][0] != 0 && QWALLET_RESULTS[i][0] == 0 )
            {
                printf("QWALLET_ARGS[%d] (%s)\n",i,QWALLET_ARGS[i]);
                char *retstr = _qwallet(QWALLET_ARGS[i]);
                printf("Q.%d returns.(%s)\n",i,retstr);
                strcpy(QWALLET_RESULTS[i],retstr);
                MAIN_THREAD_EM_ASM(
                       FS.syncfs(function (err) {
                      assert(!err);
                    });
                );
             }
        }
        emscripten_sleep(100);
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

    
