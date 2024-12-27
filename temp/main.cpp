#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <windows.h>
#include <string.h>
#include <vector>
#include <string>
#include "ext2.h"

// 将命令字符串拆分为单词，并存储在 res 向量中
void split_cmd(const char* cmd, std::vector<std::string>& res)
{
    res.clear();
    char* _cmd = new char[strlen(cmd) + 1];
    strcpy(_cmd, cmd);
    char* tmp = strtok(_cmd, " ");

    while (tmp)
    {
        res.push_back(std::string(tmp));
        tmp = strtok(NULL, " ");
    }
}

int main(int argc, char* argv[])
{
    if (argc != 3)
    {
        printf("dumpext2 <vmdk_filename> <partition_num>\n");
        return 1;
    }
    ext2_t ext2(argv[1], atoi(argv[2])); // 初始化 ext2 文件系统对象
    if (!ext2.valid) return 1; // 如果文件系统无效，退出

    //ext2.dump_inode(0x44001);
    //ext2.dump_inode(0xc);

    //return 0;

    char cmd[MAX_PATH];
    std::vector<std::string> arg;
    while (1)
    {
        // 显示提示符，等待输入
        printf("\n-");
        gets_s(cmd, MAX_PATH);
        split_cmd(cmd, arg);

        if (arg.size() == 0 || arg[0] == "") continue; //空命令

        if (arg[0] == "q" || arg[0] == "Q") // 退出命令
        {
            break;
        }
        else if (arg[0] == "b") // 显示块命令
        {
            unsigned __int32 bn = (unsigned __int32)_strtoi64(arg[1].c_str(), NULL, 16);
            ext2.dump_block(bn);
        }
        else if (arg[0] == "dump_inode") // 显示索引节点命令
        {
            unsigned __int32 in = (unsigned __int32)_strtoi64(arg[1].c_str(), NULL, 10);
            ext2.dump_inode(in);
        }
        else if (arg[0] == "ls_root") // 查找根目录命令
        {
            ext2.ls_root();
        }
        else if (arg[0] == "ls")
        {
            if (arg.size() < 2) {
                printf("Usage: ls <inode_number>\n");
            }
            else {
                unsigned __int32 inode_num = (unsigned __int32)_strtoi64(arg[1].c_str(), NULL, 10);
                ext2.get_file_blocks(inode_num);  // 获取指定 inode 对应文件的所有数据块
            }
        }
        else if (arg[0] == "super")
        {
            ext2.dump_super_block();
        }
        else if (arg[0] == "mkdir") // 创建新目录命令
        {
            if (arg.size() < 3) {
                printf("Usage: mkdir <parent_inode> <directory_name>\n");
            }
            else {
                unsigned __int32 parent_inode = (unsigned __int32)_strtoi64(arg[1].c_str(), NULL, 10);
                ext2.create_directory(parent_inode, arg[2].c_str());
            }
        }
        else if (arg[0] == "touch") {
            if (arg.size() < 3) {
                printf("Usage: touch <parent_inode> <filename>\n");
            }
            else {
                unsigned int parent_inode = (unsigned int)_strtoi64(arg[1].c_str(), NULL, 10);
                unsigned int mode = 0x81A4; // 普通文件，权限 644
                ext2.create_file(parent_inode, arg[2].c_str(), mode);
            }
        }
        else if (arg[0] == "write") {
            if (arg.size() < 3) {
                printf("Usage: write <inode_num> <content>\n");
            }
            else {
                unsigned int inode_num = (unsigned int)_strtoi64(arg[1].c_str(), NULL, 10);
                ext2.write_file(inode_num, arg[2].c_str(), strlen(arg[2].c_str()));
            }
        }
        else if (arg[0] == "read") {
            if (arg.size() < 2) {
                printf("Usage: read <inode_num>\n");
            }
            else {
                unsigned int inode_num = (unsigned int)_strtoi64(arg[1].c_str(), NULL, 10);
                size_t size;
                char* content = ext2.read_file(inode_num, &size);
                if (content) {
                    printf("File content: %s\n", content);
                    delete[] content;
                }
            }
        }
        else if (arg[0] == "rm") {
            if (arg.size() < 3) {
                printf("Usage: rm <parent_inode> <name>\n");
            }
            else {
                unsigned int parent_inode = (unsigned int)_strtoi64(arg[1].c_str(), NULL, 10);
                ext2.delete_file(parent_inode, arg[2].c_str());
            }
        }
        else if (arg[0] == "rmdir") {
            if (arg.size() < 3) {
                printf("Usage: rmdir <parent_inode> <dirname>\n");
            }
            else {
                unsigned int parent_inode = (unsigned int)_strtoi64(arg[1].c_str(), NULL, 10);
                ext2.delete_directory(parent_inode, arg[2].c_str());
            }
        }
        else
        {
            if (arg[0] == "?" || arg[0] == "h" || arg[0] == "H") // 帮助命令
            {
                printf("不可识别的命令\n");
                printf("可识别的命令如下：\n");
            }

            printf("q|Q   退出\n");
            printf("?|h|H 显示帮助\n");
            printf("b N   显示文件系统中的第 N 个块\n");
            printf("dump_inode N   显示文件系统中的第 N 个索引节点\n");
            printf("ls_root   显示根目录内容\n");
            printf("ls   显示根目录内容\n");
            printf("mkdir <parent_inode> <directory_name>   创建新目录\n");
            printf("touch <parent_inode> <filename>    创建新文件\n");
            printf("write <inode_num> <content>        写入文件内容\n");
            printf("read <inode_num>        读取文件内容\n");
            printf("rm <parent_inode> <name>        删除指定文件\n");
            printf("rmdir <parent_inode> <dirname>        删除指定文件\n");
        }
    }

    return 0;
}

