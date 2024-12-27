#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <time.h>
#include "ext2.h"

// 定义块大小，假设是 1024 字节
#define BLOCK_SIZE 1024

// 计算每个指针项的大小
#define POINTER_SIZE sizeof(uint32_t)

unsigned int* ext2_t::read_block(unsigned int block_num)
{
    unsigned int* block = new unsigned int[BLOCK_SIZE / POINTER_SIZE];
    if (!block) {
        printf("Memory allocation failed for block %u.\n", block_num);
        return nullptr;
    }

    // 跳转到块的偏移位置并读取块内容
    _fseeki64(fp, partition_start * 512 + block_num * BLOCK_SIZE, SEEK_SET);
    fread(block, BLOCK_SIZE, 1, fp);

    return block;
}

void ext2_t::get_file_blocks(unsigned _int32 inode_num) {
    if (inode_num < 1 || inode_num > inodes_count) {
        printf("Invalid inode number.\n");
        return;
    }

    // 读取inode
    unsigned char* inode = new unsigned char[inode_size];
    if (!inode) {
        printf("Memory allocation failed for inode.\n");
        return;
    }

    // 计算 inode 所在的块组和偏移
    unsigned int gn = (inode_num - 1) / inodes_per_group;
    unsigned int index = (inode_num - 1) % inodes_per_group;

    // 获取块组描述符表中的 inode 表位置
    unsigned long bgdt_offset = gn * 32;
    if (bgdt_offset >= block_group_count * 32) {
        printf("Invalid block group number.\n");
        delete[] inode;
        return;
    }

    unsigned long inode_table_block = *(unsigned long*)(block_group_descriptor_table + bgdt_offset + 8);
    if (inode_table_block >= blocks_count) {
        printf("Invalid inode table block number.\n");
        delete[] inode;
        return;
    }

    // 计算 inode 在文件系统中的实际位置
    unsigned long long inode_offset = (unsigned long long)partition_start * 512 +
        (unsigned long long)inode_table_block * block_size +
        (unsigned long long)index * inode_size;

    // 读取 inode
    if (_fseeki64(fp, inode_offset, SEEK_SET) != 0) {
        printf("Failed to seek to inode position.\n");
        delete[] inode;
        return;
    }

    if (fread(inode, inode_size, 1, fp) != 1) {
        printf("Failed to read inode.\n");
        delete[] inode;
        return;
    }

    // 获取文件大小以确定是否需要处理间接块
    unsigned int file_size = *(unsigned int*)(inode + 0x04);
    printf("File size: %u bytes\n", file_size);

    // 获取 i_block 数组
    unsigned int* block_pointers = new unsigned int[15];
    memcpy(block_pointers, inode + 0x28, 60);

    // 处理直接块
    for (int i = 0; i < 12; i++) {
        if (block_pointers[i] != 0) {
            if (block_pointers[i] >= blocks_count) {
                printf("Warning: Invalid direct block number: %u\n", block_pointers[i]);
                continue;
            }
            printf("Direct block %d: %u\n", i, block_pointers[i]);
        }
    }

    // 处理一级间接块
    if (block_pointers[12] != 0 && block_pointers[12] < blocks_count) {
        printf("\nSingle indirect block: %u\n", block_pointers[12]);
        unsigned int* indirect = read_block(block_pointers[12]);
        if (indirect) {
            int entries = block_size / sizeof(unsigned int);
            for (int i = 0; i < entries; i++) {
                if (indirect[i] != 0 && indirect[i] < blocks_count) {
                    printf("  Block %d: %u\n", i, indirect[i]);
                }
            }
            delete[] indirect;
        }
    }

    // 处理二级间接块
    if (block_pointers[13] != 0 && block_pointers[13] < blocks_count) {
        printf("\nDouble indirect block: %u\n", block_pointers[13]);
        unsigned int* dbl_indirect = read_block(block_pointers[13]);
        if (dbl_indirect) {
            int entries = block_size / sizeof(unsigned int);
            for (int i = 0; i < entries; i++) {
                if (dbl_indirect[i] != 0 && dbl_indirect[i] < blocks_count) {
                    printf("  Single indirect block %d: %u\n", i, dbl_indirect[i]);
                    unsigned int* indirect = read_block(dbl_indirect[i]);
                    if (indirect) {
                        for (int j = 0; j < entries; j++) {
                            if (indirect[j] != 0 && indirect[j] < blocks_count) {
                                printf("    Block %d: %u\n", j, indirect[j]);
                            }
                        }
                        delete[] indirect;
                    }
                }
            }
            delete[] dbl_indirect;
        }
    }

    delete[] block_pointers;
    delete[] inode;
}

// 将文件名为 vdfn 的虚拟磁盘文件的第 p (>=0)个分区按照 ext2 文件系统解释
ext2_t::ext2_t(const char* vdfn, int p)
{
    valid = false;
    fp = fopen("d:\\20GB-flat.vmdk", "r+b"); // 以读写二进制方式打开
    if (!fp)
    {
        printf("Open fail\n");
        return;
    }

    // 首先读取磁盘的第 0 扇中的分区表中的第 p 项 ，得到第 p 个分区的起始地址和大小
    unsigned __int8 boot[512];
    fread(boot, 1, 512, fp);
    partition_start = *(unsigned __int32*)(boot + 0x1be + p * 16 + 8); // 得到第 p 个分区的起始扇区号 
    partition_size = *(unsigned __int32*)(boot + 0x1be + p * 16 + 12);  // 得到第 p 个分区的大小

    _fseeki64(fp, partition_start * 512L + 1024, 0);
    fread(super_block, 1, 1024, fp);
    block_size = 1024 << *(unsigned __int32*)(super_block + 0x18); // 0x18  4 s_log_block_size  Block size
    blocks_per_group = *(unsigned __int32*)(super_block + 0x20); // 0x20    4       s_blocks_per_group      # Blocks per group
    inodes_per_group = *(unsigned __int32*)(super_block + 0x28); // 0x28    4       s_inodes_per_group      # Inodes per group
    inode_size = *(unsigned __int16*)(super_block + 0x58); // 0x58  2       s_inode_size        size of inode structure
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

// 将缓冲区中的数据以十六进制和 ASCII 形式打印出来
void ext2_t::dump(unsigned __int8* buf, unsigned __int32 size, unsigned __int64 offset)
{
    for (unsigned int p = 0; p < size;)
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

// 显示指定块的内容
void ext2_t::dump_block(unsigned int bn)
{
    unsigned __int8* block = new unsigned __int8[block_size];
    if (!block) return;

    _fseeki64(fp, (unsigned __int64)partition_start * 512 + bn * block_size, 0);
    fread(block, block_size, 1, fp);

    dump(block, block_size, bn * (unsigned __int64)block_size);
    delete[] block;
}

// 显示超级块的内容
void ext2_t::dump_super_block()
{
    dump(super_block, 1024, 1024);
}

// 显示指定索引节点的内容
void ext2_t::dump_inode(unsigned _int32 i)
{
    if (i < 1 || i > inodes_count)
        return; // 不存在索引节点号为 0 的索引节点
    unsigned __int8* inode = new unsigned __int8[inode_size];
    if (!inode) return;

    unsigned __int32 gn = (i - 1) / inodes_per_group;
    unsigned __int32 index = (i - 1) % inodes_per_group;

    unsigned __int64 bgdt = *(unsigned __int32*)(block_group_descriptor_table + 32 * gn + 8);// 计算第 gn 块中的索引表的地址
    unsigned __int64 off = (unsigned __int64)partition_start * 512 + bgdt * block_size + index * (unsigned __int64)inode_size;
    _fseeki64(fp, off, 0);
    fread(inode, 1, inode_size, fp);
    dump(inode, inode_size, bgdt * block_size + (unsigned __int64)index * inode_size);

    // 打印索引节点的详细信息
    printf("\ni_mode\t%04X", *(unsigned __int16*)(inode + 0));
    printf("\ni_uid\t%04X", *(unsigned __int16*)(inode + 0x02));
    printf("\ni_size\t%08X", *(unsigned __int32*)(inode + 0x04));
    printf("\ni_atime\t%08X (%s)", *(unsigned __int32*)(inode + 0x08), time2str((time_t) * (unsigned __int32*)(inode + 0x08)));
    printf("\ni_ctime\t%08X", *(unsigned __int32*)(inode + 0x0C));
    printf("\ni_mtime\t%08X", *(unsigned __int32*)(inode + 0x10));
    printf("\ni_dtime\t%08X", *(unsigned __int32*)(inode + 0x14));
    printf("\ni_gid\t%04X", *(unsigned __int16*)(inode + 0x18));
    printf("\ni_links_count\t%04X", *(unsigned __int16*)(inode + 0x1A));
    printf("\ni_blocks\t%08X", *(unsigned __int32*)(inode + 0x1C));
    printf("\ni_flags\t%08X", *(unsigned __int32*)(inode + 0x20));
    printf("\ni_reserved1\t%08X", *(unsigned __int32*)(inode + 0x24));

    for (int j = 0; j < 15; j++)
    {
        printf("\ni_block[%d]\t%08X", j, *(unsigned __int32*)(inode + 0x28 + j * 4));
    }

    delete[] inode;
}

void ext2_t::ls_root()
{
    // 根目录的索引节点号通常为 2
    unsigned __int32 root_inode_num = 2;
    unsigned __int8* inode = new unsigned __int8[inode_size];
    if (!inode) return;

    unsigned __int32 gn = (root_inode_num - 1) / inodes_per_group;
    unsigned __int32 index = (root_inode_num - 1) % inodes_per_group;

    unsigned __int64 bgdt = *(unsigned __int32*)(block_group_descriptor_table + 32 * gn + 8);
    unsigned __int64 off = (unsigned __int64)partition_start * 512 + bgdt * block_size + index * (unsigned __int64)inode_size;
    _fseeki64(fp, off, 0);
    fread(inode, 1, inode_size, fp);

    // 读取根目录的数据块
    unsigned __int32 block = *(unsigned __int32*)(inode + 0x28);
    unsigned __int8* block_data = new unsigned __int8[block_size];
    if (!block_data) {
        delete[] inode;
        return;
    }

    _fseeki64(fp, (unsigned __int64)partition_start * 512 + block * block_size, 0);
    fread(block_data, block_size, 1, fp);

    // 解析目录项
    unsigned __int32 offset = 0;
    while (offset < block_size) {
        unsigned __int32 inode_num = *(unsigned __int32*)(block_data + offset);
        unsigned __int16 rec_len = *(unsigned __int16*)(block_data + offset + 4);
        unsigned __int8 name_len = *(unsigned __int8*)(block_data + offset + 6);
        char file_type = *(block_data + offset + 7);
        char name[256];
        strncpy(name, (char*)(block_data + offset + 8), name_len);
        name[name_len] = '\0';

        printf("Inode: %u, Name: %s\n", inode_num, name);

        offset += rec_len;
    }

    delete[] block_data;
    delete[] inode;
}

void ext2_t::create_directory(unsigned _int32 parent_inode_num, const char* dir_name) {
    // 检查父目录 inode 是否有效
    if (parent_inode_num < 1 || parent_inode_num > inodes_count) {
        printf("Invalid parent inode number.\n");
        return;
    }

    // 读取父目录 inode
    unsigned char* parent_inode_data = new unsigned char[inode_size];
    if (!parent_inode_data) {
        printf("Memory allocation failed for parent inode.\n");
        return;
    }

    // 计算父 inode 位置
    unsigned int parent_gn = (parent_inode_num - 1) / inodes_per_group;
    unsigned int parent_index = (parent_inode_num - 1) % inodes_per_group;
    unsigned long parent_inode_table_block = *(unsigned long*)(block_group_descriptor_table + parent_gn * 32 + 8);
    unsigned long long parent_inode_offset = (unsigned long long)partition_start * 512 +
        (unsigned long long)parent_inode_table_block * block_size +
        (unsigned long long)parent_index * inode_size;

    // 读取父 inode
    if (_fseeki64(fp, parent_inode_offset, SEEK_SET) != 0 ||
        fread(parent_inode_data, inode_size, 1, fp) != 1) {
        printf("Failed to read parent inode.\n");
        delete[] parent_inode_data;
        return;
    }

    // 验证父目录是否为目录类型
    unsigned short parent_mode = *(unsigned short*)(parent_inode_data + 0x00);
    if ((parent_mode & 0x4000) != 0x4000) {
        printf("Parent inode is not a directory.\n");
        delete[] parent_inode_data;
        return;
    }

    // 分配新的 inode
    unsigned int new_inode_num = allocate_inode();
    if (new_inode_num == 0) {
        printf("Failed to allocate inode.\n");
        delete[] parent_inode_data;
        return;
    }

    // 分配新的数据块
    unsigned int new_block = allocate_block();
    if (new_block == 0) {
        printf("Failed to allocate block.\n");
        delete[] parent_inode_data;
        return;
    }

    // 创建并初始化新目录的 inode
    unsigned char* new_inode = new unsigned char[inode_size];
    memset(new_inode, 0, inode_size);

    // 设置新目录 inode 的属性
    time_t current_time = time(NULL);
    *(unsigned short*)(new_inode + 0x00) = 0x41ED;    // i_mode: 目录类型 + 权限
    *(unsigned short*)(new_inode + 0x02) = 0;         // i_uid
    *(unsigned int*)(new_inode + 0x04) = block_size;  // i_size: 目录大小
    *(unsigned int*)(new_inode + 0x08) = current_time; // i_atime
    *(unsigned int*)(new_inode + 0x0C) = current_time; // i_ctime
    *(unsigned int*)(new_inode + 0x10) = current_time; // i_mtime
    *(unsigned int*)(new_inode + 0x14) = 0;           // i_dtime
    *(unsigned short*)(new_inode + 0x18) = 0;         // i_gid
    *(unsigned short*)(new_inode + 0x1A) = 2;         // i_links_count: . 和 ..
    *(unsigned int*)(new_inode + 0x1C) = 1;           // i_blocks (以512字节为单位)
    *(unsigned int*)(new_inode + 0x28) = new_block;   // i_block[0]

    // 写入新的 inode
    unsigned int new_gn = (new_inode_num - 1) / inodes_per_group;
    unsigned int new_index = (new_inode_num - 1) % inodes_per_group;
    unsigned long new_inode_table_block = *(unsigned long*)(block_group_descriptor_table + new_gn * 32 + 8);
    unsigned long long new_inode_offset = (unsigned long long)partition_start * 512 +
        (unsigned long long)new_inode_table_block * block_size +
        (unsigned long long)new_index * inode_size;

    if (_fseeki64(fp, new_inode_offset, SEEK_SET) != 0 ||
        fwrite(new_inode, inode_size, 1, fp) != 1) {
        printf("Failed to write new inode.\n");
        delete[] new_inode;
        delete[] parent_inode_data;
        return;
    }

    // 初始化新目录的数据块
    unsigned char* dir_block = new unsigned char[block_size];
    memset(dir_block, 0, block_size);

    // 创建 "." 目录项
    struct ext2_dir_entry {
        unsigned int inode;
        unsigned short rec_len;
        unsigned char name_len;
        unsigned char file_type;
        char name[256];
    } *dir_entry;

    dir_entry = (ext2_dir_entry*)dir_block;
    dir_entry->inode = new_inode_num;
    dir_entry->name_len = 1;
    dir_entry->file_type = 2;  // 目录类型
    strcpy(dir_entry->name, ".");
    dir_entry->rec_len = 12;  // 4 + 2 + 1 + 1 + 4 (对齐到4字节)

    // 创建 ".." 目录项
    dir_entry = (ext2_dir_entry*)(dir_block + dir_entry->rec_len);
    dir_entry->inode = parent_inode_num;
    dir_entry->name_len = 2;
    dir_entry->file_type = 2;
    strcpy(dir_entry->name, "..");
    dir_entry->rec_len = block_size - 12;  // 剩余空间全部分配给 ".."

    // 写入目录数据块
    if (_fseeki64(fp, (unsigned long long)partition_start * 512 + new_block * block_size, SEEK_SET) != 0 ||
        fwrite(dir_block, block_size, 1, fp) != 1) {
        printf("Failed to write directory block.\n");
        delete[] dir_block;
        delete[] new_inode;
        delete[] parent_inode_data;
        return;
    }

    // 更新父目录
    unsigned int parent_block = *(unsigned int*)(parent_inode_data + 0x28);
    unsigned char* parent_block_data = new unsigned char[block_size];
    if (_fseeki64(fp, (unsigned long long)partition_start * 512 + parent_block * block_size, SEEK_SET) != 0 ||
        fread(parent_block_data, block_size, 1, fp) != 1) {
        printf("Failed to read parent directory block.\n");
        delete[] parent_block_data;
        delete[] dir_block;
        delete[] new_inode;
        delete[] parent_inode_data;
        return;
    }

    // 查找父目录中的空闲空间
    unsigned int offset = 0;
    while (offset < block_size) {
        dir_entry = (ext2_dir_entry*)(parent_block_data + offset);
        if (offset + dir_entry->rec_len >= block_size) {
            // 找到最后一个目录项
            unsigned int actual_len = 8 + ((dir_entry->name_len + 3) & ~3);
            if (dir_entry->rec_len - actual_len >= 8 + ((strlen(dir_name) + 3) & ~3)) {
                // 有足够空间插入新目录项
                unsigned int new_rec_len = dir_entry->rec_len - actual_len;
                dir_entry->rec_len = actual_len;

                // 创建新目录项
                dir_entry = (ext2_dir_entry*)(parent_block_data + offset + actual_len);
                dir_entry->inode = new_inode_num;
                dir_entry->name_len = strlen(dir_name);
                dir_entry->file_type = 2;
                strcpy(dir_entry->name, dir_name);
                dir_entry->rec_len = new_rec_len;

                break;
            }
        }
        offset += dir_entry->rec_len;
    }

    // 更新父目录的修改时间
    *(unsigned int*)(parent_inode_data + 0x10) = current_time; // i_mtime
    *(unsigned int*)(parent_inode_data + 0x0C) = current_time; // i_ctime

    // 增加父目录的链接计数
    unsigned short parent_links = *(unsigned short*)(parent_inode_data + 0x1A);
    *(unsigned short*)(parent_inode_data + 0x1A) = parent_links + 1;

    // 写回父目录的 inode 和数据块
    if (_fseeki64(fp, parent_inode_offset, SEEK_SET) != 0 ||
        fwrite(parent_inode_data, inode_size, 1, fp) != 1 ||
        _fseeki64(fp, (unsigned long long)partition_start * 512 + parent_block * block_size, SEEK_SET) != 0 ||
        fwrite(parent_block_data, block_size, 1, fp) != 1) {
        printf("Failed to update parent directory.\n");
    }

    printf("Directory '%s' created successfully with inode %u\n", dir_name, new_inode_num);

    // 清理
    delete[] parent_block_data;
    delete[] dir_block;
    delete[] new_inode;
    delete[] parent_inode_data;
}

unsigned int ext2_t::allocate_block() {
    // 遍历所有块组，查找空闲块
    for (unsigned int group = 0; group < block_group_count; group++) {
        // 读取块位图
        unsigned long bgdt_offset = group * 32;
        unsigned long block_bitmap_block = *(unsigned long*)(block_group_descriptor_table + bgdt_offset);
        unsigned char* block_bitmap = new unsigned char[block_size];
        if (!block_bitmap) {
            printf("Memory allocation failed for block bitmap.\n");
            return 0;
        }

        _fseeki64(fp, (unsigned long long)partition_start * 512 + block_bitmap_block * block_size, SEEK_SET);
        fread(block_bitmap, block_size, 1, fp);

        // 查找空闲块
        for (unsigned int byte = 0; byte < block_size; byte++) {
            if (block_bitmap[byte] != 0xFF) { // 不是全1，说明有空闲块
                for (unsigned int bit = 0; bit < 8; bit++) {
                    if (!(block_bitmap[byte] & (1 << bit))) { // 找到空闲块
                        // 标记块为已使用
                        block_bitmap[byte] |= (1 << bit);
                        _fseeki64(fp, (unsigned long long)partition_start * 512 + block_bitmap_block * block_size, SEEK_SET);
                        fwrite(block_bitmap, block_size, 1, fp);

                        delete[] block_bitmap;
                        return group * blocks_per_group + byte * 8 + bit;
                    }
                }
            }
        }

        delete[] block_bitmap;
    }

    printf("No free blocks available.\n");
    return 0;
}

unsigned int ext2_t::allocate_inode() {
    // 遍历所有块组，查找空闲 inode
    for (unsigned int group = 0; group < block_group_count; group++) {
        // 读取 inode 位图
        unsigned long bgdt_offset = group * 32;
        unsigned long inode_bitmap_block = *(unsigned long*)(block_group_descriptor_table + bgdt_offset + 4);
        unsigned char* inode_bitmap = new unsigned char[block_size];
        if (!inode_bitmap) {
            printf("Memory allocation failed for inode bitmap.\n");
            return 0;
        }

        _fseeki64(fp, (unsigned long long)partition_start * 512 + inode_bitmap_block * block_size, SEEK_SET);
        fread(inode_bitmap, block_size, 1, fp);

        // 查找空闲 inode
        for (unsigned int byte = 0; byte < block_size; byte++) {
            if (inode_bitmap[byte] != 0xFF) { // 不是全1，说明有空闲 inode
                for (unsigned int bit = 0; bit < 8; bit++) {
                    if (!(inode_bitmap[byte] & (1 << bit))) { // 找到空闲 inode
                        // 标记 inode 为已使用
                        inode_bitmap[byte] |= (1 << bit);
                        _fseeki64(fp, (unsigned long long)partition_start * 512 + inode_bitmap_block * block_size, SEEK_SET);
                        fwrite(inode_bitmap, block_size, 1, fp);

                        delete[] inode_bitmap;
                        return group * inodes_per_group + byte * 8 + bit + 1; // inode 编号从 1 开始
                    }
                }
            }
        }

        delete[] inode_bitmap;
    }

    printf("No free inodes available.\n");
    return 0;
}

unsigned int ext2_t::create_file(unsigned _int32 parent_inode, const char* filename, unsigned int mode) {
    // 分配新的 inode
    unsigned int new_inode_num = allocate_inode();
    if (new_inode_num == 0) {
        printf("Failed to allocate inode for file.\n");
        return 0;
    }

    // 读取父目录的 inode
    unsigned char* parent_inode_data = new unsigned char[inode_size];
    if (!parent_inode_data) {
        printf("Memory allocation failed for parent inode.\n");
        return 0;
    }

    // 计算父目录 inode 的位置
    unsigned int parent_gn = (parent_inode - 1) / inodes_per_group;
    unsigned int parent_index = (parent_inode - 1) % inodes_per_group;
    unsigned long parent_inode_table_block = *(unsigned long*)(block_group_descriptor_table + parent_gn * 32 + 8);
    unsigned long long parent_inode_offset = (unsigned long long)partition_start * 512 +
        (unsigned long long)parent_inode_table_block * block_size +
        (unsigned long long)parent_index * inode_size;

    // 读取父目录的 inode
    if (_fseeki64(fp, parent_inode_offset, SEEK_SET) != 0 ||
        fread(parent_inode_data, inode_size, 1, fp) != 1) {
        printf("Failed to read parent inode.\n");
        delete[] parent_inode_data;
        return 0;
    }

    // 初始化新文件的 inode
    unsigned char* new_inode = new unsigned char[inode_size];
    memset(new_inode, 0, inode_size);

    // 设置文件属性
    time_t current_time = time(NULL);
    *(unsigned short*)(new_inode + 0x00) = mode;              // 文件模式
    *(unsigned short*)(new_inode + 0x02) = 0;                 // uid
    *(unsigned int*)(new_inode + 0x04) = 0;                   // size
    *(unsigned int*)(new_inode + 0x08) = current_time;        // atime
    *(unsigned int*)(new_inode + 0x0C) = current_time;        // ctime
    *(unsigned int*)(new_inode + 0x10) = current_time;        // mtime
    *(unsigned short*)(new_inode + 0x1A) = 1;                 // links count
    *(unsigned int*)(new_inode + 0x1C) = 0;                   // blocks count (512-byte units)
    *(unsigned int*)(new_inode + 0x28) = 0;                   // first direct block

    // 写入新的 inode
    unsigned int new_gn = (new_inode_num - 1) / inodes_per_group;
    unsigned int new_index = (new_inode_num - 1) % inodes_per_group;
    unsigned long new_inode_table_block = *(unsigned long*)(block_group_descriptor_table + new_gn * 32 + 8);
    unsigned long long new_inode_offset = (unsigned long long)partition_start * 512 +
        (unsigned long long)new_inode_table_block * block_size +
        (unsigned long long)new_index * inode_size;

    if (_fseeki64(fp, new_inode_offset, SEEK_SET) != 0 ||
        fwrite(new_inode, inode_size, 1, fp) != 1) {
        printf("Failed to write new inode.\n");
        delete[] new_inode;
        delete[] parent_inode_data;
        return 0;
    }

    // 读取父目录的数据块
    unsigned int parent_block = *(unsigned int*)(parent_inode_data + 0x28);  // 修正：使用parent_inode_data
    unsigned char* dir_block = new unsigned char[block_size];
    if (!dir_block) {
        delete[] new_inode;
        delete[] parent_inode_data;
        return 0;
    }

    if (_fseeki64(fp, (unsigned long long)partition_start * 512 + parent_block * block_size, SEEK_SET) != 0 ||
        fread(dir_block, block_size, 1, fp) != 1) {
        printf("Failed to read parent directory block.\n");
        delete[] dir_block;
        delete[] new_inode;
        delete[] parent_inode_data;
        return 0;
    }

    // 在父目录中添加新文件的目录项
    struct ext2_dir_entry {
        unsigned int inode;
        unsigned short rec_len;
        unsigned char name_len;
        unsigned char file_type;
        char name[256];
    } *dir_entry;

    unsigned int offset = 0;
    bool entry_added = false;

    while (offset < block_size) {
        dir_entry = (ext2_dir_entry*)(dir_block + offset);

        if (dir_entry->inode == 0 || offset + dir_entry->rec_len == block_size) {
            // 找到空闲位置或最后一个目录项
            unsigned int required_space = 8 + ((strlen(filename) + 3) & ~3);  // 对齐到4字节边界
            unsigned int available_space;

            if (dir_entry->inode == 0) {
                available_space = dir_entry->rec_len;
            }
            else {
                unsigned int actual_size = 8 + ((dir_entry->name_len + 3) & ~3);
                available_space = dir_entry->rec_len - actual_size;
                offset += actual_size;
            }

            if (available_space >= required_space) {
                // 创建新的目录项
                dir_entry = (ext2_dir_entry*)(dir_block + offset);
                dir_entry->inode = new_inode_num;
                dir_entry->name_len = strlen(filename);
                dir_entry->file_type = 1;  // 普通文件
                strncpy(dir_entry->name, filename, dir_entry->name_len);
                dir_entry->rec_len = available_space;

                entry_added = true;
                break;
            }
        }

        if (!entry_added) {
            offset += dir_entry->rec_len;
        }
    }

    if (!entry_added) {
        printf("No space available in parent directory block.\n");
        delete[] dir_block;
        delete[] new_inode;
        delete[] parent_inode_data;
        return 0;
    }

    // 写回目录块
    if (_fseeki64(fp, (unsigned long long)partition_start * 512 + parent_block * block_size, SEEK_SET) != 0 ||
        fwrite(dir_block, block_size, 1, fp) != 1) {
        printf("Failed to write directory block.\n");
        delete[] dir_block;
        delete[] new_inode;
        delete[] parent_inode_data;
        return 0;
    }

    // 更新父目录的时间戳
    *(unsigned int*)(parent_inode_data + 0x10) = current_time; // mtime
    *(unsigned int*)(parent_inode_data + 0x0C) = current_time; // ctime

    // 写回父目录的 inode
    if (_fseeki64(fp, parent_inode_offset, SEEK_SET) != 0 ||
        fwrite(parent_inode_data, inode_size, 1, fp) != 1) {
        printf("Failed to update parent inode.\n");
    }

    printf("File '%s' created successfully with inode %u\n", filename, new_inode_num);

    delete[] dir_block;
    delete[] new_inode;
    delete[] parent_inode_data;
    return new_inode_num;
}

bool ext2_t::write_file(unsigned int inode_num, const char* content, size_t size) {
    // 读取文件的 inode
    unsigned char* inode = new unsigned char[inode_size];
    unsigned int gn = (inode_num - 1) / inodes_per_group;
    unsigned int index = (inode_num - 1) % inodes_per_group;
    unsigned long inode_table_block = *(unsigned long*)(block_group_descriptor_table + gn * 32 + 8);
    unsigned long long inode_offset = (unsigned long long)partition_start * 512 +
        (unsigned long long)inode_table_block * block_size +
        (unsigned long long)index * inode_size;

    if (_fseeki64(fp, inode_offset, SEEK_SET) != 0 ||
        fread(inode, inode_size, 1, fp) != 1) {
        printf("Failed to read inode.\n");
        delete[] inode;
        return false;
    }

    // 计算需要的块数
    unsigned int blocks_needed = (size + block_size - 1) / block_size;
    unsigned int* block_nums = new unsigned int[blocks_needed];

    // 分配所需的块
    for (unsigned int i = 0; i < blocks_needed; i++) {
        block_nums[i] = allocate_block();
        if (block_nums[i] == 0) {
            printf("Failed to allocate block.\n");
            delete[] block_nums;
            delete[] inode;
            return false;
        }
    }

    // 更新 inode 的数据块指针
    for (unsigned int i = 0; i < blocks_needed && i < 12; i++) {
        *(unsigned int*)(inode + 0x28 + i * 4) = block_nums[i];
    }

    // 如果需要间接块
    if (blocks_needed > 12) {
        unsigned int indirect_block = allocate_block();
        *(unsigned int*)(inode + 0x28 + 48) = indirect_block; // i_block[12]

        unsigned int* indirect_data = new unsigned int[block_size / 4];
        for (unsigned int i = 12; i < blocks_needed; i++) {
            indirect_data[i - 12] = block_nums[i];
        }

        _fseeki64(fp, (unsigned long long)partition_start * 512 + indirect_block * block_size, SEEK_SET);
        fwrite(indirect_data, block_size, 1, fp);
        delete[] indirect_data;
    }

    // 更新 inode 的文件大小和时间
    *(unsigned int*)(inode + 0x04) = size;  // i_size
    time_t current_time = time(NULL);
    *(unsigned int*)(inode + 0x08) = current_time; // i_atime
    *(unsigned int*)(inode + 0x0C) = current_time; // i_ctime
    *(unsigned int*)(inode + 0x10) = current_time; // i_mtime

    // 写回 inode
    _fseeki64(fp, inode_offset, SEEK_SET);
    fwrite(inode, inode_size, 1, fp);

    // 写入文件内容
    size_t remaining = size;
    const char* current_pos = content;
    for (unsigned int i = 0; i < blocks_needed; i++) {
        size_t write_size = (remaining > block_size) ? block_size : remaining;
        _fseeki64(fp, (unsigned long long)partition_start * 512 + block_nums[i] * block_size, SEEK_SET);
        fwrite(current_pos, write_size, 1, fp);
        current_pos += write_size;
        remaining -= write_size;
    }

    delete[] block_nums;
    delete[] inode;
    return true;
}

char* ext2_t::read_file(unsigned int inode_num, size_t* size) {
    // 读取文件的 inode
    unsigned char* inode = new unsigned char[inode_size];
    unsigned int gn = (inode_num - 1) / inodes_per_group;
    unsigned int index = (inode_num - 1) % inodes_per_group;
    unsigned long inode_table_block = *(unsigned long*)(block_group_descriptor_table + gn * 32 + 8);
    unsigned long long inode_offset = (unsigned long long)partition_start * 512 +
        (unsigned long long)inode_table_block * block_size +
        (unsigned long long)index * inode_size;

    if (_fseeki64(fp, inode_offset, SEEK_SET) != 0 ||
        fread(inode, inode_size, 1, fp) != 1) {
        printf("Failed to read inode.\n");
        delete[] inode;
        return nullptr;
    }

    // 获取文件大小
    *size = *(unsigned int*)(inode + 0x04);
    char* content = new char[*size + 1];
    content[*size] = '\0';

    // 读取直接块
    size_t bytes_read = 0;
    for (int i = 0; i < 12 && bytes_read < *size; i++) {
        unsigned int block_num = *(unsigned int*)(inode + 0x28 + i * 4);
        if (block_num == 0) break;

        size_t read_size = (*size - bytes_read > block_size) ? block_size : (*size - bytes_read);
        _fseeki64(fp, (unsigned long long)partition_start * 512 + block_num * block_size, SEEK_SET);
        fread(content + bytes_read, read_size, 1, fp);
        bytes_read += read_size;
    }

    // 读取间接块
    if (bytes_read < *size) {
        unsigned int indirect_block = *(unsigned int*)(inode + 0x28 + 48);
        if (indirect_block != 0) {
            unsigned int* indirect_data = new unsigned int[block_size / 4];
            _fseeki64(fp, (unsigned long long)partition_start * 512 + indirect_block * block_size, SEEK_SET);
            fread(indirect_data, block_size, 1, fp);

            for (unsigned int i = 0; i < block_size / 4 && bytes_read < *size; i++) {
                if (indirect_data[i] == 0) break;

                size_t read_size = (*size - bytes_read > block_size) ? block_size : (*size - bytes_read);
                _fseeki64(fp, (unsigned long long)partition_start * 512 + indirect_data[i] * block_size, SEEK_SET);
                fread(content + bytes_read, read_size, 1, fp);
                bytes_read += read_size;
            }

            delete[] indirect_data;
        }
    }

    delete[] inode;
    return content;
}

bool ext2_t::delete_file(unsigned int parent_inode, const char* name) {
    if (name == nullptr || strlen(name) == 0) {
        printf("Invalid filename (null pointer or empty).\n");
        return false;
    }

    // 读取父目录的 inode
    unsigned char* parent_inode_data = new unsigned char[inode_size];
    if (!parent_inode_data) {
        printf("Memory allocation failed for parent inode.\n");
        return false;
    }

    // 计算父目录 inode 的位置
    unsigned int parent_gn = (parent_inode - 1) / inodes_per_group;
    unsigned int parent_index = (parent_inode - 1) % inodes_per_group;
    unsigned long parent_inode_table_block = *(unsigned long*)(block_group_descriptor_table + parent_gn * 32 + 8);
    unsigned long long parent_inode_offset = (unsigned long long)partition_start * 512 +
        (unsigned long long)parent_inode_table_block * block_size +
        (unsigned long long)parent_index * inode_size;

    // 读取父目录的 inode
    if (_fseeki64(fp, parent_inode_offset, SEEK_SET) != 0 ||
        fread(parent_inode_data, inode_size, 1, fp) != 1) {
        printf("Failed to read parent inode.\n");
        delete[] parent_inode_data;
        return false;
    }

    // 读取父目录的数据块
    unsigned int parent_block = *(unsigned int*)(parent_inode_data + 0x28);
    unsigned char* dir_data = new unsigned char[block_size];
    if (!dir_data) {
        printf("Memory allocation failed for directory data.\n");
        delete[] parent_inode_data;
        return false;
    }

    if (_fseeki64(fp, (unsigned long long)partition_start * 512 + parent_block * block_size, SEEK_SET) != 0 ||
        fread(dir_data, block_size, 1, fp) != 1) {
        printf("Failed to read directory block.\n");
        delete[] dir_data;
        delete[] parent_inode_data;
        return false;
    }

    // 在目录中查找文件条目
    struct ext2_dir_entry {
        unsigned int inode;
        unsigned short rec_len;
        unsigned char name_len;
        unsigned char file_type;
        char name[256];
    } *curr_entry = nullptr, * prev_entry = nullptr;

    unsigned int curr_offset = 0;
    unsigned int prev_offset = 0;
    unsigned int target_inode = 0;
    unsigned int entry_offset = 0;
    bool entry_found = false;

    // 遍历目录项
    while (curr_offset < block_size) {
        curr_entry = (ext2_dir_entry*)(dir_data + curr_offset);

        // 1. 添加边界检查
        if (curr_offset + sizeof(ext2_dir_entry) > block_size) {
            break;
        }

        // 2. 计算实际的目录项长度（4字节对齐）
        unsigned int actual_len = sizeof(ext2_dir_entry) - 256 + curr_entry->name_len;
        actual_len = (actual_len + 3) & ~3; // 4字节对齐

        // 3. 正确比较文件名
        if (curr_entry->inode != 0 &&
            curr_entry->name_len == strlen(name) &&
            strncmp(curr_entry->name, name, curr_entry->name_len) == 0) {
            target_inode = curr_entry->inode;
            entry_offset = curr_offset;
            entry_found = true;
            break;
        }

        // 4. 使用记录的长度进行偏移，而不是计算的长度
        curr_offset += curr_entry->rec_len;

        // 5. 添加额外的安全检查
        if (curr_entry->rec_len == 0 || curr_offset > block_size) {
            printf("Invalid directory entry found\n");
            break;
        }
    }

    if (!entry_found) {
        printf("File '%s' not found in directory.\n", name);
        delete[] dir_data;
        delete[] parent_inode_data;
        return false;
    }

    // 读取文件的 inode
    unsigned char* file_inode = new unsigned char[inode_size];
    if (!file_inode) {
        printf("Memory allocation failed for file inode.\n");
        delete[] dir_data;
        delete[] parent_inode_data;
        return false;
    }

    // 计算文件 inode 的位置
    unsigned int file_gn = (target_inode - 1) / inodes_per_group;
    unsigned int file_index = (target_inode - 1) % inodes_per_group;
    unsigned long file_inode_table_block = *(unsigned long*)(block_group_descriptor_table + file_gn * 32 + 8);
    unsigned long long file_inode_offset = (unsigned long long)partition_start * 512 +
        (unsigned long long)file_inode_table_block * block_size +
        (unsigned long long)file_index * inode_size;

    // 读取文件 inode
    if (_fseeki64(fp, file_inode_offset, SEEK_SET) != 0 ||
        fread(file_inode, inode_size, 1, fp) != 1) {
        printf("Failed to read file inode.\n");
        delete[] file_inode;
        delete[] dir_data;
        delete[] parent_inode_data;
        return false;
    }

    // 释放文件的数据块
    for (int i = 0; i < 12; i++) {
        unsigned int block_num = *(unsigned int*)(file_inode + 0x28 + i * 4);
        if (block_num != 0) {
            free_block(block_num);
            printf("Freed direct block %u\n", block_num);
        }
    }

    // 处理间接块
    unsigned int indirect_block = *(unsigned int*)(file_inode + 0x28 + 48);
    if (indirect_block != 0) {
        unsigned int* indirect_blocks = new unsigned int[block_size / 4];
        if (indirect_blocks) {
            _fseeki64(fp, (unsigned long long)partition_start * 512 + indirect_block * block_size, SEEK_SET);
            fread(indirect_blocks, block_size, 1, fp);

            for (unsigned int i = 0; i < block_size / 4; i++) {
                if (indirect_blocks[i] != 0) {
                    free_block(indirect_blocks[i]);
                    printf("Freed indirect block %u\n", indirect_blocks[i]);
                }
            }

            free_block(indirect_block);
            delete[] indirect_blocks;
        }
    }

    // 更新目录项
    curr_entry = (ext2_dir_entry*)(dir_data + entry_offset);
    if (entry_offset + curr_entry->rec_len == block_size && prev_entry) {
        // 如果是最后一个条目，增加前一个条目的长度
        prev_entry->rec_len += curr_entry->rec_len;
    }
    else {
        // 将后续条目向前移动
        unsigned int next_offset = entry_offset + curr_entry->rec_len;
        unsigned int move_size = block_size - next_offset;
        if (move_size > 0) {
            memmove(dir_data + entry_offset, dir_data + next_offset, move_size);
        }
    }

    // 写回修改后的目录块
    if (_fseeki64(fp, (unsigned long long)partition_start * 512 + parent_block * block_size, SEEK_SET) != 0 ||
        fwrite(dir_data, block_size, 1, fp) != 1) {
        printf("Failed to write directory block.\n");
        delete[] file_inode;
        delete[] dir_data;
        delete[] parent_inode_data;
        return false;
    }

    // 释放文件的 inode
    free_inode(target_inode);
    printf("Freed inode %u\n", target_inode);

    // 更新父目录的时间戳
    time_t current_time = time(NULL);
    *(unsigned int*)(parent_inode_data + 0x10) = current_time; // mtime
    *(unsigned int*)(parent_inode_data + 0x0C) = current_time; // ctime

    // 写回父目录的 inode
    if (_fseeki64(fp, parent_inode_offset, SEEK_SET) != 0 ||
        fwrite(parent_inode_data, inode_size, 1, fp) != 1) {
        printf("Failed to update parent directory inode.\n");
    }

    printf("Successfully deleted file '%s' (inode %u)\n", name, target_inode);

    // 清理内存
    delete[] file_inode;
    delete[] dir_data;
    delete[] parent_inode_data;
    return true;
}

bool ext2_t::delete_directory(unsigned _int32 parent_inode, const char* name) {
    // 首先找到目录的 inode 号
    unsigned char* parent_inode_data = new unsigned char[inode_size];
    unsigned int parent_gn = (parent_inode - 1) / inodes_per_group;
    unsigned int parent_index = (parent_inode - 1) % inodes_per_group;
    unsigned long parent_inode_table_block = *(unsigned long*)(block_group_descriptor_table + parent_gn * 32 + 8);
    unsigned long long parent_inode_offset = (unsigned long long)partition_start * 512 +
        (unsigned long long)parent_inode_table_block * block_size +
        (unsigned long long)parent_index * inode_size;

    if (_fseeki64(fp, parent_inode_offset, SEEK_SET) != 0 ||
        fread(parent_inode_data, inode_size, 1, fp) != 1) {
        printf("Failed to read parent inode.\n");
        delete[] parent_inode_data;
        return false;
    }

    // 获取目录的 inode 号
    unsigned int dir_block = *(unsigned int*)(parent_inode_data + 0x28);
    unsigned char* dir_data = new unsigned char[block_size];
    if (_fseeki64(fp, (unsigned long long)partition_start * 512 + dir_block * block_size, SEEK_SET) != 0 ||
        fread(dir_data, block_size, 1, fp) != 1) {
        printf("Failed to read directory block.\n");
        delete[] dir_data;
        delete[] parent_inode_data;
        return false;
    }

    // 查找目录项
    struct ext2_dir_entry {
        unsigned int inode;
        unsigned short rec_len;
        unsigned char name_len;
        unsigned char file_type;
        char name[256];
    } *dir_entry;

    unsigned int offset = 0;
    unsigned int target_inode = 0;
    while (offset < block_size) {
        dir_entry = (ext2_dir_entry*)(dir_data + offset);
        if (dir_entry->inode != 0 &&
            dir_entry->name_len == strlen(name) &&
            strncmp(dir_entry->name, name, dir_entry->name_len) == 0) {
            target_inode = dir_entry->inode;
            break;
        }
        offset += dir_entry->rec_len;
    }

    if (target_inode == 0) {
        printf("Directory not found.\n");
        delete[] dir_data;
        delete[] parent_inode_data;
        return false;
    }

    // 递归删除目录及其内容
    if (!recursive_delete_directory(target_inode)) {
        printf("Failed to delete directory contents.\n");
        delete[] dir_data;
        delete[] parent_inode_data;
        return false;
    }

    // 从父目录中移除目录项
    if (!remove_directory_entry(parent_inode, name)) {
        printf("Failed to remove directory entry.\n");
        delete[] dir_data;
        delete[] parent_inode_data;
        return false;
    }

    delete[] dir_data;
    delete[] parent_inode_data;
    return true;
}

bool ext2_t::recursive_delete_directory(unsigned int dir_inode) {
    unsigned char* inode_data = new unsigned char[inode_size];
    unsigned int gn = (dir_inode - 1) / inodes_per_group;
    unsigned int index = (dir_inode - 1) % inodes_per_group;
    unsigned long inode_table_block = *(unsigned long*)(block_group_descriptor_table + gn * 32 + 8);
    unsigned long long inode_offset = (unsigned long long)partition_start * 512 +
        (unsigned long long)inode_table_block * block_size +
        (unsigned long long)index * inode_size;

    if (_fseeki64(fp, inode_offset, SEEK_SET) != 0 ||
        fread(inode_data, inode_size, 1, fp) != 1) {
        delete[] inode_data;
        return false;
    }

    // 读取目录的数据块
    unsigned int dir_block = *(unsigned int*)(inode_data + 0x28);
    unsigned char* dir_data = new unsigned char[block_size];
    if (_fseeki64(fp, (unsigned long long)partition_start * 512 + dir_block * block_size, SEEK_SET) != 0 ||
        fread(dir_data, block_size, 1, fp) != 1) {
        delete[] dir_data;
        delete[] inode_data;
        return false;
    }

    // 遍历目录项
    struct ext2_dir_entry {
        unsigned int inode;
        unsigned short rec_len;
        unsigned char name_len;
        unsigned char file_type;
        char name[256];
    } *dir_entry;

    unsigned int offset = 0;
    while (offset < block_size) {
        dir_entry = (ext2_dir_entry*)(dir_data + offset);
        if (dir_entry->inode != 0) {
            // 跳过 "." 和 ".." 目录
            if (!(dir_entry->name_len == 1 && dir_entry->name[0] == '.') &&
                !(dir_entry->name_len == 2 && dir_entry->name[0] == '.' && dir_entry->name[1] == '.')) {

                char entry_name[256];
                strncpy(entry_name, dir_entry->name, dir_entry->name_len);
                entry_name[dir_entry->name_len] = '\0';

                if (dir_entry->file_type == 2) { // 目录
                    if (!recursive_delete_directory(dir_entry->inode)) {
                        delete[] dir_data;
                        delete[] inode_data;
                        return false;
                    }
                }
                // 释放 inode 和数据块
                free_inode(dir_entry->inode);
            }
        }
        offset += dir_entry->rec_len;
    }

    // 释放目录自身的 inode 和数据块
    free_block(dir_block);
    free_inode(dir_inode);

    delete[] dir_data;
    delete[] inode_data;
    return true;
}

bool ext2_t::remove_directory_entry(unsigned int parent_inode, const char* name) {
    unsigned char* parent_inode_data = new unsigned char[inode_size];
    unsigned int parent_gn = (parent_inode - 1) / inodes_per_group;
    unsigned int parent_index = (parent_inode - 1) % inodes_per_group;
    unsigned long parent_inode_table_block = *(unsigned long*)(block_group_descriptor_table + parent_gn * 32 + 8);
    unsigned long long parent_inode_offset = (unsigned long long)partition_start * 512 +
        (unsigned long long)parent_inode_table_block * block_size +
        (unsigned long long)parent_index * inode_size;

    if (_fseeki64(fp, parent_inode_offset, SEEK_SET) != 0 ||
        fread(parent_inode_data, inode_size, 1, fp) != 1) {
        delete[] parent_inode_data;
        return false;
    }

    // 读取父目录的数据块
    unsigned int parent_block = *(unsigned int*)(parent_inode_data + 0x28);
    unsigned char* dir_data = new unsigned char[block_size];
    if (_fseeki64(fp, (unsigned long long)partition_start * 512 + parent_block * block_size, SEEK_SET) != 0 ||
        fread(dir_data, block_size, 1, fp) != 1) {
        delete[] dir_data;
        delete[] parent_inode_data;
        return false;
    }

    struct ext2_dir_entry {
        unsigned int inode;
        unsigned short rec_len;
        unsigned char name_len;
        unsigned char file_type;
        char name[256];
    } *dir_entry, * prev_entry = nullptr;

    unsigned int offset = 0;
    while (offset < block_size) {
        dir_entry = (ext2_dir_entry*)(dir_data + offset);
        if (dir_entry->name_len == strlen(name) &&
            strncmp(dir_entry->name, name, dir_entry->name_len) == 0) {

            // 如果不是最后一个条目，将后面的条目向前移动
            if (offset + dir_entry->rec_len < block_size) {
                memmove(dir_data + offset,
                    dir_data + offset + dir_entry->rec_len,
                    block_size - offset - dir_entry->rec_len);
                if (prev_entry) {
                    prev_entry->rec_len += dir_entry->rec_len;
                }
            }
            else if (prev_entry) {
                // 如果是最后一个条目，增加前一个条目的长度
                prev_entry->rec_len += dir_entry->rec_len;
            }

            // 写回目录块
            _fseeki64(fp, (unsigned long long)partition_start * 512 + parent_block * block_size, SEEK_SET);
            fwrite(dir_data, block_size, 1, fp);

            delete[] dir_data;
            delete[] parent_inode_data;
            return true;
        }
        prev_entry = dir_entry;
        offset += dir_entry->rec_len;
    }

    delete[] dir_data;
    delete[] parent_inode_data;
    return false;
}

void ext2_t::free_inode(unsigned int inode_num) {
    unsigned int group = (inode_num - 1) / inodes_per_group;
    unsigned int index = (inode_num - 1) % inodes_per_group;
    unsigned int byte_index = index / 8;
    unsigned int bit_index = index % 8;

    // 读取 inode 位图
    unsigned long inode_bitmap_block = *(unsigned long*)(block_group_descriptor_table + group * 32 + 4);
    unsigned char* bitmap = new unsigned char[block_size];

    _fseeki64(fp, (unsigned long long)partition_start * 512 + inode_bitmap_block * block_size, SEEK_SET);
    fread(bitmap, block_size, 1, fp);

    // 清除位图中的相应位
    bitmap[byte_index] &= ~(1 << bit_index);

    // 写回位图
    _fseeki64(fp, (unsigned long long)partition_start * 512 + inode_bitmap_block * block_size, SEEK_SET);
    fwrite(bitmap, block_size, 1, fp);

    delete[] bitmap;
}

void ext2_t::free_block(unsigned int block_num) {
    unsigned int group = block_num / blocks_per_group;
    unsigned int index = block_num % blocks_per_group;
    unsigned int byte_index = index / 8;
    unsigned int bit_index = index % 8;

    // 读取块位图
    unsigned long block_bitmap_block = *(unsigned long*)(block_group_descriptor_table + group * 32);
    unsigned char* bitmap = new unsigned char[block_size];

    _fseeki64(fp, (unsigned long long)partition_start * 512 + block_bitmap_block * block_size, SEEK_SET);
    fread(bitmap, block_size, 1, fp);

    // 清除位图中的相应位
    bitmap[byte_index] &= ~(1 << bit_index);

    // 写回位图
    _fseeki64(fp, (unsigned long long)partition_start * 512 + block_bitmap_block * block_size, SEEK_SET);
    fwrite(bitmap, block_size, 1, fp);

    delete[] bitmap;
}