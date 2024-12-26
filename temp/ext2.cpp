#define _CRT_SECURE_NO_WARNINGS

#include <math.h>
#include <time.h>
#include "ext2.h"

#define EXT2_MAGIC_NUMBER 0xEF53  // ext2超级块的魔数

// 假设ext2_t类已经定义，并且包含了其他必需的成员变量和函数


// 将文件名为 vdfn 的虚拟磁盘文件的第 p (>=0)个分区按照 ext2 文件系统解释
ext2_t::ext2_t(const char* vdfn, int p)
{
	valid = false;
	fp = fopen("d:\\20GB-flat.vmdk", "rb"); // 以仅读二进制方式打开
	if (!fp)
	{
		printf("Open fail\n");
		return;
	}
	
	// 首先读取磁盘的第 0 扇中的分区表中的第 p 项 ，得到第 p 个分区的起始地址和大小
	unsigned __int8 boot[512];
	fread(boot, 1, 512, fp);
	partition_start = *(unsigned __int32 *)(boot + 0x1be + p * 16 + 8); // 得到第 p 个分区的起始扇区号 
	partition_size = *(unsigned __int32*)(boot + 0x1be + p * 16 + 12);  // 得到第 p 个分区的大小

	_fseeki64(fp, partition_start*512L+1024, 0);
	fread(super_block, 1, 1024, fp);
	block_size = 1024 << *(unsigned __int32*)(super_block + 0x18); // 0x18 	4 s_log_block_size  Block size
	blocks_per_group = *(unsigned __int32*)(super_block + 0x20); // 0x20 	4		s_blocks_per_group		# Blocks per group
	inodes_per_group = *(unsigned __int32*)(super_block + 0x28); // 0x28 	4		s_inodes_per_group		# Inodes per group
	inode_size = *(unsigned __int16*)(super_block + 0x58); // 0x58 	2		s_inode_size		size of inode structure
	blocks_count = *(unsigned __int32*)(super_block + 0x4);
	block_group_count = (unsigned __int32)ceil((double)blocks_count / blocks_per_group);
	inodes_count = *(unsigned __int32*)(super_block + 0);
	
	block_group_descriptor_table = new unsigned __int8[32 * block_group_count];
	if (!block_group_descriptor_table) return;
	unsigned __int64 bgdt = partition_start * 512L + align_up(1024 + 1024, block_size);
	_fseeki64(fp, bgdt, 0); //gdt 的在卷中的始址必须按照块边界对齐
	fread(block_group_descriptor_table, 1, 32 * block_group_count, fp);


	valid = true;

}

void ext2_t::dump(unsigned __int8* buf, unsigned __int32 size, unsigned __int64 offset)
{
	for (unsigned int p = 0;p<size;)
	{
		printf("%016llX ", offset);
		for (int i = 0; i < 16; i++)
		{
			if (i == 8) printf("- ");
			printf("%02X ", (unsigned __int8)buf[p + i]);
		}

		for (int i = 0; i < 16; i++)
		{
			if (buf[p + i] >= 0x20 && buf[p + i] <= 0x7e)
				printf("%c", buf[p + i]);
			else
				printf(".");
		}

		printf("\n");
		p = p + 16;
		offset = offset + 16;
	}
}

void ext2_t::dump_block(unsigned int bn)
{
	unsigned __int8* block = new unsigned __int8[block_size];
	if (!block) return;

	_fseeki64(fp, (unsigned __int64)partition_start * 512 + bn* block_size, 0);
	fread(block, block_size, 1, fp);
		
	dump(block, block_size, bn* (unsigned __int64)block_size);
	delete[] block;
}

void ext2_t::dump_super_block()
{
	dump(super_block, 1024, 1024);
}

void ext2_t::list_root_directory() {
	if (!valid) return;
	dump_inode(2); // 根目录通常为 inode 2

	unsigned __int32 root_inode_num = 2; // 根目录索引节点号
	unsigned __int8* inode = new unsigned __int8[inode_size];
	unsigned __int32 gn = (root_inode_num - 1) / inodes_per_group;
	unsigned __int32 index = (root_inode_num - 1) % inodes_per_group;

	unsigned __int64 bgdt = *(unsigned __int32*)(block_group_descriptor_table + 32 * gn + 8);
	unsigned __int64 off = (unsigned __int64)partition_start * 512 + bgdt * block_size + index * inode_size;
	_fseeki64(fp, off, 0);
	fread(inode, 1, inode_size, fp);

	unsigned __int32* block_pointers = (unsigned __int32*)(inode + 0x28);
	for (int i = 0; i < 12; i++) { // 只处理直接块
		if (block_pointers[i] == 0) break;
		unsigned __int8* block = new unsigned __int8[block_size];
		_fseeki64(fp, (unsigned __int64)partition_start * 512 + block_pointers[i] * block_size, 0);
		fread(block, 1, block_size, fp);

		unsigned __int8* entry = block;
		while (entry < block + block_size) {
			unsigned __int32 inode = *(unsigned __int32*)entry;
			unsigned __int16 entry_length = *(unsigned __int16*)(entry + 4);
			unsigned __int8 name_length = *(unsigned __int8*)(entry + 6);
			if (inode == 0) break;
			printf("Inode: %u, Name: %.*s\n", inode, name_length, entry + 8);
			entry += entry_length;
		}
		delete[] block;
	}
	delete[] inode;
}

void ext2_t::read_file(unsigned __int32 inode_num) {
	if (inode_num < 1 || inode_num > inodes_count) return;
	unsigned __int8* inode = new unsigned __int8[inode_size];
	unsigned __int32 gn = (inode_num - 1) / inodes_per_group;
	unsigned __int32 index = (inode_num - 1) % inodes_per_group;

	unsigned __int64 bgdt = *(unsigned __int32*)(block_group_descriptor_table + 32 * gn + 8);
	unsigned __int64 off = (unsigned __int64)partition_start * 512 + bgdt * block_size + index * inode_size;
	_fseeki64(fp, off, 0);
	fread(inode, 1, inode_size, fp);

	unsigned __int32* block_pointers = (unsigned __int32*)(inode + 0x28);
	for (int i = 0; i < 12; i++) { // 只处理直接块
		if (block_pointers[i] == 0) break;
		unsigned __int8* block = new unsigned __int8[block_size];
		_fseeki64(fp, (unsigned __int64)partition_start * 512 + block_pointers[i] * block_size, 0);
		fread(block, 1, block_size, fp);
		fwrite(block, 1, block_size, stdout);
		delete[] block;
	}
	delete[] inode;
}

void ext2_t::dump_inode(unsigned _int32 i)
{
	if (i<0 || i> inodes_count) return; // 不存在索引节点号为 0 的索引节点
	unsigned __int8* inode = new unsigned __int8[inode_size];
	if (!inode) return;

	unsigned __int32 gn = (i - 1) / inodes_per_group;
	unsigned __int32 index = (i - 1) % inodes_per_group;

	
	unsigned __int64 bgdt = *(unsigned __int32*)(block_group_descriptor_table + 32 * gn + 8);// 计算第 gn 块中的索引表的地址
	unsigned __int64 off = (unsigned __int64)partition_start * 512 + bgdt * block_size + index * (unsigned __int64)inode_size;
	_fseeki64(fp, off, 0);
	//int c= fread(inode,  inode_size, 1, fp);
	int c = fread(inode,  1, inode_size, fp);
	dump(inode, inode_size, bgdt * block_size + (unsigned __int64)index * inode_size);

	/*
	0x00	2	i_mode	File mode
	0x02	2	i_uid	Low 16 bits of Owner Uid
	0x04	4	i_size	Size in bytes	文件以字节为单位的大小 。
	0x08	4	i_atime	Access time
	0x0C	4	i_ctime	Creation time
	0x10	4	i_mtime	Modification time
	0x14	4	i_dtime	Deletion Time
	0x18	2	i_gid	Low 16 bits of Group Id
	0x1A	2	i_links_count	Links count
	0x1C	4	i_blocks	Blocks count		文件以扇区（512字节）为单位的大小 。警告：这里的“块”的实际含义是扇区 。
	0x20	4	i_flags	File flags
	0x24	4	i_reserved1	OS dependent 1
	0x28	4*15	i_block[15]	Pointers to blocks
	0x64	4	i_generation	File version (for NFS)
	0x68	4	i_file_acl	File ACL
	0x6C	4	i_dir_acl		Directory ACL
	0x70	4	i_faddr	Fragment address
	0x74	1	l_i_frag	Fragment number
	0x75	1	l_i_fsize	Fragment size
	*/
	printf("\ni_mode\t%04X",	*(unsigned __int16*)(inode + 0));
	printf("\ni_uid\t%04X",		*(unsigned __int16*)(inode + 0x02));
	printf("\ni_size\t%08X",	*(unsigned __int32*)(inode + 0x04));

	//time_t t = (time_t)*(unsigned __int32 *)(inode + 0x08);
	//char* s = ctime(&t);
	//s[strlen(s) - 1] = '\0';
	//printf("\ni_atime\t%08X (%s)",	*(unsigned __int32*)(inode + 0x08), s );
	printf("\ni_atime\t%08X (%s)", *(unsigned __int32*)(inode + 0x08), time2str((time_t)*(unsigned __int32*)(inode + 0x08)) );

	printf("\ni_ctime\t%08X",	*(unsigned __int32*)(inode + 0x0C));
	printf("\ni_mtime\t%08X",	*(unsigned __int32*)(inode + 0x10));
	printf("\ni_dtime\t%08X",	*(unsigned __int32*)(inode + 0x14));
	printf("\ni_gid\t%04X",		*(unsigned __int16*)(inode + 0x18));
	printf("\ni_links_count\t%04X", *(unsigned __int16*)(inode + 0x1A));
	printf("\ni_blocks\t%08X", *(unsigned __int32*)(inode + 0x1C));
	printf("\ni_flags\t%08X", *(unsigned __int32*)(inode + 0x20));
	printf("\ni_reserved1\t%08X", *(unsigned __int32*)(inode + 0x24));

	for (int i = 0; i < 15; i++)
	{
		printf("\ni_block[%d]\t%08X", i,*(unsigned __int32*)(inode + 0x28+i*4));
	}

	delete[] inode;
}
