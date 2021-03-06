#include <stdio.h>
#include <string.h>
#include "mfs.h"

#define BUFFER_SIZE (1000)

typedef struct mssg {
    int tag;
    int type;
    int size;
    int pinum;
    int inum;
    int block;
    char name[24];
    char buffer[MFS_BLOCK_SIZE];
} mssg;

// client code
int main(int argc, char *argv[]) {
    int r; 
    r = MFS_Init("localhost", 11113);
    printf("%d\n",r);

    //r = MFS_Lookup(1, "a");
    //printf("%d\n",r);
    
    //MFS_Stat_t m; 
    //r = MFS_Stat(1,&m);
    //printf("%d\n",r);

    //char message[MFS_BLOCK_SIZE]; 
    //char buf1[MFS_BLOCK_SIZE];

    r = MFS_Creat(0,0,"test");
    printf("%d\n",r);

	r = MFS_Lookup(0, "test");
	printf("%d\n", r);

    r = MFS_Unlink(0, "test");
    printf("%d\n",r);

	r = MFS_Lookup(0, "test");
	printf("inum%d\n", r);
    //r = MFS_Read(1, buf1, 0);
    //printf("%d\n",r);

    //printf("OUTPUT FROM READ: _%s_ _%s_\n", message, buf1);
    //r = MFS_Unlink(1,"a");
    //printf("%d\n",r);

    r = MFS_Shutdown();
    printf("%d\n",r);
    // memcpy(&msg, (mssg*) message, sizeof(mssg));
    // printf("client:: got reply [contents:(%d %d %d %d %d %s %s)\n", msg.tag, msg.type, msg.pinum, msg.block, msg.inum, msg.name, msg.buffer);
    //

    return 0;
}

