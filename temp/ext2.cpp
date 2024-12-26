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

// 获取文件的数据块，支持多级索引
void ext2_t::get_file_blocks(unsigned _int32 inode_num)
{
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

    // 计算 inode 所在的块组
    unsigned int gn = (inode_num - 1) / inodes_per_group;
    unsigned int index = (inode_num - 1) % inodes_per_group;

    unsigned long bgdt = *(unsigned long*)(block_group_descriptor_table + gn * 32 + 8);
    unsigned long inode_table_offset = partition_start * 512 + bgdt * block_size + index * inode_size;
    _fseeki64(fp, inode_table_offset, SEEK_SET);
    fread(inode, inode_size, 1, fp);

    // 获取 inode 中的 i_block 数组，表示数据块的指针
    unsigned int* block_pointers = new unsigned int[15]; // 12 个直接指针 + 3 个间接指针
    memcpy(block_pointers, inode + 0x28, 60);  // 从 inode 结构中复制数据块指针

    // 处理直接指针（前 12 个）
    for (int i = 0; i < 12; ++i) {
        if (block_pointers[i] != 0) {
            printf("Direct block %d: %u\n", i, block_pointers[i]);
        }
    }

    // 处理一级间接索引
    if (block_pointers[12] != 0) {
        unsigned int* indirect_block = read_block(block_pointers[12]);
        if (indirect_block) {
            for (int i = 0; i < BLOCK_SIZE / POINTER_SIZE; ++i) {
                if (indirect_block[i] != 0) {
                    printf("Indirect block 1 - Block %d: %u\n", i, indirect_block[i]);
                }
            }
            delete[] indirect_block;
        }
    }

    // 处理二级间接索引
    if (block_pointers[13] != 0) {
        unsigned int* indirect_block1 = read_block(block_pointers[13]);
        if (indirect_block1) {
            for (int i = 0; i < BLOCK_SIZE / POINTER_SIZE; ++i) {
                if (indirect_block1[i] != 0) {
                    unsigned int* indirect_block2 = read_block(indirect_block1[i]);
                    if (indirect_block2) {
                        for (int j = 0; j < BLOCK_SIZE / POINTER_SIZE; ++j) {
                            if (indirect_block2[j] != 0) {
                                printf("Indirect block 2 - Block %d: %u\n", j, indirect_block2[j]);
                            }
                        }
                        delete[] indirect_block2;
                    }
                }
            }
            delete[] indirect_block1;
        }
    }

    // 处理三级间接索引
    if (block_pointers[14] != 0) {
        unsigned int* indirect_block1 = read_block(block_pointers[14]);
        if (indirect_block1) {
            for (int i = 0; i < BLOCK_SIZE / POINTER_SIZE; ++i) {
                if (indirect_block1[i] != 0) {
                    unsigned int* indirect_block2 = read_block(indirect_block1[i]);
                    if (indirect_block2) {
                        for (int j = 0; j < BLOCK_SIZE / POINTER_SIZE; ++j) {
                            if (indirect_block2[j] != 0) {
                                unsigned int* indirect_block3 = read_block(indirect_block2[j]);
                                if (indirect_block3) {
                                    for (int k = 0; k < BLOCK_SIZE / POINTER_SIZE; ++k) {
                                        if (indirect_block3[k] != 0) {
                                            printf("Indirect block 3 - Block %d: %u\n", k, indirect_block3[k]);
                                        }
                                    }
                                    delete[] indirect_block3;
                                }
                            }
                        }
                        delete[] indirect_block2;
                    }
                }
            }
            delete[] indirect_block1;
        }
    }

    // 释放内存
    delete[] inode;
    delete[] block_pointers;
}

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
    if (i<0 || i> inodes_count) return; // 不存在索引节点号为 0 的索引节点
    unsigned __int8* inode = new unsigned __int8[inode_size];
    if (!inode) return;

    unsigned __int32 gn = (i - 1) / inodes_per_group;
    unsigned __int32 index = (i - 1) % inodes_per_group;

    unsigned __int64 bgdt = *(unsigned __int32*)(block_group_descriptor_table + 32 * gn + 8);// 计算第 gn 块中的索引表的地址
    unsigned __int64 off = (unsigned __int64)partition_start * 512 + bgdt * block_size + index * (unsigned __int64)inode_size;
    _fseeki64(fp, off, 0);
    int c = fread(inode, 1, inode_size, fp);
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

    for (int i = 0; i < 15; i++)
    {
        printf("\ni_block[%d]\t%08X", i, *(unsigned __int32*)(inode + 0x28 + i * 4));
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
