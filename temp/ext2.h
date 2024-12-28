#pragma once

#include <stdio.h>
#include <time.h>
#include <string.h>
#include <set>
#include <vector>
#include <string>
#include<algorithm>

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
    void list_directory(unsigned int dir_inode, const std::string& prefix);
    void ls_root(); // 查找并显示根目录内容
    unsigned int* read_block(unsigned int block_num);
    void get_file_blocks(unsigned _int32 inode_num); // 获取文件的数据块，支持多级索引
    bool validate_block_number(unsigned int block_num, const char* block_type);
    void read_indirect_block(unsigned int block_num, int level, std::set<unsigned int>& seen_blocks);
    void create_directory(unsigned _int32 parent_inode, const char* dir_name); // 创建新目录
    unsigned int allocate_inode(); // 分配一个新的 inode
    unsigned int allocate_block();
    // 文件操作函数
    unsigned int create_file(unsigned int parent_inode, const char* filename, unsigned int mode);
    bool write_file(unsigned int inode_num, const char* content, size_t size);
    char* read_file(unsigned int inode_num, size_t* size);
    bool delete_file(unsigned int parent_inode, const char* name);
    bool delete_directory(unsigned int parent_inode, const char* name);
    // 辅助函数
    bool remove_directory_entry(unsigned int parent_inode, const char* name);
    void free_inode(unsigned int inode_num);
    void free_block(unsigned int block_num);
    void show_tree(unsigned int inode_num);
    void show_tree_recursive(unsigned int inode_num, const char* prefix, bool last);
    bool recursive_delete_directory(unsigned int dir_inode);
    bool add_entry_to_dir(FILE* fp, unsigned __int64 partition_start, unsigned int block_size, unsigned int dir_block,
        unsigned int new_inode, const char* name, unsigned char file_type);

    bool valid; // 文件系统是否有效


};