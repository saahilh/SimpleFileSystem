//Returns -1 for errors besides mksfs

void 	mksfs				(int fresh);
int 	sfs_fopen		(char *name);
int 	sfs_fclose	(int fileID);
int 	sfs_frseek	(int fileID, 
									 int loc);
int 	sfs_fwseek	(int fileID, 
									 int loc);
int 	sfs_fwrite	(int fileID, 
									 char *buf, 
									 int length);
int 	sfs_fread		(int fileID, 
									 char *buf, 
									 int length);
int 	sfs_remove	(char *file);

#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <fcntl.h>
#include "tests/disk_emu.h"
#include <stdbool.h>

#define DISK_NAME 		"shamay_disk"
#define BLOCK_SIZE 		1024
#define NUM_BLOCKS 		1024
#define NUM_ATTR_BLOCKS 3		//sb, fbm, wm
#define INODE_SIZE 		16
#define NUM_DIR_BLOCKS	4

#define SB_POS			0
#define START_OF_DIRECTORY_BLOCKS			1
#define INB_POS			START_OF_DIRECTORY_BLOCKS + NUM_DIR_BLOCKS
#define FBM_POS			NUM_BLOCKS - 2
#define WM_POS			NUM_BLOCKS - 1

#define MAGIC_NUM 	0xACBD0005
#define NAME_SIZE		10

#define DIR_BLOCK_SIZE		BLOCK_SIZE/(NAME_SIZE+8)
#define NUM_INODE_BLOCKS	14
#define NUM_INODES_PER_INB	BLOCK_SIZE/ (INODE_SIZE * sizeof(int))
#define NUM_INODES 			NUM_DIR_BLOCKS*DIR_BLOCK_SIZE

#define NUMBER_OF_DIRECT_POINTERS_PER_INODE	INODE_SIZE - 2
#define	NUMBER_OF_INDIRECT_POINTERS_IN_BLOCK	BLOCK_SIZE / sizeof(int)
#define MAX_BLOCKS_IN_FILE	NUMBER_OF_DIRECT_POINTERS_PER_INODE + NUMBER_OF_INDIRECT_POINTERS_IN_BLOCK
#define MAX_FILE_SIZE		MAX_BLOCKS_IN_FILE * BLOCK_SIZE

typedef struct super_block_t
{
	int magic_num;
	int block_size;
	char filler[BLOCK_SIZE];
} SuperBlock;

//---------------------------------------------------------------

typedef struct inode_t
{
	int fsize;
	int direct[NUMBER_OF_DIRECT_POINTERS_PER_INODE];
	int indirect;
} 
INode;

typedef struct indirect
{
	int block_ptrs[NUMBER_OF_INDIRECT_POINTERS_IN_BLOCK];
} 
Indirect;

typedef struct inode_block_t
{
	INode inodes[NUM_INODES_PER_INB];
	char filler[BLOCK_SIZE];
} 
INodeBlock;

typedef struct directory_entry_t
{
	char name[NAME_SIZE];
	int block_number;
	int entry_number;
} 
DirectoryEntry;

typedef struct directory_block_t
{
	DirectoryEntry directory_entries[DIR_BLOCK_SIZE];
	char filler[BLOCK_SIZE];
} 
DirectoryBlock;

typedef struct directory_index_t
{
	int block_number;
	int entry_index;
}
DirectoryIndex;

typedef struct block_t
{
	unsigned char bytes[BLOCK_SIZE];
} 
Block;

typedef struct open_file_t
{
	DirectoryIndex directory_index;
	int read_ptr;
	int write_ptr;
} 
OpenFile;

typedef struct file_descr_table_t
{
	OpenFile open_files[NUM_INODES];
} 
FileDescrTable;