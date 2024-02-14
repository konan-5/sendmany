    

char dir_delim(void)
{
    return('/');
}

void makedir(const char *dirname)
{
    FILE *fp;
    char fname[512],cmd[512];
    sprintf(fname,"%s%c.exists",dirname,dir_delim());
    if ( (fp= fopen(fname,"rb")) != 0 )
        fclose(fp);
    else if ( (fp= fopen(fname,"wb")) != 0 )
        fclose(fp);
    else
    {
        sprintf(cmd,"mkdir %s",dirname);
        system(cmd);
    }
}

void deletefile(char *fname)
{
    char cmd[1023];
    sprintf(cmd,"rm %s",fname);
    system(cmd);
}

void cpfile(char *src,char *dest)
{
    char cmd[1024];
    sprintf(cmd,"cp %s %s",src,dest);
    system(cmd);
}

void makepeerslist(const char *fname)
{
    char cmd[1024];
    sprintf(cmd,"ls -w 16 peer > %s",fname);
    system(cmd);
}

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

#ifdef __APPLE__
#define FMT64 "%lld"
#else
#ifdef EMSCRIPTEN
#define FMT64 "%lld"
#else
#define FMT64 "%ld"
#endif
#endif

char *amountstr(uint64_t amount)
{
    static char str[64];
    sprintf(str,FMT64,amount);
    return(str);
}

char *amountstr2(uint64_t amount)
{
    static char str[64];
    sprintf(str,FMT64,amount);
    return(str);
}

char *amountstr3(uint64_t amount)
{
    static char str[64];
    sprintf(str,FMT64,amount);
    return(str);
}

char *amountstr4(uint64_t amount)
{
    static char str[64];
    sprintf(str,FMT64,amount);
    return(str);
}


    
