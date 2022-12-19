#include <stdio.h>

#include "mfs.h"

int main(int argc, char *argv[]) {
    int rc = MFS_Init("localhost", 10000);
    printf("init %d\n", rc); //0
    
    // rc = MFS_Creat(0,MFS_REGULAR_FILE,"test");
    // printf("create %d\n", rc);//0
    // int inum = MFS_Lookup(0, "test");
    // printf("inum %d\n", inum);//0

    // rc = MFS_Unlink(0,"test");
    // printf("Unlink %d\n", rc);//

    // inum = MFS_Lookup(0, "test");
    // printf("Lookup %d\n", rc);//0

    rc = MFS_Shutdown();
    printf("shut %d\n", rc);//0
    return 0;
}