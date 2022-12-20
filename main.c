#include <stdio.h>
#include <string.h>
#include "mfs.h"

int main(int argc, char *argv[]) {
    int rc = MFS_Init("localhost", 10000);
    printf("init %d\n", rc); //0
    
    rc = MFS_Creat(0,MFS_DIRECTORY,"testdir");
    int inum = MFS_Lookup(0,"testdir");
    //printf("inum of dir created:%d\n",inum);
    // rc = MFS_Creat(inum,MFS_REGULAR_FILE,"0");
    // printf("created: %d rc:%d\n",0,rc);
    int max_files = 126;
    char name[128][3];

    //48-112
    for(int i=0;i<64;i++)
    {
        name[i][0]=(i+48);
    }
    for(int i=64;i<126;i++)
    {
        name[i][0]=(i-64+48);
        name[i][1]='0';
    }
    
    

    for(int i=0;i<max_files;i++)
    {
        rc = MFS_Creat(inum,MFS_REGULAR_FILE,name[i]);
        //printf("created: %d rc: %d\n ",i,rc);
        //printf("%s           \n",name[i]);
    }
    
    for(int i=0;i<1;i++)
    {
        rc = MFS_Lookup(inum,name[i]);
        printf("Lookup inodes:%d\n",rc);
    }

    rc = MFS_Shutdown();
    printf("shut %d\n", rc);//0
    return 0;
}