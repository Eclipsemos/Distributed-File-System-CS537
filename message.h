#ifndef __message_h__
#define __message_h__

#define MFS_INIT (1)
#define MFS_LOOKUP (2)
#define MFS_STAT (3)
#define MFS_WRITE (4)
#define MFS_READ (5)
#define MFS_CREAT (6)
#define MFS_UNLINK (7)
#define MFS_SHUTDOWN (8)

#include "mfs.h"

typedef struct {
    int mtype; // message type from above
    int rc;    // return code

    // put more here ...
    int pinum;
    char name[28]; 
    char bufferSent[4096];
    int offset;
    int nbytes;
    int type;
    int inum; 
    char bufferReceived[4096];
    int bufferReceived_size;
    MFS_Stat_t mfs_stat;
    MFS_DirEnt_t mfs_DifEnt;
    
} message_t;

#endif // __message_h__
