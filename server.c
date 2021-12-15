#include <stdio.h>
#include <unistd.h>
#include "udp.h"
#include "mfs.h"

#define BUFFER_SIZE (1000)

typedef struct dir {
    MFS_DirEnt_t entries [128];
} dir;

typedef struct inode { 
    int type;
    int size;
    int dp[14];
} inode;

typedef struct imap {
    int inodes[16];
} imap;

typedef struct cp {
    int end;
    int im[256];
} cp;

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

int fd;
cp checkPoint;

mssg lookUp(mssg m) { mssg msg = {0, 0, 0, 0, 0, 0, "\0", "\0"};
    printf("server:: Processing lookUp()\n");
    msg.tag = 0;
    msg.inum = 9;
    return msg;
}

mssg stat(mssg m) {
    mssg msg = {0, 0, 0, 0, 0, 0, "\0", "\0"};
    printf("server:: Processing stat()\n");
    msg.tag = 0;
    msg.type = 0;
    return msg;
}

mssg fWrite(mssg m) {
    mssg msg = {0, 0, 0, 0, 0, 0, "\0", "\0"};
    printf("server:: Processing fWrite()\n");
    msg.tag = 0;
    return msg;
}

mssg fRead(mssg m) {
    mssg msg = {0, 0, 0, 0, 0, 0, "\0", "\0"};
    printf("server:: Processing fRead()\n");
    msg.inum = 9;
    strcpy(msg.name,"a");
    return msg;
}

mssg Creat(mssg m) {
    mssg msg = {0, 0, 0, 0, 0, 0, "\0", "\0"};
    printf("server:: Processing Creat()\n");
    msg.tag = 0;
    return msg;
}

mssg Unlink(mssg m) {
    mssg msg = {0, 0, 0, 0, 0, 0, "\0", "\0"};
    printf("server:: Processing Unlink()\n");
    msg.tag = 0;
    return msg;
}

dir creatDir(int pinum) {
    dir initDr;

    //Initialize initial directory structure    
    MFS_DirEnt_t parent = {"..", pinum};
    MFS_DirEnt_t child = {".", pinum};
    initDr.entries[0] = parent; 
    initDr.entries[1] = child;

    for(int i = 2; i < 128; i++) {
        initDr.entries[i] = {"\0", -1};
    }

    return initDr;
}

void loadImage(char* pathname) {
    fd = open(pathname, O_RDWR | O_SYNC);
    char buffer[MFS_BLOCK_SIZE];

    pread(fd, buffer, sizeof(cp*), 0);
    memcpy(&checkPoint, (cp*)buffer, sizeof(cp));
}

void createImage() {
    fd = open("file", O_RDWR | O_SYNC); 
    char buffer[MFS_BLOCK_SIZE];

    //Initialize Directory
    dir dr = createDir(0);

    //Initialize initial Inode
    inode i;
    i.type = MFS_DIRECTORY
    i.size = 1;
    i.dp[0] = 1;

    //Initialize initial Inode map
    imap im;
    im.entries[0] = 2;


    //Write Directory
    memcpy((dir*)buffer, &dr, sizeof(dir));
    pwrite(fd,buffer,sizeof(dir*),i.dp[0]*MFS_BLOCK_SIZE);

    //Write inode
    memcpy((inode*)buffer, &i, sizeof(i));
    pwrite(fd,buffer,sizeof(i*),im.entries[0]*MFS_BLOCK_SIZE);
 
    //
    checkpoint.im[0] = 3;

    //Write imap
    memcpy((imap*)buffer, &im, sizeof(imap));
    pwrite(fd,buffer,sizeof(imap*),checkpoint.im[0]*MFS_BLOCK_SIZE);

    struct stat sb;
    if(fstat(fd,&sb) == -1) {
        perror("stat");
        return -1;
    }
    //Initialize initial CheckPoint
    checkPoint.end = sd.st_size + MFS_BLOCK_SIZE;
    
    //write checkpoint
    memcpy((cp*)buffer, &checkPoint, sizeof(cp));
    pwrite(fd,buffer,sizeof(cp*),0);
}

// server code
int main(int argc, char *argv[]) {
    if(argc < 2)
        return -1;
    if(argv[2] == NULL) {
        createImage();
    }
    loadImage();

    int sd = UDP_Open(strtol(argv[1],NULL,10));
    assert(sd > -1);
    while (1) {
	struct sockaddr_in addr;
	mssg r_msg;
	char message[BUFFER_SIZE];
	printf("server:: waiting...\n");

	int rc = UDP_Read(sd, &addr, message, BUFFER_SIZE);
	memcpy(&r_msg, (mssg*) message, sizeof(mssg));
	printf("server:: read message [size:%d contents:( tag:%d type:%d size:%d pinum:%d inum:%d block:%d name:%s buffer:%s)]\n", rc, r_msg.tag, r_msg.type, r_msg.size, r_msg.pinum, r_msg.inum, r_msg.block, r_msg.name, r_msg.buffer); 
    if(rc > 0) {
        mssg s_msg;
        switch(r_msg.tag) {
            case 1:
                s_msg = lookUp(r_msg); 
                break;
            case 2:
                s_msg = stat(r_msg); 
                break;
            case 3:
                s_msg = fWrite(r_msg);
                break;
            case 4:
                s_msg = fRead(r_msg);
                break;
            case 5:
                s_msg = Creat(r_msg);
                break;
            case 6:
                s_msg = Unlink(r_msg);
                break;
        }
        
        char reply[BUFFER_SIZE];
	    printf("server:: send message [size:%d contents:( tag:%d type:%d size:%d pinum:%d inum:%d block:%d name:%s buffer:%s)]\n", rc, s_msg.tag, s_msg.type, s_msg.size, s_msg.pinum, s_msg.inum, s_msg.block, s_msg.name, s_msg.buffer);
	    memcpy((mssg*)reply, &s_msg, sizeof(mssg));
	    rc = UDP_Write(sd, &addr, reply, BUFFER_SIZE);
        printf("server:: reply\n");
    }

    }
    return 0; 
}    
