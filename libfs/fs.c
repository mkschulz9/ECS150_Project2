#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "disk.h"
#include "fs.h"

#define signature_default "ECS150FS"

struct superblock {
	char sig[10];                 // must be equal to "ECS150FS"
	uint16_t diskBlk_total;       // total amount of blocks of virtual disk
	uint16_t rootBlk_index;       // root directory block index
	uint16_t firstDataBlk_index;  // data block start index
	uint16_t num_datablks;        // amount of data blocks
	uint8_t num_fatBlks;          // number of blocks for FAT
	uint8_t pad[4079];            // unused, padding
};

struct fat {
	uint16_t entries[4 * (BLOCK_SIZE/2)];
};

struct file {
	uint8_t filename[FS_FILENAME_LEN];         // filename (including NULL char)
	uint32_t file_size;           // size of file (in bytes)
	uint16_t index_DataBlk;       // index of first data block
	uint8_t pad[10];              // unused, padding
	size_t offset;

};

struct openFiles {
	int openCount;
	struct file file[FS_OPEN_MAX_COUNT]
}

struct root_dir {
	struct file root_dir[FS_FILE_MAX_COUNT];
};

// define struct objs needed in functions below
struct superblock superblock;
struct fat fat;
struct root_dir root_dir;
struct openFiles openFiles;

// create empty structs to use in unmount
const struct superblock empty_superblock;
const struct fat empty_fat;
const struct root_dir empty_root_dir;
const struct openFiles empty_openFiles;

int fs_mount(const char *diskname)
{
	// return -1, if virtual disk file @diskname cannot be opened or is already open
	if (block_disk_open(diskname) == -1) return -1;

	// return -1 if superblock can't be read
	if (block_read(0, &superblock) == -1) return -1;

	// return -1 if root directory can't be read
	if (block_read(superblock.rootBlk_index, &root_dir.root_dir) == -1) return -1;

	// read over fat entries
	for (int i = 1; i < superblock.num_fatBlks; i++) {
		// return -1 if cant read fat entry
		if (block_read(i, &fat.entries[i * (BLOCK_SIZE/2)]) == -1) 		return -1;
	}

	// return -1 if signature doesn't match specifications
	if (superblock.sig != signature_default) return -1; // double check this part

	// return -1 if total amount of blocks doesn;t correspond to what  block_disk_count() returns
	if (superblock.num_datablks != block_disk_count()) return -1;

	return 0;
}

int fs_umount(void)
{
	// return -1 if no FS is currently mounted
	if ((block_disk_count()) == -1) return -1;

	// write root_dir to disk
	if (block_write(superblock.rootBlk_index, &root_dir.root_dir) == -1) return -1;

	// write fat entries to disk
	for (int i = 1; i < superblock.num_fatBlks; i++) {
		if (block_write(i, &(fat.entries[i * (BLOCK_SIZE/2)])) == -1) 		return -1;
	}

	//return 0 if VFS cannot be closed
	if (block_disk_close() == -1) return -1;

	//check if there are still open FDs (ToDo Phase 3)

	// empty out structs
	superblock = empty_superblock;
	fat = empty_fat;
	root_dir = empty_root_dir;
	openFiles = empty_openFiles;

	return 0;
}

int fs_info(void)
{
	/* TODO: Phase 1 */
}

int fs_create(const char *filename)
{
	// Check validity of filename
	if (!filename || FS_FILENAME_LEN < strlen(filename)) return -1;

	// Check number of files in directory
	int count = 0, i = 0;

	while (i < FS_FILE_MAX_COUNT) {
		if(strlen(root_dir.root_dir[i].filename) != 0) {
			if(!strcmp(root_dir.root_dir[i].filename, filename)) return -1;
		} else {
			count = count + 1;
		}
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
			return 0;
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
		if(!strcmp(root_dir.root_dir[i].filename, filename)) {
			check = 0;
			blockIndex = root_dir.root_dir[i].index_DataBlk;
			strcpy(root_dir.root_dir[i].filename, "\0");
			root_dir.root_dir[i].file_size = 0;
			root_dir.root_dir[i].index_DataBlk = FAT_EOC;

			if(block_write(superblock.rootBlk_index, &root_dir.root_dir) == -1) return -1;
			break;
		}
		i = i + 1;
	}

	if (check) return -1;

	while (blockIndex != FAT_EOC) {
		uint16_t newIndex = fat.entries[blockIndex];
		fat.arr[blockIndex] = 0;
		blockIndex = newIndex;
	}

	return 0;
}

int fs_ls(void)
{
	printf("FS Ls:")
	int i = 0;
	while (i < FS_FILE_MAX_COUNT) {
		if(!strlen(root_dir.root_dir[0])) {
			printf("\nfile: %s, size: %i, data_blk: %i", root_dir.root_dir[i].filename, root_dir.root_dir[i].file_size, root_dir.root_dir[i].index_DataBlk);
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
		if (!strcmp(root_dir.root_dir[i].filename,filename))
			check = 0;
		i = i + 1;
	}

	if (check) return -1;

	if (openFiles.openCount >= FS_OPEN_MAX_COUNT) return -1;

	int fd = -1;
	i = 0;

	while (i < FS_OPEN_MAX_COUNT) {
		if (!strlen(openFiles.file[i].filename)) {
			openFiles.openCount = openFiles.openCount + 1;
			strcpy(openFiles.file[i].filename, filename);
			openFiles.file[i].offset = 0;
			fd = i;
			break;
		}

		i = i + 1;
	}

	return fd;
}

int fs_close(int fd)
{
	if (fd > 31 || fd < 0 || !strlen(openFiles.file[fd].filename)) return -1;
	strcpy(openFiles.file[fd].filename, "\0");
	openFiles.file[fd].offset = 0;
	openFiles.openCount = openFiles.openCount - 1;
	return 0;
}

int fs_stat(int fd)
{
	if (fd > 31 || fd < 0 || !strlen(openFiles.file[fd].filename)) return -1;

	int size = -1, i = 0;

	while (i < FS_FILE_MAX_COUNT) {
		if(!strcmp(root_dir.root_dir[i].filename, filename)) {
			size = root_dir.root_dir[i].file_size;
		}
		i = i + 1;
	}

	return size;
}

int fs_lseek(int fd, size_t offset)
{
	if (fd > 31 || fd < 0 || !strlen(openFiles.file[fd].filename)) return -1;
	if (offset < 0 || offset > fs_stat(fd)) return -1;
	if (fs_stat(fd) == -1) return -1;

	openFiles.file[fd].offset = offset;
	return 0;
}

int fs_write(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
}

int fs_read(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
}

