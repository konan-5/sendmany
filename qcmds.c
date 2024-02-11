


CurrentTickInfo getTickInfoFromNode(const char *nodeIp,int32_t nodePort)
{
    struct quheader R;
    CurrentTickInfo result,*rp;
    int32_t sock;
    uint8_t buf[4096];
    memset(&result,0,sizeof(result));
    if ( (sock= myconnect(nodeIp,nodePort)) < 0 )
        return(result);
    if ( (rp= reqresponse(buf,sizeof(buf),sock,REQUEST_CURRENT_TICK_INFO,(uint8_t *)&R,sizeof(R),RESPOND_CURRENT_TICK_INFO)) != 0 )
        memcpy(&result,rp,sizeof(result));
    close(sock);
    return result;
}

RespondedEntity getBalance(const char *nodeIp,const int nodePort,const uint8_t *pubkey)
{
    struct EntityRequest ER;
    RespondedEntity result,*rp;
    int32_t sock;
    uint8_t buf[4096];
    memset(&result,0,sizeof(result));
    if ( (sock= myconnect(nodeIp,nodePort)) < 0 )
        return(result);
    memcpy(ER.pubkey,pubkey,sizeof(ER.pubkey));
    if ( (rp= reqresponse(buf,sizeof(buf),sock,REQUEST_ENTITY,(uint8_t *)&ER,sizeof(ER),RESPOND_ENTITY)) != 0 )
        memcpy(&result,rp,sizeof(result));
    close(sock);
    return result;
}

void sendrawtransaction(const char *ipaddr,uint16_t port,const char *rawhex)
{
    int32_t sock,datalen;
    uint8_t buf[4096],reqbuf[4096],*rp;
    if ( (sock= myconnect(ipaddr,port)) < 0 )
    {
        printf("Error getting connection %s for sendrawtransaction\n",ipaddr);
        return;
    }
    datalen = (int32_t)strlen(rawhex) / 2;
    hexToByte(rawhex,&reqbuf[sizeof(struct quheader)],datalen);
    if ( (rp= reqresponse(buf,sizeof(buf),sock,BROADCAST_TRANSACTION,reqbuf,datalen+sizeof(struct quheader),EXCHANGE_PUBLIC_PEERS)) != 0 )
    {
        //for (int i=0; i<16; i++)
        //    printf("%02x",rp[i]);
        //printf(" respnse\n");
    }
    close(sock);
    return;
}

////////////////// wrappers


int32_t timebasedepoch(int32_t *startp,int32_t *latestp)
{
    uint32_t utime;
    int32_t i,year,month,day,seconds,epoch;
    utime = set_current_ymd(&year,&month,&day,&seconds);
    epoch = utime_to_epoch(utime,&seconds);
    for (i=0; i<3; i++)
    {
        CurrentTickInfo info = getTickInfoFromNode(randpeer(0),DEFAULT_NODE_PORT);
        printf("Epoch %d, days %d, hours %d, seconds %d vs %d/%d start.%d\n",epoch,seconds/(3600 * 24),(seconds % (3600 * 24))/3600,seconds%3600,info.epoch,info.tick,info.initialTick);
        if ( epoch == info.epoch && info.initialTick > 12000000 )
        {
            *startp = info.initialTick;
            *latestp = info.tick;
            return(epoch);
        }
    }
    *startp = 0;
    *latestp = 0;
    return(0);
}
