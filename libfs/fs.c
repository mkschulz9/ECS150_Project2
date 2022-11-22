#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "disk.h"
#include "fs.h"

#define signature_default "ECS150FS"

struct superblock {
	uint64_t sig;                 // must be equal to "ECS150FS"
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

};

struct root_dir {
	struct file root_dir[FS_OPEN_MAX_COUNT];
};

// define struct objs needed in functions below
struct superblock superblock;
struct fat fat;
struct root_dir root_dir;

// create empty structs to use in unmount
const struct superblock empty_superblock;
const struct fat empty_fat;
const struct root_dir empty_root_dir;

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

	return 0;
}

int fs_info(void)
{
	/* TODO: Phase 1 */
}

int fs_create(const char *filename)
{
	/* TODO: Phase 2 */
}

int fs_delete(const char *filename)
{
	/* TODO: Phase 2 */
}

int fs_ls(void)
{
	/* TODO: Phase 2 */
}

int fs_open(const char *filename)
{
	/* TODO: Phase 3 */
}

int fs_close(int fd)
{
	/* TODO: Phase 3 */
}

int fs_stat(int fd)
{
	/* TODO: Phase 3 */
}

int fs_lseek(int fd, size_t offset)
{
	/* TODO: Phase 3 */
}

int fs_write(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
}

int fs_read(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
}

