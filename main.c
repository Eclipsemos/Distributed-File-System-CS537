#include <stdio.h>
#include <string.h>
#include "mfs.h"

int main(int argc, char *argv[]) {
    int rc = MFS_Init("localhost", 10000);
    printf("init %d\n", rc); //0
    
    rc = MFS_Creat(0,MFS_REGULAR_FILE,"test1");
    if(rc == -1)
        printf("Create failed\n");//0
    else
        printf("Create success\n");

   

    int inum = MFS_Lookup(0, "test1");
    printf("Lookup get num:%d\n", inum);
    char buffer[4098];
     for(int i=0;i<=4094;i++)
        *(buffer+i) = '0';
    *(buffer+4095) = '1';
    printf("Length: %d  SIZE: %d\n", strlen(buffer),MFS_BLOCK_SIZE);
     rc = MFS_Write(inum, buffer, 30*MFS_BLOCK_SIZE, MFS_BLOCK_SIZE);
     printf("Write rc:%d\n",rc);
    // char buffer_get[4098];
    // MFS_Read(inum,buffer_get,0,MFS_BLOCK_SIZE);
    // printf("Read buffer is:%s\n",buffer_get);
    // MFS_Stat_t ans;
    // MFS_Stat(inum, &ans);
    // printf("type:%d size:%d \n",ans.type,ans.size);
    //rc = MFS_Stat();
    // // printf("create %d\n", rc);//0

    // int inum = MFS_Lookup(0, "test1");
    //     printf("inum lookup:%d\n", inum);
    
    // printf("inum %d\n", inum);//


    rc = MFS_Shutdown();
    printf("shut %d\n", rc);//0
    return 0;
}