


//#define TESTNET

#define MAX_INPUT_SIZE 1024
#ifdef TESTNET
#define DEFAULT_NODE_PORT 31841
#define DEFAULT_NODE_IP ((char *)"57.129.19.155") //193.135.9.63") //
#define TICKOFFSET 3
#else
#define DEFAULT_NODE_PORT 21841
#define DEFAULT_NODE_IP ((char *)"185.70.186.149")
#define TICKOFFSET 10
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

#ifdef EMSCRIPTEN
#include <emscripten.h>
#include <emscripten/websocket.h>
#endif

#include "K12AndKeyUtil.h"

#include "qdefines.h"
#include "qstructs.h"
#include "json.c"
#include "qkeys.c"
#include "qhelpers.c"


#define JSON_BUFSIZE (65536)

int32_t LATEST_TICK,NUMADDRESSES;
char LOGINADDRESSES[64][64];
#ifdef QUEUE_COMMANDS
char QWALLET_ARGS[4][1024],QWALLET_RESULTS[(sizeof(QWALLET_ARGS)/sizeof(*QWALLET_ARGS))][JSON_BUFSIZE],QWALLET_lasti;
#endif

// C code "linked" in by #include
#include "qtime.c"
//#include "qconn.c"
//#include "qcmds.c"
#include "qtx.c"
//#include "qthreads.c"
//#include "qpeers.c"
//#include "qfanout.c"
//#include "qsendmany.c"
//#include "qtests.c"


char *wasm_result(int32_t retval,char *displaystr,int32_t seedpage)
{
    static char json[JSON_BUFSIZE],tmpstr[JSON_BUFSIZE-128];
    if ( displaystr[0] != '{' )
        sprintf(tmpstr,"\"%s\"",displaystr);
    else strcpy(tmpstr,displaystr);
    sprintf(json,"{\"result\":%d,\"display\":%s,\"seedpage\":%d}",retval,tmpstr,seedpage);
    return(json);
}

int32_t statusupdate(char *jsonstr)
{
    long val;
    result(json_element) element_result = json_parse(jsonstr);
    if ( result_is_err(json_element)(&element_result) )
    {
        typed(json_error) error = result_unwrap_err(json_element)(&element_result);
        fprintf(stderr, "Error parsing JSON: %s\n", json_error_to_string(error));
        return(-1);
    }
    typed(json_element) element = result_unwrap(json_element)(&element_result);
    result(json_element) latest_element_result = json_object_find(element.value.as_object, "latest");
    if ( result_is_err(json_element)(&latest_element_result) )
    {
        typed(json_error) error = result_unwrap_err(json_element)(&latest_element_result);
        fprintf(stderr, "Error getting element \"latest\": %s\n", json_error_to_string(error));
        return(-1);
    }
    typed(json_element) latest_element = result_unwrap(json_element)(&latest_element_result);
    typed(json_array) *arr = latest_element.value.as_array;
    if ( arr->count == 2 )
    {
        typed(json_element) element = arr->elements[1];
        typed(json_element_value) value = element.value;
        val = value.as_number.value.as_long;
        if ( val > LATEST_TICK )
        {
            LATEST_TICK = val;
            printf("LATEST_TICK.%d\n",LATEST_TICK);
        }
    }
    json_free(&element);
    return(LATEST_TICK);
}

void loginaddress(char *addr)
{
    int32_t i;
    
    for (i=0; i<NUMADDRESSES; i++)
    {
        if ( strcmp(LOGINADDRESSES[i],addr) == 0 )
            return;
    }
    if ( NUMADDRESSES >= sizeof(LOGINADDRESSES)/sizeof(*LOGINADDRESSES) )
    {
        printf("too many addresses, cannot track anymore. restart Qwallet\n");
        return;
    }
    strcpy(LOGINADDRESSES[NUMADDRESSES],addr);
    NUMADDRESSES++;
}

void accountfname(char *password,int32_t index,char *fname,uint8_t salt[32])
{
    char saltstr[65];
    KangarooTwelve((uint8_t *)password,(int32_t)strlen(password),salt,32);
    byteToHex(salt,saltstr,32);
    sprintf(fname,"%cqwallet%ckeys%c%s.%d",dir_delim(),dir_delim(),dir_delim(),saltstr+48,index);
}

int32_t accountcodec(char *rw,char *password,int32_t index,uint8_t subseed[32])
{
    FILE *fp;
    uint8_t salt[32];
    char fname[512];
    int32_t i,retval = -1;
    if ( index < 0 )
        return(-2);
    accountfname(password,index,fname,salt);
    //printf("check (%s) %s index.%d\n",fname,rw,index);
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
    int32_t txtick,datalen=0,index = 0;
    if ( argc < 4 || argc > 5 )
        return(wasm_result(-2,"sendfunc needs password index dest amount [hexstr]",0));
    password = argv[0];
    index = atoi(argv[1]);
    dest = argv[2];
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
                    return(wasm_result(-3,str,0));
                }
            }
        }
        return(wasm_result(-4,"illegal destination address, bad checksum",0));
    }
    amount = atoll(argv[3]);
    txid[0] = 0;
    if ( accountcodec("rb",password,index,subseed) == 0 )
    {
        getPrivateKeyFromSubSeed(subseed,privatekey);
        getPublicKeyFromPrivateKey(privatekey,publickey);
        pubkey2addr(publickey,addr);
        if ( argc == 5 )
        {
            datalen = strlen(argv[4]) / 2;
            hexToByte(argv[4],extradata,datalen);
        }
        txtick = LATEST_TICK + TICKOFFSET;
        create_rawtxhex(rawhex,txid,txdigest,subseed,0,publickey,destpub,amount,extradata,datalen,txtick);
        txid[60] = 0;
        sprintf(str,"{\"txtick\":%d,\"txid\":\"%s\",\"rawhex\":\"%s\",\"addr\":\"%s\",\"amount\":%s,\"dest\":\"%s\"}",txtick,txid,rawhex,addr,amountstr(amount),dest);
        memset(subseed,0xff,sizeof(subseed));
        memset(privatekey,0xff,sizeof(privatekey));
        char *retstr = wasm_result(0,str,0);
        printf("%s\n",retstr);
        return(retstr);
    }
    return(wasm_result(-5,"unknown user account password file not found or invalid index",0));
}

void subseedcombine(uint8_t subseed[32],uint8_t subseed2[32])
{
    uint8_t seedbuf[64];
    memcpy(seedbuf,subseed,32);
    memcpy(seedbuf+32,subseed2,32);
    KangarooTwelve(seedbuf,64,subseed,32);
    memset(seedbuf,0xff,sizeof(seedbuf));
}

char *loginfunc(char **argv,int32_t argc)
{
    int32_t i,retval,index = 0;
    uint16_t bipi;
    uint64_t tmp;
    uint8_t privatekey[32],publickey[32],subseed[32],subseed2[32];
    char addr[64],seed[512],*password,bipwords[24][16];
    if ( argc == 0 || argc > 3 )
        return(wasm_result(-6,"login needs password",0));
    password = argv[0];
    devurandom(subseed,32);
    if ( argc >= 2 )
    {
        index = atoi(argv[1]);
        if ( index < 0 )
            return(wasm_result(-7,"login needs non negative index less than maxaddresses",0));
    }
    if ( accountcodec("rb",password,0,subseed) == 0 )
    {
        if ( index > 0 )
        {
            if ( argc == 3 )
            {
                if ( accountcodec("rb",password,index,subseed2) == 0 )
                {
                    memset(subseed,0xff,sizeof(subseed));
                    memset(subseed2,0xff,sizeof(subseed2));
                    return(wasm_result(-8,"password already has derived subseed at index",0));
                }
                KangarooTwelve((uint8_t *)argv[2],strlen(argv[2]),subseed2,32);
                subseedcombine(subseed,subseed2);
                retval = accountcodec("wb",password,index,subseed2);
                memset(subseed2,0xff,sizeof(subseed2));
                if ( retval < 0 )
                {
                    memset(subseed,0xff,sizeof(subseed));
                    return(wasm_result(-9,"error creating encrypted derived index account",0));
                }
            }
            else
            {
                if ( accountcodec("rb",password,index,subseed2) != 0 )
                {
                    memset(subseed,0xff,sizeof(subseed));
                    return(wasm_result(-10,"password does not have derived subseed at index",0));
                }
                subseedcombine(subseed,subseed2);
                memset(subseed2,0xff,sizeof(subseed2));
            }
        }
        getPrivateKeyFromSubSeed(subseed,privatekey);
        getPublicKeyFromPrivateKey(privatekey,publickey);
        pubkey2addr(publickey,addr);
        loginaddress(addr);
        //printf("found encrypted file for (%s) -> %s\n",password,addr);
        memset(subseed,0xff,sizeof(subseed));
        memset(privatekey,0xff,sizeof(privatekey));
        return(wasm_result(0,addr,0));
    }
    if ( index != 0 || argc > 2 )
        return(wasm_result(-11,"cannot create nonzero index or derivation without index.0",0));
  //printf("create encrypted file for %s\n",password);
    if ( argv[0][0] == 'Q' )
    {
        for (i=0; i<55; i++)
        {
            devurandom((uint8_t *)&tmp,sizeof(tmp));
            seed[i] = (tmp % 26) + 'a';
        }
        seed[55] = 0;
        getSubseedFromSeed((uint8_t *)seed,subseed);
    }
    else
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
    getPrivateKeyFromSubSeed(subseed,privatekey);
    getPublicKeyFromPrivateKey(privatekey,publickey);
    memset(privatekey,0xff,sizeof(privatekey));
    retval = accountcodec("wb",password,index,subseed);
    memset(subseed,0xff,sizeof(subseed));
    if ( retval < 0 )
    {
        memset(seed,0xff,sizeof(seed));
        return(wasm_result(-12,"error creating encrypted account",0));
    }
    getIdentityFromPublicKey(publickey,addr,false);
    addr[60] = 0;
    printf("loginfunc got (%s) -> seed {%s} %s\n",password,seed,addr);
    return(wasm_result(retval,seed,1));
}

char *addseedfunc(char **argv,int32_t argc)
{
    int32_t i,retval=0;
    char *password,*seed;
    uint8_t privatekey[32],publickey[32],subseed[32];
    char addr[64],*origseed;
    if ( argc != 2 )
        return(wasm_result(-13,"addseed needs password seed",0));
    password = argv[0];
    seed = argv[1];
    if ( accountcodec("rb",password,0,subseed) == 0 )
        return(wasm_result(-14,"password already has seed",0));
    for (i=0; i<55; i++)
    {
        if ( seed[i] < 'a' || seed[i] > 'z' )
            break;
    }
    if ( i == 55 )
        getSubseedFromSeed((uint8_t *)seed,subseed);
    else KangarooTwelve((uint8_t *)seed,strlen(seed),subseed,32);
    getPrivateKeyFromSubSeed(subseed,privatekey);
    getPublicKeyFromPrivateKey(privatekey,publickey);
    memset(privatekey,0xff,sizeof(privatekey));
    retval = accountcodec("wb",password,0,subseed);
    memset(subseed,0xff,sizeof(subseed));
    if ( retval < 0 )
    {
        memset(seed,0xff,strlen(seed));
        return(wasm_result(-15,"error creating encrypted account",0));
    }
    getIdentityFromPublicKey(publickey,addr,false);
    addr[60] = 0;
    printf("addseed got (%s) -> seed {%s} %s\n",password,seed,addr);
    memset(seed,0xff,strlen(seed));
    return(wasm_result(retval,addr,0));
}

char *deletefunc(char **argv,int32_t argc)
{
    FILE *fp;
    uint8_t salt[32];
    int32_t index;
    char *password,fname[512],retstr[512];
    if ( argc != 2 )
        return(wasm_result(-20,"delete needs password,index",0));
    password = argv[0];
    index = atoi(argv[1]);
    accountfname(password,index,fname,salt);
    if ( (fp= fopen(fname,"rb")) == 0 )
        return(wasm_result(-21,"password,index has no file",0));
    fclose(fp);
    deletefile(fname);
    if ( index > 0 )
        return(wasm_result(0,"password,index file deleted",0));
    while ( 1 )
    {
        index++;
        accountfname(password,index,fname,salt);
        if ( (fp= fopen(fname,"rb")) == 0 )
            return(wasm_result(0,"all consecutive index files for password deleted",0));
        fclose(fp);
        deletefile(fname);
    }
    sprintf(retstr,"%d index files deleted for password",index);
    return(wasm_result(0,retstr,0));
}

struct qcommands
{
    const char *command;
    char *(*func)(char **,int32_t);
    const char *helpstr;
} QCMDS[] =
{
    { "addseed", addseedfunc, "addseed password,seed" },
    { "login", loginfunc, "login password,[index [,derivation]]" },
    { "delete", deletefunc, "delete password,index" },
    { "send", sendfunc, "send password,index,dest,amount[,extrahex]" },
};

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
                if ( args[len] == ',' || args[len] == ';' || args[len] == '&' )
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

char *qwallet(char *_args)
{
    int32_t i,pendingid;
    static char retbuf[JSON_BUFSIZE],toggle;
    //if ( strcmp(_args,"v1request") != 0 )
    //    printf("qwallet(%s)\n",_args);
    if ( strcmp(_args,(char *)"help") == 0 )
    {
        retbuf[0] = 0;
        for (i=0; i<(sizeof(QCMDS)/sizeof(*QCMDS)); i++)
        {
            sprintf(retbuf+strlen(retbuf),"%s;",QCMDS[i].helpstr);
            printf("%s\n",QCMDS[i].helpstr);
        }
        return(wasm_result(0,retbuf,0));
    }
    else if ( strncmp(_args,(char *)"status",6) == 0 )
    {
#ifdef QUEUE_COMMANDS
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
#else
        return(wasm_result(pendingid,"command pending",0));
#endif
    }
    else if ( strncmp(_args,(char *)"v1request",9) == 0 )
    {
        char reqstr[512];
        if ( (rand() % 2) == 0 )
        {
            if ( LOGINADDRESSES[toggle] == 0 || (rand() % 2) == 0 )
                return(wasm_result(0,"status",0));
            sprintf(reqstr,"address/%s",LOGINADDRESSES[toggle]);
            toggle = (toggle + 1) % NUMADDRESSES;
            toggle %= (sizeof(LOGINADDRESSES)/sizeof(*LOGINADDRESSES));
            if ( LOGINADDRESSES[toggle][0] == 0 )
                toggle = 0;
            return(wasm_result(0,reqstr,0));
        }
        return(wasm_result(-13,(char *)"no v1requests available",0));
    }
    else if ( strncmp(_args,(char *)"v1status",8) == 0 )
    {
        statusupdate(&_args[9]);
        return(wasm_result(0,(char *)"thank you for status!",0));
    }
    else if ( strncmp(_args,(char *)"v1address",9) == 0 )
    {
        printf("%s\n",_args);
        return(wasm_result(0,(char *)"thank you for address!",0));
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
#ifdef QUEUE_COMMANDS
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
#else
    return(_qwallet(_args));
#endif
}

#ifdef EMSCRIPTEN

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
            MAIN_THREAD_EM_ASM(
                   FS.syncfs(function (err) {
                  assert(!err);
                });
            );
        }
#ifdef QUEUE_COMMANDS
        for (i=0; i<(sizeof(QWALLET_ARGS)/sizeof(*QWALLET_ARGS)); i++)
        {
            if ( QWALLET_ARGS[i][0] != 0 && QWALLET_RESULTS[i][0] == 0 )
            {
                printf("QWALLET_ARGS[%d] (%s)\n",i,QWALLET_ARGS[i]);
                char *retstr = _qwallet(QWALLET_ARGS[i]);
                printf("Q.%d returns.(%s)\n",i,retstr);
                strcpy(QWALLET_RESULTS[i],retstr);
             }
        }
#endif
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
