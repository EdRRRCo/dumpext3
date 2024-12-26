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
            unsigned __int32 in = (unsigned __int32)_strtoi64(arg[1].c_str(), NULL, 16);
            ext2.dump_inode(in);
        }
        else if (arg[0] == "ls_root") // 查找根目录命令
        {
            ext2.ls_root();
        }
        else if (arg[0] == "ls")
        {
            // 假设 inode 号为 3 是你要获取的文件的 inode
            ext2.get_file_blocks(11);  // 获取 inode 3 对应文件的所有数据块
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

        }
    }

    return 0;
}


