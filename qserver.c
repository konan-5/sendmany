    
/*#define MAX_INPUT_SIZE 1024
#ifdef TESTNET
#define DEFAULT_NODE_PORT 31841
#define DEFAULT_NODE_IP ((char *)"57.129.19.155") //193.135.9.63") //
#define TICKOFFSET 3
#else
#define DEFAULT_NODE_PORT 21841
#define DEFAULT_NODE_IP ((char *)"185.70.186.149")
#define TICKOFFSET 10
#endif*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include "utlist.h"

//#include "K12AndKeyUtil.h"

#include "qdefines.h"
#include "qstructs.h"
//#include "qconn.c"
//#include "qpeers.c"


const char *Peers[] =
{
#ifdef TESTNET
    "193.135.9.63"
#else
    "45.135.201.80",
    "31.214.243.32",
    "185.117.0.116",
    "88.198.25.67",
    "136.243.43.230",
    "31.214.243.25",
    "144.76.237.194",
    "136.243.36.28",
    "148.251.185.19",
    "5.199.134.150",
#endif
};

#define MAXPEERS 128
#define MAXADDRESSES 1000001

char Newpeers[64][16];
int32_t Numnewpeers,Numpeers,Numsubs,Numaddrs;
pthread_mutex_t addpeer_mutex;
CurrentTickInfo Peertickinfo[MAXPEERS];

struct subscriber
{
    uint8_t rawtx[MAX_INPUT_SIZE*2];
    uint64_t peersflag,key;
    int32_t respid,numpubkeys,errs,flag;
    uint8_t *pubkeys;
} *SUBS;

struct addrhash
{
    struct Entity entity;
    uint32_t createtime;
    int32_t tick;
} *Addresses;

struct addrhash *Addresshash(uint8_t pubkey[32],uint32_t utime)
{
    uint64_t hashi;
    int32_t i;
    struct addrhash *ap;
    hashi = *(uint64_t *)&pubkey[8] % MAXADDRESSES;
    for (i=0; i<MAXADDRESSES; i++)
    {
        ap = &Addresses[(hashi + i) % MAXADDRESSES];
        if ( memcmp(ap->entity.publicKey,pubkey,32) == 0 )
            return(ap);
        if ( ap->createtime == 0 )
        {
            if ( utime == 0 )
                return(0);
            memcpy(ap->entity.publicKey,pubkey,32);
            ap->createtime = utime;
            return(ap);
        }
    }
    printf("hash table full\n");
    return(0);
}

struct subscriber *Subscriberget(uint64_t key)
{
    int32_t i;
    struct subscriber *sp;
    for (i=0; i<Numsubs; i++)
        if ( SUBS[i].key == key )
            return(&SUBS[i]);
    SUBS = realloc(SUBS,sizeof(*SUBS) * (Numsubs+1));
    sp = &SUBS[Numsubs++];
    memset(sp,0,sizeof(*sp));
    sp->key = key;
    return(sp);
}

int32_t SubscriberAddr(struct subscriber *sp,struct addrhash *ap)
{
    int32_t i;
    for (i=0; i<sp->numpubkeys; i++)
        if ( memcmp(sp->pubkeys + i*32,ap->entity.publicKey,32) == 0 )
            return(0);
    sp->pubkeys = realloc(sp->pubkeys,32 * (sp->numpubkeys+1));
    memcpy(sp->pubkeys + sp->numpubkeys*32,ap->entity.publicKey,32);
    sp->numpubkeys++;
    return(1);
}

void addnewpeer(char *ipaddr)
{
    int32_t i,flag = 0;
    pthread_mutex_lock(&addpeer_mutex);
    for (i=0; i<(sizeof(Peers)/sizeof(*Peers)); i++)
        if ( strcmp(ipaddr,Peers[i]) == 0 )
        {
            flag = 1;
            break;
        }
    for (i=0; i<Numnewpeers; i++)
        if ( strcmp(ipaddr,Newpeers[i]) == 0 )
        {
            flag = 1;
            break;
        }
    if ( flag == 0 )
    {
        strcpy(Newpeers[i],ipaddr);
        //printf("Newpeer.%d %s\n",Numnewpeers,ipaddr);
        Numnewpeers++;
    }
    pthread_mutex_unlock(&addpeer_mutex);
}

void process_publicpeers(int32_t peerid,char *ipaddr,ExchangePublicPeers *peers)
{
    int32_t i,j;
    char peeraddr[64];
    for (i=0; i<4; i++)
    {
        peeraddr[0] = 0;
        for (j=0; j<4; j++)
            sprintf(peeraddr + strlen(peeraddr),"%d%c",peers->peers[i][j],j<3?'.':0);
        addnewpeer(peeraddr);
    }
}

void process_tickinfo(int32_t peerid,char *ipaddr,CurrentTickInfo *I)
{
    Peertickinfo[peerid] = *I;
    if ( I->tick > LATEST_TICK )
    {
        LATEST_TICK = I->tick;
        printf("%s epoch.%d tick.%d LATEST.%d lag.%d\n",ipaddr,I->epoch,I->tick,LATEST_TICK,LATEST_TICK - I->tick);
    }
}

void process_entity(int32_t peerid,char *ipaddr,RespondedEntity *E)
{
    struct addrhash *ap;
    if ( (ap= Addresshash(E->entity.publicKey,0)) != 0 )
    {
        char addr[64];
        pubkey2addr(E->entity.publicKey,addr);
        for (int j=0; j<32; j++)
            printf("%02x",E->entity.publicKey[j]);
        printf(" %s got entity %s tick.%d vs %d\n",ipaddr,addr,E->tick,ap->tick);
        if ( E->tick > ap->tick )
        {
            memcpy(&ap->entity,&E->entity,sizeof(ap->entity));
            ap->tick = E->tick;
        }
        // do merkle validation
    } else printf("unexpected entity data without address?\n");
    /*if ( respid != 0 )
    {
        M.mesg_type = 1;
        entityjson((char *)M.mesg_text,ipaddr,addr,E->entity,E->tick);
        msgsnd(respid,&M,strlen((char *)M.mesg_text)+1+sizeof(uint64_t),IPC_NOWAIT);
        printf("%s\n",(char *)M.mesg_text);
    }*/
}

void process_computors(int32_t peerid,char *ipaddr,Computors *computors)
{
    
}

void process_quorumdata(int32_t peerid,char *ipaddr,struct QuorumData *qdata)
{
    
}

void process_tickdata(int32_t peerid,char *ipaddr,TickData *tickdata)
{
    
}

void process_transaction(int32_t peerid,char *ipaddr,Transaction *txdata)
{
    
}

void process_issued(int32_t peerid,char *ipaddr,RespondIssuedAssets *issued)
{
    
}

void process_owned(int32_t peerid,char *ipaddr,RespondOwnedAssets *owned)
{
    
}

void process_possessed(int32_t peerid,char *ipaddr,RespondPossessedAssets *possessed)
{
    
}

int32_t process_response(int32_t peerid,char *ipaddr,struct quheader *H,void *data,int32_t datasize)
{
    switch ( H->_type )
    {
        case EXCHANGE_PUBLIC_PEERS:         if ( datasize != sizeof(ExchangePublicPeers) ) return(-1);
            process_publicpeers(peerid,ipaddr,(ExchangePublicPeers *)data);
            break;
        case BROADCAST_COMPUTORS:           if ( datasize != sizeof(Computors) ) return(-1);
            process_computors(peerid,ipaddr,(Computors *)data);
            break;
      case BROADCAST_TICK:                if ( datasize != sizeof(struct QuorumData) ) return(-1);
            process_quorumdata(peerid,ipaddr,(struct QuorumData *)data);
            break;
       case BROADCAST_FUTURE_TICK_DATA:    if ( datasize != sizeof(TickData) ) return(-1);
            process_tickdata(peerid,ipaddr,(TickData *)data);
            break;
       case BROADCAST_TRANSACTION:         if ( datasize < sizeof(Transaction) ) return(-1);
            process_transaction(peerid,ipaddr,(Transaction *)data);
            break;
       case RESPOND_CURRENT_TICK_INFO:     if ( datasize != sizeof(CurrentTickInfo) ) return(-1);
            process_tickinfo(peerid,ipaddr,(CurrentTickInfo *)data);
            break;
        case RESPOND_ENTITY:                if ( datasize != sizeof(RespondedEntity) ) return(-1);
            process_entity(peerid,ipaddr,(RespondedEntity *)data);
            break;
        case RESPOND_ISSUED_ASSETS:         if ( datasize != sizeof(RespondIssuedAssets) ) return(-1);
            process_issued(peerid,ipaddr,(RespondIssuedAssets *)data);
            break;
        case RESPOND_OWNED_ASSETS:          if ( datasize != sizeof(RespondOwnedAssets) ) return(-1);
            process_owned(peerid,ipaddr,(RespondOwnedAssets *)data);
            break;
        case RESPOND_POSSESSED_ASSETS:      if ( datasize != sizeof(RespondPossessedAssets) ) return(-1);
            process_possessed(peerid,ipaddr,(RespondPossessedAssets *)data);
            break;
        //case PROCESS_SPECIAL_COMMAND:       if ( datasize != sizeof() ) return(-1);
        default: printf("%s unknown type.%d sz.%d\n",ipaddr,H->_type,datasize);
            break;
    }
    //printf("peerid.%d got %d cmd.%d from %s\n",peerid,datasize,H->_type,ipaddr);
    return(0);
}

void *peerthread(void *_ipaddr)
{
    char *ipaddr = _ipaddr;
    struct EntityRequest E;
    int32_t peerid=0,sock=-1,i,hashi,lasthashi,ptr,sz,recvbyte,prevutime = 0,prevtick = 0,iter = 0;
    struct quheader H;
    struct addrhash *ap;
    uint8_t buf[4096];
    while ( iter++ < 10 )
    {
        if ( (sock= myconnect(ipaddr,DEFAULT_NODE_PORT)) < 0 )
            printf("iter.%d peerthread error connecting to %s\n",iter,ipaddr);
        else break;
    }
    if ( iter >= 10 )
        return(0);
    Numpeers++;
    peerid = Numpeers;
    lasthashi = peerid * (MAXADDRESSES / 64);
    printf("connected.%d peerthread %s\n",Numpeers,ipaddr);
    H = quheaderset(REQUEST_CURRENT_TICK_INFO,sizeof(H));
    sock = socksend(ipaddr,sock,(uint8_t *)&H,sizeof(H));
    while ( 1 )
    {
        if ( (recvbyte= receiveall(sock,buf,sizeof(buf))) > 0 )
        {
            ptr = 0;
            sz = 1;
            while ( ptr < recvbyte && sz != 0 && sz < recvbyte )
            {
                memcpy(&H,&buf[ptr],sizeof(H));
                sz = ((H._size[2] << 16) + (H._size[1] << 8) + H._size[0]);
                if ( H._type == RESPOND_ENTITY )
                {
                    //for (int j=0; j<sz; j++)
                    //    printf("%02x",buf[ptr+j]);
                    //printf(" %s received %d H.(%d %d bytes)\n",ipaddr,recvbyte,H._type,sz);
                }
                if ( sz < 1 || sz > recvbyte-ptr )
                {
                    //printf("illegal sz.%d vs recv.%d ptr.%d\n",sz,recvbyte,ptr);
                    break;
                }
                if ( process_response(peerid,ipaddr,&H,&buf[ptr + sizeof(H)],sz - sizeof(H)) < 0 )
                {
                    //printf("peerid.%d Error processing H.type %d size.%ld\n",peerid,H._type,sz - sizeof(H));
                }
                ptr += sz;
            }
        }
        if ( LATEST_UTIME > prevutime )
        {
            prevutime = LATEST_UTIME;
            H = quheaderset(REQUEST_CURRENT_TICK_INFO,sizeof(H));
            sock = socksend(ipaddr,sock,(uint8_t *)&H,sizeof(H));
        }
        if ( LATEST_TICK > prevtick )
        {
            prevtick = LATEST_TICK;
            if ( Peertickinfo[peerid].tick < LATEST_TICK - 1000 )
            {
                //printf("peerid.%d latest.%d lag.%d, skip entity request\n",peerid,Peertickinfo[peerid].tick,LATEST_TICK-Peertickinfo[peerid].tick);
            }
            else
            {
                //printf("peerid.%d lasthashi.%d\n",peerid,lasthashi);
                for (hashi=i=0; i<8192; i++)
                {
                    hashi = (lasthashi + i) % MAXADDRESSES;
                    ap = &Addresses[hashi];
                    if ( ap->createtime == 0 || ap->tick >= Peertickinfo[peerid].tick )
                        continue;
                    printf("peerid.%d >>>>>>>>>>>>> found address at %d\n",peerid,hashi);
                    memset(&E,0,sizeof(E));
                    E.H = quheaderset(REQUEST_ENTITY,sizeof(E));
                    memcpy(E.pubkey,ap->entity.publicKey,sizeof(E.pubkey));
                    sock = socksend(ipaddr,sock,(uint8_t *)&E,sizeof(E));
                }
                lasthashi = hashi;
            }
        }
    }
    return(0);
}

void *findpeers(void *args)
{
    pthread_t peer_threads[sizeof(Peers)/sizeof(*Peers)];
    pthread_t newpeer_threads[sizeof(Newpeers)/sizeof(*Newpeers)];
    int32_t i,num;
    for (i=0; i<(sizeof(Peers)/sizeof(*Peers)); i++)
        pthread_create(&peer_threads[i],NULL,&peerthread,(void *)Peers[i]);
    while ( Numnewpeers < (sizeof(Newpeers)/sizeof(*Newpeers)) )
    {
        num = Numnewpeers;
        while ( Numnewpeers == num )
            sleep(1);
        for (i=num; i<Numnewpeers && i<(sizeof(Newpeers)/sizeof(*Newpeers)); i++)
            pthread_create(&newpeer_threads[i],NULL,&peerthread,(void *)Newpeers[i]);
    }
    printf("find peers thread finished\n");
    return(0);
}

#define MSG_COPY        040000


void entityjson(char *str,char *addr,struct Entity E,int32_t tick)
{
    sprintf(str,"{\"address\":\"%s\",\"balance\":\"%s\",\"tick\":%d,\"numin\":%d,\"totalincoming\":\"%s\",\"latestin\":%d,\"numout\":%d,\"totaloutgoing\":\"%s\",\"latestout\":%d}",addr,amountstr(E.incomingAmount - E.outgoingAmount),tick,E.numberOfIncomingTransfers,amountstr2(E.incomingAmount),E.latestIncomingTransferTick,E.numberOfOutgoingTransfers,amountstr3(E.outgoingAmount),E.latestOutgoingTransferTick);
}

//int main(int argc, const char * argv[])
void qserver(void)
{
    pthread_t findpeers_thread;
    key_t key,respkey;
    char addr[64];
    uint8_t pubkey[32],*ptr;
    int32_t i,j,msgid,sz,hsz;
    uint32_t utime;
    int32_t year,month,day,seconds,latest;
    struct qbuffer M,S,C;
    struct quheader H;
    struct subscriber *sp;
    struct addrhash *ap;
    devurandom((uint8_t *)&utime,sizeof(utime));
    srand(utime);
    signal(SIGPIPE, SIG_IGN);
    makefile(QUBIC_MSGPATH);
    key = ftok(QUBIC_MSGPATH, 'Q');
    msgid = msgget(key, 0666 | IPC_CREAT);
    Addresses = (struct addrhash *)calloc(MAXADDRESSES,sizeof(*Addresses));
    printf("key %ld msgid %d\n",(long)key,msgid);
    pthread_mutex_init(&addpeer_mutex,NULL);
    pthread_create(&findpeers_thread,NULL,&findpeers,0);

    utime = 0;
    latest = 0;
    while ( 1 )
    {
        memset(&M,0,sizeof(M));
        sz = (int32_t)msgrcv(msgid,&M,sizeof(M),0,IPC_NOWAIT);
        //printf("sz.%d sizeH %ld, sz < 0 %d, sz > 8 %d\n",sz,sizeof(H),sz < 0,sz > sizeof(H));
        if ( sz >= (int32_t)sizeof(H) )
        {
            sp = 0;
            memcpy(&H,M.mesg_text,sizeof(H));
            hsz = ((H._size[2] << 16) + (H._size[1] << 8) + H._size[0]);
            if ( hsz == (sz - sizeof(uint64_t)) || hsz == (sz - 2*sizeof(uint64_t)) )
            {
                if ( H._type == REQUEST_ENTITY && hsz == sizeof(H)+sizeof(pubkey) )
                {
                    ptr = &M.mesg_text[sizeof(H)];
                    memcpy(pubkey,ptr,sizeof(pubkey));
                    ptr += sizeof(pubkey);
                    pubkey2addr(pubkey,addr);
                    if ( hsz == (sz - 2*sizeof(uint64_t)) )
                    {
                        memcpy(&respkey,ptr,sizeof(respkey));
                        ptr += sizeof(respkey);
                        if ( (sp= Subscriberget(respkey)) != 0 )
                        {
                            if ( (sp->respid= msgget(sp->key,0666 | IPC_CREAT)) != 0 )
                            {
                                printf("%s %s to respid.%d key %s\n",M.mesg_type == 2 ? "subscribe":"",addr,sp->respid,amountstr(sp->key));
                                ap = Addresshash(pubkey,utime);
                                SubscriberAddr(sp,ap);
                            }
                        }
                    }
                    printf("%s %s\n",addr,sp!=0?"monitor":"");
                }
                else
                {
                    printf("add to peerQs type.%d hsz.%d\n",H._type,hsz);
                    //addpeerqueue(H._type,(uint8_t *)&M.mesg_text[sizeof(H)],hsz - sizeof(H));
                    //addpeerqueue(H._type,(uint8_t *)&M.mesg_text[sizeof(H)],hsz - sizeof(H));
                    //addpeerqueue(H._type,(uint8_t *)&M.mesg_text[sizeof(H)],hsz - sizeof(H));
                }
                printf(" H.type %d, sz.%d, type.%ld RECV (hsz.%d %u)\n",H._type,sz,(long)M.mesg_type,hsz,sp->respid);
                if ( sp != 0 && sp->respid != 0 )
                {
                    if ( (sz= (int32_t)msgrcv(sp->respid,&C,sizeof(C),1,MSG_COPY | IPC_NOWAIT)) > 0 )
                    {
                        printf("respid.%d still has message %d\n",sp->respid,sz);
                    }
                    else
                    {
                        S.mesg_type = 1;
                        sprintf((char *)S.mesg_text,"qserver got your message of (%d) %s %d %s",hsz,M.mesg_type == 2 ? "subscribe":"",sp->respid,amountstr(sp->key));
                        msgsnd(sp->respid,&S,strlen((char *)S.mesg_text)+1,IPC_NOWAIT);
                    }
                }
            }
        }
        else usleep(10000);
        utime = set_current_ymd(&year,&month,&day,&seconds);
        if ( utime > LATEST_UTIME )
            LATEST_UTIME = utime;
        if ( LATEST_TICK > latest )
        {
            latest = LATEST_TICK;
            printf("update subscribers with %d\n",latest);
            for (i=0; i<Numsubs; i++)
            {
                sp = &SUBS[i];
                S.mesg_type = 1;
                sprintf((char *)S.mesg_text,"{\"tick\":%d,\"wasm\":1}",latest);
                if ( sp->respid != 0 )
                {
                    if ( (sz= (int32_t)msgrcv(sp->respid,&C,sizeof(C),1,MSG_COPY | IPC_NOWAIT)) > 0 )
                    {
                        sp->errs++;
                        printf("TSUBS[%d][0] respid.%d still has message %d errs.%d\n",i,sp->respid,sz,sp->errs);
                        // increase counter and close channel if too many errors
                    }
                    else
                    {
                        msgsnd(sp->respid,&S,strlen((char *)S.mesg_text)+1,IPC_NOWAIT);
                        for (j=0; j<sp->numpubkeys; j++)
                        {
                            if ( (ap= Addresshash(&sp->pubkeys[j * 32],0)) != 0 )
                            {
                                if ( ap->tick >= LATEST_TICK-1000 )
                                {
                                    pubkey2addr(ap->entity.publicKey,addr);
                                    entityjson((char *)S.mesg_text,addr,ap->entity,ap->tick);
                                    msgsnd(sp->respid,&S,strlen((char *)S.mesg_text)+1+sizeof(uint64_t),IPC_NOWAIT);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    msgctl(msgid,IPC_RMID,NULL);
}


    
