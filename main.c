//
//  main.c
//  sendmany

//#define TESTNET
//#define RECLAIMDEST // reclaims funds in dest.yyy test addresses

#ifdef TESTNET
#define DEFAULT_NODE_PORT 31841
#define DEFAULT_NODE_IP ((char *)"57.129.19.155") //193.135.9.63") //
#define TICKOFFSET 3
#else
#define DEFAULT_NODE_PORT 21841
#define DEFAULT_NODE_IP ((char *)"148.251.185.19")
#define TICKOFFSET 3
#endif

// queue sendrawtx for recvloop
// save entity data + spectrum hash for cryptographic proof of payment, along with merkle proving

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <pthread.h>
#include <signal.h>
#include "K12AndKeyUtil.h"

#include "qdefines.h"
#include "qstructs.h"
#include "qhelpers.c"
int32_t LATEST_TICK = 1;    // start at nonzero value to trigger initial requests
pthread_mutex_t txq_mutex,txq_sendmutex,conn_mutex;

// C code "linked" in by #include
#include "qtime.c"
#include "qconn.c"
#include "qcmds.c"
#include "qtx.c"
#include "qthreads.c"
#include "qpeers.c"
#include "qfanout.c"

void *sendtx(void *_rawhex)
{
    sendrawtransaction(randpeer(0),DEFAULT_NODE_PORT,(char *)_rawhex);
    return(0);
}

int64_t reclaimbalance(int64_t *totalp,char *iFAN,char *addr,uint8_t subseed[32],char *dest,int32_t starttick)
{
    uint8_t pubkey[32],digest[32];
    char rawhex[4096],txhash[64];
    int64_t balance;
    //pthread_t sendt;
    getPublicKeyFromIdentity(addr,pubkey);
    balancetickhash(pubkey,starttick);
    if ( (balance= waitforbalance(pubkey,starttick)) != 0 )
    {
        (*totalp) += balance;
        create_rawtransaction(rawhex,txhash,digest,subseed,dest,balance,0,"");
        printf("reclaim %s %s %s %s\n",iFAN,addr,amountstr(balance),rawhex);
        //pthread_create(&sendt,NULL,&sendtx,rawhex);
        sendrawtransaction(randpeer(0),DEFAULT_NODE_PORT,rawhex);
    }
    return(balance != 0);
}

int reclaim(char *origseed,int32_t numaddrs,int32_t starttick)
{
    uint8_t subseed[32],pubkey[32];
    int32_t i,nonz = 0;
    int64_t reclaimed = 0;
    char origaddr[64],addr[64],iFAN[6];
#ifdef RECLAIMDEST
    char destiFAN[64];
#endif
    fanout_subseed(origseed,(char *)"",subseed,origaddr);
    //printf("%s -> %s\n",origseed,origaddr);
    fanout_subseed(origseed,(char *)"Z",subseed,addr);
    //if ( reclaimbalance(&reclaimed,(char *)"Z",addr,subseed,origaddr,starttick) != 0 )
    //    nonz++;
    if ( numaddrs > 0 )
    {
        for (i=0; i<25; i++)
        {
            calc_iFAN(iFAN,i,25);
            fanout_subseed(origseed,iFAN,subseed,addr);
            getPublicKeyFromIdentity(addr,pubkey);
            balancetickhash(pubkey,starttick);
        }
        for (i=0; i<25*25; i++)
        {
            if ( i/FAN >= numaddrs )
                break;
            calc_iFAN(iFAN,i,25*25);
            fanout_subseed(origseed,iFAN,subseed,addr);
            getPublicKeyFromIdentity(addr,pubkey);
            balancetickhash(pubkey,starttick);
        }
#ifdef RECLAIMDEST
        for (i=0; i<25*25*25; i++)
        {
            if ( i >= numaddrs )
                break;
            calc_iFAN(iFAN,i,25*25*25);
            sprintf(destiFAN,"dest.%s",iFAN);
            fanout_subseed(origseed,destiFAN,subseed,addr);
            getPublicKeyFromIdentity(addr,pubkey);
            balancetickhash(pubkey,starttick);
        }
#endif
        printf("add to hashtable\n");
        for (i=0; i<25; i++)
        {
            calc_iFAN(iFAN,i,25);
            fprintf(stderr,"%s ",iFAN);
            fanout_subseed(origseed,iFAN,subseed,addr);
            if ( reclaimbalance(&reclaimed,iFAN,addr,subseed,origaddr,starttick) != 0 )
                nonz++;
        }
        printf("level 1\n");
        for (i=0; i<25*25; i++)
        {
            if ( i/FAN >= numaddrs )
                break;
            calc_iFAN(iFAN,i,25*25);
            fprintf(stderr,"%s ",iFAN);
            fanout_subseed(origseed,iFAN,subseed,addr);
            if ( reclaimbalance(&reclaimed,iFAN,addr,subseed,origaddr,starttick) != 0 )
                nonz++;
        }
#ifdef RECLAIMDEST
        for (i=0; i<25*25*25; i++)
        {
            if ( i >= numaddrs )
                break;
            calc_iFAN(iFAN,i,25*25*25);
            sprintf(destiFAN,"dest.%s",iFAN);
            fprintf(stderr,"%s ",destiFAN);
            fanout_subseed(origseed,destiFAN,subseed,addr);
            if ( reclaimbalance(&reclaimed,destiFAN,addr,subseed,origaddr,starttick) != 0 )
                nonz++;
        }
#endif
        printf("level 2\n");
    }
    printf("issued reclaims from %d addrs for %s\n",nonz,amountstr(reclaimed));
    return(nonz);
}

int sendmany(char *origseed,char *fname)
{
    int32_t i,n,starttick,endtick,numerrors = 0;
    int64_t paid = 0;
    char dest[64];
    struct Fanout *fan;
    CurrentTickInfo cur;
    TXQ_PAUSE = 1;
    cur = getTickInfoFromNode(randpeer(0),DEFAULT_NODE_PORT);
    printf("T.%ld dur.%d epoch.%d tick.%d aligned.%d misaligned.%d initialTick.%d\n",sizeof(Transaction),cur.tickDuration,cur.epoch,cur.tick,cur.numberOfAlignedVotes,cur.numberOfMisalignedVotes,cur.initialTick);

    while ( (starttick= getlatest()) == 0 )
    {
        printf("waiting for starttick\n");
        sleep(3);
    }
    if ( (fan= fanout_create(origseed,fname)) != 0 )
    {
        reclaim(origseed,fan->numdests,starttick);
        fanout_send(fan);
        printf("\nTOTAL REQUIRED FOR SENDMANY INCLUDING FEES: %s\nAll data logged in %s\n",amountstr(fan->total),fan->txidsdir);
        TXQ_PAUSE = 0;
        printf("numdests.%d depth.%d total %s sum1 %s, sum2 %s, sum3 %s\n",fan->numdests,fan->depth,amountstr(fan->total),amountstr2(fan->depth1sum),amountstr3(fan->depth2sum),amountstr4(fan->depth3sum));
        while ( (n= txq_queue_numactive()) > 0 )
        {
            if ( (rand() % 20) == 0 )
                printf("num remaining in TXQ.%d\n",n);
            sleep(1);
        }
    }
    else
        printf("error creating fanout from %s\n",fname);
    endtick = getlatest();
    printf("elapsed %d ticks, generating report\n",endtick - starttick);
    if ( Paymentamounts[0] != 0 )
    {
        for (i=0; i<fan->numdests; i++)
            balancetickhash(Paymentpubkeys[i],1);
        for (i=0; i<fan->numdests; i++)
            Endingbalances[i] = waitforbalance(Paymentpubkeys[i],endtick);
        for (i=0; i<fan->numdests; i++)
        {
            pubkey2addr(Paymentpubkeys[i],dest);
            if ( Paymentamounts[i] - (Endingbalances[i] - Startingbalances[i]) != 0 )
            {
                printf("paid %s: %s error %s\n",dest,amountstr(Endingbalances[i] - Startingbalances[i]),amountstr2(Paymentamounts[i] - (Endingbalances[i] - Startingbalances[i])));
                numerrors++;
            }
            else paid += (Endingbalances[i] - Startingbalances[i]);
        }
    }
    reclaim(origseed,fan->numdests,LATEST_TICK);
    endtick = getlatest();
    printf("total elapsed %d ticks, numerror.%d paid %s totalcost %s numsendmany.%d numpaid.%d\n",endtick - starttick,numerrors,amountstr(paid),amountstr2(fan->total),txq_Numtx,fan->numdests);
    free(fan);
    return(0);
}

void testsend(char *seed55,char *destaddr,int64_t amount)
{
    char rawhex[4096],txidstr[64],senderaddr[64],iFAN[6];
    uint8_t txdigest[32],subseed[32];
    create_rawtransaction(rawhex,txidstr,txdigest,(uint8_t *)seed55,destaddr,amount,0,"");
    fanout_subseed(seed55,iFAN,subseed,senderaddr);
    printf("created transaction send from %s to %s %ld, %s rawhex.%s\n",senderaddr,destaddr,(long)amount,txidstr,rawhex);
    //sendrawtransaction(randpeer(0,0),DEFAULT_NODE_PORT,rawhex);
}

int main(int argc, const char * argv[])
{
    FILE *fp;
    int32_t i,len;
    char *origseed,*argstr,line[512];
    uint64_t tmp;
    uint32_t seed;
    signal(SIGPIPE, SIG_IGN);
    txq_HASHSIZE = MAXFANS * 3 + 1;
    Balances = (struct balancetick *)calloc(txq_HASHSIZE,sizeof(*Balances));
    devurandom((uint8_t *)&seed,sizeof(seed));
    srand(seed);
    pthread_mutex_init(&conn_mutex,NULL);
    pthread_mutex_init(&txq_mutex,NULL);
    pthread_mutex_init(&txq_sendmutex,NULL);
#ifdef TESTNET
    pthread_t txq_recvloop_thread;
    uint8_t ipbytes[4];
    ipaddr2ipbytes(DEFAULT_NODE_IP,ipbytes);
    pthread_create(&txq_recvloop_thread,NULL,&txq_recvloop,ipbytes);
#else
    pthread_t initpeers_thread;
    pthread_create(&initpeers_thread,NULL,&initpeers,0);
#endif
    pthread_t txq_loop_thread;
    pthread_create(&txq_loop_thread,NULL,&txq_loop,0);
    if ( argc == 3 )
    {
        origseed = (char *)argv[1];
        argstr = (char *)argv[2];
    }
    else if ( argc == 2 )
    {
        origseed = (char *)"seed";
        argstr = (char *)argv[1];
    }
    else
    {
        origseed = (char *)"seed";
        argstr = "0";
        printf("usage:\n%s <seed> <csvname>\n%s <csvname>\nseed can be actual 55 char seed or filename with seed\ncsvname can be filename of payments CSV or a number for N test payments, using 0 for N will generate random N above 10000\nIf you use the name for seed of 'autogen', a random seed will automatically be generated and used.",argv[0],argv[0]);
        //return(0);
    }
    if ( strcmp(origseed,"autogen") == 0 )
    {
        for (i=0; i<55; i++)
        {
            devurandom((uint8_t *)&tmp,sizeof(tmp));    // slight bias for (2**64-1) % 26 and below
            line[i] = (tmp % 26) + 'a';
        }
        line[55] = 0;
        origseed = line;
        printf("autogenerated seed will be stored in file called seed in sendmany/addr.tick dir\n");
    }
    else if ( strlen(origseed) != 55 )
    {
        printf("use (%s) as seed filename\n",origseed);
        if ( (fp= fopen(origseed,"r")) != 0 )
        {
            if ( fgets(line,sizeof(line),fp) != 0 )
            {
                len = (int32_t)strlen(line);
                if ( len == 56 )
                {
                    line[55] = 0;
                    len = 55;
                }
                if ( len != 55 )
                {
                    printf("invalid seed len.%d\n",len);
                    return(-1);
                }
                origseed = line;
            }
            fclose(fp);
        }
    }
    sendmany(origseed,argstr);

    return(0);
}

