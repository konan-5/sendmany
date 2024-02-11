
int32_t Numpeers;
uint8_t IPbytes[256][4];
char IPADDRS[256][16];

char *randpeer(char *ipbuf)
{
    char *ipaddr;
    if ( Numpeers == 0 )
        ipaddr = DEFAULT_NODE_IP;
    else
        ipaddr = IPADDRS[rand() % Numpeers];
    if ( ipbuf != 0 )
        strcpy(ipaddr,DEFAULT_NODE_IP);
    return(ipaddr);
}

void ipaddr2ipbytes(char *ipaddr,uint8_t ipbytes[4])
{
    int32_t i;
    memset(ipbytes,0,4);
    ipbytes[0] = atoi(ipaddr);
    for (i=0; i<5; i++)
        if ( ipaddr[i] == '.' )
        {
            ipaddr += i+1;
            ipbytes[1] = atoi(ipaddr);
            for (i=0; i<5; i++)
                if ( ipaddr[i] == '.' )
                {
                    ipaddr += i+1;
                    ipbytes[2] = atoi(ipaddr);
                    for (i=0; i<5; i++)
                        if ( ipaddr[i] == '.' )
                        {
                            ipaddr += i+1;
                            ipbytes[3] = atoi(ipaddr);
                            break;
                        }
                    break;
                }
            break;
        }
}

int32_t addpeer(char *origipaddr,int32_t epoch)
{
    char cmpaddr[16];
    uint8_t ipbytes[4];
    ipaddr2ipbytes(origipaddr,ipbytes);
    sprintf(cmpaddr,"%d.%d.%d.%d",ipbytes[0],ipbytes[1],ipbytes[2],ipbytes[3]);
    memcpy(IPbytes[Numpeers],ipbytes,sizeof(ipbytes));
    strcpy(IPADDRS[Numpeers],cmpaddr);
    if ( strcmp(cmpaddr,origipaddr) != 0 )
        printf("ERROR: ");
    CurrentTickInfo I;
    I = getTickInfoFromNode(cmpaddr,DEFAULT_NODE_PORT);
    if ( I.epoch == epoch )
    {
        pthread_t *ptr = calloc(1,sizeof(*ptr));
        pthread_create(ptr,NULL,&txq_recvloop,IPbytes[Numpeers]);
        printf("Numpeers.%d: %s add %s\n",Numpeers,cmpaddr,origipaddr);
        Numpeers++;
        return(Numpeers);
    }
    printf("invalid epoch.%d from %s\n",I.epoch,cmpaddr);
    return(-1);
}

void *initpeers(void *ptr)
{
    static int32_t didinit;
    int32_t tbepoch,starttick,latesttick;
    uint32_t utime;
    CurrentTickInfo info;
    FILE *fp,*infofp;
    char line[512],fname[1024];
    if ( didinit != 0 )
        return(0);
    tbepoch = timebasedepoch(&starttick,&latesttick);
    Numpeers = 0;
    makepeerslist("peers.list");
    if ( (fp= fopen("peers.list","r")) != 0 )
    {
        while ( fgets(line,sizeof(line),fp) != 0 )
        {
            line[strlen(line)-1] = 0;
            if ( strcmp(line,"0.0,0,0") != 0 )
            {
                sprintf(fname,"peer%c%s",dir_delim(),line);
                if ( (infofp= fopen(fname,"rb")) != 0 )
                {
                    fread(&info,1,sizeof(info),infofp);
                    fread(&utime,1,sizeof(utime),infofp);
                    fclose(infofp);
                    if ( info.epoch != 0 )
                        if ( addpeer(line,tbepoch) < 0 )
                            deletefile(fname);
                }
            }
        }
        fclose(fp);
    }
    didinit = 1;
    while ( 1 )
        sleep(1);
}

