#pragma once

#include <stdio.h>
#include <time.h>
#include <string.h>

class ext2_t
{
    FILE* fp; // 文件指针
    unsigned __int32 partition_start; // 分区起始地址
    unsigned __int32 partition_size; // 分区大小
    unsigned __int8 super_block[1024]; // 超级块
    unsigned __int32 inodes_count; // 索引节点数量
    unsigned __int32 block_size; // 块大小
    unsigned __int32 blocks_per_group; // 每组块数
    unsigned __int32 inodes_per_group; // 每组索引节点数
    unsigned __int16 inode_size; // 索引节点大小
    unsigned __int8* block_group_descriptor_table;  // 组描述符表，记得在析构中释放
    unsigned __int32 blocks_count; // 块总数
    unsigned __int32 block_group_count; // 块组总数

    // 向上对齐
    unsigned __int64 align_up(unsigned __int64 p, unsigned __int32 s)
    {
        if (p % s == 0)
            return p;
        else
            return p / s * s + s;
    }

    // 将时间转换为字符串
    char* time2str(time_t t)
    {
        char* s = ctime(&t);
        s[strlen(s) - 1] = '\0';
        return s;
    }

    // 打印缓冲区内容
    void dump(unsigned __int8* buf, unsigned __int32 size, unsigned __int64 offset);

public:
    ext2_t(const char* vdfn, int p); // 将文件名为 vdfn 的虚拟磁盘文件的第 p 个分区按照 ext2 文件系统解释
    void dump_block(unsigned int bn); // 打印指定块
    void dump_super_block(); // 打印超级块
    void dump_inode(unsigned _int32 inode); // 打印指定索引节点
    void ls_root(); // 查找并显示根目录内容
    unsigned int* read_block(unsigned int block_num);
    void get_file_blocks(unsigned _int32 inode_num);


    bool valid; // 文件系统是否有效
};
