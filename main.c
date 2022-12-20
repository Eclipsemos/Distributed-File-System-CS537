#include <stdio.h>

#include "mfs.h"

int main(int argc, char *argv[]) {
    int rc = MFS_Init("localhost", 10000);
    printf("init %d\n", rc); //0
    
    rc = MFS_Creat(0,MFS_REGULAR_FILE,"test1");
    if(rc == -1)
        printf("create failed\n");//0
    else
        printf("creat success\n");
    rc = MFS_Creat(1,MFS_REGULAR_FILE,"test2");
    if(rc == -1)
        printf("create failed\n");//0
    else
        printf("creat success\n");
    // // printf("create %d\n", rc);//0

    // int inum = MFS_Lookup(0, "test1");
    //     printf("inum lookup:%d\n", inum);
    // inum = MFS_Lookup(0, "test2");
    // printf("inum %d\n", inum);//


    rc = MFS_Shutdown();
    printf("shut %d\n", rc);//0
    return 0;
}