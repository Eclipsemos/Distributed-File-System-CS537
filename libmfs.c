#include <stdio.h>
#include "mfs.h"
#include "udp.h"
#include "message.h"
#include <sys/time.h>
#include <sys/select.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

struct sockaddr_in addrSnd, addrRcv;
int sd, rc;

int MFS_Init(char *hostname, int port) {
    if(hostname == NULL) {
        return -1;
    }

    if(port < 0) {
        return -1;
    }

    sd = UDP_Open(20000);
    rc = UDP_FillSockAddr(&addrSnd, hostname, port);
    
    return rc;
}

int MFS_Lookup(int pinum, char *name) {
    if(pinum < 0) {
        return -1;
    }

    if(name == NULL) {
        return -1;
    }
    
    message_t send_msg, received_msg;
    send_msg.mtype = MFS_LOOKUP;
    send_msg.pinum = pinum;
    strcpy(send_msg.name, name);


    int send_rc = UDP_Write(sd, &addrSnd, (char*) &send_msg, sizeof(message_t));
    int received_rc = UDP_Read(sd, &addrRcv, (char*) &received_msg, sizeof(message_t));
    printf("Client::lookup::msg received inum is: %d\n",received_msg.inum);
   
    return received_msg.inum; 
}

int MFS_Creat(int pinum, int type, char *name) {
    if(pinum < 0) {
        return -1;
    }

    if(name == NULL) {
        return -1;
    }

    if(strlen(name) > 28) {
        return -1;
    } 


    message_t send_msg, received_msg;
    send_msg.mtype = MFS_CRET;
    send_msg.type = type;
    send_msg.pinum = pinum;
    strcpy(send_msg.name, name);

    
    int send_rc = UDP_Write(sd, &addrSnd, (char*) &send_msg, sizeof(message_t));
    int received_rc = UDP_Read(sd, &addrRcv, (char*) &received_msg, sizeof(message_t));
    if(received_msg.rc == -1)
    {
        return -1;
    }
    //printf("Client::created node is:%d\n",received_msg.inum);
    return 0;
}

int MFS_Stat(int inum, MFS_Stat_t *m) {
    if(inum < 0) {
        return -1;
    }
    
    if(m == NULL) {
        return -1;
    }

    message_t msg;
    msg.pinum = inum;
    msg.mtype = MFS_STAT;

    rc = UDP_Write(sd, &addrSnd, (char*) &msg, sizeof(message_t));

    if(rc < 0) {
        printf("client:: MFS_Stat WRITE failed; libmfs.c\n");
        return -1;
    }

    rc = UDP_Read(sd, &addrRcv, (char*) &msg, sizeof(message_t));
    
    if(rc < 0) {
        printf("client:: MFS_Stat READ failed; libmfs.c\n");
        return -1;
    }

    m->type = msg.mfs_stat.type;
    m->size = msg.mfs_stat.size;

    return msg.rc;
}

int MFS_Write(int inum, char *buffer, int offset, int nbytes) {
    if(inum < 0) {
        return -1;
    }

    if(buffer == NULL) {
        return -1;
    }
    
    if(offset < 0) {
        return -1;
    }

    if(nbytes > 4096) {
        return -1;
    }
    
    if(nbytes < 0) {
        return -1;
    }

    message_t msg;
    msg.pinum = inum;
    strcpy(msg.bufferSent, buffer);
    msg.offset = offset;
    msg.nbytes = nbytes;
    msg.mtype = MFS_WRITE;

    rc = UDP_Write(sd, &addrSnd, (char*) &msg, sizeof(message_t));

    if(rc < 0) {
        printf("client:: MFS_Stat WRITE failed; libmfs.c\n");
        return -1;
    }

    rc = UDP_Read(sd, &addrRcv, (char*) &msg, sizeof(message_t));

    if(rc < 0) {
        printf("client:: MFS_Stat READ failed; libmfs.c\n");
        return -1;
    }

    return msg.rc;
}

int MFS_Read(int inum, char *buffer, int offset, int nbytes) {
    if(inum < 0) {
        return -1;
    }
    
    if(buffer == NULL) {
        return -1;
    }

    if(offset < 0) {
        return -1;
    }
    
    if(nbytes > 4096) {
        return -1;
    }
    
    if (nbytes < 0) {
        return -1;
    }

    message_t msg;
    msg.pinum = inum;
    strcpy(msg.bufferSent, buffer);
    msg.offset = offset;
    msg.nbytes = nbytes;
    msg.mtype = MFS_READ;

    rc = UDP_Write(sd, &addrSnd, (char*) &msg, sizeof(message_t));

    if(rc < 0) {
        printf("client:: MFS_Stat WRITE failed; libmfs.c\n");
        return -1;
    }

    rc = UDP_Read(sd, &addrRcv, (char*) &msg, sizeof(message_t));

    if(rc < 0) {
        printf("client:: MFS_Stat READ failed; libmfs.c\n");
        return -1;
    }

    for(int z = 0; z< nbytes; z++) {
        buffer[z] = msg.bufferReceived[z];
    }
    printf("buffer: %s\n",buffer);

    return msg.rc;
}

int MFS_Unlink(int pinum, char *name) {
    if(pinum < 0) {
        return -1;
    }

    if (name == NULL) {
        return -1;
    }

    message_t msg;
    msg.pinum = pinum;
    strcpy(msg.name, name);
    msg.mtype = MFS_UNLINK;

    rc = UDP_Write(sd, &addrSnd, (char*) &msg, sizeof(message_t));

    if(rc < 0) {
        printf("client:: MFS_Stat WRITE failed; libmfs.c\n");
        return -1;
    }

    rc = UDP_Read(sd, &addrRcv, (char*) &msg, sizeof(message_t));

    if(rc < 0) {
        printf("client:: MFS_Stat READ failed; libmfs.c\n");
        return -1;
    }

    return msg.rc;
}

int MFS_Shutdown() {
    message_t msg;
    msg.mtype = MFS_SHUTDOWN;

    rc = UDP_Write(sd, &addrSnd, (char*) &msg, sizeof(message_t));
    if(rc < 0){
        printf("client:: MFS_Shutdown failed; libmfs.c\n");
        return -1;
    }

    return 0;
}