
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

char Newpeers[64][16];
int32_t Numnewpeers,Numpeers;//,Numesubs,NumTsubs;
pthread_mutex_t addpeer_mutex;
CurrentTickInfo Peertickinfo[MAXPEERS];
struct qrequest *REQS[MAXPEERS];

struct esub
{
    uint8_t pubkey[32];
    uint32_t respid,onetimeflag;
} ESUBS[1024];

uint32_t TSUBS[1024][2];

void Tsubscriberadd(uint32_t respid)
{
    int32_t i;
    if ( respid == 0 )
        return;
    for (i=0; i<1024; i++)
    {
        if ( TSUBS[i][0] == respid )
            return;
        if ( TSUBS[i][0] == 0 )
        {
            TSUBS[i][0] = respid;
            break;
        }
    }
}

int32_t pubkeyrespid(uint8_t pubkey[32])
{
    int32_t i;
    for (i=0; i<1024; i++)
    {
        if ( memcmp(ESUBS[i].pubkey,pubkey,sizeof(ESUBS[i].pubkey)) == 0 )
            return(ESUBS[i].respid);
    }
    return(0);
}

void entitysubscription(int32_t onetimeflag,uint32_t respid,uint8_t pubkey[32])
{
    int32_t i;
    for (i=0; i<1024; i++)
    {
        if ( memcmp(ESUBS[i].pubkey,pubkey,sizeof(ESUBS[i].pubkey)) == 0  && ESUBS[i].respid == respid && ESUBS[i].onetimeflag == onetimeflag )
            return;
        if ( ESUBS[i].respid == 0 )
        {
            memcpy(ESUBS[i].pubkey,pubkey,sizeof(ESUBS[i].pubkey));
            ESUBS[i].respid = respid;
            ESUBS[i].onetimeflag = onetimeflag;
            break;
        }
    }
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

struct qrequest *qrequest_poll(int32_t peerid)
{
    struct qrequest *rp;
    pthread_mutex_lock(&qrequest_mutex);
    if ( (rp= REQS[peerid]) != 0 )
    {
        DL_DELETE(REQS[peerid],rp);
    }
    pthread_mutex_unlock(&qrequest_mutex);
    return(rp);
}

struct qrequest *qrequest_add(int32_t peerid,uint8_t type,uint8_t *payload,int32_t payloadlen)
{
    struct qrequest *rp,*tmp;
    int32_t count,size = sizeof(*rp) + payloadlen;
    rp = (struct qrequest *)calloc(1,size);
    rp->H = quheaderset(type,size);
    if ( payload != 0 && payloadlen > 0 )
        memcpy(rp->payload,payload,payloadlen);
    rp->size = payloadlen + sizeof(rp->H);
    pthread_mutex_lock(&qrequest_mutex);
    DL_APPEND(REQS[peerid],rp);
    pthread_mutex_unlock(&qrequest_mutex);
    DL_COUNT(REQS[peerid],tmp,count);
    //for (int j=0; j<rp->size; j++)
    //    printf("%02x",((uint8_t *)&rp->H)[j]);
    //printf(" peerid.%d count.%d after add\n",peerid,count);
    return(rp);
}
  
void addpeerqueue(int32_t type,uint8_t *payload,int32_t payloadlen)
{
    static int32_t lastpeerid,lastpeerid2;
    int32_t i,peerid;
    for (i=0; i<64; i++)
    {
        peerid = (rand() % (Numpeers+1));
        if ( peerid == lastpeerid || peerid == lastpeerid2 || Peertickinfo[peerid].tick < LATEST_TICK - 10 )
            continue;
        lastpeerid2 = lastpeerid;
        lastpeerid = peerid;
        qrequest_add(peerid,type,payload,payloadlen);
        return;
    }
    printf("could not find peerid\n");
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

void entityjson(char *str,char *addr,struct Entity E,int32_t tick)
{
    sprintf(str,"{\"address\":\"%s\",\"balance\":\"%s\",\"tick\":%d,\"numin\":%d,\"totalincoming\":\"%s\",\"latestin\":%d,\"numout\":%d,\"totaloutgoing\":\"%s\",\"latestout\":%d}",addr,amountstr(E.incomingAmount - E.outgoingAmount),tick,E.numberOfIncomingTransfers,amountstr2(E.incomingAmount),E.latestIncomingTransferTick,E.numberOfOutgoingTransfers,amountstr3(E.outgoingAmount),E.latestOutgoingTransferTick);
}

void process_entity(int32_t peerid,char *ipaddr,RespondedEntity *E)
{
    char addr[64];
    int32_t respid;
    struct qbuffer M;
    pubkey2addr(E->entity.publicKey,addr);
    for (int j=0; j<32; j++)
        printf("%02x",E->entity.publicKey[j]);
    respid = pubkeyrespid(E->entity.publicKey);
    printf(" %s got entity %s tick.%d respid.%d\n",ipaddr,addr,E->tick,respid);
    if ( respid != 0 )
    {
        M.mesg_type = 1;
        entityjson((char *)M.mesg_text,addr,E->entity,E->tick);
        msgsnd(respid,&M,strlen((char *)M.mesg_text)+1+sizeof(uint64_t),IPC_NOWAIT);
        printf("%s\n",(char *)M.mesg_text);
    }
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
    struct qrequest *rp;
    int32_t peerid=0,sock=-1,ptr,sz,recvbyte,iter = 0;
    struct quheader H;
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
        while ( (rp= qrequest_poll(peerid)) == 0 )
        {
            if ( (rp= qrequest_poll(0)) != 0 )
                break;
            usleep(100000);
        }
        //netpackets++;
        //printf("%s send %d req\n",ipaddr,rp->size);
        sock = socksend(ipaddr,sock,(uint8_t *)&rp->H,rp->size);
        //printf("freeing %p\n",rp);
        free(rp);
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

//int main(int argc, const char * argv[])
void qserver(void)
{
    pthread_t findpeers_thread;
    pthread_mutex_init(&addpeer_mutex,NULL);
    pthread_mutex_init(&qrequest_mutex,NULL);
    pthread_create(&findpeers_thread,NULL,&findpeers,0);
    key_t key;
    char addr[64];
    uint8_t pubkey[32],*ptr;
    int32_t i,msgid,sz,hsz,respid = 0;
    uint32_t utime;
    int32_t year,month,day,seconds,latest;
    struct qbuffer M,S,C;
    struct EntityRequest E;
    struct quheader H;
    devurandom((uint8_t *)&utime,sizeof(utime));
    srand(utime);
    utime = 0;
    signal(SIGPIPE, SIG_IGN);
    makefile(QUBIC_MSGPATH);
    key = ftok(QUBIC_MSGPATH, 'Q');
    msgid = msgget(key, 0666 | IPC_CREAT);
    printf("key %ld msgid %d\n",(long)key,msgid);
    latest = 0;
    while ( 1 )
    {
        memset(&M,0,sizeof(M));
        sz = (int32_t)msgrcv(msgid,&M,sizeof(M),0,IPC_NOWAIT);
        //printf("sz.%d sizeH %ld, sz < 0 %d, sz > 8 %d\n",sz,sizeof(H),sz < 0,sz > sizeof(H));
        if ( sz >= (int32_t)sizeof(H) )
        {
            respid = 0;
            memcpy(&H,M.mesg_text,sizeof(H));
            hsz = ((H._size[2] << 16) + (H._size[1] << 8) + H._size[0]);
            if ( hsz == (sz - sizeof(uint64_t)) || hsz == (sz - sizeof(uint64_t) - sizeof(uint32_t)) )
            {
                if ( H._type == REQUEST_ENTITY && hsz == sizeof(H)+sizeof(pubkey) )
                {
                    ptr = &M.mesg_text[sizeof(H)];
                    memcpy(pubkey,ptr,sizeof(pubkey));
                    ptr += sizeof(pubkey);
                    pubkey2addr(pubkey,addr);
                    if ( hsz == (sz - sizeof(uint64_t) - sizeof(uint32_t)) )
                    {
                        memcpy(&respid,ptr,sizeof(respid));
                        ptr += sizeof(respid);
                        if ( respid != 0 )
                        {
                            printf("%s %s to respid.%d\n",M.mesg_type == 2 ? "subscribe":"",addr,respid);
                            Tsubscriberadd(respid);
                            entitysubscription(M.mesg_type != 2,respid,pubkey);
                        }
                    }
                    printf("%s %s\n",addr,respid!=0?"monitor":"");
                }
                else
                {
                    printf("add to peerQs\n");
                    addpeerqueue(H._type,(uint8_t *)&M.mesg_text[sizeof(H)],hsz - sizeof(H));
                    addpeerqueue(H._type,(uint8_t *)&M.mesg_text[sizeof(H)],hsz - sizeof(H));
                    addpeerqueue(H._type,(uint8_t *)&M.mesg_text[sizeof(H)],hsz - sizeof(H));
                }
                printf(" H.type %d, sz.%d, type.%ld RECV (hsz.%d %u)\n",H._type,sz,(long)M.mesg_type,hsz,respid);
                if ( respid != 0 )
                {
                    if ( (sz= (int32_t)msgrcv(respid,&C,sizeof(C),1,MSG_COPY | IPC_NOWAIT)) > 0 )
                    {
                        printf("respid.%d still has message %d\n",respid,sz);
                    }
                    else
                    {
                        S.mesg_type = 1;
                        sprintf((char *)S.mesg_text,"qserver got your message of (%d) %s",hsz,M.mesg_type == 2 ? "subscribe":"");
                        msgsnd(respid,&S,strlen((char *)S.mesg_text)+1,IPC_NOWAIT);
                    }
                }
            }
        }
        else usleep(10000);
        utime = set_current_ymd(&year,&month,&day,&seconds);
        if ( utime > LATEST_UTIME )
        {
            LATEST_UTIME = utime;
            for (i=1; i<=Numpeers; i++)
                qrequest_add(i,REQUEST_CURRENT_TICK_INFO,0,0);
        }
        if ( LATEST_TICK > latest )
        {
            latest = LATEST_TICK;
            printf("update subscribers with %d\n",latest);
            S.mesg_type = 1;
            sprintf((char *)S.mesg_text,"{\"tick\":%d}",latest);
            for (i=0; i<sizeof(TSUBS)/sizeof(*TSUBS); i++)
            {
                if ( TSUBS[i][0] != 0 )
                {
                    if ( (sz= (int32_t)msgrcv(TSUBS[i][0],&C,sizeof(C),1,MSG_COPY | IPC_NOWAIT)) > 0 )
                    {
                        TSUBS[i][1]++;
                        printf("TSUBS[%d][0] respid.%d still has message %d errs.%d\n",i,respid,sz,TSUBS[i][1]);
                        // increase counter and close channel if too many errors
                    }
                    else msgsnd(TSUBS[i][0],&S,strlen((char *)S.mesg_text)+1,IPC_NOWAIT);
                }
                else break;
            }
            for (i=0; i<sizeof(ESUBS)/sizeof(*ESUBS); i++)
            {
                if ( ESUBS[i].respid == 0 )
                    break;
                memset(&E,0,sizeof(E));
                pubkey2addr(pubkey,addr);
                memcpy(E.pubkey,ESUBS[i].pubkey,sizeof(E.pubkey));
                addpeerqueue(REQUEST_ENTITY,E.pubkey,sizeof(E.pubkey));
                addpeerqueue(REQUEST_ENTITY,E.pubkey,sizeof(E.pubkey));
                addpeerqueue(REQUEST_ENTITY,E.pubkey,sizeof(E.pubkey));
            }
        }
    }
    msgctl(msgid,IPC_RMID,NULL);
}

