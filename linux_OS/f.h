#ifndef F_H
#define F_H
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

int f_open(const char *fname, int mode);
int f_read(int fd, int n, char *buf);
int f_unlink(const char *fname);
int f_write(int fd, const char *str, int n);
int f_close(int fd);
int f_ls(const char *filename);
int f_lseek(int fd, int offset, int whence);
int f_copy(char *source, char *dest1);
int f_unmount();
int f_size(int fd);
int f_cat(char **cmd, int filenum,int writefd);
int f_rm(char **file,int filenum);
int f_mv(char *source, char *dest);
int f_copy(char *source, char *dest1);
int f_touch(char **filename);
int fatfs_init(char *fatfs);
int f_chmod(char *filename, char *perm);
#define f_STDIN_FILENO -1
#define f_STDOUT_FILENO -2

#endif 