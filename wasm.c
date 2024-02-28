    


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
    char fname[512];
    int32_t i,retval = -1;
    KangarooTwelve((uint8_t *)password,(int32_t)strlen(password),salt,32);
    sprintf(fname,"%cqwallet%c%s",dir_delim(),dir_delim(),password);
    //sprintf(fname,"%s",password);
    //printf("check (%s) %s\n",fname,rw);
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
        printf("found encrypted file for (%s) -> %s\n",password,addr);
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
};

char *qwallet(char *_args)
{
    int32_t i,pendingid;
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
            memset(QWALLET_ARGS[pendingid],0,sizeof(QWALLET_ARGS[pendingid]));
            return(QWALLET_RESULTS[pendingid]);
        }
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
static EMSCRIPTEN_WEBSOCKET_T bridgeSocket = 0;

EM_JS(void, start_timer, (),
    {
        Module.timer = false;
        setTimeout(function() { Module.timer = true; }, 500);
    }
);
EM_JS(bool, check_timer, (), { return Module.timer; });

EM_BOOL WebSocketOpen(int eventType, const EmscriptenWebSocketOpenEvent *e, void *userData)
{
    printf("open(eventType=%d, userData=%ld)\n", eventType, (long)userData);

    emscripten_websocket_send_utf8_text(e->socket, "hello on the other side");

    char data[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
    emscripten_websocket_send_binary(e->socket, data, sizeof(data));

    emscripten_websocket_close(e->socket, 0, 0);
    return 0;
}

EM_BOOL WebSocketClose(int eventType, const EmscriptenWebSocketCloseEvent *e, void *userData)
{
    printf("close(eventType=%d, wasClean=%d, code=%d, reason=%s, userData=%ld)\n", eventType, e->wasClean, e->code, e->reason, (long)userData);
    return 0;
}

EM_BOOL WebSocketError(int eventType, const EmscriptenWebSocketErrorEvent *e, void *userData)
{
    printf("error(eventType=%d, userData=%ld)\n", eventType, (long)userData);
    return 0;
}

EM_BOOL WebSocketMessage(int eventType, const EmscriptenWebSocketMessageEvent *e, void *userData)
{
    printf("message(eventType=%d, userData=%ld, data=%p, numBytes=%d, isText=%d)\n", eventType, (long)userData, e->data, e->numBytes, e->isText);
    if (e->isText)
        printf("text data: \"%s\"\n", e->data);
    else
    {
        printf("binary data:");
        for(int i = 0; i < e->numBytes; ++i)
            printf(" %02X", e->data[i]);
        printf("\n");

        emscripten_websocket_delete(e->socket);
        exit(0);
    }
    return 0;
}

/*typedef struct {
    Uint32 host;
    Uint16 port;
} IPaddress;*/

void *mainloop(void *arg)
{
    /*IPaddress ip;
    if(SDLNet_ResolveHost(&ip, NULL, 8099) == -1) {
        fprintf(stderr, "ER: SDLNet_ResolveHost: %d\n", SDLNet_GetError());
        exit(-1);
    }
     
    int32_t server_socket = SDLNet_TCP_Open(&ip);
    if(server_socket == NULL) {
        fprintf(stderr, "ER: SDLNet_TCP_Open: %d\n", SDLNet_GetError());
        exit(-1);
    }*/
    
    /*emscripten_fetch_attr_t attr;
    emscripten_fetch_attr_init(&attr);
    strcpy(attr.requestMethod, "GET");
    attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY | EMSCRIPTEN_FETCH_SYNCHRONOUS;
    emscripten_fetch_t *fetch = emscripten_fetch(&attr, "file.dat"); // Blocks here until the operation is complete.
    if (fetch->status == 200) {
      printf("Finished downloading %llu bytes from URL %s.\n", fetch->numBytes, fetch->url);
      // The data is now available at fetch->data[0] through fetch->data[fetch->numBytes-1];
    } else {
      printf("Downloading %s failed, HTTP failure status code: %d.\n", fetch->url, fetch->status);
    }
    emscripten_fetch_close(fetch);*/
    /*
    if (!emscripten_websocket_is_supported())
    {
        printf("WebSockets are not supported, cannot continue!\n");
        //exit(1);
    }
    printf("step1\n");

    EmscriptenWebSocketCreateAttributes attr;
    emscripten_websocket_init_create_attributes(&attr);

    attr.url = "ws://146.59.150.157:22841";

    EMSCRIPTEN_WEBSOCKET_T socket = emscripten_websocket_new(&attr);
    if (socket <= 0)
    {
        printf("WebSocket creation failed, error code %d!\n", (EMSCRIPTEN_RESULT)socket);
        exit(1);
    }
    printf("step2 sock.%d\n",socket);

    emscripten_websocket_set_onopen_callback(socket, (void*)42, WebSocketOpen);
    emscripten_websocket_set_onclose_callback(socket, (void*)43, WebSocketClose);
    emscripten_websocket_set_onerror_callback(socket, (void*)44, WebSocketError);
    emscripten_websocket_set_onmessage_callback(socket, (void*)45, WebSocketMessage);*/
    return(0);
}

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
    pthread_t mainloop_thread;
    pthread_create(&mainloop_thread,NULL,&mainloop,0);
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

    
