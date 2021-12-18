#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "udp.h"
#include "mfs.h"

#define DIR_NUMBER (128)
#define DP_NUMBER (14)
#define INODE_NUMBER (16)
#define IMAP_NUMBER (256)

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
    int im[256];
    int end;
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
int sd;
cp checkPoint;
struct sockaddr_in addr;

void print() {
printf("----------------------------------------------------------------------------\n");
printf("PRINTING DISK IMAGE: \n\n");
    char buffer[MFS_BLOCK_SIZE];
    for(int i = 0; i < 256; i++) {
        if(checkPoint.im[i] != 0) {
            printf("imap: %d %d\n", checkPoint.im[i], checkPoint.im[i]*MFS_BLOCK_SIZE); 
            imap im; 
            pread(fd, buffer, sizeof(imap),checkPoint.im[i]*MFS_BLOCK_SIZE);
            memcpy(&im, (imap*)buffer, sizeof(imap)); 
            for(int j = 0; j < INODE_NUMBER; j++) {
                //if(im.inodes[j] != 0) {
                    inode n; 
                    pread(fd, buffer, sizeof(inode), im.inodes[j]*MFS_BLOCK_SIZE); 
                    memcpy(&n, (inode*)buffer, sizeof(inode));
                    printf("\tinode: inum %d addr %d type %d size %d\n", j, im.inodes[j], n.type, n.size);
                    for(int z = 0; z < (((n.size+1)/MFS_BLOCK_SIZE) && 14); z++) { 
                        if(n.type == MFS_DIRECTORY) {
                            dir d; 
                            pread(fd, buffer, sizeof(dir), n.dp[0]*MFS_BLOCK_SIZE);
                            memcpy(&d, (dir*)buffer, sizeof(dir));
                            for(int y = 0; y < DIR_NUMBER; y++) {
                                if(d.entries[y].inum != -1) {
                                    printf("\t\t\tdirectory entry: inum %d name -%s-\n",d.entries[y].inum, d.entries[y].name);
                                }
                            }
                        } else {
                            pread(fd, buffer, MFS_BLOCK_SIZE, n.dp[z]*MFS_BLOCK_SIZE);
                            printf("\t\tdp: %d %d -%s-\n", z, n.dp[z], buffer);
                        }
                    }
               // }
            }
        }
    }
printf("----------------------------------------------------------------------------\n");
}

mssg lookUp(mssg m) {
    mssg msg = {0, 0, 0, 0, 0, 0, "\0", "\0"};
    printf("server:: Processing lookUp()\n");
    char buffer[MFS_BLOCK_SIZE];

    //get the index for chunk of imap
    int imapIndex = m.pinum/INODE_NUMBER;

    //get imap index
    int inodeIndex = m.pinum % INODE_NUMBER;
    printf("imapIndex %d inodeIndex %d \n", imapIndex, inodeIndex);
    
    print();

    imap im; 
    pread(fd, buffer, sizeof(imap),checkPoint.im[imapIndex]*MFS_BLOCK_SIZE);
    memcpy(&im, (imap*)buffer, sizeof(imap));
    printf("imap: %d inodeIndex: %d imap[0]: %d imap[1] %d\n", checkPoint.im[imapIndex],inodeIndex, im.inodes[0], im.inodes[1]);

    printf("\tim.inodes[inodeIndex] %d m.pinum %d\n", im.inodes[inodeIndex], m.pinum);
    inode n; 
    pread(fd, buffer, sizeof(inode), im.inodes[inodeIndex]*MFS_BLOCK_SIZE); 
    memcpy(&n, (inode*)buffer, sizeof(inode));
    printf("\tinode n info: type: %d size: %d n.dp[0] %d\n", n.type, n.size, n.dp[0]);
    
    //Check if file is directory if not return -1
    if(n.type != MFS_DIRECTORY) { 
        printf("not a directory\n");
        msg.tag = -1; return msg; 
    }

    //create directory object to iterate over
    dir d; 
    pread(fd, buffer, sizeof(dir), n.dp[0]*MFS_BLOCK_SIZE);
    memcpy(&d, (dir*)buffer, sizeof(dir));

    //May have to iterate over all blocks in inode if directory
    //takes up more than one block
    for(int j = 0; j < DIR_NUMBER; j++) {
        //printf("\t\t\tdir ent: name %s inum %d\n",d.entries[j].name, d.entries[j].inum);
        //See if name of file entry matches name if so return m.inum
        if(strcmp(d.entries[j].name,m.name) == 0) {
            //printf("Inum of m.name: %d\n", d.entries[j].inum);
            msg.inum = d.entries[j].inum;
            print();
            return msg;
        }
    }

    //Not found return -1
	//printf("not found\n");
    msg.tag = -1;
    print();
    return msg;
}

mssg Stat(mssg m) {
    mssg msg = {0, 0, 0, 0, 0, 0, "\0", "\0"};
    printf("server:: Processing stat()\n");    
    char buffer[MFS_BLOCK_SIZE];
    
    //get the index for chunk of imap
    int imapIndex = m.inum/INODE_NUMBER;

    //get imap index
    int inodeIndex = m.inum % INODE_NUMBER;
    //printf("imapIndex %d inodeIndex %d \n", imapIndex, inodeIndex);
    
    //print();
    imap im; 
    pread(fd, buffer, sizeof(imap),checkPoint.im[imapIndex]*MFS_BLOCK_SIZE);
    memcpy(&im, (imap*)buffer, sizeof(imap));
    //printf("imap: %d inodeIndex: %d imap[0]: %d imap[1] %d\n", checkPoint.im[imapIndex],inodeIndex, im.inodes[0], im.inodes[1]);
    
    if(im.inodes[inodeIndex] == 0) {
        msg.tag = -1;
        return msg;
    }
    
    inode n;
    pread(fd, buffer, sizeof(inode), im.inodes[inodeIndex]*MFS_BLOCK_SIZE);
    memcpy(&n, (inode*)buffer, sizeof(inode)); 
    //printf("inode n info: type: %d size: %d n.dp[0] %d\n", n.type, n.size, n.dp[0]);

    //Pass info into message
    msg.type = n.type;
    msg.size = n.size; 
    //printf("MFS_STAT info: type: %d size: %d\n", msg.type, msg.size);
    //print();
    return msg;
}

mssg fWrite(mssg m) {
    mssg msg = {0, 0, 0, 0, 0, 0, "\0", "\0"};
    printf("server:: Processing fWrite()\n");
    char buffer[MFS_BLOCK_SIZE];

    printf("%s\n", m.buffer);
    
    //get the index for chunk of imap
    int imapIndex = m.inum/INODE_NUMBER;

    //get imap index
    int inodeIndex = m.inum % INODE_NUMBER;
    printf("imapIndex %d inodeIndex %d m.block %d\n", imapIndex, inodeIndex, m.block);
     
    imap im; 
    pread(fd, buffer, sizeof(imap),checkPoint.im[imapIndex]*MFS_BLOCK_SIZE);
    memcpy(&im, (imap*)buffer, sizeof(imap));
    
    print();

    if(im.inodes[inodeIndex] == 0) {
        msg.tag = -1;
        return msg;
    }

    printf("\tinode match: inodeIndex %d m.pinum %d im.inodes[inodesIndex] %d\n",inodeIndex, m.inum, im.inodes[inodeIndex]);
    inode n;
    pread(fd, buffer, sizeof(inode), im.inodes[inodeIndex]*MFS_BLOCK_SIZE);
    memcpy(&n, (inode*)buffer, sizeof(inode));

    printf("\tinode n info: type %d size %d n.dp[0] %d\n", n.type, n.size, n.dp[0]);

    //Check if directory
    if(n.type == MFS_DIRECTORY || n.size/MFS_BLOCK_SIZE > 14) {
        msg.tag = -1;
        return msg;
    }

    //Write m.buffer to block; buffer is size of MFS_BLOCK_SIZE.
    printf("WRITING -%s- to %d \n", m.buffer, checkPoint.end/MFS_BLOCK_SIZE);
    pwrite(fd, m.buffer, MFS_BLOCK_SIZE, checkPoint.end);
    n.dp[m.block] = (checkPoint.end/MFS_BLOCK_SIZE);
    checkPoint.end += MFS_BLOCK_SIZE;
    printf("\t\t\tNewBlock address %d n.dp[0] %d\n", n.dp[m.block]*MFS_BLOCK_SIZE, n.dp[m.block]);

    //Update Inode size
    if(n.size < ((m.block+1)*MFS_BLOCK_SIZE))
        n.size = ((m.block+1)*MFS_BLOCK_SIZE); 
    
    //Write inode
    memcpy((inode*)buffer, &n, sizeof(inode));
    pwrite(fd,buffer,sizeof(inode),checkPoint.end);
    printf("\t\t\tnewInode address %d %d\n", checkPoint.end, checkPoint.end/MFS_BLOCK_SIZE);

    //Update Imap
    im.inodes[inodeIndex] = (checkPoint.end/MFS_BLOCK_SIZE);
    checkPoint.end += MFS_BLOCK_SIZE;
    printf("\t\t\tinodeInode, im.inodes[newInum] %d %d\n", inodeIndex, im.inodes[inodeIndex]);

    //Write imap
    memcpy((imap*)buffer, &im, sizeof(imap));
    pwrite(fd,buffer,sizeof(imap*),checkPoint.end);

    //Update checkPoint
    checkPoint.im[imapIndex] = (checkPoint.end/MFS_BLOCK_SIZE);
    printf("\t\t\timap address: %d %d %d\n", imapIndex, checkPoint.end, checkPoint.im[imapIndex]);
    checkPoint.end += MFS_BLOCK_SIZE;
    printf("\t\t\tcheckPoint.end %d %d\n", checkPoint.end, (checkPoint.end/MFS_BLOCK_SIZE));
    
    //Write checkpoint
    memcpy((cp*)buffer, &checkPoint, sizeof(cp));
    pwrite(fd,buffer,sizeof(cp),0);
    //printf("o.inodes[0] %d o.inodes[1] %d checkPoint.im[imapIndex] %d checkPoint.end %d\n", im.inodes[0], im.inodes[1], checkPoint.im[imapIndex], (checkPoint.end/MFS_BLOCK_SIZE));
    
    fsync(fd);
    print();
    return msg;
}

mssg fRead(mssg m) {
    mssg msg = {0, 0, 0, 0, 0, 0, "\0", "\0"};
    printf("server:: Processing fRead()\n");
    char buffer[MFS_BLOCK_SIZE];

    //get the index for chunk of imap
    int imapIndex = m.inum/INODE_NUMBER;

    //get imap index
    int inodeIndex = m.inum % INODE_NUMBER;
    //printf("imapIndex %d inodeIndex %d \n", imapIndex, inodeIndex);
     
    imap im; 
    pread(fd, buffer, sizeof(imap),checkPoint.im[imapIndex]*MFS_BLOCK_SIZE);
    memcpy(&im, (imap*)buffer, sizeof(imap));

    print();

    if(im.inodes[inodeIndex] == 0) {
        msg.tag = -1;
        return msg;
    }

    //printf("\t\tinode match: inodeIndex %d m.pinum %d im.inodes[inodesIndex] %d\n",inodeIndex, m.inum, im.inodes[inodeIndex]);
    inode n;
    pread(fd, buffer, sizeof(inode), im.inodes[inodeIndex]*MFS_BLOCK_SIZE);
    memcpy(&n, (inode*)buffer, sizeof(inode));            
    //printf("\t\tinode n info: type %d size %d n.dp[m.block] %d\n", n.type, n.size, n.dp[m.block]);
    
    if( n.size/MFS_BLOCK_SIZE > DP_NUMBER) {
        msg.tag = -1;
        return msg;
    }

    //Check if directory
    if(n.type == MFS_DIRECTORY) { 
        //Set type to directory and read in block to directory
        //object before copying it to buffer
        msg.type = MFS_DIRECTORY; dir d;
        //TODO: This may be redundant should Play around see whats
        //up I know it should return an array of directory entries
        pread(fd, buffer, MFS_BLOCK_SIZE, n.dp[m.block]*MFS_BLOCK_SIZE);        
        memcpy(&d, (dir*)buffer, sizeof(dir));
        memcpy((dir*)msg.buffer, &d, sizeof(dir));
        print();
        return msg;
    } else { 
        //Set type to regular file and read in block
        msg.type = MFS_REGULAR_FILE;
        pread(fd, buffer, MFS_BLOCK_SIZE, n.dp[m.block]*MFS_BLOCK_SIZE); 
        memcpy(msg.buffer, buffer, MFS_BLOCK_SIZE); 
        //printf("buffer: %s\n", buffer);
        print();
        return msg;
    }
   
    msg.tag = -1;
    //print();
    return msg;
}

dir creatDir(int pinum, int curInum) {
    dir initDr;

    //Initialize initial directory structure    
    MFS_DirEnt_t parent = {"..", pinum};
    MFS_DirEnt_t child = {".", curInum};
    initDr.entries[0] = parent; 
    initDr.entries[1] = child;

    for(int i = 2; i < DIR_NUMBER; i++) {
		MFS_DirEnt_t empty;
        memset(empty.name, '\0', 24*sizeof(char));
        empty.inum = -1;
        initDr.entries[i] = empty;
    }

    return initDr;
}

//TODO:
mssg Creat(mssg m) {
    mssg msg = {0, 0, 0, 0, 0, 0, "\0", "\0"};
    printf("server:: Processing Creat()\n");
    char buffer[MFS_BLOCK_SIZE];

    int imapIndex = m.pinum/INODE_NUMBER;

    //get imap index
    int inodeIndex = m.pinum % INODE_NUMBER;
    printf("imapIndex %d inodeIndex %d \n", imapIndex, inodeIndex);

	printf("imapIndex %d imap number %d\n",imapIndex, checkPoint.im[imapIndex]);
    imap o; 
    pread(fd, buffer, sizeof(imap),checkPoint.im[imapIndex]*MFS_BLOCK_SIZE);
    memcpy(&o, (imap*)buffer, sizeof(imap));

    print();

    //check if inode exists
    if(o.inodes[inodeIndex] == 0) {
        msg.tag = -1;
        return msg;
    }

    printf("\tinode match: inodeIndex %d m.pinum %d o.inodes[inodesIndex] %d\n",inodeIndex, m.pinum, o.inodes[inodeIndex]);
    inode n; 
    pread(fd, buffer, sizeof(inode), o.inodes[inodeIndex]*MFS_BLOCK_SIZE); 
    memcpy(&n,(inode*)buffer, sizeof(inode));
    printf("\tinode n info: type %d size %d n.dp[0] %d\n", n.type, n.size, n.dp[0]); 

    if(n.type != MFS_DIRECTORY) {
        msg.tag = -1;
        return msg;
    }

    //create directory object to iterate over
    dir d; 
    pread(fd, buffer, sizeof(dir), n.dp[0]*MFS_BLOCK_SIZE);
    memcpy(&d, (dir*)buffer, sizeof(dir));

    int count = 0;
    for(int i = 0; i < DP_NUMBER; i++) {

        //Check if name already exist
        for(int j = 0; j < DIR_NUMBER; j++) {
            printf("%d\n", count);
            count++;
            printf("\t\t\tdirectory entry %d -%s-\n",d.entries[j].inum, d.entries[j].name);
            if(strcmp(d.entries[j].name,m.name) == 0) 
                //print();
                return msg;
            if(d.entries[j].inum == -1) {
                int newInum = d.entries[j-1].inum + 1;
                printf("\t\t\tpInum %d nInum %d\n",d.entries[j-1].inum, newInum);
                if(m.type == MFS_DIRECTORY) {

                    //Create new directory
                    dir newDir = creatDir(m.pinum,newInum);

                    //Create new inode
                    inode in;
                    in.type = MFS_DIRECTORY;
                    in.size = MFS_BLOCK_SIZE;
                    in.dp[0] = (checkPoint.end/MFS_BLOCK_SIZE);
                    printf("\t\t\tNew Inode: type %d size %d dp[0] %d\n", in.type, in.size, in.dp[0]); 

                    //Write new Directory block
                    memcpy((dir*)buffer, &newDir, sizeof(dir));
                    pwrite(fd, buffer, sizeof(dir), checkPoint.end);
                    printf("\t\t\tnewDir address %d %d\n", checkPoint.end, checkPoint.end*MFS_BLOCK_SIZE);
                    checkPoint.end += MFS_BLOCK_SIZE;

                    //Write new inode
                    memcpy((inode*)buffer, &in, sizeof(inode));
                    pwrite(fd, buffer, sizeof(inode), checkPoint.end);
                    printf("\t\t\tnewInode address %d %d\n", checkPoint.end, checkPoint.end/MFS_BLOCK_SIZE);
                    int nInodePoint = checkPoint.end;
                    checkPoint.end += MFS_BLOCK_SIZE;

                    if(j == 13 && i != 13) {
                        dir newDirBlock;
                        for(int e = 0; e < DIR_NUMBER; e++) {
		                    MFS_DirEnt_t empty;
                            memset(empty.name, '\0', 24*sizeof(char));
                            empty.inum = -1;
                            newDirBlock.entries[e] = empty;
                        }
                        //Write newDirectoryBlock
                        memcpy((dir*)buffer, &newDirBlock, sizeof(dir));
                        pwrite(fd, buffer, sizeof(dir), checkPoint.end);
                        n.dp[i+1] = checkPoint.end/MFS_BLOCK_SIZE;
                        printf("\t\t\tpDir address %d n.dp[i+1] %d\n", checkPoint.end, n.dp[i+1]);
                        checkPoint.end += MFS_BLOCK_SIZE;
                    }

                    memcpy(d.entries[j].name,m.name,24);
                    d.entries[j].inum = newInum;
                    //printf("\t\t\tnew dir_ent: name: %s inum: %d\n", d.entries[j].name, d.entries[j].inum);

                    //Write pDirectory
                    memcpy((dir*)buffer, &d, sizeof(dir));
                    pwrite(fd, buffer, sizeof(dir), checkPoint.end);
                    n.dp[i] = checkPoint.end/MFS_BLOCK_SIZE;
    //create directory object to iterate over
                    //printf("\t\t\tpDir address %d n.dp[0] %d\n", n.dp[0]*MFS_BLOCK_SIZE, n.dp[0]);
                    checkPoint.end += MFS_BLOCK_SIZE;

                    //Write pInode
                    memcpy((inode*)buffer, &n, sizeof(inode));
                    pwrite(fd, buffer, sizeof(inode),checkPoint.end);
                    //printf("\t\t\tpInode address %d %d\n",checkPoint.end, (checkPoint.end/MFS_BLOCK_SIZE));
                    int pInodePoint = checkPoint.end;
                    checkPoint.end += MFS_BLOCK_SIZE;

                    //Update imap
                    o.inodes[newInum] = (nInodePoint/MFS_BLOCK_SIZE);
                    o.inodes[inodeIndex] = (pInodePoint/MFS_BLOCK_SIZE);
                    //printf("\t\t\tnInode, o.inodes[newInum] %d %d pInode, o.inodes[inodeIndex] %d %d \n", newInum, o.inodes[newInum], inodeIndex, o.inodes[inodeIndex]);

                    //Write imap
                    memcpy((imap*)buffer, &o, sizeof(imap));
                    pwrite(fd,buffer,sizeof(imap), checkPoint.end);

                    //Update checkPoint
                    checkPoint.im[imapIndex] = (checkPoint.end/MFS_BLOCK_SIZE);
                    //printf("\t\t\timap address: %d %d %d\n", imapIndex, checkPoint.end, checkPoint.im[imapIndex]);
                    checkPoint.end += MFS_BLOCK_SIZE;
                    //printf("\t\t\tcheckPoint.end %d %d\n", checkPoint.end, (checkPoint.end/MFS_BLOCK_SIZE));

                    //Write checkpoint
                    memcpy((cp*)buffer, &checkPoint, sizeof(cp));
                    pwrite(fd,buffer,sizeof(cp),0);
     
                    //printf("o.inodes[0] %d o.inodes[1] %d checkPoint.im[imapIndex] %d checkPoint.end %d\n", o.inodes[0], o.inodes[1], checkPoint.im[imapIndex], (checkPoint.end/MFS_BLOCK_SIZE));
                    print();
                    fsync(fd);
                    return msg;  
                } else {

                    //Create new inode
                    inode in;
                    in.type = MFS_REGULAR_FILE;
                    in.size = 0;
                    in.dp[0] = (checkPoint.end/MFS_BLOCK_SIZE);
                    printf("\t\t\tNew Inode: type %d size %d dp[0] %d\n", in.type, in.size, in.dp[0]); 

                    //Reserve Block at end of log for new file
                    checkPoint.end += MFS_BLOCK_SIZE;

                    //Write new inode
                    memcpy((inode*)buffer, &in, sizeof(inode));
                    pwrite(fd, buffer, sizeof(inode), checkPoint.end);
                    printf("\t\t\tnewInode address %d %d\n", checkPoint.end, checkPoint.end/MFS_BLOCK_SIZE);
                    int nInodePoint = checkPoint.end;
                    checkPoint.end += MFS_BLOCK_SIZE;
                    /*
                    if(j == 13 && i != 13) {
                        dir newDirBlock;
                        for(int e = 0; e < DIR_NUMBER; e++) {
		                    MFS_DirEnt_t empty;
                            memset(empty.name, '\0', 24*sizeof(char));
                            empty.inum = -1;
                            newDirBlock.entries[e] = empty;
                        }
                        //Write newDirectoryBlock
                        memcpy((dir*)buffer, &newDirBlock, sizeof(dir));
                        pwrite(fd, buffer, sizeof(dir), checkPoint.end);
                        n.dp[i+1] = checkPoint.end/MFS_BLOCK_SIZE;
                        printf("\t\t\tNew DB address %d n.dp[i+1] %d\n", checkPoint.end, n.dp[i+1]);
                        checkPoint.end += MFS_BLOCK_SIZE;
                    }
                    */
                    printf("\t\t\tnew file: name %s, inum %d\n", m.name, newInum);
                    memcpy(d.entries[j].name,m.name,24);
                    d.entries[j].inum = newInum;
                   
                    //Write pDirectory
                    memcpy((dir*)buffer, &d, sizeof(dir));
                    pwrite(fd, buffer, sizeof(dir), checkPoint.end);
                    n.dp[i] = checkPoint.end/MFS_BLOCK_SIZE;
                    printf("\t\t\tpDir address %d n.dp[i] %d\n", checkPoint.end, n.dp[i]);
                    checkPoint.end += MFS_BLOCK_SIZE;

                    //Write pInode
                    memcpy((inode*)buffer, &n, sizeof(inode));
                    pwrite(fd, buffer, sizeof(inode),checkPoint.end);
                    printf("\t\t\tpInode address %d %d\n",checkPoint.end, (checkPoint.end/MFS_BLOCK_SIZE));
                    int pInodePoint = checkPoint.end;
                    checkPoint.end += MFS_BLOCK_SIZE;

                    //Update imap
                    o.inodes[newInum] = (nInodePoint/MFS_BLOCK_SIZE);
                    o.inodes[inodeIndex] = (pInodePoint/MFS_BLOCK_SIZE);
                    printf("\t\t\tnInode, o.inodes[newInum] %d %d pInode, o.inodes[inodeIndex] %d %d \n", newInum, o.inodes[newInum], inodeIndex, o.inodes[inodeIndex]);

                    //Write imap
                    memcpy((imap*)buffer, &o, sizeof(imap));
                    pwrite(fd,buffer,sizeof(imap), checkPoint.end);

                    //Update checkPoint
                    checkPoint.im[imapIndex] = (checkPoint.end/MFS_BLOCK_SIZE);
                    printf("\t\t\timap address: %d %d %d\n", imapIndex, checkPoint.end, checkPoint.im[imapIndex]);
                    checkPoint.end += MFS_BLOCK_SIZE;
                    printf("\t\t\tcheckPoint.end %d %d\n", checkPoint.end, (checkPoint.end/MFS_BLOCK_SIZE));

                    //Write checkpoint
                    memcpy((cp*)buffer, &checkPoint, sizeof(cp));
                    pwrite(fd,buffer,sizeof(cp),0);

                    //pread(fd, buffer, sizeof(imap),checkPoint.im[0]*MFS_BLOCK_SIZE);
                    //memcpy(&o, (imap*)buffer, sizeof(imap));

                    //printf("o.inodes[0] %d o.inodes[1] %d checkPoint.im[imapIndex] %d checkPoint.end %d\n", o.inodes[0], o.inodes[1], checkPoint.im[imapIndex], (checkPoint.end/MFS_BLOCK_SIZE));
                    print();
                    fsync(fd);
                    return msg;  
                }
            }              
        }
        
        //create directory object to iterate over 
        pread(fd, buffer, sizeof(dir), n.dp[i+1]*MFS_BLOCK_SIZE);
        memcpy(&d, (dir*)buffer, sizeof(dir));
        count++;
    }
    //not found return -1
    msg.tag = -1;
    print();
    return msg;
}

//TODO:
mssg Unlink(mssg m) {
    mssg msg = {0, 0, 0, 0, 0, 0, "\0", "\0"};
    printf("server:: Processing Unlink()\n");
    char buffer[MFS_BLOCK_SIZE];

    //get the index for chunk of imap
    int imapIndex = m.pinum/INODE_NUMBER;

    //get imap index
    int inodeIndex = m.pinum % INODE_NUMBER;
    //printf("imapIndex %d inodeIndex %d \n", imapIndex, inodeIndex);

    print();
     
    imap im; 
    pread(fd, buffer, sizeof(imap),checkPoint.im[imapIndex]*MFS_BLOCK_SIZE);
    memcpy(&im, (imap*)buffer, sizeof(imap));

    if(im.inodes[inodeIndex] == 0) {
        msg.tag = -1;
        return msg;
    }

    inode n;
    pread(fd, buffer, sizeof(inode), im.inodes[inodeIndex]*MFS_BLOCK_SIZE);
    memcpy(&n, (inode*)buffer, sizeof(inode));
    //printf("\t\tinode n info: type %d size %d n.dp[m.block] %d\n", n.type, n.size, n.dp[m.block]);
 
    //create directory object to iterate over
    dir d; 
    pread(fd, buffer, sizeof(dir), n.dp[0]*MFS_BLOCK_SIZE);
    memcpy(&d, (dir*)buffer, sizeof(dir));

    //May have to iterate over all blocks in inode if directory
    //takes up more than one block
    int unlinkedInum;
    for(int j = 1; j < DIR_NUMBER; j++) {
        //printf("\t\tdirectory entry %d -%s-\n",d.entries[j].inum, d.entries[j].name);

        //See if name of file entry matches name if so return m.inum
        if(strcmp(d.entries[j].name,m.name) == 0) {
            unlinkedInum = d.entries[j].inum;

            //Get unlinkedInode to access
            inode unlinkedInode;
            pread(fd, buffer, sizeof(inode), im.inodes[unlinkedInum]*MFS_BLOCK_SIZE); 
            memcpy(&unlinkedInode,(inode*)buffer, sizeof(inode));
            //printf("\t\tunlinkedInode: type %d size %d unlinkedInum %d im.inodes[unlinkedInum] %d\n",unlinkedInode.type, unlinkedInode.size, unlinkedInum, im.inodes[unlinkedInum]);

            if(unlinkedInode.type == MFS_DIRECTORY) { 
                //pull directory of unlinked inode to iterate over
                dir unlinkedDir; 
                pread(fd, buffer, sizeof(dir), unlinkedInode.dp[0]*MFS_BLOCK_SIZE);
                memcpy(&unlinkedDir, (dir*)buffer, sizeof(dir));

                //check if inode is directory and if so the directory is not empty; return -1
                if(unlinkedDir.entries[2].inum != -1) {
                    msg.tag = -1;
                    //print();
                    return msg;
                }
            }

            //set name to null by setting all values to "\0" using memset
            d.entries[j].inum = -1;
            memset(d.entries[j].name, '\0', 24*sizeof(char));

            //clear imap entry of unlinked inode
            im.inodes[unlinkedInum] = 0;

            //Write Directory
            memcpy((dir*)buffer, &d, sizeof(d));
            pwrite(fd,buffer, sizeof(dir), n.dp[0]*MFS_BLOCK_SIZE);

            //copy and write inode
            memcpy((inode*)buffer, &n, sizeof(inode));
            pwrite(fd,buffer,sizeof(inode),checkPoint.end);
            //printf("\t\t\tInode n address %d %d\n", checkPoint.end, checkPoint.end/MFS_BLOCK_SIZE);

            //Update Imap
            im.inodes[inodeIndex] = (checkPoint.end/MFS_BLOCK_SIZE);
            //printf("\t\t\tcheckPoint.end %d im.inodes[inodeIndex] %d\n", checkPoint.end/MFS_BLOCK_SIZE, im.inodes[inodeIndex]);
            checkPoint.end += MFS_BLOCK_SIZE;

            //Write imap
            memcpy((imap*)buffer, &im, sizeof(imap));
            pwrite(fd,buffer,sizeof(imap),checkPoint.end);
            //printf("\t\t\timap address: %d %d %d\n", imapIndex, checkPoint.end, checkPoint.im[imapIndex]);

            //Update checkPoint
            checkPoint.im[imapIndex] = (checkPoint.end/MFS_BLOCK_SIZE); 
            checkPoint.end += MFS_BLOCK_SIZE;
            //printf("\t\t\tcheckPoint.end %d %d\n", checkPoint.end, (checkPoint.end/MFS_BLOCK_SIZE));

            //Write checkpoint
            memcpy((cp*)buffer, &checkPoint, sizeof(cp));
            pwrite(fd,buffer,sizeof(cp),0);

            print();
            fsync(fd);
            return msg;
        }
    }

//case name not existing return 0;
//print();
return msg; 
}

void Shutdown() {
    fsync(fd);
    close(fd);
    char reply[MFS_BLOCK_SIZE];
    mssg msg = {0, 0, 0, 0, 0, 0, "\0", "\0"};

	memcpy((mssg*)reply, &msg, sizeof(mssg));
    int rc = UDP_Write(sd, &addr, reply, MFS_BLOCK_SIZE);
	if (rc == -1) {
		exit(1);
	}
    UDP_Close(sd);
    exit(0);
}

void createImage(char* pathname) {
    fd = open(pathname, O_RDWR | O_CREAT | O_SYNC, 0666); 
    char buffer[MFS_BLOCK_SIZE];

    //Initialize Directory
    dir dr = creatDir(0,0);

    //Initialize initial Inode
    inode i;
    i.type = MFS_DIRECTORY;
    i.size = MFS_BLOCK_SIZE;
    i.dp[0] = 1;
	printf("initial Inode %d %d %d\n", i.type, i.size, i.dp[0]);

    //Initialize initial Inode map
    imap im;
    im.inodes[0] = 2;
	printf("init inode %d\n",im.inodes[0]);

    //Write Directory
    memcpy((dir*)buffer, &dr, sizeof(dir));
    pwrite(fd,buffer,sizeof(dir),i.dp[0]*MFS_BLOCK_SIZE);
	printf("initDir Address %d\n", i.dp[0]*MFS_BLOCK_SIZE);

    //Write inode
    memcpy((inode*)buffer, &i, sizeof(inode));
    pwrite(fd,buffer,sizeof(inode),im.inodes[0]*MFS_BLOCK_SIZE);
	printf("initInode Address %d\n", im.inodes[0]*MFS_BLOCK_SIZE);
 
    //Assign checkpoint imap number 
    checkPoint.im[0] = 3;
	printf("initImap number %d\n",checkPoint.im[0]);

    //Write imap
    memcpy((imap*)buffer, &im, sizeof(imap));
    pwrite(fd,buffer,sizeof(imap),checkPoint.im[0]*MFS_BLOCK_SIZE);
	printf("initImap address %d\n",checkPoint.im[0]*MFS_BLOCK_SIZE);

    //Initialize initial CheckPoint should be 4*4Kb
    checkPoint.end = 4*MFS_BLOCK_SIZE;
    printf("init checkPoint.end %d %d\n",checkPoint.end, checkPoint.end/MFS_BLOCK_SIZE);
    
    //write checkpoint
    memcpy((cp*)buffer, &checkPoint, sizeof(cp));
    pwrite(fd,buffer,sizeof(cp),0);
    print();
    fsync(fd);
}

void loadImage(char* pathname) {
    printf("=%s_\n",pathname);
    fd = open(pathname, O_RDWR | O_SYNC, 0666);
	if (fd == -1) {
	    perror("open");	
		createImage(pathname);
	}
    char buffer[MFS_BLOCK_SIZE];

    //Load checkPoint
    pread(fd, buffer, sizeof(cp), 0);
    memcpy(&checkPoint, (cp*)buffer, sizeof(cp));
    printf("init checkPoint.end %d %d\n",checkPoint.end, checkPoint.end/MFS_BLOCK_SIZE);
    print();
}

// server code
int main(int argc, char *argv[]) {
    if(argc < 2)
        return -1;
    if(argv[2] == NULL) {
		printf("argv[2] null, creating image\n");
        createImage("testimage");
    } else {
		printf("loading image %s\n", argv[2]);
        loadImage(argv[2]);
    }

    sd = UDP_Open(strtol(argv[1],NULL,10));
    assert(sd > -1);
    while (1) {
	//struct sockaddr_in addr;
	mssg r_msg;
	char message[sizeof(mssg)];
	printf("server:: waiting...\n");

	int rc = UDP_Read(sd, &addr, message, sizeof(mssg));
	memcpy(&r_msg, (mssg*) message, sizeof(mssg));
	printf("server:: read message [size:%d contents:( tag:%d type:%d size:%d pinum:%d inum:%d block:%d name:%s buffer:%s)]\n", rc, r_msg.tag, r_msg.type, r_msg.size, r_msg.pinum, r_msg.inum, r_msg.block, r_msg.name, r_msg.buffer); 
    if(rc > 0) {
        mssg s_msg;
        switch(r_msg.tag) {
            case 1:
                s_msg = lookUp(r_msg); 
                break;
            case 2:
                s_msg = Stat(r_msg); 
                break;
            case 3:
                //printf("r_msg.buffer: -%s-\n", r_msg.buffer);
                s_msg = fWrite(r_msg);
                //printf("s_msg.buffer: -%s-\n", s_msg.buffer);
                break;
            case 4:
                s_msg = fRead(r_msg);
                //printf("s_msg.buffer -%s-\n", s_msg.buffer);
                break;
            case 5:
                s_msg = Creat(r_msg);
                break;
            case 6:
                s_msg = Unlink(r_msg);
                break;
            case 7:
                Shutdown();
        }
        
        char reply[sizeof(mssg)];
	    printf("server:: send message [size:%d contents:( tag:%d type:%d size:%d pinum:%d inum:%d block:%d name:%s buffer:%s)]\n", rc, s_msg.tag, s_msg.type, s_msg.size, s_msg.pinum, s_msg.inum, s_msg.block, s_msg.name, s_msg.buffer);
	    memcpy((mssg*)reply, &s_msg, sizeof(mssg));
	    rc = UDP_Write(sd, &addr, reply, sizeof(mssg));
        printf("server:: reply\n");
    }

    }
    return 0; 
}    
