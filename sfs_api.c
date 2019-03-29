#include "sfs_api.h"

FileDescrTable fdt;

void set_free_bytes(int block_id, int byte, int num_bytes){
	if(block_id==FBM_POS||block_id==WM_POS){
		Block buffer;
		read_blocks(block_id, 1, &buffer);

		for(int i = byte; i < byte + num_bytes; i++){
			buffer.bytes[i] <<= 1;
			buffer.bytes[i] |= 0;
		}
		
		write_blocks(block_id, 1, &buffer);
	}
	else
		printf("Cannot set free bytes for this type of block.\n");
}

void set_busy_bytes(int block_id, int byte, int num_bytes){
	if(block_id==FBM_POS||block_id==WM_POS){
		Block buffer;
		read_blocks(block_id, 1, &buffer);

		for(int i = byte; i < byte + num_bytes; i++){
			buffer.bytes[i] <<= 1;
			buffer.bytes[i] |= 1;
		}

		write_blocks(block_id, 1, &buffer);
	}
	else
		printf("Cannot set busy bytes for this type of block.\n");
}

INode get_inode(int (*dir_position)[2]){
	DirectoryBlock db;

	int dir_num = (*dir_position)[0];
	int dir_offset = (*dir_position)[1];
	read_blocks(dir_num, 1, &db);

	INodeBlock inb;
	int inb_num = db.position[dir_offset][0];
	int inb_offset = db.position[dir_offset][1];
	read_blocks(inb_num, 1, &inb);

	return inb.inodes[inb_offset];
}

INode get_inode_fdt(int fileID){
	int dir_position[2] = {fdt.dir_ptr[fileID][0], fdt.dir_ptr[fileID][1]};
	return get_inode(&dir_position);
}

void write_inode(int (*dir_position)[2], INode *new_node){
	DirectoryBlock db;
	int dir_num = (*dir_position)[0];
	int dir_offset = (*dir_position)[1];
	read_blocks(dir_num, 1, &db);

	INodeBlock inb;
	int inb_num = db.position[dir_offset][0];
	int inb_offset = db.position[dir_offset][1];
	read_blocks(inb_num, 1, &inb);

	memcpy(&(inb.inodes[inb_offset]), new_node, sizeof(INode));
	write_blocks(inb_num, 1, &inb);
}

void write_inode_fdt(int fileID, INode *node){
	int dir_position[2] = {fdt.dir_ptr[fileID][0], fdt.dir_ptr[fileID][1]};
	write_inode(&dir_position, node);
}

int find_free_inode(int (*inb_position)[2]){
	INodeBlock current_block;

	for(int block_num = INB_POS; block_num < INB_POS + NUM_INODE_BLOCKS; block_num++){
		read_blocks(block_num, 1, &current_block);
		for(int offset = 0; offset < NUM_INODES_PER_INB; offset++)
			if(current_block.inodes[offset].fsize==-1){
				(*inb_position)[0] = block_num;
				(*inb_position)[1] = offset;
				current_block.inodes[offset].fsize++;
				write_blocks(block_num, 1, &current_block);
				return 0;
			}
	}
	return -1;
}

int find_free_file(int (*dir_position)[2]){
	DirectoryBlock db;
	for(int dir_num = DB_POS; dir_num < DB_POS + NUM_DIR_BLOCKS; dir_num++){
		read_blocks(dir_num, 1, &db);
		for(int dir_offset = 0; dir_offset < DIR_BLOCK_SIZE; dir_offset++){
			if(db.position[dir_offset][0]==-1){
				(*dir_position)[0] = dir_num;
				(*dir_position)[1] = dir_offset;
				return 0;
			}
		}
	}
	return -1;		
}

int find_free_block(){
	Block fbm;
	read_blocks(FBM_POS, 1, &fbm);
	for(int i = 0; i < NUM_BLOCKS; i++)
		if(!(fbm.bytes[i]%2)) //if lsb = 1
			return i;
	return -1;
}

void init_block(int block_num){
	set_busy_bytes(FBM_POS, block_num, 1);
	Block init;
	memset(&init, -1, sizeof(init));
	write_blocks(block_num, 1, &init);
}

void init_sb(){
	SuperBlock sb;
	memset(&sb, -1, sizeof(sb));
	sb.magic_num = MAGIC_NUM;
	sb.block_size = BLOCK_SIZE;
	write_blocks(SB_POS, 1, &sb);
}

void mksfs(int fresh){
	if(fresh == 0)
		init_disk(DISK_NAME, BLOCK_SIZE, NUM_BLOCKS);
	else{
		init_fresh_disk(DISK_NAME, BLOCK_SIZE, NUM_BLOCKS);
		set_busy_bytes(FBM_POS, FBM_POS, 2); //fbm - set fbm and wm to busy
		set_busy_bytes(WM_POS, FBM_POS, 2); 	// wm - set fbm and wm to busy
		init_sb();
		for(int dir_num = DB_POS; dir_num < DB_POS + NUM_DIR_BLOCKS; dir_num++)
			init_block(dir_num);
		for(int inb_num = DB_POS; inb_num < INB_POS + NUM_INODE_BLOCKS; inb_num++)
			init_block(inb_num);
	}
	memset(&fdt, -1, sizeof(fdt)); //initialize fdt
}

int search_directory(char *name, int (*dir_position)[2]){
	DirectoryBlock current_directory;

	for(int dir_num = DB_POS; dir_num < DB_POS + NUM_DIR_BLOCKS; dir_num++){
		read_blocks(dir_num, 1, &current_directory);
		for(int offset = 0; offset < DIR_BLOCK_SIZE; offset++)
			if(strcmp(name, current_directory.names[offset])==0){
				(*dir_position)[0] = dir_num;
				(*dir_position)[1] = offset;
				return 0;
			}
	}
	return -1;
}

void store_file_data(char *name, int (*dir_position)[2], int (*inb_position)[2]){
	DirectoryBlock db;
	read_blocks((*dir_position)[0], 1, &db);

	memcpy(db.names[(*dir_position)[1]], name, NAME_SIZE);

	db.position[(*dir_position)[1]][0] = (*inb_position)[0];
	db.position[(*dir_position)[1]][1] = (*inb_position)[1];

	write_blocks((*dir_position)[0], 1, &db);
}

int new_file(char *name, int (*dir_position)[2]){
	int inb_position[2] = {0, 0};
	if(find_free_file(dir_position)==-1)
		return -1;
	if(find_free_inode(&inb_position)==-1)
		return -1;
	INode node;
	memset(&node, -1, sizeof(node));
	node = get_inode(&inb_position);


	int new_block = find_free_block();
	if(new_block == -1)
		return -1;
	init_block(new_block);
	node.direct[0] = new_block;
	store_file_data(name, dir_position, &inb_position);

	return 0;
}

int fdt_add(int (*dir_position)[2]){
	int fdt_pos = 0;

	while(fdt.dir_ptr[fdt_pos][0]!=-1&&fdt_pos<NUM_INODES)
		fdt_pos++;

	if(fdt_pos==NUM_INODES)
		return -1;

	int dir_num = (*dir_position)[0];
	int dir_offset = (*dir_position)[1];

	DirectoryBlock db;
	read_blocks(dir_num, 1, &db);

	int inb_num = db.position[dir_offset][0];
	int inb_offset = db.position[dir_offset][1];

	INodeBlock inb;
	read_blocks(inb_num, 1, &inb);

	fdt.dir_ptr[fdt_pos][0] = dir_num;
	fdt.dir_ptr[fdt_pos][1] = dir_offset;

	fdt.read_ptr[fdt_pos] = 0;
	fdt.write_ptr[fdt_pos] = inb.inodes[inb_offset].fsize;

	return fdt_pos;
}

int sfs_fopen(char *name){
	int dir_position[2] = {0, 0};

	if(search_directory(name, &dir_position)==-1)
		if(new_file(name, &dir_position)==-1)
			return -1;
	int fdt_pos = fdt_add(&dir_position);
	return fdt_pos;
}

int sfs_fclose(int fileID){
	if(fileID < 0||fileID > NUM_INODES)
		return -1;
	if(fdt.dir_ptr[fileID][0]==-1)
		return -1;
	fdt.dir_ptr[fileID][0] = -1;
	fdt.dir_ptr[fileID][1] = -1;
	fdt.read_ptr[fileID] = -1;
	fdt.write_ptr[fileID] = -1;
	return 0;
}

int sfs_frseek(int fileID,int loc){
	if(fdt.dir_ptr[fileID][0]==-1||loc<0||loc>get_inode_fdt(fileID).fsize||fileID<0)
		return -1;
	fdt.read_ptr[fileID] = loc;
	return 0;
}

int sfs_fwseek(int fileID, int loc){
	if(fdt.dir_ptr[fileID][0]==-1||loc<0||loc>get_inode_fdt(fileID).fsize||fileID<0)
		return -1;
	fdt.write_ptr[fileID] = loc;
	return 0;
}

int block_space(INode node, int start_ptr){
	return (BLOCK_SIZE - (start_ptr % BLOCK_SIZE));
}

int next_block(INode *node, int start_ptr){
	int ptr = start_ptr / BLOCK_SIZE;
	if(ptr!=0&&ptr%BLOCK_SIZE==0)
		ptr--;

	if(ptr>(BLOCK_SIZE/sizeof(int) + 13))
		return -1;
	else if(ptr >= 14 && ptr < 13 + (BLOCK_SIZE/sizeof(int))){
		ptr-=13;
		Indirect more;
		memset(&more, -1, sizeof(more));
		if((*node).indirect==-1){
			if(((*node).indirect = find_free_block())==-1)
				return -1;
			init_block((*node).indirect);
		}
		read_blocks((*node).indirect, 1, &more);
		if(more.block_ptrs[ptr]==-1){
			if((more.block_ptrs[ptr] = find_free_block())==-1)
				return -1;
			init_block(more.block_ptrs[ptr]);
			write_blocks((*node).indirect, 1, &more);
		}
		return more.block_ptrs[ptr];
	}
	else if (ptr < 14 && ptr > -1){
		if((*node).direct[ptr]==-1){
			if(((*node).direct[ptr] = find_free_block())==-1)
				return -1;
			init_block((*node).direct[ptr]);
		}
		return (*node).direct[ptr];	
	}
	else
		return -1;
}

int sfs_fwrite(int fileID, char *buf, int length){
	if(fileID<0||fdt.dir_ptr[fileID][0]==-1)
		return -1;

	INode node = get_inode_fdt(fileID);

	Block write_buf;
	int amount_written = 0;
	while(length > 0){
		int current_pos = fdt.write_ptr[fileID] + amount_written;
		int current_block = next_block(&node, current_pos);

		if(current_block==-1)
			return -1;

		int free_space = block_space(node, current_pos);
		int offset = BLOCK_SIZE - free_space;

		read_blocks(current_block, 1, &write_buf.bytes);

		if(length < free_space && length!=0)
			free_space = length;

		memcpy(write_buf.bytes+offset, buf, free_space);

		buf += free_space;
		length -= free_space;
		amount_written += free_space;

		write_blocks(current_block, 1, &write_buf);
		write_inode_fdt(fileID, &node);
	}
	fdt.write_ptr[fileID] += amount_written;
	if(node.fsize < fdt.write_ptr[fileID]){
		node.fsize = fdt.write_ptr[fileID];
		write_inode_fdt(fileID, &node);
	}
	return amount_written;
}

int sfs_fread(int fileID, char *buf, int length){
	if(fileID<0||fdt.dir_ptr[fileID][0]==-1||length < 0)
		return -1;

	INode node = get_inode_fdt(fileID);
	Block read_buf;

	int amount_read = 0;

	if(fdt.read_ptr[fileID] + length > node.fsize)
		length = node.fsize - fdt.read_ptr[fileID];

	while(length > 0 && fdt.read_ptr[fileID] <= node.fsize){
		memset(&read_buf, 0, sizeof(read_buf));
		int current_pos = fdt.read_ptr[fileID];
		int current_block = next_block(&node, current_pos);
		int read_space = block_space(node, current_pos);

		int offset = fdt.read_ptr[fileID] % BLOCK_SIZE;

		if(read_space==0||current_block>NUM_BLOCKS||current_block==-1)
			break;

		read_blocks(current_block, 1, &read_buf.bytes);

		if(length < read_space)
			read_space = length;


		memcpy(buf, read_buf.bytes+offset, read_space);
		node.fsize += read_space;

		fdt.read_ptr[fileID] += read_space;
		buf += read_space;
		length -= read_space;
		amount_read += read_space;

	}
	return amount_read;
}

void free_inode(int (*dir_position)[2]){
	INode to_free = get_inode(dir_position);

	for(int ptr_num = 0; ptr_num < 14; ptr_num++){
		if(to_free.direct[ptr_num]==-1)
			continue;
		else
			set_free_bytes(FBM_POS, to_free.direct[ptr_num], 1);
	}
	if(to_free.indirect!=-1){
		Indirect to_free_ind;
		read_blocks(to_free.indirect, 1, &to_free_ind);
		for(int ind_ptr_num = 0; ind_ptr_num < (BLOCK_SIZE / sizeof(int)); ind_ptr_num++)
			if(to_free_ind.block_ptrs[ind_ptr_num]==-1)
				continue;
			else
				set_free_bytes(FBM_POS, to_free_ind.block_ptrs[ind_ptr_num], 1);
			
	}
	memset(&to_free, -1, sizeof(to_free));
	write_inode(dir_position, &to_free);
}

int search_fdt(int dir_position[2]){
	int found = 0;
	while(found < NUM_INODES){
		if(fdt.dir_ptr[found][0]==dir_position[0]&&fdt.dir_ptr[found][1]==dir_position[1])
			return found;
		found++;
	}
	return -1;
}

void clear_dir_pos(int dir_position[2]){
	DirectoryBlock db;
	read_blocks(dir_position[0], 1, &db);
	int entry = dir_position[1];
	memset(&(db.names[entry]), -1, NAME_SIZE);
	db.position[entry][0] = -1;
	db.position[entry][1] = -1;
	write_blocks(dir_position[0], 1, &db);
}

int sfs_remove(char *file){
	int dir_position[2] = {0, 0};

	if(search_directory(file, &dir_position)==-1)
		return -1;

	int pid = search_fdt(dir_position);

	sfs_fclose(pid);
	free_inode(&dir_position);
	clear_dir_pos(dir_position);
	return 0;
}
