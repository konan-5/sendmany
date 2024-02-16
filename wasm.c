    
// emcc -O2 -sFETCH -lwebsocket.js -sPROXY_POSIX_SOCKETS -pthread -sPROXY_TO_PTHREAD -s PTHREAD_POOL_SIZE=8 -s INITIAL_MEMORY=33554432 -s WASM_ASYNC_COMPILATION=0 -s SINGLE_FILE=1 -s -D_LARGEFILE64_SOURCE=1 -fPIC -Wno-implicit-function-declaration -msse2 -msse3 -msse4.1 -msimd128 -msse4.2 -mavx  wasm.c
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
#include "K12AndKeyUtil.h"

typedef struct
{
    unsigned char sourcePublicKey[32];
    unsigned char destinationPublicKey[32];
    long long amount;
    unsigned int tick;
    unsigned short inputType;
    unsigned short inputSize;
} Transaction;


void devurandom(uint8_t *buf,long len)
{
    static int32_t fd = -1;
    int32_t i;
    if ( fd == -1 )
    {
        while ( 1 )
        {
            if ( (fd= open("/dev/urandom",O_RDONLY)) != -1 )
                break;
            sleep(1);
        }
    }
    while ( len > 0 )
    {
        if ( len < 1048576 )
            i = (int32_t)len;
        else i = 1048576;
        i = (int32_t)read(fd,buf,i);
        if ( i < 1 )
        {
            sleep(1);
            continue;
        }
        buf += i;
        len -= i;
    }
}

void create_rawtxhex(char *rawhex,char *txhash,uint8_t digest[32],const uint8_t subseed[32],int32_t type,const uint8_t srcpub[32],const uint8_t destpub[32],int64_t total,const uint8_t *extradata,int32_t extrasize,int32_t txtick)
{
    struct
    {
        Transaction transaction;
        unsigned char extradata[MAX_INPUT_SIZE];
        unsigned char signature[64];
    } packet;
    uint8_t signature[64];
    memcpy(packet.transaction.sourcePublicKey,srcpub,32);
    memcpy(packet.transaction.destinationPublicKey,destpub,32);
    packet.transaction.amount = total;
    packet.transaction.tick = txtick;
    packet.transaction.inputType = type;
    packet.transaction.inputSize = extrasize;
    if ( extrasize > 0 )
        memcpy(packet.extradata,extradata,extrasize);
    KangarooTwelve((unsigned char *)&packet.transaction,sizeof(packet.transaction) + extrasize,digest,32);
    sign(subseed,srcpub,digest,signature);
    memcpy(&packet.extradata[extrasize],signature,64);
    KangarooTwelve((unsigned char *)&packet.transaction,sizeof(packet.transaction) + extrasize + 64,digest,32); // recompute for txhash
    getTxHashFromDigest(digest,txhash);
    memset(rawhex,0,(sizeof(packet.transaction) + extrasize + 64)*2 + 1);
    byteToHex((uint8_t *)&packet.transaction,rawhex,sizeof(packet.transaction) + extrasize + 64);
}

void create_rawtransaction(char *rawhex,char *txhash,uint8_t digest[32],const uint8_t *subseed,const char *targetIdentity,const uint64_t amount,uint32_t scheduledTick,char *datastr)
{
    uint8_t privateKey[32],sourcePublicKey[32],destPublicKey[32],extradata[MAX_INPUT_SIZE];
    int32_t extradatasize = 0;
    char addr[64];
    getPrivateKeyFromSubSeed(subseed,privateKey);
    getPublicKeyFromPrivateKey(privateKey,sourcePublicKey);
    getPublicKeyFromIdentity(targetIdentity, destPublicKey);
    if ( datastr != 0 && (extradatasize= (int32_t)strlen(datastr)) >= 2 && extradatasize <= MAX_INPUT_SIZE*2 )
    {
        extradatasize >>= 1;
        hexToByte(datastr,extradata,extradatasize);
        //printf("extradatasize.%d (%s)\n",extradatasize,datastr);
    }
    create_rawtxhex(rawhex,txhash,digest,subseed,0,sourcePublicKey,destPublicKey,amount,extradata,extradatasize,scheduledTick);
    txhash[60] = 0;
    pubkey2addr(sourcePublicKey,addr);
    printf("%s %ld -> %s\n%s\n%s\n\n",addr,(long)amount,targetIdentity,txhash,rawhex);
}

int32_t checktxsig(char *addr,char *rawhex)
{
    uint8_t txbytes[MAX_INPUT_SIZE*2],digest[32],pubkey[32];
    int32_t datalen = (int32_t)strlen(rawhex) / 2;
    hexToByte(rawhex,txbytes,datalen);
    KangarooTwelve(txbytes,datalen-64,digest,32);
    getPublicKeyFromIdentity(addr,pubkey);
    int v = verify(pubkey,digest,&txbytes[datalen-64]);
    printf("checksig %d\n",v);
    return(v);
}

int seed2addr(char *seed,char *addr)
{
    uint8_t digest[32],subseed[32],privatekey[32],publickey[32];
    deriveaddr(seed,(char *)"",subseed,privatekey,publickey,addr);
    return(0);
}

int int_sqrt(int x)
{
  return sqrt(x);
}


int main(int argc, const char * argv[])
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
 }

    
