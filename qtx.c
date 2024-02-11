#define FAN 25

typedef struct
{
    unsigned char sourcePublicKey[32];
    unsigned char destinationPublicKey[32];
    long long amount;
    unsigned int tick;
    unsigned short inputType;
    unsigned short inputSize;
} Transaction;

struct pubkeypay
{
    uint8_t publickeys[FAN][32];
    int64_t amounts[FAN];
};

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
    if ( txtick == 0 )
    {
        txtick = LATEST_TICK + TICKOFFSET;
        printf("auto set scheduledTick %d\n",txtick);
    }
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

void create_rawsendmany(char *rawhex,char *txhash,uint8_t digest[32],const uint8_t *subseed,uint32_t scheduledTick,struct pubkeypay *payments,int32_t numpayments)
{
    uint8_t privateKey[32],sourcePublicKey[32],destPublicKey[32];
    int64_t total = SENDMANYFEE;
    getPrivateKeyFromSubSeed(subseed,privateKey);
    getPublicKeyFromPrivateKey(privateKey,sourcePublicKey);
    memset(destPublicKey,0,sizeof(destPublicKey));
    ((uint64_t *)destPublicKey)[0] = QUTIL_CONTRACT_ID;
    memset(digest,0,32);
    for (int i=0; i<numpayments; i++)
        total += payments->amounts[i];
    create_rawtxhex(rawhex,txhash,digest,subseed,SENDTOMANYV1,sourcePublicKey,destPublicKey,total,(uint8_t *)payments,sizeof(*payments),scheduledTick);
    txhash[60] = 0;
    char publicIdentity[64] = {0};
    pubkey2addr(sourcePublicKey,publicIdentity);
    //printf("%s sent %d payments for total of %s\n",publicIdentity,numpayments,amountstr(total));
    //printf("%s datalen.%d\n%s\n\n",txhash,(int32_t)strlen(rawhex)/2,rawhex);
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
    printf("%s %s -> %s\n%s\n%s\n\n",addr,amountstr(amount),targetIdentity,txhash,rawhex);
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
