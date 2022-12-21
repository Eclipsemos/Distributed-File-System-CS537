#include <stdio.h>
#include <string.h>
#include "mfs.h"

int main(int argc, char *argv[]) {
    int rc = MFS_Init("localhost", 10000);
    printf("init %d\n", rc);
    
    rc = MFS_Creat(0,MFS_REGULAR_FILE, "testfile");

    int inum = MFS_Lookup(0, "testfile");
    printf("Inum: %d\n", inum);

    char* buffer = "abcd";
    MFS_Write(inum, buffer, 0, 4);

    int new_inum = MFS_Lookup(0, "testfile");
    printf("Inum: %d\n", inum);

    //rc = MFS_Unlink(inum,"testfile");
    rc = MFS_Shutdown();
    printf("shut %d\n", rc);

    rc = MFS_Init("localhost", 10000);
    new_inum = MFS_Lookup(0, "testfile");
    printf("Inum: %d\n", inum);

    return 0;
}