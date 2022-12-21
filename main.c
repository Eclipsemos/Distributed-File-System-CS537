#include <stdio.h>
#include <string.h>
#include "mfs.h"

int main(int argc, char *argv[]) {
    int rc = MFS_Init("localhost", 10000);
    printf("init %d\n", rc); //0
    
    rc = MFS_Creat(0,MFS_DIRECTORY,"testdir");
    int inum = MFS_Lookup(0,"testdir");

    rc = MFS_Creat(inum,MFS_REGULAR_FILE,"testfile");

    //rc = MFS_Unlink(inum,"testfile");

    rc = MFS_Unlink(0,"testdir");
    printf("rc :%d\n",rc);

    rc = MFS_Shutdown();
    printf("shut %d\n", rc);//0
    return 0;
}