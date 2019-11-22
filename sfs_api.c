#include "sfs_api.h"

FileDescrTable fdt;

void 
set_free_bytes(int block_id, 
							 int byte, 
							 int num_bytes)
{
	if (block_id==FBM_POS||block_id==WM_POS)
	{
		Block buffer;
		read_blocks(block_id, 1, &buffer);

		for(int i = byte; i < byte + num_bytes; i++)
		{
			buffer.bytes[i] <<= 1;
			buffer.bytes[i] |= 0;
		}
		
		write_blocks(block_id, 1, &buffer);
	}
	else
	{
		printf("Cannot set free bytes for this type of block.\n");
	}
}

void 
set_busy_bytes(int block_id, 
							 int byte, 
							 int num_bytes)
{
	if (block_id==FBM_POS||block_id==WM_POS)
	{
		Block buffer;
		read_blocks(block_id, 1, &buffer);

		for(int i = byte; i < byte + num_bytes; i++)
		{
			buffer.bytes[i] <<= 1;
			buffer.bytes[i] |= 1;
		}

		write_blocks(block_id, 1, &buffer);
	}
	else
	{
		printf("Cannot set busy bytes for this type of block.\n");
	}
}

INode 
get_inode(int dir_position[2])
{
	DirectoryBlock db;

	int dir_num = dir_position[0];
	int dir_offset = dir_position[1];
	read_blocks(dir_num, 1, &db);

	INodeBlock inb;
	int inb_num = db.directory_entries[dir_offset].block_number;
	int inb_offset = db.directory_entries[dir_offset].entry_number;
	read_blocks(inb_num, 1, &inb);

	return inb.inodes[inb_offset];
}

INode 
get_inode_fdt(int fileID)
{
	int dir_position[2] = { fdt.open_files[fileID].directory_number, fdt.open_files[fileID].offset };
	return get_inode(dir_position);
}

void 
write_inode(int dir_position[2], 
						INode *new_node)
{
	DirectoryBlock db;
	int dir_num = dir_position[0];
	int dir_offset = dir_position[1];
	read_blocks(dir_num, 1, &db);

	INodeBlock inb;
	int inb_num = db.directory_entries[dir_offset].block_number;
	read_blocks(inb_num, 1, &inb);

	memcpy(&inb.inodes[db.directory_entries[dir_offset].entry_number], new_node, sizeof(INode));
	write_blocks(inb_num, 1, &inb);
}

void 
write_inode_fdt(int fileID, 
								INode *node)
{
	int dir_position[2] = { fdt.open_files[fileID].directory_number, fdt.open_files[fileID].offset };
	write_inode(dir_position, node);
}

int 
find_free_inode(int inb_position[2])
{
	INodeBlock current_block;

	for(int block_num = INB_POS; block_num < INB_POS + NUM_INODE_BLOCKS; block_num++)
	{
		read_blocks(block_num, 1, &current_block);
		for(int offset = 0; offset < NUM_INODES_PER_INB; offset++)
		{
			if (current_block.inodes[offset].fsize==-1)
			{
				inb_position[0] = block_num;
				inb_position[1] = offset;
				current_block.inodes[offset].fsize++;
				write_blocks(block_num, 1, &current_block);
				return 0;
			}
		}
	}
	
	return -1;
}

DirectoryIndex
search_directory(char *target_file_name)
{
	DirectoryIndex directory_index;

	directory_index.block_number = -1;
	directory_index.entry_index = -1;

	for(int current_directory_block_number = START_OF_DIRECTORY_BLOCKS; 
			 current_directory_block_number < START_OF_DIRECTORY_BLOCKS + NUM_DIR_BLOCKS; 
			 current_directory_block_number++)
	{
		DirectoryBlock current_directory;
		read_blocks(current_directory_block_number, 1, &current_directory);

		for(int current_entry_number = 0; current_entry_number < DIR_BLOCK_SIZE; current_entry_number++)
		{

			if(target_file_name==NULL)
			{
				int current_file_block = current_directory.directory_entries[current_entry_number].block_number;

				if (current_file_block==-1)
				{
					directory_index.block_number = current_directory_block_number;
					directory_index.entry_index = current_entry_number;

					break;
				}
			}
			else
			{
				char *current_file_name = current_directory.directory_entries[current_entry_number].name;

				if (strcmp(target_file_name, current_file_name)==0)
				{
					directory_index.block_number = current_directory_block_number;
					directory_index.entry_index = current_entry_number;

					break;
				}
			}
		}
	}

	return directory_index;
}

void 
init_block(int block_num)
{
	set_busy_bytes(FBM_POS, block_num, 1);
	Block init;
	memset(&init, -1, sizeof(init));
	write_blocks(block_num, 1, &init);
}

int 
get_and_init_next_free_block()
{
	Block fbm;

	read_blocks(FBM_POS, 1, &fbm);

	for(int i = 0; i < NUM_BLOCKS; i++)
	{
		if (!(fbm.bytes[i]%2))
		{ //if lsb = 1
			init_block(i);
			return i;
		}
	}

	return -1;
}

void 
init_sb()
{
	SuperBlock sb;
	init_block(0);
	
	sb.magic_num = MAGIC_NUM;
	sb.block_size = BLOCK_SIZE;
	write_blocks(SB_POS, 1, &sb);
}

void 
mksfs(int fresh)
{
	if (fresh == 0)
	{
		init_disk(DISK_NAME, BLOCK_SIZE, NUM_BLOCKS);
	}
	else
	{
		init_fresh_disk(DISK_NAME, BLOCK_SIZE, NUM_BLOCKS);
		set_busy_bytes(FBM_POS, FBM_POS, 2); //fbm - set fbm and wm to busy
		set_busy_bytes(WM_POS, FBM_POS, 2); // wm - set fbm and wm to busy
		init_sb();
		for(int block_num = START_OF_DIRECTORY_BLOCKS; block_num < START_OF_DIRECTORY_BLOCKS + NUM_DIR_BLOCKS + NUM_INODE_BLOCKS; block_num++){
			init_block(block_num);
		}
	}
	memset(&fdt, -1, sizeof(fdt)); //initialize fdt
}

void 
store_file_data(char *name, 
								DirectoryIndex directory_index, 
								int inb_position[2])
{
	DirectoryBlock db;
	read_blocks(directory_index.block_number, 1, &db);

	DirectoryEntry *current_entry = &db.directory_entries[directory_index.entry_index];

	memcpy(current_entry -> name, name, NAME_SIZE);
	current_entry -> block_number = inb_position[0];
	current_entry -> entry_number = inb_position[1];

	write_blocks(directory_index.block_number, 1, &db);
}

DirectoryIndex
new_file(char *name)
{
	int inb_position[2] = { 0, 0 };
	
	DirectoryIndex next_free_directory_index = search_directory(NULL);

	if (next_free_directory_index.block_number==-1 || find_free_inode(inb_position)==-1)
	{
		return next_free_directory_index;
	}
	
	INode node = get_inode(inb_position);

	int new_block = get_and_init_next_free_block();	
	
	if (new_block != -1)
	{
		node.direct[0] = new_block;
		store_file_data(name, next_free_directory_index, inb_position);
	}

	return next_free_directory_index;
}

int 
fdt_add(DirectoryIndex directory_index)
{
	int fdt_pos = 0;

	while(fdt.open_files[fdt_pos].directory_number!=-1&&fdt_pos<NUM_INODES)
	{
		fdt_pos++;
	}

	if (fdt_pos==NUM_INODES)
	{
		return -1;
	}

	DirectoryBlock db;
	read_blocks(directory_index.block_number, 1, &db);

	int inb_num = db.directory_entries[directory_index.entry_index].block_number;
	int inb_offset = db.directory_entries[directory_index.entry_index].entry_number;

	INodeBlock inb;
	read_blocks(inb_num, 1, &inb);

	OpenFile *file = &fdt.open_files[fdt_pos];
	file -> directory_number = directory_index.block_number;
	file -> offset = directory_index.entry_index;
	file -> read_ptr = 0;
	file -> write_ptr = inb.inodes[inb_offset].fsize;

	return fdt_pos;
}

int 
sfs_fopen(char *name)
{
	DirectoryIndex directory_index = search_directory(name);
	int dir_position[2] = { directory_index.block_number, directory_index.entry_index };

	if (directory_index.block_number==-1)
	{
		directory_index = new_file(name);

		if(directory_index.block_number==-1){
			return -1;
		}
	}
	
	int fdt_pos = fdt_add(directory_index);
	return fdt_pos;
}

int 
sfs_fclose(int fileID)
{
	if (fileID < 0 || fileID > NUM_INODES || fdt.open_files[fileID].directory_number==-1)
	{
		return -1;
	}
	
	OpenFile *file = &fdt.open_files[fileID];
	file -> directory_number = -1;
	file -> offset = -1;
	file -> read_ptr = -1;
	file -> write_ptr = -1;
	return 0;
}

int 
set_ptr(int *ptr, 
				int fileID, 
				int loc)
{
	if (loc<0 || fdt.open_files[fileID].directory_number==-1 || loc>get_inode_fdt(fileID).fsize || fileID<0)
	{
		return -1;
	}

	*ptr = loc;
	return 0;
}

int 
sfs_frseek(int fileID, 
					 int loc)
{
	return set_ptr(&fdt.open_files[fileID].read_ptr, fileID, loc);
}

int 
sfs_fwseek(int fileID, 
					 int loc)
{
	return set_ptr(&fdt.open_files[fileID].write_ptr, fileID, loc);
}

bool
get_or_create_indirect_block(INode *node, Indirect *indirect){
	if(node -> indirect == -1)
	{
		int next_free_block = get_and_init_next_free_block();

		if (next_free_block == -1)
		{
			return false;
		}

		node -> indirect = next_free_block;
	}

	read_blocks(node -> indirect, 1, indirect);
	return true;
}

int 
next_block(INode *node, 
					 int start_ptr)
{
	if(start_ptr >= MAX_FILE_SIZE)
	{
		return -1;
	}

	int file_block_number = start_ptr / BLOCK_SIZE;
	int *block_index;

	bool indirect = false;
	Indirect more;

	if (file_block_number >= NUMBER_OF_DIRECT_POINTERS_PER_INODE)
	{
		indirect = true;
		file_block_number-=13;

		int success = get_or_create_indirect_block(node, &more);

		if (!success)
		{
			return -1;
		}

		block_index = &more.block_ptrs[file_block_number];
	}
	else if (file_block_number < 14 && file_block_number > -1)
	{
		block_index = &(node -> direct[file_block_number]);
	}
	else
	{
		return -1;
	}

	if (*block_index==-1)
	{
		*block_index = get_and_init_next_free_block();

		if (*block_index ==-1)
		{
			return -1;
		}

		if (indirect)
		{
			write_blocks(node -> indirect, 1, &more);
		}
	}

	return *block_index;	
}

int 
sfs_fwrite(int fileID, 
					 char *buf, 
					 int length)
{
	if (fileID<0||fdt.open_files[fileID].directory_number==-1)
	{
		return -1;
	}

	INode node = get_inode_fdt(fileID);

	Block write_buf;
	int amount_written = 0;
	
	while(length > 0)
	{
		int current_pos = fdt.open_files[fileID].write_ptr + amount_written;
		int current_block = next_block(&node, current_pos);

		if (current_block==-1)
		{
			return -1;
		}

		int free_space = BLOCK_SIZE - (current_pos % BLOCK_SIZE);
		int offset = BLOCK_SIZE - free_space;

		read_blocks(current_block, 1, &write_buf.bytes);

		if (length < free_space && length!=0)
		{
			free_space = length;
		}

		memcpy(write_buf.bytes+offset, buf, free_space);

		buf += free_space;
		length -= free_space;
		amount_written += free_space;

		write_blocks(current_block, 1, &write_buf);
		write_inode_fdt(fileID, &node);
	}
	
	fdt.open_files[fileID].write_ptr += amount_written;
	
	if (node.fsize < fdt.open_files[fileID].write_ptr)
	{
		node.fsize = fdt.open_files[fileID].write_ptr;
		write_inode_fdt(fileID, &node);
	}
	
	return amount_written;
}

int 
sfs_fread(int fileID, 
					char *buf, 
					int length)
{
	if (fileID<0||fdt.open_files[fileID].directory_number==-1||length < 0)
	{
		return -1;
	}

	INode node = get_inode_fdt(fileID);
	Block read_buf;

	int amount_read = 0;

	if (fdt.open_files[fileID].read_ptr + length > node.fsize)
	{
		length = node.fsize - fdt.open_files[fileID].read_ptr;
	}

	while(length > 0 && fdt.open_files[fileID].read_ptr <= node.fsize)
	{
		memset(&read_buf, 0, sizeof(read_buf));
		int current_pos = fdt.open_files[fileID].read_ptr;
		int current_block = next_block(&node, current_pos);
		int read_space = BLOCK_SIZE - (current_pos % BLOCK_SIZE);

		int offset = fdt.open_files[fileID].read_ptr % BLOCK_SIZE;

		if (read_space==0||current_block>NUM_BLOCKS||current_block==-1)
		{
			break;
		}

		read_blocks(current_block, 1, &read_buf.bytes);

		if (length < read_space)
		{
			read_space = length;
		}

		memcpy(buf, read_buf.bytes+offset, read_space);
		node.fsize += read_space;

		fdt.open_files[fileID].read_ptr += read_space;
		buf += read_space;
		length -= read_space;
		amount_read += read_space;

	}
	
	return amount_read;
}

void 
free_inode(int dir_position[2])
{
	INode to_free = get_inode(dir_position);

	for(int ptr_num = 0; ptr_num < 14; ptr_num++)
	{
		if (to_free.direct[ptr_num]==-1)
		{
			continue;
		}
		else
		{
			set_free_bytes(FBM_POS, to_free.direct[ptr_num], 1);
		}
	}
	
	if (to_free.indirect!=-1)
	{
		Indirect to_free_ind;
		read_blocks(to_free.indirect, 1, &to_free_ind);

		for(int ind_ptr_num = 0; ind_ptr_num < (BLOCK_SIZE / sizeof(int)); ind_ptr_num++)
			if (to_free_ind.block_ptrs[ind_ptr_num]==-1)
			{
				continue;
			}
			else
			{
				set_free_bytes(FBM_POS, to_free_ind.block_ptrs[ind_ptr_num], 1);
			}
			
	}
	
	memset(&to_free, -1, sizeof(to_free));
	write_inode(dir_position, &to_free);
}

int 
get_pid(int dir_position[2])
{
	int found = 0;
	
	while(found < NUM_INODES)
	{
		if (fdt.open_files[found].directory_number==dir_position[0]&&fdt.open_files[found].offset==dir_position[1])
		{
			return found;
		}
		found++;
	}
	
	return -1;
}

void 
clear_dir_pos(int dir_position[2])
{
	DirectoryBlock db;
	read_blocks(dir_position[0], 1, &db);
	
	DirectoryEntry *current_entry = &db.directory_entries[dir_position[1]];
	memset(current_entry -> name, -1, NAME_SIZE);
	current_entry -> block_number = -1;
	current_entry -> entry_number = -1;
	
	write_blocks(dir_position[0], 1, &db);
}

int 
sfs_remove(char *file)
{
	DirectoryIndex directory_index = search_directory(file);

	if (directory_index.block_number==-1)
	{
		return -1;
	}

	int dir_position[2] = { directory_index.block_number, directory_index.entry_index };

	sfs_fclose(get_pid(dir_position));
	free_inode(dir_position);
	clear_dir_pos(dir_position);
	
	return 0;
}
