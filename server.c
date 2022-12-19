#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <math.h>
#include <inttypes.h>
#include <signal.h>
#include <stdbool.h>
#include "ufs.h"
#include "udp.h"
#include "message.h"

#define BLOCKSIZE (4096)

static int PORTNUM;
static char* IMGFILENAME;
bool isShutdown = false;

static super_t* SUPERBLOCKPTR;
int sd;
int fd;
void *image;

typedef struct {
	inode_t inodes[UFS_BLOCK_SIZE / sizeof(inode_t)];
} inode_block_t;

typedef struct {
	dir_ent_t entries[128];
} dir_block_t;

typedef struct {
	unsigned int bits[UFS_BLOCK_SIZE / sizeof(unsigned int)];
} bitmap_t;

void intHandler(int dummy) {
    UDP_Close(sd);
    exit(130);
}

unsigned int get_bit(unsigned int *bitmap, int position) {
   int index = position / 32;
   int offset = 31 - (position % 32);

   return (bitmap[index] >> offset) & 0x1;
}

void set_bit_to_one(unsigned int *bitmap, int position) {
   int index = position / 32;
   int offset = 31 - (position % 32);
   bitmap[index] |= 0x1 << offset;   
}

void set_bit_to_zero(unsigned int *bitmap, int position) {
   int index = position / 32;
   int offset = 31 - (position % 32);

   bitmap[index] &= ~(0x1 << offset);
}

int getBitmapValGivenBlockNumAndInum(int blockNumberAddress, int inum) {
	char bufBlock[BLOCKSIZE];

	lseek(fd, blockNumberAddress * BLOCKSIZE, SEEK_SET);
	read(fd, bufBlock, BLOCKSIZE);

	return get_bit((unsigned int*) bufBlock, inum);
}

int setOneToBitMap(int blockNumberAddress, int inum) {	
	char bufBlock[BLOCKSIZE];

	lseek(fd, blockNumberAddress * BLOCKSIZE, SEEK_SET);
	read(fd, bufBlock, BLOCKSIZE);
	set_bit_to_one((unsigned int*) bufBlock, inum);
	lseek(fd, blockNumberAddress * BLOCKSIZE, SEEK_SET);
	write(fd, bufBlock, BLOCKSIZE);

	return 0;
}

int setZeroToBitMap(int blockNumberAddress, int inum) {
	char bufBlock[BLOCKSIZE];

	lseek(fd, blockNumberAddress * BLOCKSIZE, SEEK_SET);
	read(fd, bufBlock, BLOCKSIZE);
	set_bit_to_zero((unsigned int*) bufBlock, inum);
	lseek(fd, blockNumberAddress * BLOCKSIZE, SEEK_SET);
	write(fd, bufBlock, BLOCKSIZE);

	return 0;
}

void visualizeInode(inode_t* inode) {
	printf("~~~~~~Inode_t inode visualization!!!~~~~~~~~~\n");
	printf("inode.type (0 is directory): %d\n",inode->type);
	printf("----\n");
	printf("inode.size: %d\n",inode->size);
	printf("----\n");

	for(int i = 0; i< DIRECT_PTRS; i++){
		printf("direct[%d]: %d\n", i,inode->direct[i]);
	}
}

int getInodeCopyFromInodeTable(int inum, inode_t* inode) {
	int blockNumberOffsetInInodeTable = floor((inum * sizeof(inode_t)) / BLOCKSIZE);
	char bufBlock[BLOCKSIZE];

	lseek(fd, (SUPERBLOCKPTR->inode_region_addr + blockNumberOffsetInInodeTable) * BLOCKSIZE, SEEK_SET);
	read(fd, bufBlock, BLOCKSIZE);

	inode_block_t* inodeBlockPtr = (inode_block_t*) bufBlock;

	int remainingInodeOffset = (inum * sizeof(inode_t)) - (blockNumberOffsetInInodeTable * BLOCKSIZE);
	
	inode_t inumInode = inodeBlockPtr->inodes[(int)(remainingInodeOffset / sizeof(inode_t))];

	inode->type = inumInode.type;
	inode->size = inumInode.size;

	for(int i = 0; i< DIRECT_PTRS; i++) {
		inode->direct[i] = inumInode.direct[i];
	}

	return 0;
}

int getDataForDirectoryEntryBlockCopyFromDataRegion(int blockNumberOffset, dir_block_t* dirEntryBlock) {
	char bufBlock[BLOCKSIZE];

	lseek(fd, (SUPERBLOCKPTR->data_region_addr + blockNumberOffset) * BLOCKSIZE, SEEK_SET);
	read(fd, bufBlock, BLOCKSIZE);

	dir_block_t* foundDirEntryBlock = (dir_block_t*) bufBlock;

	for(int i = 0; i< 128; i++) {
		dirEntryBlock->entries[i] = foundDirEntryBlock->entries[i];
	}

	return 0;
}

int getDataForDirectoryEntryBlockCopyFromDataRegionWithAbsoluteOffset(int absoluteOffset, dir_block_t* dirEntryBlock) {
	char bufBlock[BLOCKSIZE];

	lseek(fd, (absoluteOffset) * BLOCKSIZE, SEEK_SET);
	read(fd, bufBlock, BLOCKSIZE);

	dir_block_t* foundDirEntryBlock = (dir_block_t*) bufBlock;

	for(int i = 0; i< 128; i++) {
		dirEntryBlock->entries[i] = foundDirEntryBlock->entries[i];
	}

	return 0;
}

int getDataForNormalBlockCopyFromDataRegion(int blockNumberOffset, char* normalBlock) {
	char bufBlock[BLOCKSIZE];

	lseek(fd, (SUPERBLOCKPTR->data_region_addr + blockNumberOffset) * BLOCKSIZE, SEEK_SET);
	read(fd, bufBlock, BLOCKSIZE);

	char* foundBufBlock = (char*) bufBlock;

	for(int i = 0; i< BLOCKSIZE; i++) {
		normalBlock[i] = foundBufBlock[i];
	}

	return 0;
}

int getFreeInodeCopyFromInodeTable(int* inum, inode_t* inode){
	int unallocatedInodeNumber = 0;

	for(int i = 0; i< SUPERBLOCKPTR->num_inodes; i++) {
		unsigned int bitVal = getBitmapValGivenBlockNumAndInum(SUPERBLOCKPTR->inode_bitmap_addr, i);

		if(bitVal == 0) {
			unallocatedInodeNumber = i;

			setOneToBitMap(SUPERBLOCKPTR->inode_bitmap_addr, i);

			int rc = getInodeCopyFromInodeTable(unallocatedInodeNumber, inode);

			if (rc < 0) {
				return -1;
			}

			*inum = unallocatedInodeNumber;

			return 0;
		}
	}
	
	return -1;
}

int getFreeDirectoryEntryDataBlockCopyFromDataRegion(int* blockNumber, dir_block_t* bufferBlock) {
	int unallocatedDatablockNumber = 0;

	for(int i = 0; i < SUPERBLOCKPTR->num_data; i++){
		unsigned int bitVal = getBitmapValGivenBlockNumAndInum(SUPERBLOCKPTR->data_bitmap_addr, i);
		
		if(bitVal == 0) {
			unallocatedDatablockNumber = i;

			setOneToBitMap(SUPERBLOCKPTR->data_bitmap_addr, i);
			getDataForDirectoryEntryBlockCopyFromDataRegion(unallocatedDatablockNumber, (dir_block_t*) bufferBlock);

			*blockNumber = unallocatedDatablockNumber;

			return 0;
		}
	}
	
	return -1;
}

int getFreeNormalDataBlockCopyFromDataRegion(int* blockNumber, char* bufferBlock){
	int unallocatedDatablockNumber = 0;

	for(int i = 0; i< SUPERBLOCKPTR->num_data; i++) {
		unsigned int bitVal = getBitmapValGivenBlockNumAndInum(SUPERBLOCKPTR->data_bitmap_addr, i);
		
		if(bitVal == 0) {
			unallocatedDatablockNumber = i;

			setOneToBitMap(SUPERBLOCKPTR->data_bitmap_addr, i);			
			getDataForNormalBlockCopyFromDataRegion(unallocatedDatablockNumber, bufferBlock);

			*blockNumber = unallocatedDatablockNumber;

			return 0;
		}
	}
	
	return -1;
}

int addInodeToInodeTable(int inum, inode_t* inode){
	int blockNumberOffsetInInodeTable = floor((inum * sizeof(inode_t))/ BLOCKSIZE);
	int remainingOffsetWithinABlock = (inum * sizeof(inode_t)) - (blockNumberOffsetInInodeTable * BLOCKSIZE);
	int offset = ((SUPERBLOCKPTR->inode_region_addr + blockNumberOffsetInInodeTable) * BLOCKSIZE) + remainingOffsetWithinABlock;

	lseek(fd, offset, SEEK_SET);
	write(fd, inode, sizeof(inode_t));

	return 0;
}

int addDirectoryEntryBlockToDataRegion(int blockNumber, dir_block_t* dirEntryBlock){
	lseek(fd, (SUPERBLOCKPTR->data_region_addr + blockNumber) * BLOCKSIZE, SEEK_SET);
	write(fd, dirEntryBlock, sizeof(dir_block_t));

	return 0;
}

int addNormalBlockToDataRegion(int blockNumber, char* dirEntryBlock){
	lseek(fd, (SUPERBLOCKPTR->data_region_addr + blockNumber) * BLOCKSIZE, SEEK_SET);
	write(fd, dirEntryBlock, BLOCKSIZE);

	return 0;
}

int findParentNameGivenInum(int pinum, char* name){
	int blockNumberOffsetInInodeTable = floor((pinum * sizeof(inode_t)) / BLOCKSIZE);
	int remainingOffsetWithinABlock = floor((pinum * sizeof(inode_t)) % BLOCKSIZE);
	char bufBlock[sizeof(inode_t)];

	lseek(fd, (SUPERBLOCKPTR->inode_region_addr + blockNumberOffsetInInodeTable) * BLOCKSIZE + remainingOffsetWithinABlock, SEEK_SET);
	read(fd, bufBlock, sizeof(inode_t));

	inode_t* inodeBlockPtr = (inode_t*) bufBlock;

	int firstBlockOfDirectBlockPtr = inodeBlockPtr->direct[0];
	char dirBlockBuf[sizeof(dir_block_t)];

	lseek(fd, firstBlockOfDirectBlockPtr * BLOCKSIZE, SEEK_SET);
	read(fd, dirBlockBuf, sizeof(dir_block_t));

	dir_block_t* dirBlock = (dir_block_t*) dirBlockBuf;
	dir_ent_t parentDirEntry = dirBlock->entries[0];

	strcpy(name, parentDirEntry.name);

	return 0;
}

void visualizeDirBlock(dir_block_t* dirBlock){
	printf("~~~~~~~~~~~~~~~~dir_block_t VISUALIZER~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");

	for(int i = 0; i< 128; i++) {
		dir_ent_t dirEntry = dirBlock->entries[i];
		printf("dir_ent_t entries[%d].inum: %d   ;", i, dirEntry.inum);
		printf("dir_ent_t entries[%d].name: %s\n", i, dirEntry.name);
	}
}


int addDirEntryToDirectoryInode(inode_t* dinode, int dinum, inode_t* addedInode, dir_ent_t* copyOfDirEntryToAdd){
	if(dinode->type != MFS_DIRECTORY) {
		return -1;
	}

	bool isThereEmptyDirEnt = false;
	char bufBlock[BLOCKSIZE];
	int lastIndexInDirectPtrArr = 0;

	for(int i = 0; i < DIRECT_PTRS && (!isThereEmptyDirEnt); i++) {
		int blockNumber = dinode->direct[i];

		if(blockNumber == 0 || blockNumber == -1) {
			break;
		}

		lseek(fd, blockNumber * BLOCKSIZE, SEEK_SET);
		read(fd, bufBlock, BLOCKSIZE);
		dir_block_t* dirEntBlock = (dir_block_t*) bufBlock;

		for(int j = 0; j< 128; j++) {
			dir_ent_t dirEntry = dirEntBlock->entries[j];
			lastIndexInDirectPtrArr = i;

			if(dirEntry.inum == -1) {
				isThereEmptyDirEnt = true;

				dirEntry.inum = copyOfDirEntryToAdd->inum;
				strcpy(dirEntry.name, copyOfDirEntryToAdd->name);

				int offsetToDirEntry = (blockNumber * BLOCKSIZE) + (j * sizeof(dir_ent_t));

				lseek(fd, offsetToDirEntry, SEEK_SET);
				write(fd, &dirEntry, sizeof(dir_ent_t));
				
				break;
			}
		}
	}

	if(!isThereEmptyDirEnt) {
		int newDataBlockNumber;

		dir_block_t newDirEntryBlock;

		getFreeDirectoryEntryDataBlockCopyFromDataRegion(&newDataBlockNumber, &newDirEntryBlock);

		newDirEntryBlock.entries[0].inum = copyOfDirEntryToAdd->inum;
		strcpy(newDirEntryBlock.entries[0].name, copyOfDirEntryToAdd->name);
		
		lseek(fd, (newDataBlockNumber + SUPERBLOCKPTR->data_region_addr) * BLOCKSIZE, SEEK_SET);
		write(fd, &newDirEntryBlock, sizeof(dir_block_t));

		dinode->direct[lastIndexInDirectPtrArr + 1] = newDataBlockNumber;
	}

	if(addedInode->type == MFS_DIRECTORY) {
		int newBlockNumberForDirectory = 0;

		dir_block_t newDirEntryBlock;
		getFreeDirectoryEntryDataBlockCopyFromDataRegion(&newBlockNumberForDirectory, &newDirEntryBlock);

		newDirEntryBlock.entries[1].inum = dinum;
		strcpy(newDirEntryBlock.entries[1].name, "..");

		newDirEntryBlock.entries[0].inum = copyOfDirEntryToAdd->inum;
		strcpy(newDirEntryBlock.entries[0].name, ".");

		addDirectoryEntryBlockToDataRegion(newBlockNumberForDirectory, &newDirEntryBlock);

		addedInode->type = MFS_DIRECTORY;
		addedInode->size = 0;
		addedInode->direct[0] = newBlockNumberForDirectory + SUPERBLOCKPTR->data_region_addr;
		addInodeToInodeTable(copyOfDirEntryToAdd->inum, addedInode);
	} else {
		int newBlockNumberForRegularFile = 0;
		char newDirEntryBlock[BLOCKSIZE];

		getFreeNormalDataBlockCopyFromDataRegion(&newBlockNumberForRegularFile, newDirEntryBlock);

		addNormalBlockToDataRegion(newBlockNumberForRegularFile, newDirEntryBlock);

		addedInode->type = MFS_REGULAR_FILE;
		addedInode->size = 0;
		addedInode->direct[0] = newBlockNumberForRegularFile + SUPERBLOCKPTR->data_region_addr;
		addInodeToInodeTable(copyOfDirEntryToAdd->inum, addedInode);
	}

	return 0;
}

int run_stat(message_t* m) {
	int inum = m->pinum;

	if(getBitmapValGivenBlockNumAndInum(SUPERBLOCKPTR->inode_bitmap_addr, inum) == 0) {
		m->rc = -1;
		return -1;
	}

	inode_t inode;

	int rc = getInodeCopyFromInodeTable(inum, &inode);

	if(rc < 0) {
		m->rc = -1;
		return -1;
	}

	m->mfs_stat.size = inode.size;
	m->mfs_stat.type = inode.type;

	fsync(fd);

	return 0;
}

int run_write(message_t* m) {
	int inum = m->pinum;
	int offset = m->offset;
	int nbytes = m->nbytes;
	char *bufferToWrite = strdup(m->bufferSent);
	
	if (inum < 0) { 
		m->rc = -1;
		return -1;
	}
	if (offset < 0) {
		m->rc = -1;
		return -1;
	}
	if (nbytes > 4096) {
		m->rc = -1;
		return -1;
	}
	if(offset + nbytes > DIRECT_PTRS * BLOCKSIZE) {
		m->rc = -1;
		return -1;
	}

	if(getBitmapValGivenBlockNumAndInum(SUPERBLOCKPTR->inode_bitmap_addr, inum) == 0) {
		m->rc = -1;
		return -1;
	}

	if(getBitmapValGivenBlockNumAndInum(SUPERBLOCKPTR->data_bitmap_addr, inum) == 0) {
		m->rc = -1;
		return -1;
	}

	inode_t inode;
	int rc = getInodeCopyFromInodeTable(inum, &inode);

	if(rc < 0) {
		m->rc = -1;
		return -1;
	}

	if(inode.type == MFS_DIRECTORY) {
		m->rc = -1;
		return -1;
	}

	int totalOffset = offset + nbytes;

	if(totalOffset > (DIRECT_PTRS * BLOCKSIZE)) {
		m->rc = -1;
		return -1;
	}

	int blockIndex = floor(offset/ BLOCKSIZE);

	if(blockIndex >= DIRECT_PTRS){
		m->rc = -1;
		return -1;
	}

	int remainingOffsetWithinABlock = (offset) - (blockIndex * BLOCKSIZE);
	int absoluteBlockNumber = inode.direct[blockIndex];
	int crossedBlockIndex = 0;
	crossedBlockIndex = floor(totalOffset/ BLOCKSIZE);

	if(absoluteBlockNumber == -1 || absoluteBlockNumber == 0) {
		int relativeBlockNumber = 0;
		char bufBlockRegfile[BLOCKSIZE];

		getFreeNormalDataBlockCopyFromDataRegion(&relativeBlockNumber, bufBlockRegfile);

		addNormalBlockToDataRegion(relativeBlockNumber, bufBlockRegfile);

		inode.direct[blockIndex] = relativeBlockNumber + SUPERBLOCKPTR->data_region_addr;
		inode.size += BLOCKSIZE;
	}

	if(crossedBlockIndex != blockIndex && (inode.direct[crossedBlockIndex] == -1 || inode.direct[crossedBlockIndex] == 0)) {
		int relativeBlockNumber2 = 0;
		char bufBlockRegfile2[BLOCKSIZE];

		getFreeNormalDataBlockCopyFromDataRegion(&relativeBlockNumber2, bufBlockRegfile2);

		addNormalBlockToDataRegion(relativeBlockNumber2, bufBlockRegfile2);

		inode.direct[crossedBlockIndex] = relativeBlockNumber2 + SUPERBLOCKPTR->data_region_addr;
		inode.size += BLOCKSIZE;
	}

	addInodeToInodeTable(inum, &inode);

	char writtenBuf[nbytes];

	for(int z = 0; z < nbytes; z++) {
		writtenBuf[z] = bufferToWrite[z];
	}

	if(blockIndex != crossedBlockIndex) {
		int remainingBytesToWriteBeforeCross = ((blockIndex + 1) * BLOCKSIZE)  - offset;

		lseek(fd, (inode.direct[blockIndex] * BLOCKSIZE) + remainingOffsetWithinABlock, SEEK_SET);
		write(fd, writtenBuf, remainingBytesToWriteBeforeCross);

		int remainingBytesToWriteAfterCross = nbytes - remainingBytesToWriteBeforeCross;

		lseek(fd, (inode.direct[crossedBlockIndex] * BLOCKSIZE), SEEK_SET);
		write(fd, (writtenBuf + remainingBytesToWriteBeforeCross), remainingBytesToWriteAfterCross);

	} else {
		lseek(fd, (absoluteBlockNumber * BLOCKSIZE) + remainingOffsetWithinABlock, SEEK_SET);

		write(fd, writtenBuf, nbytes);
	}
	
	free(bufferToWrite);
	fsync(fd);

	return 0;
}

int offsetToFile(inode_t inode, int offset, int nbytes, message_t* m) {
	int dirPtrBlockIndex = offset / BLOCKSIZE;
	int remainingOffsetWithinABlock = offset % BLOCKSIZE;
	int blockNumber = inode.direct[dirPtrBlockIndex];

	lseek(fd, (blockNumber * BLOCKSIZE) + remainingOffsetWithinABlock, SEEK_SET);

	char bufBlock[BLOCKSIZE];
	read(fd, bufBlock, BLOCKSIZE);

	m->bufferReceived_size = nbytes;
	strcpy(m->bufferReceived, bufBlock);
	
	return 0;
}

int offsetToDirectory(inode_t inode, int offset, int nbytes, message_t* m) {
	int dirPtrBlockIndex = offset / BLOCKSIZE;
	int remainingOffsetWithinABlock = offset % BLOCKSIZE;

	int blockNumber = inode.direct[dirPtrBlockIndex];
	lseek(fd, (blockNumber * BLOCKSIZE) + remainingOffsetWithinABlock, SEEK_SET);

	char bufBlock[BLOCKSIZE];
	read(fd, bufBlock, BLOCKSIZE);

	m->bufferReceived_size = nbytes;
	strcpy(m->bufferReceived, bufBlock);

	return 0;
}

int run_read(message_t* m) {
	int inum = m->pinum;
	int offset = m->offset;
	int nbytes = m->nbytes;

	if(getBitmapValGivenBlockNumAndInum(SUPERBLOCKPTR->inode_bitmap_addr, inum) == 0) {
		m->rc = -1;
		return -1;
	}

	if(getBitmapValGivenBlockNumAndInum(SUPERBLOCKPTR->data_bitmap_addr, inum) == 0) {
		m->rc = -1;
		return -1;
	}

	inode_t inode;

	int rc = getInodeCopyFromInodeTable(inum, &inode);

	if(rc < 0) {
		m->rc = -1;
		return -1;
	}

	int totalOffset = offset + nbytes;

	if(totalOffset > (DIRECT_PTRS * BLOCKSIZE)) {
		m->rc = -1;
		return -1;
	}

	int blockIndex = floor(offset/ BLOCKSIZE);

	if(blockIndex >= DIRECT_PTRS){
		m->rc = -1;
		return -1;
	}

	int remainingOffsetWithinABlock = (offset) - (blockIndex * BLOCKSIZE);

	int absoluteBlockNumber = inode.direct[blockIndex];
	int crossedBlockIndex = 0;
	crossedBlockIndex = floor(totalOffset/ BLOCKSIZE);

	if(absoluteBlockNumber == -1 || absoluteBlockNumber == 0) {
		m->rc = -1;
		return -1;
	}

	if(inode.direct[crossedBlockIndex] == -1 || inode.direct[crossedBlockIndex] == 0) {
		m->rc = -1;
		return -1;
	}
	
	if(inode.type == MFS_DIRECTORY){
		dir_block_t dirEntryBlockCopy;
		getDataForDirectoryEntryBlockCopyFromDataRegionWithAbsoluteOffset(absoluteBlockNumber, &dirEntryBlockCopy);	
		
		int offsetToDirEnt = floor(offset/ sizeof(dir_ent_t));
		char dirEntBuf[sizeof(dir_ent_t)];
		lseek(fd, (absoluteBlockNumber * BLOCKSIZE) + offsetToDirEnt, SEEK_SET);
		read(fd, dirEntBuf, sizeof(dir_ent_t));
		dir_ent_t* dirEntry = (dir_ent_t*) dirEntBuf;

		m->mfs_DifEnt.inum = dirEntry->inum;
		strcpy(m->mfs_DifEnt.name, dirEntry->name);
	
	}

	char* readBuffer = (char*) malloc(nbytes);

	if(blockIndex != crossedBlockIndex) {
		int remainingBytesToWriteBeforeCross = ((blockIndex + 1) * BLOCKSIZE)  - offset;
		lseek(fd, (inode.direct[blockIndex] * BLOCKSIZE) + remainingOffsetWithinABlock, SEEK_SET);
		read(fd, readBuffer, remainingBytesToWriteBeforeCross);

		int remainingBytesToWriteAfterCross = nbytes - remainingBytesToWriteBeforeCross;
		lseek(fd, (inode.direct[crossedBlockIndex] * BLOCKSIZE), SEEK_SET);
		read(fd, (readBuffer + remainingBytesToWriteBeforeCross), remainingBytesToWriteAfterCross);

	}else{
		lseek(fd, (inode.direct[blockIndex] * BLOCKSIZE) + (remainingOffsetWithinABlock), SEEK_SET);
		read(fd, readBuffer, nbytes);
	}

	for(int z = 0; z< nbytes; z++){
		m->bufferReceived[z] = readBuffer[z];
	}
	
	m->bufferReceived_size = nbytes;

	fsync(fd);

	return 0;
}

int run_lookup(message_t* m) {
	int pinum = m->pinum;
	char name[28];
	strcpy(name, m->name);

	if(getBitmapValGivenBlockNumAndInum(SUPERBLOCKPTR->inode_bitmap_addr, pinum) == 0){
		m->rc = -1;
		return -1;
	}

	if(getBitmapValGivenBlockNumAndInum(SUPERBLOCKPTR->data_bitmap_addr, pinum) == 0){
		m->rc = -1;
		return -1;
	}

	inode_t pinode;
	int rc = getInodeCopyFromInodeTable(pinum, &pinode);
	if(rc < 0){
		m->rc = -1;
		return -1;
	}

	if(pinode.type == MFS_REGULAR_FILE){
		m->inum = -1;
		return -1;
	}

	char bufBlock[BLOCKSIZE];
	for(int i = 0; i< DIRECT_PTRS; i++){
		int blockNumber = pinode.direct[i];

		if(blockNumber == 0 || blockNumber == -1){
			break;
		}

		lseek(fd, blockNumber * BLOCKSIZE, SEEK_SET);
		read(fd, bufBlock, BLOCKSIZE);
		dir_block_t* dirEntBlock = (dir_block_t*) bufBlock;

		for(int j = 0; j< 128; j++){
			dir_ent_t dirEntry = dirEntBlock->entries[j];
			if(strcmp(dirEntry.name, name) == 0){
				m->inum = dirEntry.inum;
				break;
			}
		}
	}
	
	fsync(fd);
	return 0;
}

int run_cret(message_t* m){
	int pinum = m->pinum;
	int type = m->type;
	
	char name[28];
	strcpy(name, m->name);

	if(getBitmapValGivenBlockNumAndInum(SUPERBLOCKPTR->inode_bitmap_addr, pinum) == 0){
		m->rc = -1;
		return -1;
	}

	if(getBitmapValGivenBlockNumAndInum(SUPERBLOCKPTR->data_bitmap_addr, pinum) == 0){
		m->rc = -1;
		return -1;
	}

	inode_t pinode;
	int rc = getInodeCopyFromInodeTable(pinum, &pinode);
	if(rc < 0){
		m->rc = -1;
		return -1;
	}

	if(pinode.type == MFS_REGULAR_FILE){
		m->rc = -1;
		return -1;
	}

	int newInodeNumber;
	inode_t newInode;
	rc = getFreeInodeCopyFromInodeTable(&newInodeNumber, &newInode);
	if(rc < 0){
		m->rc = -1;
		return -1;
	}

	newInode.type = type;
	newInode.size = 0;

	addInodeToInodeTable(newInodeNumber, &newInode);

	dir_ent_t dirEntry;
	dirEntry.inum = newInodeNumber;
	strcpy(dirEntry.name, name);

	addDirEntryToDirectoryInode(&pinode, pinum, &newInode, &dirEntry);

	fsync(fd);
	return 0;
}

int run_unlink(message_t* m){
	int pinum = m->pinum;
	char name[28];
	strcpy(name, m->name);

	char bufBlock[BLOCKSIZE];
	lseek(fd, SUPERBLOCKPTR->inode_bitmap_addr * BLOCKSIZE, SEEK_SET);
	read(fd, bufBlock, BLOCKSIZE);
	unsigned int bitVal = get_bit((unsigned int*) bufBlock, pinum);
	if(bitVal == 0){
		m->rc = -1;
		return -1;
	}

	int blockNumberOffsetInInodeTable = floor((pinum * sizeof(inode_t))/ BLOCKSIZE);
	char bufBlock2[BLOCKSIZE];
	lseek(fd, (SUPERBLOCKPTR->inode_region_addr + blockNumberOffsetInInodeTable) * BLOCKSIZE, SEEK_SET);
	read(fd, bufBlock2, BLOCKSIZE);

	inode_block_t* inodeBlockPtr = (inode_block_t*) bufBlock2;
	int remainingInodeOffset = (pinum * sizeof(inode_t)) % BLOCKSIZE;

	inode_t pinode = inodeBlockPtr->inodes[remainingInodeOffset/ sizeof(inode_t)];

	for(int i = 0; i< DIRECT_PTRS; i++){
		int blockNumber = pinode.direct[i];

		char bufBlockForDirBlock[BLOCKSIZE];
		lseek(fd, blockNumber * BLOCKSIZE, SEEK_SET);
		read(fd, bufBlock, BLOCKSIZE);
		dir_block_t* dirEntBlock = (dir_block_t*) bufBlockForDirBlock;

		for(int j = 0; j< 128; j++){
			dir_ent_t dirEntry = dirEntBlock->entries[j];
			if (pinode.type == 0) {
				if (dirEntry.inum == -1) {
					memcpy(dirEntry.name, "", 0);
				} else {
					m->rc = -1;
					return -1;
				}
			}
			else if (pinode.type == 1) {
				dirEntry.inum = -1;
				memcpy(dirEntry.name, "", 0);
			}
		}

	}
	fsync(fd);
	return 0;
}

int run_shutdown(message_t* m){
	fsync(fd);
	close(fd);

	UDP_Close(PORTNUM);
	exit(0);
	return 0;
}


int message_parser(message_t* m){
	int message_func = m->mtype;
	int rc;

	if(message_func == MFS_INIT){
		return 0;
	}else if(message_func == MFS_LOOKUP){
		rc = run_lookup(m);

	}else if(message_func == MFS_STAT){
		rc = run_stat(m);

	}else if(message_func == MFS_WRITE){
		rc = run_write(m);

	}else if(message_func == MFS_READ){
		rc = run_read(m);

	}else if(message_func == MFS_CRET){
		rc = run_cret(m);

	}else if(message_func == MFS_UNLINK){
		rc = run_unlink(m);

	}else if(message_func == MFS_SHUTDOWN){
		rc = run_shutdown(m);
	}

	return rc;
}

int readImage(){

	fd = open(IMGFILENAME, O_RDWR);
	assert(fd > -1);

	char* bufBlockPtr = (char*)malloc(sizeof(char) * BLOCKSIZE);
	read(fd, bufBlockPtr, BLOCKSIZE);
	SUPERBLOCKPTR = (super_t*) bufBlockPtr;

	printf("-------Describes IMG + super block-----------\n");

	printf("SUPERBLOCK\n");
	printf(" inode bitmap address %d [len %d]\n", SUPERBLOCKPTR->inode_bitmap_addr, SUPERBLOCKPTR->inode_bitmap_len);
    printf(" data bitmap address %d [len %d]\n", SUPERBLOCKPTR->data_bitmap_addr, SUPERBLOCKPTR->data_bitmap_len);
	printf(" inode region address %d [len %d]\n", SUPERBLOCKPTR->inode_region_addr, SUPERBLOCKPTR->inode_region_len);
	printf(" data region address %d [len %d]\n", SUPERBLOCKPTR->data_region_addr, SUPERBLOCKPTR->data_region_len);
	printf(" num inodes: %d ;num data: [len %d]\n", SUPERBLOCKPTR->num_inodes, SUPERBLOCKPTR->num_data);

	return 0;
}

int main(int argc, char *argv[]) {
	signal(SIGINT, intHandler);
	
	PORTNUM = atoi(argv[1]);
	IMGFILENAME = (char*) malloc(100 * sizeof(char));
	strcpy(IMGFILENAME, argv[2]);

	int rc = readImage();
	assert(rc > -1);

    sd = UDP_Open(PORTNUM);
    assert(sd > -1);
    while (!isShutdown) {
	struct sockaddr_in addr;

	message_t m;
	
	rc = UDP_Read(sd, &addr, (char *) &m, sizeof(message_t));
	if (rc > 0) {
		rc = message_parser(&m);

		m.rc = rc;

		rc = UDP_Write(sd, &addr, (char *) &m, sizeof(message_t));
	} 
    }
    return 0; 
}