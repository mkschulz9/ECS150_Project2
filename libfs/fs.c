#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "disk.h"
#include "fs.h"

#define FAT_EOC 0xFFFF

#define signature_default "ECS150FS"

struct superblock {
	char sig[8];                 // must be equal to "ECS150FS"
	uint16_t diskBlk_total;       // total amount of blocks of virtual disk
	uint16_t rootBlk_index;       // root directory block index
	uint16_t firstDataBlk_index;  // data block start index
	uint16_t num_datablks;        // amount of data blocks
	uint8_t num_fatBlks;          // number of blocks for FAT
	uint8_t pad[4079];            // unused, padding
} superblock;

struct fat {
	uint16_t *entries;
} fat;

struct file {
	char filename[FS_FILENAME_LEN];         // filename (including NULL char)
	uint32_t file_size;           // size of file (in bytes)
	uint16_t index_DataBlk;       // index of first data block
	uint8_t pad[10];              // unused, padding
};

struct openEntry {
	char filename[FS_FILENAME_LEN];
	uint16_t offset;
};

struct openFiles {
	int openCount;
	struct openEntry entry[FS_OPEN_MAX_COUNT];
} openFiles;

struct root_dir {
	struct file root_dir[FS_FILE_MAX_COUNT];
} root_dir;

int fs_mount(const char *diskname)
{
	// return -1, if virtual disk file @diskname cannot be opened or is already open
	if (block_disk_open(diskname) == -1) return -1;

	// return -1 if superblock can't be read
	if (block_read(0, &superblock) == -1) return -1;
	
	// return -1 if total amount of blocks doesn;t correspond to what block_disk_count() returns
	if(superblock.diskBlk_total != block_disk_count()) return -1;

	// return -1 if signature doesn't match specifications
	if (memcmp(superblock.sig, signature_default, 8) != 0) return -1; // double check this part

	fat.entries = (uint16_t*)malloc(superblock.num_datablks * sizeof(uint16_t));

	void *buf = (void*)malloc(BLOCK_SIZE);
	int i = 1;
	// read over fat entries
	while (i < superblock.rootBlk_index) {
		// return -1 if cant read fat entry
		if(block_read(i, buf) == -1) return -1;
		memcpy(fat.entries + (i-1)*BLOCK_SIZE, buf, BLOCK_SIZE);
		i = i + 1;
	}

	if(fat.entries[0] != FAT_EOC) return -1;

	// return -1 if root directory can't be read
	if (block_read(superblock.rootBlk_index, &root_dir) == -1) return -1;

	return 0;
}

int fs_umount(void)
{

	

	int i = 1;
	// write fat entries to disk
	while (i < superblock.rootBlk_index) {
		if (block_write(i, fat.entries + (i - 1) * BLOCK_SIZE) == -1) return -1;
		i = i+1;
	}

	if(block_write(0, &superblock) == -1) return -1;
	
	// write root_dir to disk
	if (block_write(superblock.rootBlk_index, &root_dir) == -1) return -1;

	if (block_disk_close() == -1) return -1;

	return 0;
}

int fs_info(void)
{
	int free_dataBlks = superblock.num_datablks;
	int free_rdir = FS_FILE_MAX_COUNT;
	int disk_block_count = block_disk_count();

	// return -1 if no underlying virtual disk was opened
	if (disk_block_count == -1) return -1;

	// determine number of free data blks
	for (int i = 0; i < superblock.num_datablks; i++) {
		if (fat.entries[i] == 0) continue;
		free_dataBlks--;
	}

	// determine number of free rdirs
	for (int i = 0; i < FS_FILE_MAX_COUNT; i++) { 
		if(root_dir.root_dir[i].filename[0] == 0) continue;
		free_rdir--;
	}


	//display info about the currently mounted FS
	printf("FS Info:\n");
	printf("total_blk_count=%i\n", superblock.diskBlk_total);
	printf("fat_blk_count=%i\n", superblock.num_fatBlks);
	printf("rdir_blk=%i\n", superblock.rootBlk_index);
	printf("data_blk=%i\n", superblock.firstDataBlk_index);
	printf("data_blk_count=%i\n", superblock.num_datablks);
	//need helper functions to calculate
	printf("fat_free_ratio=%i/%i\n", free_dataBlks, superblock.num_datablks);
	printf("rdir_free_ratio=%i/%i\n", free_rdir, FS_FILE_MAX_COUNT);

	return 0;
}

int fs_create(const char *filename)
{
	// Check validity of filename
	if (!filename || FS_FILENAME_LEN < strlen(filename)) return -1;

	// Check number of files in directory
	int count = 0, i = 0;

	while (i < FS_FILE_MAX_COUNT) {
		if(strlen(root_dir.root_dir[i].filename) != 0) {
			count = count + 1;
		} 
		if(strcmp(root_dir.root_dir[i].filename, filename) == 0) return -1;
		i = i + 1;
	}

	// Max number of files reached
	if (count >= FS_FILE_MAX_COUNT) return -1;

	i = 0;
	while (i < FS_FILE_MAX_COUNT) {
		// Initializing empty file with filename
		if(strlen(root_dir.root_dir[i].filename) == 0) {
			strcpy(root_dir.root_dir[i].filename, filename);
			root_dir.root_dir[i].file_size = 0;
			root_dir.root_dir[i].index_DataBlk = FAT_EOC;
			break;
		}
		i = i + 1;
	}
	return 0;
}

int fs_delete(const char *filename)
{
	// Check validity of filename
	if (!filename || FS_FILENAME_LEN < strlen(filename)) return -1;

	// Use check to test whether file is found or not
	int check = 1, i = 0;

	uint16_t blockIndex = FAT_EOC;

	while (i < FS_FILE_MAX_COUNT) {
		if(strcmp(root_dir.root_dir[i].filename, filename) == 0) {
			check = 0;
			blockIndex = root_dir.root_dir[i].index_DataBlk;
			strcpy(root_dir.root_dir[i].filename, "\0");
			root_dir.root_dir[i].file_size = 0;
			root_dir.root_dir[i].index_DataBlk = FAT_EOC;

			if(block_write(superblock.rootBlk_index, &root_dir) == -1) return -1;
			break;
		}
		i = i + 1;
	}

	if (check) return -1;

	while (blockIndex != FAT_EOC) {
		uint16_t newIndex = fat.entries[blockIndex];
		fat.entries[blockIndex] = 0;
		blockIndex = newIndex;
	}

	return 0;
}

int fs_ls(void)
{
	printf("FS Ls:\n");
	int i = 0;
	while (i < FS_FILE_MAX_COUNT) {
		if(strlen(root_dir.root_dir[i].filename) != 0) {
			printf("file: %s, size: %i, data_blk: %i\n", root_dir.root_dir[i].filename, root_dir.root_dir[i].file_size, root_dir.root_dir[i].index_DataBlk);
		}
		i = i + 1;
	}
	return 0;
}

int fs_open(const char *filename)
{
	if (!filename || FS_FILENAME_LEN < strlen(filename)) return -1;

	int check = 1, i = 0;

	while (i < FS_FILE_MAX_COUNT) {
		if (strcmp(root_dir.root_dir[i].filename,filename) == 0) check = 0;
		i = i + 1;
	}

	if (check) return -1;

	if (openFiles.openCount >= FS_OPEN_MAX_COUNT) return -1;

	int fd = -1;
	i = 0;

	while (i < FS_OPEN_MAX_COUNT) {
		if (strlen(openFiles.entry[i].filename) == 0) {
			openFiles.openCount = openFiles.openCount + 1;
			strcpy(openFiles.entry[i].filename, filename);
			openFiles.entry[i].offset = 0;
			fd = i;
			break;
		}

		i = i + 1;
	}

	return fd;
}

int fs_close(int fd)
{
	if (fd > 31 || fd < 0 || strlen(openFiles.entry[fd].filename) == 0) return -1;
	strcpy(openFiles.entry[fd].filename, "\0");
	openFiles.entry[fd].offset = 0;
	openFiles.openCount = openFiles.openCount - 1;
	return 0;
}

int fs_stat(int fd)
{
	if (fd > 31 || fd < 0 || strlen(openFiles.entry[fd].filename) == 0) return -1;

	int size = -1, i = 0;

	while (i < FS_FILE_MAX_COUNT) {
		if(strcmp(root_dir.root_dir[i].filename, openFiles.entry[fd].filename) == 0) {
			size = root_dir.root_dir[i].file_size;
		}
		i = i + 1;
	}

	return size;
}

int fs_lseek(int fd, size_t offset)
{
	if (fd > 31 || fd < 0 || !strlen(openFiles.entry[fd].filename)) return -1;
	if (offset < 0 || offset > fs_stat(fd)) return -1;
	if (fs_stat(fd) == -1) return -1;

	openFiles.entry[fd].offset = offset;
	return 0;
}

int fs_write(int fd, void *buf, size_t count)
{
	// return -1 if fd is out of range or out of range or if no FS is mounted
	if (fs_stat(fd) == -1) return -1;

	// return -1 if buf is NULL
	if (buf == NULL) return -1;

	return 0;
}

int fs_read(int fd, void *buf, size_t count)
{
	// return -1 if fd is out of range or out of range or if no FS is mounted
	if (fs_stat(fd) == -1) return -1;

	// return -1 if buf is NULL
	if (buf == NULL) return -1;

	return 0;
}
