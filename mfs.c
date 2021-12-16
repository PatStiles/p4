#include <unistd.h>
#include <stdio.h>
#include <sys/select.h>
#include "udp.h"
#include "mfs.h"

struct sockaddr_in addrSnd, addrRcv;
int sd;
int rc;

typedef struct mssg {
    int tag;
    int type;
    int size;
    int pinum;
    int inum;
    int block;
    char name[2];
    char buffer[MFS_BLOCK_SIZE];
} mssg;

//no tag
int MFS_Init(char *hostname, int port)
{
    printf("Initializing Client Connection ...\n");
    sd = UDP_Open(11114);
    rc = UDP_FillSockAddr(&addrSnd, hostname, port); 
    printf("Connection Success ...");
    return 0;
}

//tag=1
int MFS_Lookup(int pinum, char *name)
{
    mssg msg = {0, 0, 0, 0, 0, 0, "\0", "\0"};
    msg.tag = 1;
    msg.pinum = pinum;
    strcpy(msg.name,name);

    char message[sizeof(mssg)];
    memcpy((mssg*)message, &msg, sizeof(mssg)); 
    printf("client(Lookup-->):: message [size:%d contents:( tag:%d type:%d size:%d pinum:%d inum:%d block:%d name:%s buffer:%s)]\n", rc, msg.tag, msg.type, msg.size, msg.pinum, msg.inum, msg.block, msg.name, msg.buffer);
 
    rc = UDP_Write(sd, &addrSnd, message, MFS_BLOCK_SIZE); 
    if(rc < 0) { 
        printf("client:: failed to send %d\n", rc);
        return -1;
    }

    while(1) {
        //reset select() interface values everytime it returns
        fd_set rfds;
        struct timeval tv;
        int rd_ready;

        FD_ZERO(&rfds);
        FD_SET(sd, &rfds); //set select to monitor sd for reading

        tv.tv_sec = 5;
        tv.tv_usec = 0;

        rd_ready = select(sd+1, &rfds, NULL, NULL, &tv); //declare sd to monitor sd only for reading.

        switch (rd_ready) {
            case -1:
                perror("select().\n sending a new request\n");
                rc = UDP_Write(sd, &addrSnd, message, MFS_BLOCK_SIZE); 
                break;
            case 0:
                printf("select() returns 0.\n sending a new request\n");
                rc = UDP_Write(sd, &addrSnd, message, MFS_BLOCK_SIZE); 
                break;  
            default: {
                rc = UDP_Read(sd, &addrRcv, message, MFS_BLOCK_SIZE);
                if(rc < 0)
                    return -1;
                memcpy(&msg, (mssg*) message, sizeof(mssg));
                printf("client(<--Lookup):: message [size:%d contents:( tag:%d type:%d size:%d pinum:%d inum:%d block:%d name:%s buffer:%s)]\n", rc, msg.tag, msg.type, msg.size, msg.pinum, msg.inum, msg.block, msg.name, msg.buffer);
                if(msg.tag != -1) {
                    return msg.inum;
				}
                return msg.tag;
            }
        }
        printf("client:: waiting 5 sec\n");
    } 
    printf("client:: lookUp done \n");
    return -1;
}

//tag=2
int MFS_Stat(int inum, MFS_Stat_t *m)
{
    mssg msg = {0, 0, 0, 0, 0, 0, "\0", "\0"};
    msg.tag = 2;
    msg.inum = inum;

    char message[sizeof(mssg)];
    memcpy((mssg*)message, &msg, sizeof(mssg)); 
    printf("client(Stat-->):: message [size:%d contents:( tag:%d type:%d size:%d pinum:%d inum:%d block:%d name:%s buffer:%s)]\n", rc, msg.tag, msg.type, msg.size, msg.pinum, msg.inum, msg.block, msg.name, msg.buffer);
 
    rc = UDP_Write(sd, &addrSnd, message, MFS_BLOCK_SIZE); 
    if(rc < 0) {
    printf("client:: failed to send %d\n", rc);
    exit(1);
    }

    while(1) {
        //reset select() interface values everytime it returns
        fd_set rfds;
        struct timeval tv;
        int rd_ready;

        FD_ZERO(&rfds);
        FD_SET(sd, &rfds); //set select to monitor sd for reading
        tv.tv_sec = 5;
        tv.tv_usec = 0;

        rd_ready = select(sd+1, &rfds, NULL, NULL, &tv); //declare sd to monitor sd only for reading.

        switch (rd_ready) {
            case -1:
                perror("select().\n sending a new request\n");
                rc = UDP_Write(sd, &addrSnd, message, MFS_BLOCK_SIZE); 
                break;
            case 0:
                printf("select() returns 0.\n sending a new request\n");
                rc = UDP_Write(sd, &addrSnd, message, MFS_BLOCK_SIZE); 
                break;  
            default: {
                rc = UDP_Read(sd, &addrRcv, message, MFS_BLOCK_SIZE);
                if(rc < 0)
                    return -1;
                memcpy(&msg, (mssg*) message, sizeof(mssg));
                printf("client(<--Stat):: message [size:%d contents:( tag:%d type:%d size:%d pinum:%d inum:%d block:%d name:%s buffer:%s)]\n", rc, msg.tag, msg.type, msg.size, msg.pinum, msg.inum, msg.block, msg.name, msg.buffer);
                
                if(msg.tag != -1) {
                    //Add fields to MFS_STAT struture
                    m->type = msg.type; 
                    m->size = msg.size; 
                }
                return msg.tag;
            }
        }
        printf("client:: waiting 5 sec\n");
    } 
    printf("client:: lookUp done \n");
    return -1;

}

//tag=3
int MFS_Write(int inum, char *buffer, int block)
{
    if(block < 0 || block > 14 || strlen(buffer) > MFS_BLOCK_SIZE) {
        return -1;
    }

    mssg msg = {0, 0, 0, 0, 0, 0, "\0", "\0"};
    msg.tag = 3;
    msg.inum = inum;
    msg.block = block;
    strcpy(msg.buffer,buffer);

    char message[sizeof(mssg)];
    memcpy((mssg*)message, &msg, sizeof(mssg)); 
    printf("client(Write-->):: message [size:%d contents:( tag:%d type:%d size:%d pinum:%d inum:%d block:%d name:%s buffer:%s)]\n", rc, msg.tag, msg.type, msg.size, msg.pinum, msg.inum, msg.block, msg.name, msg.buffer);

    rc = UDP_Write(sd, &addrSnd, message, MFS_BLOCK_SIZE); 
    if(rc < 0) {
        printf("client:: failed to send %d\n", rc);
        exit(1);
    }

    while(1) {
        //reset select() interface values everytime it returns
        fd_set rfds;
        struct timeval tv;
        int rd_ready;

        FD_ZERO(&rfds);
        FD_SET(sd, &rfds); //set select to monitor sd for reading
        tv.tv_sec = 5;
        tv.tv_usec = 0;

        rd_ready = select(sd+1, &rfds, NULL, NULL, &tv); //declare sd to monitor sd only for reading.

        switch (rd_ready) {
            case -1:
                perror("select().\n sending a new request\n");
                rc = UDP_Write(sd, &addrSnd, message, MFS_BLOCK_SIZE); 
                break;
            case 0:
                printf("select() returns 0.\n sending a new request\n");
                rc = UDP_Write(sd, &addrSnd, message, MFS_BLOCK_SIZE); 
                break;  
            default: {
                //read success message for write
                rc = UDP_Read(sd, &addrRcv, message, MFS_BLOCK_SIZE);
                if(rc < 0)
                    return -1;
                memcpy(&msg, (mssg*) message, sizeof(mssg));
                printf("client(<--Write):: message [size:%d contents:( tag:%d type:%d size:%d pinum:%d inum:%d block:%d name:%s buffer:%s)]\n", rc, msg.tag, msg.type, msg.size, msg.pinum, msg.inum, msg.block, msg.name, msg.buffer);
                return msg.tag;
            }
        }
        printf("client:: waiting 5 sec\n");
    } 
    printf("client:: lookUp done \n");
    return -1;
}

//tag=4
int MFS_Read(int inum, char *buffer, int block)
{
    if(block < 0 || block > 14 || strlen(buffer) > MFS_BLOCK_SIZE) {
        return -1;
    }

    mssg msg = {0, 0, 0, 0, 0, 0, "\0", "\0"};
    msg.tag = 4;
    msg.inum = inum;
    msg.block = block;
    strcpy(msg.buffer,buffer);

    char message[sizeof(mssg)];
    memcpy((mssg*)message, &msg, sizeof(mssg)); 
    printf("client(Read-->):: message [size:%d contents:( tag:%d type:%d size:%d pinum:%d inum:%d block:%d name:%s buffer:%s)]\n", rc, msg.tag, msg.type, msg.size, msg.pinum, msg.inum, msg.block, msg.name, msg.buffer);
 
    rc = UDP_Write(sd, &addrSnd, message, MFS_BLOCK_SIZE); 
    if(rc < 0) {
    printf("client:: failed to send %d\n", rc);
    exit(1);
    }

    while(1) {
        //reset select() interface values everytime it returns
        fd_set rfds;
        struct timeval tv;
        int rd_ready;

        FD_ZERO(&rfds);
        FD_SET(sd, &rfds); //set select to monitor sd for reading
        tv.tv_sec = 5;
        tv.tv_usec = 0;

        rd_ready = select(sd+1, &rfds, NULL, NULL, &tv); //declare sd to monitor sd only for reading.

        switch (rd_ready) {
            case -1:
                perror("select().\n sending a new request\n");
                rc = UDP_Write(sd, &addrSnd, message, MFS_BLOCK_SIZE); 
                break;
            case 0:
                printf("select() returns 0.\n sending a new request\n");
                rc = UDP_Write(sd, &addrSnd, message, MFS_BLOCK_SIZE); 
                break;  
            default: {
                rc = UDP_Read(sd, &addrRcv, message, MFS_BLOCK_SIZE);
                if(rc < 0)
                    return -1;
                memcpy(&msg, (mssg*) message, sizeof(mssg));
                printf("client(<--Read):: message [size:%d contents:( tag:%d type:%d size:%d pinum:%d inum:%d block:%d name:%s buffer:%s)]\n", rc, msg.tag, msg.type, msg.size, msg.pinum, msg.inum, msg.block, msg.name, msg.buffer);
                memcpy(&buffer, msg.buffer, MFS_BLOCK_SIZE);
                return msg.tag;
            }
        }
        printf("client:: waiting 5 sec\n");
    } 
    printf("client:: lookUp done \n");
    return -1;

}

//tag=5
int MFS_Creat(int pinum, int type, char *name)
{ 
    if(sizeof(name) > 24 || pinum > 4096 || pinum < 0) { 
        printf("%ld\n",sizeof(name));
        return -1;
    }

    mssg msg = {0, 0, 0, 0, 0, 0, "\0", "\0"};
    msg.tag = 5;
    msg.pinum = pinum;
    msg.type = type;
    strcpy(msg.name,name);

    char message[sizeof(mssg)];
    memcpy((mssg*)message, &msg, sizeof(mssg)); 
    printf("client(Creat-->):: message [size:%d contents:( tag:%d type:%d size:%d pinum:%d inum:%d block:%d name:%s buffer:%s)]\n", rc, msg.tag, msg.type, msg.size, msg.pinum, msg.inum, msg.block, msg.name, msg.buffer);
 
    rc = UDP_Write(sd, &addrSnd, message, MFS_BLOCK_SIZE); 
    if(rc < 0) {
    printf("client:: failed to send %d\n", rc);
    exit(1);
    }

    while(1) {
        //reset select() interface values everytime it returns
        fd_set rfds;
        struct timeval tv;
        int rd_ready;

        FD_ZERO(&rfds);
        FD_SET(sd, &rfds); //set select to monitor sd for reading
        tv.tv_sec = 5;
        tv.tv_usec = 0;

        rd_ready = select(sd+1, &rfds, NULL, NULL, &tv); //declare sd to monitor sd only for reading.

        switch (rd_ready) {
            case -1:
                perror("select().\n sending a new request\n");
                rc = UDP_Write(sd, &addrSnd, message, MFS_BLOCK_SIZE); 
                break;
            case 0:
                printf("select() returns 0.\n sending a new request\n");
                rc = UDP_Write(sd, &addrSnd, message, MFS_BLOCK_SIZE); 
                break;  
            default: { 
                rc = UDP_Read(sd, &addrRcv, message, MFS_BLOCK_SIZE);
                if(rc < 0)
                    return -1;
                memcpy(&msg, (mssg*) message, sizeof(mssg));
                printf("client(<--Creat):: message [size:%d contents:( tag:%d type:%d size:%d pinum:%d inum:%d block:%d name:%s buffer:%s)]\n", rc, msg.tag, msg.type, msg.size, msg.pinum, msg.inum, msg.block, msg.name, msg.buffer); 
                return msg.tag;
            }
        }
        printf("client:: waiting 5 sec\n");
    } 
    printf("client:: lookUp done \n");
    return -1; 
}

//tag = 6
int MFS_Unlink(int pinum, char *name)
{
    mssg msg = {0, 0, 0, 0, 0, 0, "\0", "\0"};
    msg.tag = 6;
    msg.pinum = pinum;
    strcpy(msg.name,name);

    char message[sizeof(mssg)];
    memcpy((mssg*)message, &msg, sizeof(mssg)); 
    printf("client(Unlink-->):: message [size:%d contents:( tag:%d type:%d size:%d pinum:%d inum:%d block:%d name:%s buffer:%s)]\n", rc, msg.tag, msg.type, msg.size, msg.pinum, msg.inum, msg.block, msg.name, msg.buffer);
    rc = UDP_Write(sd, &addrSnd, message, MFS_BLOCK_SIZE); 
    if(rc < 0) {
    printf("client:: failed to send %d\n", rc);
    exit(1);
    } 

    while(1) {
        //reset select() interface values everytime it returns
        fd_set rfds;
        struct timeval tv;
        int rd_ready;

        FD_ZERO(&rfds);
        FD_SET(sd, &rfds); //set select to monitor sd for reading
        tv.tv_sec = 5;
        tv.tv_usec = 0;

        rd_ready = select(sd+1, &rfds, NULL, NULL, &tv); //declare sd to monitor sd only for reading.

        switch (rd_ready) {
            case -1:
                perror("select().\n sending a new request\n");
                rc = UDP_Write(sd, &addrSnd, message, MFS_BLOCK_SIZE); 
                break;
            case 0:
                printf("select() returns 0.\n sending a new request\n");
                rc = UDP_Write(sd, &addrSnd, message, MFS_BLOCK_SIZE); 
                break;  
            default: { 
                rc = UDP_Read(sd, &addrRcv, message, MFS_BLOCK_SIZE);
                if(rc < 0)
                    return -1;
                memcpy(&msg, (mssg*) message, sizeof(mssg));
                printf("client(<--Creat):: message [size:%d contents:( tag:%d type:%d size:%d pinum:%d inum:%d block:%d name:%s buffer:%s)]\n", rc, msg.tag, msg.type, msg.size, msg.pinum, msg.inum, msg.block, msg.name, msg.buffer); 
                return msg.tag;
            }
        }
        printf("client:: waiting 5 sec\n");
    } 
    printf("client:: lookUp done \n");
    return -1; 
}

//tag = 7
int MFS_Shutdown()
{
    mssg msg = {0, 0, 0, 0, 0, 0, "\0", "\0"};
    msg.tag = 7;

    char message[sizeof(mssg)];
    memcpy((mssg*)message, &msg, sizeof(mssg)); 
    printf("client(Shutdown-->):: message [size:%d contents:( tag:%d type:%d size:%d pinum:%d inum:%d block:%d name:%s buffer:%s)]\n", rc, msg.tag, msg.type, msg.size, msg.pinum, msg.inum, msg.block, msg.name, msg.buffer);
    rc = UDP_Write(sd, &addrSnd, message, MFS_BLOCK_SIZE); 
    if(rc < 0) {
    printf("client:: failed to send %d\n", rc);
    exit(1);
    } 

    while(1) {
        //reset select() interface values everytime it returns
        fd_set rfds;
        struct timeval tv;
        int rd_ready;

        FD_ZERO(&rfds);
        FD_SET(sd, &rfds); //set select to monitor sd for reading
        tv.tv_sec = 5;
        tv.tv_usec = 0;

        rd_ready = select(sd+1, &rfds, NULL, NULL, &tv); //declare sd to monitor sd only for reading.

        switch (rd_ready) {
            case -1:
                perror("select().\n sending a new request\n");
                rc = UDP_Write(sd, &addrSnd, message, MFS_BLOCK_SIZE); 
                break;
            case 0:
                printf("select() returns 0.\n sending a new request\n");
                rc = UDP_Write(sd, &addrSnd, message, MFS_BLOCK_SIZE); 
                break;  
            default: {
				printf("client reading\n"); 
                rc = UDP_Read(sd, &addrRcv, message, MFS_BLOCK_SIZE);
				printf("client reading complete\n");
                if(rc < 0)
                    return -1;
                memcpy(&msg, (mssg*) message, sizeof(mssg));
                printf("client(<--Shutdown):: message [size:%d contents:( tag:%d type:%d size:%d pinum:%d inum:%d block:%d name:%s buffer:%s)]\n", rc, msg.tag, msg.type, msg.size, msg.pinum, msg.inum, msg.block, msg.name, msg.buffer); 
                if(msg.tag == -1)
                    return -1;
                return UDP_Close(sd);
            }
        }
        printf("client:: waiting 5 sec\n");
    } 
    printf("client:: Shutdown done \n"); 
    return UDP_Close(sd);
}

