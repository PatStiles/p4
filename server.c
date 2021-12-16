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

mssg lookUp(mssg m) {
    mssg msg = {0, 0, 0, 0, 0, 0, "\0", "\0"};
    printf("server:: Processing lookUp()\n");
    char buffer[MFS_BLOCK_SIZE];

    //Iterate over all imaps
    int count = 0;
    while(checkPoint.im[count] != 0) {
		printf("loop\n");
        imap im; 
        pread(fd, buffer, sizeof(imap),checkPoint.im[count]*MFS_BLOCK_SIZE);
        memcpy(&im, (imap*)buffer, sizeof(imap));
		printf("imap: %d imap[0]: %d imap[1] %d\n", checkPoint.im[count], im.inodes[0], im.inodes[1]);

        //Iterate over all inodes in a imap for inum of interest
        for(int i = 0; i < INODE_NUMBER; i++) {
			printf("\tinode %d\n", im.inodes[i]);

            //Check if inode matches pinum
            if(im.inodes[i] == m.pinum) { 
				printf("\t\tinode found %d %d\n", im.inodes[i], m.pinum);
				inode n; 
				pread(fd, buffer, sizeof(inode), im.inodes[i]*MFS_BLOCK_SIZE); 
				memcpy(&n, (inode*)buffer, sizeof(inode));
                
                //Check if file is directory if not return -1
                //if(n.type != MFS_DIRECTORY) { 
				//	printf("not a directory\n");
				//	msg.tag = -1; return msg; 
				//}
                //create directory object to iterate over
                dir d; 
				pread(fd, buffer, sizeof(dir), n.dp[0]*MFS_BLOCK_SIZE);
                memcpy(&d, (dir*)buffer, sizeof(dir));

                //May have to iterate over all blocks in inode if directory
                //takes up more than one block
                for(int j = 2; j < DIR_NUMBER; j++) {

					printf("file name: %s\n",d.entries[j].name);
                    //See if name of file entry matches name if so return m.inum
                    if(strcmp(d.entries[j].name,m.name) == 0) {
                        msg.inum = d.entries[j].inum;
                        return msg;
                    }
                }
            }      
        } 
        count++;
    }

    //Not found return -1
	printf("not found\n");
    msg.tag = -1;
    return msg;
}

mssg Stat(mssg m) {
    mssg msg = {0, 0, 0, 0, 0, 0, "\0", "\0"};
    printf("server:: Processing stat()\n");    
    char buffer[MFS_BLOCK_SIZE];

    //Iterate over all imaps
    int count = 0;
    while(checkPoint.im[count] != 0 && count < IMAP_NUMBER) {
        imap im; 
        pread(fd, buffer, sizeof(imap),checkPoint.im[count]*MFS_BLOCK_SIZE);
        memcpy(&im, (imap*)buffer, sizeof(imap));

        //Iterate over all inodes in a imap for inum of interest
        for(int i = 0; i < INODE_NUMBER; i++) {

            //Check if inum matches
            if(im.inodes[i] == m.inum) {
                inode n;
                pread(fd, buffer, sizeof(inode), im.inodes[i]*MFS_BLOCK_SIZE);
                memcpy(&n, (inode*)buffer, sizeof(inode));
                
                //Pass info into message
                msg.type = n.type;
                msg.size = n.size; 
                return msg;
            }      
        }
        count++; 
    }

    msg.tag = -1;
    return msg;
}

mssg fWrite(mssg m) {
    mssg msg = {0, 0, 0, 0, 0, 0, "\0", "\0"};
    printf("server:: Processing fWrite()\n");
    char buffer[MFS_BLOCK_SIZE];
    
    //Iterate over all imaps
    int count = 0;
    while(checkPoint.im[count] != 0 && count < IMAP_NUMBER) {
        imap im; 
        pread(fd, buffer, sizeof(imap),checkPoint.im[count]*MFS_BLOCK_SIZE);
        memcpy(&im, (imap*)buffer, sizeof(imap));

        //Iterate over all inodes in a imap for inum of interest
        for(int i = 0; i < INODE_NUMBER; i++) {

            //if match check if directory or not check if null or not
            if(im.inodes[i] == m.inum) {
                inode n;
                pread(fd, buffer, sizeof(inode), im.inodes[i]*MFS_BLOCK_SIZE);
                memcpy(&n, (inode*)buffer, sizeof(inode));
                
                //Check if directory
                if(n.type == MFS_DIRECTORY) {
                    msg.tag = -1;
                    return msg;
                }

                //Write m.buffer to block; buffer is size of MFS_BLOCK_SIZE.
                pwrite(fd, m.buffer, MFS_BLOCK_SIZE, checkPoint.end);
                n.dp[m.block] = (checkPoint.end/MFS_BLOCK_SIZE)-1;
                checkPoint.end += MFS_BLOCK_SIZE;

                //Update Inode size
                if(n.size < ((m.block*MFS_BLOCK_SIZE)-1))
                    n.size = (m.block*MFS_BLOCK_SIZE)-1; 
                
                //Write inode
                memcpy((inode*)buffer, &n, sizeof(inode));
                pwrite(fd,buffer,sizeof(inode),checkPoint.end);

                //Update Imap
                im.inodes[i] = (checkPoint.end/MFS_BLOCK_SIZE)-1;
                checkPoint.end += MFS_BLOCK_SIZE;
 
                //Write imap
                memcpy((imap*)buffer, &im, sizeof(imap));
                pwrite(fd,buffer,sizeof(imap*),checkPoint.end);
                checkPoint.end += MFS_BLOCK_SIZE;

                //get stat to find size of file
                //struct stat sb;
                //if(fstat(fd,&sb) == -1) {
                //    perror("stat");
                //    exit(EXIT_FAILURE);
                //}

                //Update checkPoint
                checkPoint.im[count] = (checkPoint.end/MFS_BLOCK_SIZE)-1;
                //checkPoint.end = sb.st_size;
    
                //Write checkpoint
                memcpy((cp*)buffer, &checkPoint, sizeof(cp));
                pwrite(fd,buffer,sizeof(cp),0);

                return msg;
            }
        }

        count++;
    } 
    msg.tag = -1;
    return msg;
}

mssg fRead(mssg m) {
    mssg msg = {0, 0, 0, 0, 0, 0, "\0", "\0"};
    printf("server:: Processing fRead()\n");
    char buffer[MFS_BLOCK_SIZE];
     
    //Iterate over all imaps
    int count = 0;
    while(checkPoint.im[count] != 0 && count < IMAP_NUMBER) {
        imap im; 
        pread(fd, buffer, sizeof(imap),checkPoint.im[count]*MFS_BLOCK_SIZE);
        memcpy(&im, (imap*)buffer, sizeof(imap));

        //Iterate over all inodes in a imap for inum of interest
        for(int i = 0; i < INODE_NUMBER; i++) {

            //Check if match
            if(im.inodes[i] == m.inum) {
                inode n;
                pread(fd, buffer, sizeof(inode), im.inodes[i]*MFS_BLOCK_SIZE);
                memcpy(&n, (inode*)buffer, sizeof(inode));
                
                //Check if directory
                if(n.type == MFS_DIRECTORY) { 

                    //Set type to directory and read in block to directory
                    //object before copying it to buffer
                    msg.type = MFS_DIRECTORY; dir d;
                    //TODO: This may be redundant should Play around see whats
                    //up I know it should return an array of directory entries
                    pread(fd, buffer, sizeof(MFS_BLOCK_SIZE), n.dp[m.block]*MFS_BLOCK_SIZE);        
                    memcpy(&d, (dir*)buffer, sizeof(dir));
                    memcpy((dir*)msg.buffer, &d, sizeof(dir));
                    return msg;
                } 

                //Set type to regular file and read in block
                msg.type = MFS_REGULAR_FILE;
                pread(fd, buffer, sizeof(MFS_BLOCK_SIZE), n.dp[m.block]*MFS_BLOCK_SIZE); 
                memcpy(msg.buffer, buffer, sizeof(MFS_BLOCK_SIZE));
                return msg;
            }
        }
        count++;
    }

    msg.tag = -1;
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
		MFS_DirEnt_t empty = {"\0", -1};
        initDr.entries[i] = empty;
    }

    return initDr;
}

//TODO:
mssg Creat(mssg m) {
    mssg msg = {0, 0, 0, 0, 0, 0, "\0", "\0"};
    printf("server:: Processing Creat()\n");
    char buffer[MFS_BLOCK_SIZE];

    //Iterate over all imaps
        //Iterate over all inodes in a imap for inum of interest
    int count = 0;
    while(checkPoint.im[count] != 0) {
		printf("imap number %d\n", checkPoint.im[count]);
        imap o; 
        pread(fd, buffer, sizeof(imap),checkPoint.im[count]*MFS_BLOCK_SIZE);
        memcpy(&o, (imap*)buffer, sizeof(imap));

        for(int i = 0; i < INODE_NUMBER; i++) {
			printf("inum %d\n", o.inodes[i]);
            if(o.inodes[i] == m.pinum) {
				printf("inode match %d %d %d\n",i, o.inodes[i], m.pinum);
                inode n; 
                pread(fd, buffer, sizeof(inode), o.inodes[i]*MFS_BLOCK_SIZE); 
                memcpy(&n,(inode*)buffer, sizeof(inode));
                 
                //create directory object to iterate over
                dir d; 
                pread(fd, buffer, sizeof(dir), n.dp[0]*MFS_BLOCK_SIZE);
                memcpy(&d, (dir*)buffer, sizeof(dir));

                //Check if name already exist
                for(int j = 2; j < DIR_NUMBER; j++) {
					printf("directory entry %d -%s-\n",d.entries[j].inum, d.entries[j].name);
                    if(strcmp(d.entries[j].name,m.name) == 0) 
                        return msg;
                    if(d.entries[j].inum == -1) {
                        int newInum = d.entries[j-1].inum + 1;
						printf("pInum %d nInum %d\n",d.entries[j-1].inum, newInum);
                        if(m.type == MFS_DIRECTORY) {
                            strcpy(d.entries[j].name,m.name);
                            d.entries[j].inum = newInum;
	
                         	printf("new dir\n"); 
                            //Write pDirectory
                            memcpy((dir*)buffer, &d, sizeof(dir));
                            pwrite(fd, buffer, sizeof(dir), n.dp[0]*MFS_BLOCK_SIZE);
							printf("pDir address %d \n", n.dp[0]*MFS_BLOCK_SIZE);

                            //Create new directory
                            dir newDir = creatDir(newInum);

                            //Create new inode
                            inode in;
                            in.type = MFS_DIRECTORY;
                            in.size = MFS_BLOCK_SIZE - 1;
                            in.dp[0] = (checkPoint.end/MFS_BLOCK_SIZE) - 1;
							printf("New Inode: type %d size %d dp[0] %d\n", in.type, in.size, in.dp[0]);

                            //Write new Directory block
                            memcpy((dir*)buffer, &newDir, sizeof(dir));
                            pwrite(fd, buffer, sizeof(dir), checkPoint.end);
							printf("newDir address %d \n", checkPoint.end);
							checkPoint.end += MFS_BLOCK_SIZE;

                            //Write new inode
                            memcpy((inode*)buffer, &in, sizeof(inode));
                            pwrite(fd, buffer, sizeof(inode), checkPoint.end);
							printf("newInode address %d \n", checkPoint.end);
							int nInodePoint = checkPoint.end;
							checkPoint.end += MFS_BLOCK_SIZE;

                            //Write pInode
                            memcpy((inode*)buffer, &n, sizeof(inode));
                            pwrite(fd, buffer, sizeof(inode),checkPoint.end);
							printf("pInode address %d \n",checkPoint.end);
							int pInodePoint = checkPoint.end;
							checkPoint.end += MFS_BLOCK_SIZE;

                            //Update imap
                            o.inodes[newInum] = (nInodePoint/MFS_BLOCK_SIZE)-1;
                            o.inodes[i] = (pInodePoint/MFS_BLOCK_SIZE)-1;
							printf("o.inodes[newInum] %d %d 0.inodes[i] %d %d\n", o.inodes[newInum], ((checkPoint.end+MFS_BLOCK_SIZE)/MFS_BLOCK_SIZE) - 1, o.inodes[i], ((checkPoint.end+(2*MFS_BLOCK_SIZE))/MFS_BLOCK_SIZE) - 1);

                            //Write imap
                            memcpy((imap*)buffer, &o, sizeof(imap));
                            pwrite(fd,buffer,sizeof(imap), checkPoint.end);
							printf("imap address %d\n",checkPoint.end);
							checkPoint.end += MFS_BLOCK_SIZE;

                            //get stat to find size of file
                            //struct stat sb;
                            //if(fstat(fd,&sb) == -1) {
                            //    perror("stat");
                            //    exit(EXIT_FAILURE);
                            //}

                            //Update checkPoint
                            checkPoint.im[count] = (checkPoint.end/MFS_BLOCK_SIZE)-1;
							printf("checkPoint.im[count] %d\n", checkPoint.im[count]);
                            //checkPoint.end = sb.st_size;
							printf("checkPoint.end %d\n", checkPoint.end);
    
                            //Write checkpoint
                            memcpy((cp*)buffer, &checkPoint, sizeof(cp));
                            pwrite(fd,buffer,sizeof(cp),0);

                            return msg;  
                        } else {
							printf("new file, name %s, inum %d\n", m.name, newInum);
                            strcpy(d.entries[j].name,m.name);
                            d.entries[j].inum = newInum;
                            
                            //Write pDirectory
                            memcpy((dir*)buffer, &d, sizeof(dir));
                            pwrite(fd, buffer, sizeof(dir), n.dp[0]*MFS_BLOCK_SIZE);
							printf("pDir address %d \n", n.dp[0]*MFS_BLOCK_SIZE);

                            //Create new inode
                            inode in;
                            in.type = MFS_REGULAR_FILE;
                            in.size = MFS_BLOCK_SIZE - 1;
                            in.dp[0] = (checkPoint.end/MFS_BLOCK_SIZE) - 1;
							printf("New Inode: type %d size %d dp[0] %d\n", in.type, in.size, in.dp[0]);

							//Reserve Block at end of log for new file
							checkPoint.end += MFS_BLOCK_SIZE;


                            //Write new inode
                            memcpy((inode*)buffer, &in, sizeof(inode));
                            pwrite(fd, buffer, sizeof(inode), checkPoint.end);
							printf("newInode address %d \n", checkPoint.end);
							int nInodePoint = checkPoint.end;
							checkPoint.end += MFS_BLOCK_SIZE;

                            //Write pInode
                            memcpy((inode*)buffer, &n, sizeof(inode));
                            pwrite(fd, buffer, sizeof(inode),checkPoint.end);
							printf("pInode address %d \n",checkPoint.end);
							int pInodePoint = checkPoint.end;
							checkPoint.end += MFS_BLOCK_SIZE;

                            //Update imap
                            o.inodes[newInum] = (nInodePoint/MFS_BLOCK_SIZE)-1;
                            o.inodes[i] = (pInodePoint/MFS_BLOCK_SIZE)-1;
							printf("o.inodes[newInum] %d %d 0.inodes[i] %d %d\n", o.inodes[newInum], ((checkPoint.end+MFS_BLOCK_SIZE)/MFS_BLOCK_SIZE) - 1, o.inodes[i], ((checkPoint.end+(2*MFS_BLOCK_SIZE))/MFS_BLOCK_SIZE) - 1);

                            //Write imap
                            memcpy((imap*)buffer, &o, sizeof(imap));
                            pwrite(fd,buffer,sizeof(imap), checkPoint.end);
							printf("imap address %d\n",checkPoint.end);
							checkPoint.end += MFS_BLOCK_SIZE;

                            //get stat to find size of file
                            //struct stat sb;
                            //if(fstat(fd,&sb) == -1) {
                            //    perror("stat");
                            //    exit(EXIT_FAILURE);
                            //}

                            //Update checkPoint
                            checkPoint.im[count] = (checkPoint.end/MFS_BLOCK_SIZE)-1;
							printf("checkPoint.im[count] %d\n", (checkPoint.end/MFS_BLOCK_SIZE)-1);
                            //checkPoint.end = sb.st_size;
							printf("checkPoint.end %d\n", checkPoint.end);
    
                            //Write checkpoint
                            memcpy((cp*)buffer, &checkPoint, sizeof(cp));
                            pwrite(fd,buffer,sizeof(cp),0);

        					pread(fd, buffer, sizeof(imap),checkPoint.im[count]*MFS_BLOCK_SIZE);
        					memcpy(&o, (imap*)buffer, sizeof(imap));
							printf("o.inodes[0] %d 0.inodes[1] %d\n", o.inodes[0], o.inodes[1]);
                            return msg;  
                        }
                    }   
				} 
                
            }
        }
        count++;
    }        
    //not found return -1
    msg.tag = -1;
    return msg;
}

//TODO:
mssg Unlink(mssg m) {
    mssg msg = {0, 0, 0, 0, 0, 0, "\0", "\0"};
    printf("server:: Processing Unlink()\n");
    char buffer[MFS_BLOCK_SIZE];

    //Iterate over all imaps
    int count = 0;
    while(checkPoint.im[count] != 0 && count < IMAP_NUMBER) {
        imap im; 
        pread(fd, buffer, sizeof(imap),checkPoint.im[count]*MFS_BLOCK_SIZE);
        memcpy(&im, (imap*)buffer, sizeof(imap));

        //Iterate over all inodes in a imap for inum of interest
        for(int i = 0; i < INODE_NUMBER; i++) {

			//Check if inode matches pinum
			if(im.inodes[i] == m.pinum) { 
                inode n; 
                pread(fd, buffer, sizeof(inode), im.inodes[i]*MFS_BLOCK_SIZE); 
                memcpy(&n,(inode*)buffer, sizeof(inode));
                 
                //create directory object to iterate over
                dir d; 
                pread(fd, buffer, sizeof(dir), n.dp[0]*MFS_BLOCK_SIZE);
                memcpy(&d, (dir*)buffer, sizeof(dir));

                //May have to iterate over all blocks in inode if directory
                //takes up more than one block
                int unlinkedInum;
                for(int j = 2; j < DIR_NUMBER; j++) {

                    //See if name of file entry matches name if so return m.inum
                    if(strcmp(d.entries[j].name,m.name) == 0) {
                        unlinkedInum = d.entries[j].inum;

                        //Get unlinkedInode to access
                        inode unlinkedInode;
                        pread(fd, buffer, sizeof(inode), im.inodes[unlinkedInum]*MFS_BLOCK_SIZE); 
                        memcpy(&unlinkedInode,(inode*)buffer, sizeof(inode));

                        if(unlinkedInode.type == MFS_DIRECTORY) { 
                            //pull directory of unlinked inode to iterate over
                            dir unlinkedDir; 
                            pread(fd, buffer, sizeof(dir), n.dp[0]*MFS_BLOCK_SIZE);
                            memcpy(&unlinkedDir, (dir*)buffer, sizeof(dir));

                            //check if inode is directory and if so the directory is not empty; return -1
                            if(unlinkedDir.entries[2].inum != -1) {
                                msg.tag = -1;
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
                        checkPoint.end += MFS_BLOCK_SIZE;

                        //Update Imap
                        im.inodes[i] = (checkPoint.end/MFS_BLOCK_SIZE)-1;
 
                        //Write imap
                        memcpy((imap*)buffer, &im, sizeof(imap));
                        pwrite(fd,buffer,sizeof(imap),checkPoint.end);
                        checkPoint.end += MFS_BLOCK_SIZE;

                        //get stat to find size of file
                        //struct stat st;
                        //if(fstat(fd,&st) == -1) {
                        //    perror("stat");
                        //    exit(EXIT_FAILURE);
                        //}

                        //Update checkPoint
                        checkPoint.im[count] = (checkPoint.end/MFS_BLOCK_SIZE)-1;
                        //checkPoint.end = st.st_size;
    
                        //Write checkpoint
                        memcpy((cp*)buffer, &checkPoint, sizeof(cp));
                        pwrite(fd,buffer,sizeof(cp),0);

                        return msg;
                    }
                }
                //case name not existing return 0;
                return msg;
            } 
        }       
        count++;
    }

    //Not found return -1
    msg.tag = -1;
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

void loadImage(char* pathname) {
    fd = open(pathname, O_RDWR | O_SYNC);
    char buffer[MFS_BLOCK_SIZE];

    //Load checkPoint
    pread(fd, buffer, sizeof(cp), 0);
    memcpy(&checkPoint, (cp*)buffer, sizeof(cp));
}

void createImage() {
    fd = open("file", O_RDWR | O_CREAT | O_SYNC); 
    char buffer[MFS_BLOCK_SIZE];

    //Initialize Directory
    dir dr = creatDir(0);

    //Initialize initial Inode
    inode i;
    i.type = MFS_DIRECTORY;
    i.size = MFS_BLOCK_SIZE - 1;
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

    //get stat to find size of file
    struct stat sb;
    if(fstat(fd,&sb) == -1) {
        perror("stat");
        return;
    }

    //Initialize initial CheckPoint should be 4*4Kb
    checkPoint.end = 4*MFS_BLOCK_SIZE;
    printf("init checkPoint.end %d\n",checkPoint.end);
    
    //write checkpoint
    memcpy((cp*)buffer, &checkPoint, sizeof(cp));
    pwrite(fd,buffer,sizeof(cp),0);
}

// server code
int main(int argc, char *argv[]) {
    if(argc < 2)
        return -1;
    if(argv[2] == NULL) {
        createImage();
    } else {
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
