//preprocessing
#pragma warning(disable:4996)
#include <iostream>
#include <iomanip>
#include <ctime>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include "ufs.h"
#include <vector>
#include <numeric>
#include <tuple>
#include <string>

using namespace std;

//meta data of my file system
//allocate 16 MB
#define BLOCK_SIZE 1024
#define SYSTEM_SIZE 16 * 1024 * BLOCK_SIZE
#define FREE_SIZE 16 * 900* BLOCK_SIZE// < 16*1024 - 20 -1600
//design parameter
#define MAX_NAME_SIZE 28
#define MAX_DIR_DEPTH 10
#define DIR_SIZE 16
#define INODE_NUM 2500
#define FREE_BLOCK_NUM FREE_SIZE / BLOCK_SIZE
#define FREE_INODE_BUF_SIZE (int)INODE_NUM/10

#define DIR_TYPE 0
#define FILE_TYPE 1

#define SYSTEM_FILE_NAME "UFS.sys"

//Use 20 blocks, Block 0~19
struct SuperBlock {
    int Magic = 0xf0f03410;
    unsigned short rest_free_block_num;
    bool inode_bitmap[INODE_NUM];
    bool block_bitmap[FREE_BLOCK_NUM];
    int free_inode_buff[FREE_INODE_BUF_SIZE];
    int free_inode_pointer;
    int free_inode_buff_num;

};

struct DirItem {
    char item_name[MAX_NAME_SIZE];
    unsigned short item_inode_id;
};

struct INode { // Use 1600 blocks, Block 20-1600
    int inode_type;
    int inode_num_link;
    struct DirItem dir_items[DIR_SIZE];
    int dir_size;
    int file_size;
    time_t create_time;
    int inode_addr[10];
    //from where to search next inode
    int inode_indirect_addr;
};

struct FileSystem {
    struct SuperBlock sb;
    struct INode inode[INODE_NUM];
    char free_space[FREE_SIZE/BLOCK_SIZE][BLOCK_SIZE];
};

//this buffer can hold maximum file size
char display_buffer[(10 + BLOCK_SIZE / 2) * BLOCK_SIZE];

struct FileSystem* myfs = new FileSystem;

char PATH[MAX_NAME_SIZE * MAX_DIR_DEPTH] = "";

void Initialize_dir_inode(int i)
{
    myfs->inode[i].inode_type = DIR_TYPE;
    myfs->inode[i].file_size = 0;
    myfs->inode[i].create_time = time(NULL);
    myfs->inode[i].dir_size = 0;
    myfs->inode[i].inode_addr[0] = NULL;
    myfs->inode[i].inode_num_link = 1;
}




int get_inode(char* path) {
    //return inode id according to the given path
    if (path[0] != '/') {
        return -1;
    }
    else {
        // get inode 0 from the root
        struct INode cinode = myfs->inode[0];
        int id = 0;
        char cpath[MAX_NAME_SIZE * MAX_DIR_DEPTH] = "";
        strcpy(cpath, path);
        char* fpath = strtok(cpath, "/");
        while (fpath != NULL) {
            //i is used to search through directory
            for (int i = 0; i < DIR_SIZE; i++) {
                //find sutable items
                if (!strncmp(fpath, cinode.dir_items[i].item_name, max(strlen(fpath), strlen(cinode.dir_items[i].item_name)))) {
                    // if we have found that
                    id = cinode.dir_items[i].item_inode_id;//get id and change current inode
                    cinode = myfs->inode[id];
                    break;
                }
                //after traversing through all possible entries, we still can not find
                if (i == DIR_SIZE - 1) {
                    return -1;
                }
            }
            fpath = strtok(NULL, "/");
        }
        return id;
    }
}

int get_parent_inode(char* path) {
    char p_path[MAX_NAME_SIZE * MAX_DIR_DEPTH] = "";
    char pname[MAX_NAME_SIZE] = "";
    strcpy(p_path, path);
    char* fpath = strtok(p_path, "/");
    strcpy(p_path, "");
    bool f_flag = false;
    while (fpath != NULL) {
        if (f_flag) {
            strcat(p_path, "/");
            strcat(p_path, pname);
        }
        else {
            f_flag = true;
        }
        strcpy(pname, fpath);
        fpath = strtok(NULL, "/");
    }
    return get_inode(p_path);
}

void get_full_path_name(char* path, char* pname, char  p_path[280])
//combine path and name to current path name
{
    //path start from home
    if (pname[0] == '/') {
        strcpy(p_path, pname);
    }
    //path start from current directory
    else if (pname[0] == '.') {
        strcpy(p_path, path);
        strcat(p_path, "/");
        strcat(p_path, pname+2);
    }
    else {
        strcpy(p_path, path);
        strcat(p_path, "/");
        strcat(p_path, pname);
    }
}

std::tuple<std::string, std::string> split_pathname(char p_path[280])
{
    char* fpath = strtok(p_path, "/");//this function will change p_path too
    vector<string> tmp_v;
    while (fpath != NULL) {
        tmp_v.push_back("/");
        tmp_v.push_back(fpath);
        fpath = strtok(NULL, "/");
    }
    //get parent directory
    std::vector<string> parent_dir_v(tmp_v.begin(), tmp_v.end() - 1);
    std::vector<string> dir_name_v(tmp_v.end() - 1, tmp_v.end());
    std::string ret_parent_dir_s = accumulate(begin(parent_dir_v), end(parent_dir_v), std::string{});
    std::string ret_dir_name_s = accumulate(begin(dir_name_v), end(dir_name_v), std::string{});
    return { ret_parent_dir_s , ret_dir_name_s };
}



int createDir(char* path, char* pname) {//#Q why pass in pointer
    // nested-directory supported
    //TODO add condition if starting from root
    char p_path[MAX_NAME_SIZE * MAX_DIR_DEPTH] = "";

    get_full_path_name(path, pname, p_path);

    //check whether the directory exists or not
    int tmp_id = get_inode(p_path);
    if (tmp_id != -1 and myfs->inode[tmp_id].inode_type == DIR_TYPE) {
        cout << "Directory has exists!" << endl;
        return -1;
    }
    //store full path name to print
    char o_path[MAX_NAME_SIZE * MAX_DIR_DEPTH] = "";
    strcpy(o_path, p_path);

    std::tuple<std::string, std::string> tup = split_pathname(p_path);
    std::string parent_dir_s = get<0>(tup);
    std::string dir_s = get<1>(tup);

    //check whether parent directory has exist
    int inode_id = get_inode((char*)parent_dir_s.c_str());
    if (inode_id == -1) {
        cout << "parent path not found, creating..." << endl;
        //create parent path
        createDir((char*)' ', (char*)parent_dir_s.c_str());
        inode_id = get_inode((char*)parent_dir_s.c_str());
    }
    bool flag = false;
    //int inode_id;
    int free_inode_id;
    get_free_inode_num(free_inode_id);
    Initialize_dir_inode(free_inode_id);

    for (int j = 0; j < DIR_SIZE; j++) {//inode id is parent inode id
        if (strlen(myfs->inode[inode_id].dir_items[j].item_name) == 0) {
            //update parent inode with the new info
            myfs->inode[inode_id].dir_items[j].item_inode_id = free_inode_id;//update location of the file
            myfs->inode[inode_id].dir_size++;// update directory size
            strcpy(myfs->inode[inode_id].dir_items[j].item_name, (char*)dir_s.c_str());
            flag = true;
            break;
        }
    }
    cout << "Directory " << o_path << " has been created successfully!" << endl;
    return free_inode_id;
}



int changeDir(char* topath) {
    //change dir to topath, return 0 if success -1 otherwise.
    if (!strcmp(topath, "..")) {
        char path[MAX_NAME_SIZE * MAX_DIR_DEPTH] = "";
        int len;
        if (!strcmp(PATH, "/root")) {
            cout << "you are already in the root directory" << endl;
            return -1;
        }

        for (int i = strlen(PATH); i >= 0; i--) {
            if (PATH[i] == '/') {
                len = i;
                break;
            }
        }
        strncpy(path, PATH, len);//find parent path
        strcpy(PATH, path);
    }
    else {
        char p_path[MAX_NAME_SIZE * MAX_DIR_DEPTH] = "";
        get_full_path_name(PATH, topath, p_path);
        if (get_inode(p_path) == -1 || get_inode(p_path) == 0) {
            cout << "Wrong directory!" << endl;
            return -1;
        }
        else {
            strcpy(PATH, p_path);
        }
    }
    return 0;
}

bool include_itself(char* dest_path, char* cur_path) {
    if (strstr(cur_path, dest_path) != NULL) {
        return true;
    }
    return false;
}




int createFile(char* path, char* filename, int size) {
    //security check
    int num_block = 0;//how many block will be used to place this file
    if (size < 0) {
        cout << "Invalid Size!" << endl;
        return -1;
    }
    else if (size <= BLOCK_SIZE / 2 + 10) {//the size exceeds maximum file size
        get_total_block_num(size, num_block);
        int free_block = 0;
        //make sure there are enough space
        update_rest_blocks();
        if (free_block > myfs->sb.rest_free_block_num) {
            cout << "No Enough Free Space!" << endl;
            return -1;
        }
    }
    else {
        cout << "Size is too large!" << endl;
        return -1;
    }

    char p_path[MAX_NAME_SIZE * MAX_DIR_DEPTH] = "";
    get_full_path_name(path, filename, p_path);
    int tmp_id = get_inode(p_path);
    if (tmp_id != -1) {
        cout << "File has exists!" << endl;
        return -1;
    }

    std::tuple<std::string, std::string> tup = split_pathname(p_path);
    std::string parent_dir_s = get<0>(tup);
    std::string dir_s = get<1>(tup);
    int inode_id = get_inode((char*)parent_dir_s.c_str());
    if (inode_id == -1) {
        cout << "Wrong Path!" << endl;
        return -1;
    }

    //store output path
    char o_path[MAX_NAME_SIZE * MAX_DIR_DEPTH] = "";
    strcpy(o_path, p_path);

    //initialize loval variables
    int inode_loc;
    srand((unsigned)time(NULL));
    int free_inode_num;

    //get free inode num to initialize file
    get_free_inode_num(free_inode_num);
    Initialize_file_inode(free_inode_num, size);
    memset(display_buffer, '\0', sizeof(display_buffer));

    //find free directory in this inode
    int free_dir_num;
    for (int i = 0; i < DIR_SIZE; i++) {
        if (strlen(myfs->inode[inode_id].dir_items[i].item_name) == 0) {
            free_dir_num = i;
            break;
        }
    }
    int used_block = 0;//use which block
    for (int c_free_block = 0; c_free_block < FREE_BLOCK_NUM; c_free_block++) {//k is a free block number
        if (!myfs->sb.block_bitmap[c_free_block]) {// block k is empty
            if (used_block == num_block) {
                break;
            }

            if (used_block < 10) {//max direct block is 10
                myfs->inode[free_inode_num].inode_addr[used_block] = c_free_block;
                myfs->sb.block_bitmap[c_free_block] = true;
                //fill value
                char fill_value[BLOCK_SIZE];
                for (int l = 0; l < BLOCK_SIZE; l++) {
                    fill_value[l] = rand() % 26 + 65;
                }
                memcpy(myfs->free_space[c_free_block], fill_value, BLOCK_SIZE);//k is which block on earth
                memcpy(display_buffer + (used_block * BLOCK_SIZE), fill_value, BLOCK_SIZE);//use_block is set in relation to inode

            }
            else if (used_block == 10) {//find a data block
                myfs->inode[free_inode_num].inode_indirect_addr = c_free_block;
                myfs->sb.block_bitmap[c_free_block] = true;
            }
            else {
                myfs->sb.block_bitmap[c_free_block] = true;
                char tmp[2];//2 bit indirect address
                unsigned short block_pos = c_free_block;
                memcpy(tmp, &block_pos, 2);
                //set beginning of indirect position of this block
                int blocknum = myfs->inode[free_inode_num].inode_indirect_addr;
                int offset = (used_block - 11)*2;
                myfs->free_space[blocknum][offset+0] = tmp[0];
                myfs->free_space[blocknum][offset+1] = tmp[1];
                //fill value
                char fill_value[BLOCK_SIZE];
                for (int l = 0; l < BLOCK_SIZE; l++) {
                    fill_value[l] = rand() % 26 + 65;
                }
                memcpy(myfs->free_space + (c_free_block), fill_value, BLOCK_SIZE);
                memcpy(display_buffer + (used_block - 1)*BLOCK_SIZE , fill_value, BLOCK_SIZE);
            }
            used_block++;
        }
    }
    //cout << display_buffer << endl;
    //update directory info
    myfs->inode[inode_id].dir_items[free_dir_num].item_inode_id = free_inode_num;
    myfs->inode[inode_id].dir_size++;
    strcpy(myfs->inode[inode_id].dir_items[free_dir_num].item_name, (char*) dir_s.c_str());

    cout << "Create " << (char*)dir_s.c_str() << " Successfully!" << endl;
    return free_inode_num;

}

void get_total_block_num(int size, int& num_block)
//calculate the actual number by getting the size of a file
{
    if (size <= 10) {
        num_block = size;
    }
    else {
        num_block = size + 1;
    }
}

void Initialize_file_inode(int free_inode_num, int size)
{
    myfs->inode[free_inode_num].inode_type = FILE_TYPE;
    myfs->inode[free_inode_num].file_size = size;
    myfs->inode[free_inode_num].create_time = time(NULL);
    myfs->inode[free_inode_num].dir_size = 0;
    myfs->inode[free_inode_num].inode_addr[0] = NULL;
    myfs->inode[free_inode_num].inode_num_link = 1;
}


int cputil_secu_check_s(int source_inode_id, bool& retflag)
{
    retflag = true;
    if (source_inode_id == -1) {
        cout << "File doesn't exist!" << endl;
        return -1;
    }
    retflag = false;
    return {};
}
 
int cputil_secu_check_d(int dest_inode_id, bool& retflag)
{
    retflag = true;
    if (dest_inode_id != -1) {
        cout << "File has exists!" << endl;
        return -1;
    }
    retflag = false;
    return {};
}

int deep_cp(char* path, char* pname_1, char* pname_2) {
    //from 1 to 2
    //get source file name and dest file name
    char p_path[MAX_NAME_SIZE * MAX_DIR_DEPTH] = "";
    get_full_path_name(path, pname_1, p_path);
    int source_inode_id = get_inode(p_path);
    bool retflag;
    int retval = cputil_secu_check_s(source_inode_id, retflag);
    if (retflag) return retval;

    char dst_path[MAX_NAME_SIZE * MAX_DIR_DEPTH] = "";
    get_full_path_name(path, pname_2, dst_path);
    int dest_inode_id = get_inode(dst_path);
    retval = cputil_secu_check_d(dest_inode_id, retflag);
    if (retflag) return retval;

    char o_path[MAX_NAME_SIZE * MAX_DIR_DEPTH] = "";
    strcpy(o_path, dst_path);

    char chytmp[MAX_NAME_SIZE * MAX_DIR_DEPTH] = "";
    memcpy(chytmp, dst_path, 280);


    //security check for dest path
    std::tuple<std::string, std::string> tup = split_pathname(dst_path);
    std::string parent_dir_s = get<0>(tup);
    std::string dir_s = get<1>(tup);

    int path_id = get_inode((char*)parent_dir_s.c_str());
    if (path_id == -1) {
        cout << "Wrong Path" << endl;
        return -1;
    }


    srand((unsigned)time(NULL));
    bool flag = false;
    int size = myfs->inode[source_inode_id].file_size;
    int num_block = 0;
    get_total_block_num(size, num_block);
    int dest_inode = createFile((char*)' ', chytmp, 0);
    for (int j = 0; j < DIR_SIZE; j++) {
        if (strlen(myfs->inode[path_id].dir_items[j].item_name) == 0) {
            int use_block = 0;
            for (int k = 0; k < FREE_BLOCK_NUM; k++) {
                if (!myfs->sb.block_bitmap[k]) {
                    if (use_block == num_block) {
                        break;
                    }
                    if (use_block < 10) {
                        myfs->inode[dest_inode].inode_addr[use_block] = k;
                        myfs->sb.block_bitmap[k] = true;
                        memcpy(myfs->free_space[k], myfs->free_space[myfs->inode[source_inode_id].inode_addr[use_block]], BLOCK_SIZE);
                        myfs->inode[dest_inode].file_size++;
                    }
                    else if (use_block == 10) {
                        //be concise about inode_size:
                        myfs->inode[dest_inode].inode_indirect_addr = k;
                        myfs->sb.block_bitmap[k] = true;
                    }
                    else {
                        myfs->sb.block_bitmap[k] = true;
                        char tmp[2];
                        unsigned short pos = k ;//consise position
                        memcpy(tmp, &pos, 2);

                        //allocate block k to dest's indirect block
                        unsigned short block_num = myfs->inode[dest_inode].inode_indirect_addr;
                        int offset = (use_block - 11)*2;
                        myfs->free_space[block_num][offset+0] = tmp[0];
                        myfs->free_space[block_num][offset+1] = tmp[1];

                        //get block number from source block
                        unsigned short s_block_num = myfs->inode[source_inode_id].inode_indirect_addr;
                        unsigned short block_addr;
                        char tp[2];
                        tp[0] = myfs->free_space[s_block_num][offset+0];
                        tp[1] = myfs->free_space[s_block_num][offset+1];
                        //copy data from source to current block
                        memcpy(&block_addr, tp, 2);
                        memcpy(myfs->free_space + k, myfs->free_space + block_addr, BLOCK_SIZE);
                        myfs->inode[dest_inode].file_size++;
                    }
                    use_block++;
                }
            }
            cout << endl;
            flag = true;
            break;
        }
    }

    if (flag) {
        cout << "Copy to " << o_path << " Successfully!" << endl;
        return dest_inode_id;
    }
    else {
        return -1;
    }

}

//soft cp
int shallow_cp(char* path, char* pname_1, char* pname_2) {
    // from 1 to 2
    // get source file name and dest file name
    // return destination inode number if success, -1 otherwise.
    char sor_path[MAX_NAME_SIZE * MAX_DIR_DEPTH] = "";
    get_full_path_name(path, pname_1, sor_path);
    int source_inode_num = get_inode(sor_path);
    bool retflag;
    int retval = cputil_secu_check_s(source_inode_num, retflag);
    if (retflag) return retval;

    char dst_path[MAX_NAME_SIZE * MAX_DIR_DEPTH] = "";
    get_full_path_name(path, pname_2, dst_path);
    int dest_inode_num = get_inode(dst_path);
    retval = cputil_secu_check_d(dest_inode_num, retflag);
    if (retflag) return retval;

    char o_path[MAX_NAME_SIZE * MAX_DIR_DEPTH] = "";
    strcpy(o_path, dst_path);

    char chytmp[MAX_NAME_SIZE * MAX_DIR_DEPTH] = "";
    strcpy(chytmp, dst_path);

    //after security checks, we can perform our task

    std::tuple<std::string, std::string> tup = split_pathname(sor_path);
    std::string sor_parent_dir_s = get<0>(tup);
    std::string sor_dir_s = get<1>(tup);

    tup = split_pathname(dst_path);
    std::string dst_parent_dir_s = get<0>(tup);
    std::string dst_dir_s = get<1>(tup);

    int dst_parent_inode_num = get_inode((char*) dst_parent_dir_s.c_str());
    if (dst_parent_inode_num == -1) {
        cout << "Wrong Path!" << endl;
        return -1;
    }
    for (int j = 0; j < DIR_SIZE; j++) {
        if (strlen(myfs->inode[dst_parent_inode_num].dir_items[j].item_name) == 0) {
            memcpy(myfs->inode[dst_parent_inode_num].dir_items[j].item_name, pname_2, 28);
            myfs->inode[dst_parent_inode_num].dir_items[j].item_inode_id = source_inode_num;
            myfs->inode[source_inode_num].inode_num_link++;
            break;
        }
    }
    cout << "Copy to " << o_path << " Successfully!" << endl;
    return dest_inode_num;

}

int cat(char* path, char* pname) {
    //display a file, return inode number if success, -1 otherwise.
    char p_path[MAX_NAME_SIZE * MAX_DIR_DEPTH] = "";
    get_full_path_name(path, pname, p_path);

    int inode_id = get_inode(p_path);
    if (inode_id == -1) {
        cout << "File doesn't exist!" << endl;
        return -1;
    }

    int size = myfs->inode[inode_id].file_size;
    memset(display_buffer, '\0', sizeof(display_buffer));
    for (int i = 0; i < size; i++) {
        if (i < 10) {
            memcpy(display_buffer + (i * BLOCK_SIZE), myfs->free_space[myfs->inode[inode_id].inode_addr[i]], BLOCK_SIZE);
        }
        else {
            //because it's size, one less bit than block num, need only offest 10
            int blocknum = myfs->inode[inode_id].inode_indirect_addr;
            int offset = (i - 10)*2;
            unsigned short block_addr;
            char tmp[2];
            tmp[0] = myfs->free_space[blocknum][offset+0];
            tmp[1] = myfs->free_space[blocknum][offset+1];
            memcpy(&block_addr, tmp, 2);
            cout << offset << endl;
            memcpy(display_buffer + (i * BLOCK_SIZE), myfs->free_space[block_addr], BLOCK_SIZE);
        }
    }
    cout << display_buffer << endl;
    return inode_id;
}

int deleteFile(char* path, char* pname) {
    // delete a file and return the inode num deleted if success, -1 otherwise.
    // nested-directory supported
    char p_path[MAX_NAME_SIZE * MAX_DIR_DEPTH] = "";
    get_full_path_name(path, pname, p_path);

    char o_path[MAX_NAME_SIZE * MAX_DIR_DEPTH] = "";
    strcpy(o_path, p_path);

    //security check
    int inode_id = get_inode(p_path);
    if (inode_id == -1) {
        cout << "File doesn't exist!" << endl;
        return -1;
    }

    if (myfs->inode[inode_id].inode_type == DIR_TYPE) {
        cout << "It is a directory" << endl;
        return -1;
    }

    else {
        //delete itself
        myfs->inode[inode_id].inode_num_link = 0;
        int size = myfs->inode[inode_id].file_size;
        myfs->inode[inode_id].file_size = 0;
        myfs->sb.free_inode_pointer = min(inode_id, myfs->sb.free_inode_pointer);
        if (size <= 10) {
            for (int i = 0; i < size; i++) {
                myfs->sb.block_bitmap[myfs->inode[inode_id].inode_addr[i]] = false;
                myfs->inode[inode_id].inode_addr[i] = NULL;
            }
        }
        else {
            for (int i = 0; i < 10; i++) {
                myfs->sb.block_bitmap[myfs->inode[inode_id].inode_addr[i]] = false;
                myfs->inode[inode_id].inode_addr[i] = NULL;
            }
            int indirect_addr = myfs->inode[inode_id].inode_indirect_addr;
            myfs->sb.block_bitmap[indirect_addr] = false;
            for (int i = 0; i < size - 10; i++) {
                unsigned short addr;
                int block_pos = indirect_addr;
                int offset = i*2;
                char tmp[2];
                tmp[0] = myfs->free_space[block_pos][offset+0];
                tmp[1] = myfs->free_space[block_pos][offset+1];
                memcpy(&addr, tmp, 2);
                myfs->sb.block_bitmap[addr] = false;
            }
        }
        //update parent info
        int parent_id = get_parent_inode(p_path);
        for (int i = 0; i < DIR_SIZE; i++) {
            if (myfs->inode[parent_id].dir_items[i].item_inode_id == inode_id) {
                strcpy(myfs->inode[parent_id].dir_items[i].item_name, "");
                myfs->inode[parent_id].dir_items[i].item_inode_id = 0;
                myfs->inode[parent_id].dir_size--;

                break;
            }
            if (i == DIR_SIZE - 1) {
                cout << "Can't find parent directory!" << endl;
                return -1;
            }
        }
        cout << "Delete file  " << o_path << " Successfully!" << endl;
        return inode_id;
    }

}

int deleteDir(char* path, char* pname) {
    // delete directory and return the inode number deleted if success, -1 otherwise.
    // nested-directory supported
    char p_path[MAX_NAME_SIZE * MAX_DIR_DEPTH] = "";
    get_full_path_name(path, pname, p_path);

    char o_path[MAX_NAME_SIZE * MAX_DIR_DEPTH] = "";
    strcpy(o_path, p_path);

    //security check
    int inode_id = get_inode(p_path);
    if (inode_id == -1) {
        cout << "Directory doesn't exist!" << endl;
        return -1;
    }
    // keep from removing itself
    if (include_itself(p_path, path)) {
        cout << "The directory constains current directory" << endl;
        return -1;
    }
    else if (myfs->inode[inode_id].inode_type == FILE_TYPE) {
        cout << "The directory you choose is not a directory" << endl;
        return -1;
    }
    else {
        //delete everything inside itself
        for (int i = 0; i < DIR_SIZE; i++) {
            int tmp_id = myfs->inode[inode_id].dir_items[i].item_inode_id;
            if (tmp_id) {
                //use tmp to make sure not affect p_path
                char tmp[MAX_NAME_SIZE * MAX_DIR_DEPTH] = "";
                strcpy(tmp, p_path);
                char * new_p_name = strcat(tmp, "/");
                new_p_name = strcat(new_p_name, myfs->inode[inode_id].dir_items[i].item_name);
                if (myfs->inode[tmp_id].inode_type == DIR_TYPE) {
                    deleteDir((char*) " ",new_p_name);
                }
                else {
                    deleteFile((char*)" ", new_p_name);
                }
            }
        }
        //delete itself
        myfs->inode[inode_id].inode_num_link = 0;
        myfs->sb.free_inode_pointer = min(inode_id, myfs->sb.free_inode_pointer);

        //update parent inode
        int parent_id = get_parent_inode(p_path);
        for (int i = 0; i < DIR_SIZE; i++) {
            if (myfs->inode[parent_id].dir_items[i].item_inode_id == inode_id) {//find the where the item is
                strcpy(myfs->inode[parent_id].dir_items[i].item_name, "");
                myfs->inode[parent_id].dir_items[i].item_inode_id = 0;
                myfs->inode[parent_id].dir_size--;
                break;
            }
            if (i == DIR_SIZE - 1) {
                cout << "Can't find parent directory!" << endl;
                return -1;
            }
        }
        cout << "Delete directory " << o_path << " Successfully!" << endl;
        return inode_id;
    }
}

int dir(char* path) {
    int inode_id = get_inode(path);
    tm* local;
    char buf[128] = { 0 };
    cout << setw(10) << "NAME" << setw(5) << "TYPE" << setw(6) << "SIZE" << setw(21) << "TIME" << endl;
    for (int i = 0; i < DIR_SIZE; i++) {
        if (strlen(myfs->inode[inode_id].dir_items[i].item_name) != 0) {//if there is something
            cout << setw(10) << myfs->inode[inode_id].dir_items[i].item_name;
            if (myfs->inode[myfs->inode[inode_id].dir_items[i].item_inode_id].inode_type == DIR_TYPE) {
                cout << setw(5) << "DIR" << setw(6) << "-";
            }
            else {
                cout << setw(5) << "FILE" << setw(6) << myfs->inode[myfs->inode[inode_id].dir_items[i].item_inode_id].file_size;
            }
            time_t create_time = myfs->inode[myfs->inode[inode_id].dir_items[i].item_inode_id].create_time;
            local = localtime(&create_time);
            strftime(buf, 64, "%Y-%m-%d %H:%M:%S", local);
            cout << setw(21) << buf;
            cout << endl;
        }
    }
    return 0;
}

void update_rest_blocks()
{
    int use_blocks = 0;
    for (int i = 0; i < FREE_BLOCK_NUM; i++) {
        if (!myfs->sb.block_bitmap[i])
            use_blocks++;
    }
    myfs->sb.rest_free_block_num = use_blocks;
}

void sum() {
    cout << "SUM BLOCKS: " << SYSTEM_SIZE / BLOCK_SIZE << endl;
    cout << "SUPER BLOCKS: " << 20 << endl;
    cout << "INODE BLOCKS: " << 1600 << endl;
    update_rest_blocks();
    cout << "USE BLOCKS / FREE BLOCKS: " << myfs->sb.rest_free_block_num << " / " << FREE_SIZE / BLOCK_SIZE << endl;
}

void fill_in_inode_buf() {
    //fill in the number of free_inode_buf_size from empty to full
    int count = 0;
    bool flag = false;
    int tmp = myfs->sb.free_inode_pointer;
    for (int i = tmp; i < INODE_NUM; i++) {
        if (myfs->inode[i].inode_num_link == 0) {
            myfs->sb.free_inode_buff[count] = i;
            count++;
        }
        if (count == FREE_INODE_BUF_SIZE) {
            cout << "the buffer is full now"<<endl;
            flag = true;
            myfs->sb.free_inode_pointer = i;//store where to search next time
            myfs->sb.free_inode_buff_num = FREE_INODE_BUF_SIZE;
            break;
        }
    }
    if (!flag) {
        cout << "there are not enough blocks, please release some resourcs first ";
    }
}

void get_free_inode_num(int& free_inode_num)
{   //search from pointer's loc
    for (int i = 0; i < sizeof(myfs->sb.free_inode_buff) / sizeof(myfs->sb.free_inode_buff[0])-1; i++) {
        if (myfs->sb.free_inode_buff[i] != -1) {
            free_inode_num = myfs->sb.free_inode_buff[i];
            myfs->sb.free_inode_buff[i] = -1;
            return;
        }
    }
    fill_in_inode_buf();
    get_free_inode_num(free_inode_num);
}

void initialize_file_system() {
    memset(&myfs->sb, '\0', 20 * BLOCK_SIZE);
    memset(&myfs->inode, '\0', 1600 * BLOCK_SIZE);

    myfs->inode[0].dir_items[0].item_inode_id = 1;
    myfs->inode[0].create_time = time(NULL);
    myfs->inode[0].dir_size = 1;
    strcpy(myfs->inode[0].dir_items[0].item_name, "root");
    myfs->inode[0].inode_type = DIR_TYPE;
    myfs->inode[0].inode_num_link = 1;
    myfs->inode[1].inode_type = DIR_TYPE;
    myfs->inode[1].inode_num_link = 1;
    myfs->inode[1].create_time = time(NULL);
    myfs->inode[1].dir_size = 0;
    myfs->sb.free_inode_pointer = 0;
    strcpy(PATH, "/root");
    fill_in_inode_buf();
}

void help()	 
{
    printf("dir - list current directory\n");
    printf("changeDir - changedirectory\n");
    printf("createDir [name] - create directory\n");
    printf("deleteDir [name]- delete sirectory\n");
    printf("createFile [name] [size] - create a file and fill with size\n");
    printf("cat [name]- display a file\n");
    printf("deleteFile [name]- delete a file\n");
    printf("s_cp [from] [to] - shallow copy\n");
    printf("d_cp [from] [to] - deep copy\n");
    printf("sum - count the usage of inode\n");
    printf("help - show help list\n");
    printf("exit - exit the file system, store data\n");
    return;
}

int cmd(FILE*& fw)
{
    while (1) {
        cout << PATH << ">";
        char str[300];
        cin.getline(str, 300, '\n');
        char p1[100];
        char p2[100];

        char buf[100000];	//最大100K
        int tmp = 0;
        int i;
        sscanf(str, "%s", p1);
        if (strcmp(p1, "dir") == 0) {
            dir(PATH);	//显示当前目录
        }
        else if (strcmp(p1, "changeDir") == 0) {
            sscanf(str, "%s%s", p1, p2);
            changeDir(p2);
        }
        else if (strcmp(p1, "createDir") == 0) {
            sscanf(str, "%s%s", p1, p2);
            createDir(PATH, p2);
        }
        else if (strcmp(p1, "cat") == 0) {
            sscanf(str, "%s%s", p1, p2);
            cat(PATH, p2);
        }
        else if (strcmp(p1, "deleteDir") == 0) {
            sscanf(str, "%s%s", p1, p2);
            deleteDir(PATH, p2);
        }
        else if (strcmp(p1, "sum") == 0) {
            sum();
        }
        else if (strcmp(p1, "deleteFile") == 0) {
            sscanf(str, "%s%s", p1, p2);
            deleteFile(PATH, p2);
        }
        else if (strcmp(p1, "createFile") == 0) {
            char p3[100];
            sscanf(str, "%s %s %d", p1, p2, p3);
            if (strlen(p2) == 0 || strlen(p3) == 0) {
                cout << "the argument is missing";
            }
            else
            {
                createFile(PATH, p2, atoi(p3));
            }
        }
        else if (strcmp(p1, "s_cp") == 0) {
            char p3[100];
            sscanf(str, "%s%s%s", p1, p2, p3);
            shallow_cp(PATH, p2, p3);
        }
        else if (strcmp(p1, "d_cp") == 0) {
            char p3[100];
            sscanf(str, "%s%s%s", p1, p2, p3);
            deep_cp(PATH, p2, p3);
        }
        else if (strcmp(p1, "help") == 0) {
            help();
        }
        else if (strcmp(p1, "exit") == 0) {
            // save file system
            fw = fopen(SYSTEM_FILE_NAME, "wb");
            fwrite(&(myfs->sb), sizeof(myfs->sb), 1, fw);
            fseek(fw, 20 * BLOCK_SIZE, 0);
            fwrite(&(myfs->inode), sizeof(myfs->inode), 1, fw);
            fseek(fw, INODE_NUM * BLOCK_SIZE, 0);
            fwrite(&(myfs->free_space), sizeof(myfs->free_space), 1, fw);//we may not have to read 
            fclose(fw);
            return 0;
        }
        else {
            cout<<"wrong command"<<endl;
        }
    } 
    return 0;
}

void load_file_system()
{
    FILE* fr;
    if ((fr = fopen(SYSTEM_FILE_NAME, "rb")) == NULL) {
        cout << "initializing..." << endl;
        initialize_file_system();
    }
    else {
        cout << "loading..." << endl;
        fr = fopen(SYSTEM_FILE_NAME, "rb");
        fread(&(myfs->sb), sizeof(myfs->sb), 1, fr);
        fseek(fr, 20 * 1024, 0);
        fread(&(myfs->inode), sizeof(myfs->inode), 1, fr);
        fseek(fr, INODE_NUM * 1024, 0);
        fread(&(myfs->free_space), sizeof(myfs->free_space), 1, fr);
        fclose(fr);
        strcpy(PATH, "/root");
    }
}

int main() {

    cout << "Welcome to Unix-like File System  " << endl;
    cout << "Copyright: 2021, South China University of Technolohgy" << endl;
    cout << "The author Huiyang Chi has asserted her moral rights." << endl;
    cout << endl;

    FILE* fw;
    load_file_system();
    return cmd(fw);

}
