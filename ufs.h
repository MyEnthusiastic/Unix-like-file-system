
#pragma once
//Basic file system layer
	//disk management
void initialize_file_system();
void load_file_system();

//File organization module layer
	//block management
void update_rest_blocks();
void get_total_block_num(int size, int& num_block);


	//inode management
void fill_in_inode_buf();
void get_free_inode_num(int& free_inode_num);

void Initialize_file_inode(int free_inode_num, int size);
void Initialize_dir_inode(int i);

int get_inode(char* path);
int get_parent_inode(char* path);


// Logic file system module
//function APIs
	//path management
bool include_itself(char* dest_path, char* cur_path);
std::tuple<std::string, std::string> split_pathname(char  p_path[280]);
void get_full_path_name(char* path, char* pname, char p_path[280]);
 
	//change dir
int changeDir(char* topath);

	//cp functions
int cputil_secu_check_s(int source_inode_id, bool& retflag);
int cputil_secu_check_d(int dest_inode_id, bool& retflag);
int deep_cp(char* path, char* pname_1, char* pname_2);
int shallow_cp(char* path, char* pname_1, char* pname_2);
	//cat
int cat(char* path, char* pname);
	//delete
int deleteFile(char* path, char* pname);
int deleteDir(char* path, char* pname);
	//ls
int dir(char* path);
	//create
int createFile(char* path, char* filename, int size);
	//sum
void sum();

	//user control layer
int cmd(FILE*& fw);




