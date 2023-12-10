#ifndef PENNFAT_H
#define PENNFAT_H

#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>


struct superblock {
	int fd;
	int Block_size; //total amount of blocks of virtual disk
	//int16_t RdirIndex; // root directory block index
	int DBoffset; // data block start index
	int num_data_Blocks; // amount of data blocks
	int num_FATBlocks; // number of blocks for FAT
	int FAT_tablesize;
	int rdsize;
	
};
struct __attribute__((__packed__)) Root_directory_entry {
	char name[32];
    uint32_t size;
    uint16_t firstBlock;
    uint8_t type;
    uint8_t perm;
    time_t mtime;
	char remain[16];
	int cursor;

};

void mkfs(char *fs_name, int blocks_in_fat,int block_size_config);
void update_dir_entry(struct Root_directory_entry *entry, int delete);
void update_fat(int cur,int block);
void mount(char *fs_name);
void unmount();
void touch(char **file_name, int fsize);
void data_write(char *fn, char *data, int write_size);
void copy(char *source, char *dest1, bool source_os, bool dest_os);
void cat(char **cm, int filenum);
void ls();
void chmod1(char *filename, char *perm);
void mv(char *filename, char *name);
void print_status();
void rm(char *fn, int delete);
int contain_file(char *filename);
bool check_allzero(char *entry);
int find_available_block();


#endif 

