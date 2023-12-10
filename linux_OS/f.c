
#include "f.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include "parser.h"
#include <errno.h>
#include <time.h>
#include <sys/stat.h>

#define INPUT_SIZE 10000
struct superblock sb;
struct Root_directory_entry *rd;
int16_t *FAT_block;
bool check_allzero(char *entry)
{

    char compare[64]; // 64-byte char array
    // Initialize the entry array with zeros
    memset(compare, 0, sizeof(compare));
    if (memcmp(entry, compare, 64) == 0)
    {
        // printf("allzero\n");
        return true;
    }
    // printf("not zero\n");
    return false;
}
int find_available_block()
{
    // printf("function: find_available_block\n");
    for (int i = 2; i < sb.FAT_tablesize; i += 1)
    {
        if (FAT_block[i] == 0)
            return i;
    }
    return -1;
}

void update_fat(int cur, int block)
{
    lseek(sb.fd, cur * 2, SEEK_SET);
    write(sb.fd, &block, 2);
    lseek(sb.fd, block * 2, SEEK_SET);
    uint16_t data = 0xFFFF;
    write(sb.fd, &data, 2);
}
void update_dir_entry(struct Root_directory_entry *entry, int delete)
{
    // printf("update_dir\n");

    int root = 1;
    char *dir_entry = malloc(64);
    int dir_size = sb.Block_size / 64;
    do
    {
        lseek(sb.fd, sb.DBoffset + (root - 1) * sb.Block_size, SEEK_SET);
        read(sb.fd, dir_entry, 64);
        printf("ddf%d\n", dir_entry[0]);
        //printf("")
        int i = 0;
        while (i < dir_size)
        {
            if (memcmp(dir_entry, entry, 32) == 0)
            {

                lseek(sb.fd, -64, SEEK_CUR);
                if (delete == 1)
                {
                    // printf("delete\n");
                    uint16_t data = 0x0000;
                    for (int i = 0; i < 64; i += 1)
                    {
                        write(sb.fd, &data, 1);
                    }
                }
                else
                {
                    write(sb.fd, entry, 64);
                }

                break;
            }

            read(sb.fd, dir_entry, 64);
            i += 1;
        }
        root = FAT_block[root];

    } while (root != -1);

    free(dir_entry);
}

void create_newfile(char *fn)
{
    // print_hex(fn,sizeof(fn));
    // printf("rd size: %d\n", sb.rdsize);
    sb.rdsize += 1;
    int avai_block = 0;
    // printf("rd size: %d\n", sb.rdsize);
    // printf("table size: %d\n", sb.FAT_tablesize);
    int avai_rd = 0;
    for (int i = 0; i < (sb.rdsize); i += 1)
    {
        if (check_allzero(rd[i].name))
        {
            avai_rd = i;
            break;
        }
    }
    //->f6
    /// f1 f2 f3 f4
    // memcpy(&rd[sb.rdsize - 1].name, fn, 32);
    strcpy(rd[avai_rd].name, fn);
    // print_hex(rd[sb.rdsize - 1].name, sizeof(rd[sb.rdsize - 1].name));
    rd[avai_rd].firstBlock = avai_block;
    rd[avai_rd].perm = 0;
    rd[avai_rd].size = 0;
    time_t current_time = time(NULL);
    uint64_t time_value = current_time;
    memcpy(&rd[avai_rd].mtime, &time_value, 8);
    //printf("create_newfile: rd[%d]:%s with block %d\n", avai_rd, rd[avai_rd].name, rd[avai_rd].firstBlock);
    // update root_dir
    int cur = 1;
    //int next = FAT_block[cur];
    //printf("next : %d \n", next);

    int dir_size = sb.Block_size / 64;
    char *entry = malloc(64 * sizeof(char));
    //printf("DB: %d\n", sb.DBoffset + (cur - 1) * sb.Block_size);
    lseek(sb.fd, sb.DBoffset + (cur - 1) * sb.Block_size, SEEK_SET);

    int i = 0;
    while (1)
    {
        read(sb.fd, entry, 64);

        if (check_allzero(entry))
        {
            //printf("rnter\n");
            // off_t offset = lseek(sb.fd, 0, SEEK_CUR);
            // printf("find ava:%ld ", offset);
            lseek(sb.fd, -64, SEEK_CUR);
            write(sb.fd, &rd[avai_rd], 64);
            break;
        }
        i += 1;
        //printf("i= %d\n", i);
        // if dir_block is full
        
        if (i % dir_size == 0 && FAT_block[cur] != -1)
        {
            cur = FAT_block[cur];
            lseek(sb.fd, sb.DBoffset + (cur - 1) * sb.Block_size, SEEK_SET);
        }
        if (i == (sb.rdsize - 1) && i % dir_size == 0)
        {

            int last_dir_block = 0;

            for (int finf = 0; finf < sb.FAT_tablesize; finf += 1)
            {
                if (FAT_block[finf] == 0)
                {
                    last_dir_block = finf;
                    break;
                }
            }
            //printf("new block: %d", last_dir_block);
            FAT_block[cur] = last_dir_block;
            FAT_block[last_dir_block] = 0xFFFF;
            update_fat(cur, last_dir_block);
            lseek(sb.fd, sb.DBoffset + (last_dir_block - 1) * sb.Block_size, SEEK_SET);
            write(sb.fd, &rd[sb.rdsize - 1], 64);
            break;
        }
    }
    // printf("avai_block: %d",avai_block);
    free(entry);
}


int contain_file(char *filename)
{
    int j = 0;
    int i = 0;
    while (j < sb.rdsize)
    {

        if (check_allzero(rd[i].name))
        {
            i += 1;
            continue;
        }
        // printf("compare %s, %s\n", rd[i].name, filename);
        if (strcmp(rd[i].name, filename) == 0)
            return i;
        j += 1;
        i += 1;
    }
    return -1;
}

void touch(char **file_name, int fsize)
{

    int i = 1;
    while (i < (fsize + 1))
    {

        int fileidx = contain_file(file_name[i]);
        // printf("contain file, %d\n", fileidx);
        if (fileidx != -1) /// check if the file already exist(check rd)
        {
            time_t current_time = time(NULL);
            uint64_t time_value = current_time;
            memcpy(&rd[fileidx].mtime, &time_value, 8);
            update_dir_entry(&rd[fileidx], 0);
        }
        else
        {
            create_newfile(file_name[i]);
        }
        i += 1;
    }
}
void rm(char *fn, int delete)
{
    int fi = contain_file(fn);
    if (fi == -1)
    {
        perror("no such file");
        return;
    }
    int cur = rd[fi].firstBlock;
    // printf("%d\n", cur);
    int next = FAT_block[cur];
    if (rd[fi].size == 0)
    {

        update_dir_entry(&rd[fi], delete);
        if (delete == 1)
        {
            memset(&rd[fi], 0, 64);

            sb.rdsize -= 1;
        }
        
    }else{

    do
    {
        lseek(sb.fd, sb.DBoffset + (cur - 1) * sb.Block_size, SEEK_SET);
        // printf("block offset: %x\n", sb.DBoffset + (cur - 1) * sb.Block_size);
        //  printf("ddd\n");
        uint16_t data = 0x0000;
        // /// erase datablock
        for (int i = 0; i < sb.Block_size; i += 1)
        {
            write(sb.fd, &data, 1);
        }
        // ftruncate(sb.fd, sb.Block_size);
        lseek(sb.fd, cur * 2, SEEK_SET);
        write(sb.fd, &data, 2);

        next = FAT_block[cur];
        FAT_block[cur] = 0;
        cur = next;

        // printf("%d",next);

    } while (next != -1);

    rd[fi].size = 0;

    if (delete == 0) // rewrite
    {
        // uint16_t data1 = 0xFFFF;
        // lseek(sb.fd, rd[fi].firstBlock * 2, SEEK_SET);
        // write(sb.fd, &data1, 2);
        // FAT_block[rd[fi].firstBlock] = 0xFFFF;
        rd[fi].size = 0;
        update_dir_entry(&rd[fi], delete);
    }
    else
    {
        update_dir_entry(&rd[fi], delete);
        memset(&rd[fi], 0, 64);
        // printf("entry.name = \"%s\"\n", rd[fi].name);
        // printf("rd[fi].size = %u\n", rd[fi].size);
        // printf("rd[fi].firstBlock = %u\n", rd[fi].firstBlock);
        // printf("rd[fi].type = %u\n", rd[fi].type);
        // printf("rd[fi].perm = %u\n", rd[fi].perm);
        // printf("rd[fi].mtime = %lu\n", rd[fi].mtime);
        // printf("rd[fi].remain = \"%s\"\n", rd[fi].remain);
        sb.rdsize -= 1;
    }
    }
    cur = 1;
    int last = 1;
    while (1)
    {
        if (FAT_block[cur] != -1)
        {
            last = cur;
            cur = FAT_block[cur];
        }
        else
            break;
    }
    lseek(sb.fd, sb.DBoffset + (cur - 1) * sb.Block_size, SEEK_SET);
    int dir_size = sb.Block_size / 64;
    int i = 0;
    char entry[64];
    while (i < dir_size)
    {
        read(sb.fd, entry, 64);
        if (!check_allzero(entry))
            break;
        i += 1;
    }

    if (i == 4 && last != cur)
    {
        uint16_t data = 0xFFFF;
        lseek(sb.fd, last * 2, SEEK_SET);
        write(sb.fd, &data, 2);
        lseek(sb.fd, cur * 2, SEEK_SET);
        data = 0;
        write(sb.fd, &data, 2);
        FAT_block[last] = 0xFFFF;
        FAT_block[cur] = 0;
    }

}

void data_write(char *fn, char *data, int write_size) ////
{
    /**
     * 1 root dir->filename, firstblock
     * first block->data:first data, FAT_table[firstblock]=nextblock
     * use size to write.
     */
    // printf("data_write: %s", data);
    int fi = contain_file(fn);
    int avai_block = 0;

    // find last block
    if (rd[fi].size == 0)
    {
        for (int i = 0; i < sb.FAT_tablesize; i += 1)
        {

            // printf("FAT: %d\n", FAT_block[i]);
            if (FAT_block[i] == 0)
            {
                avai_block = i;
                break;
            }
        }

        /// FAT_block update
        FAT_block[avai_block] = 0xFFFF;
        /// update fat block in file
        lseek(sb.fd, 2 * (avai_block), SEEK_SET);
        uint16_t buf = 0xFFFF;
        write(sb.fd, &buf, 2);
        rd[fi].firstBlock = avai_block;
    }
    int cur = rd[fi].firstBlock;
    int next = FAT_block[rd[fi].firstBlock];
    while (next != -1)
    {
        cur = next;
        next = FAT_block[cur];
    }
    // detect cur block is full
    // hellohi +250  ->block
    /// appif
    ///
    if (rd[fi].size != 0 && rd[fi].size % sb.Block_size == 0)
    {

        int new = find_available_block();
        FAT_block[cur] = new;
        FAT_block[new] = 0xFFFF;
        update_fat(cur, new);
        // printf("full, original size:%d, then old cur %d become new cur%d\n", rd[fi].size, cur, new);
        cur = new;
    }
    /// cur is the last block

    int last_bsize = 0;

    last_bsize = sb.Block_size - rd[fi].size % sb.Block_size; //
    lseek(sb.fd, sb.DBoffset + (cur - 1) * sb.Block_size, SEEK_SET);
    lseek(sb.fd, rd[fi].size % sb.Block_size, SEEK_CUR);
    // printf("first:%d: %d\n", fi, rd[fi].size);
    int datasize = write_size;
    // printf("now writing block: %d, size can be use:%d, datasize:%d\n", cur, last_bsize, datasize);

    if (last_bsize > datasize)
    {
        printf("%d", datasize);
        printf("%s", data);
        write(sb.fd, data, datasize);
        // printf("%d",rd[fi].size);
        rd[fi].size += datasize;
        rd[fi].mtime = time(NULL);
        // printf("here debug: %s\n", rd[fi].name);
        update_dir_entry(&rd[fi], 0);
        // printf("keep debug: %s\n", rd[fi].name);
    }
    else
    {

        write(sb.fd, data, last_bsize);
        rd[fi].size += datasize;
        rd[fi].mtime = time(NULL);
        // printf("debug: %s\n", rd[fi].name);
        update_dir_entry(&rd[fi], 0);
        // printf("keep debug: %s\n", rd[fi].name);

        datasize -= last_bsize;
        char *ptr = data;
        while (datasize > 0)
        {
            int block = find_available_block();
            FAT_block[cur] = block;
            FAT_block[block] = 0xFFFF;
            update_fat(cur, block);
            cur = block;
            /// write to data
            lseek(sb.fd, sb.DBoffset + (block - 1) * sb.Block_size, SEEK_SET);
            ptr += last_bsize;
            printf("%s", ptr);
            if (datasize > sb.Block_size)
            {
                write(sb.fd, ptr, sb.Block_size);
                datasize -= sb.Block_size;
            }
            else
            {
                write(sb.fd, ptr, datasize);
                datasize = 0;
            }
        }
    }
}
void chmod1(char *filename, char *perm)
{
    int perm1 = atoi(perm);
    int fi = contain_file(filename);
    if (fi == -1)
    {
        perror("Source does not exist\n");
        return;
    }
    rd[fi].perm = perm1;
    update_dir_entry(&rd[fi], 0);
}

int f_cat(char **cmd, int filenum,int writefd)
{
    int opfd=1;

    if ((writefd)!=1)
        opfd=(writefd);
    else 
        opfd=f_STDOUT_FILENO;

    for (int i = 1; i < (filenum); i += 1)
    {

        // printf("file %s\n", cmd[i]);
        int find = contain_file(cmd[i]);
        if (find == -1)
        {
            perror("no such file");
            // exit(EXIT_FAILURE);
            return -1;
        }
        int origial_cursor=rd[find].cursor;
        rd[find].cursor=0;
        char *data=malloc(rd[find].size*sizeof(char));
        
        f_read(find,rd[find].size,data);
        printf("size %d\n",rd[find].size);
        //write(STDERR_FILENO, data, rd[find].size);
        f_write(opfd, data, rd[find].size);
        //write(STDERR_FILENO, "\n", 2);
        rd[find].cursor=origial_cursor;
        free(data);
    }
    p_exit();
    return 0;
}

int fatfs_init(char *fatfs)
{
    // int fd = open(fatfs, O_RDWR);
    int fs_fd = open(fatfs, O_RDWR);
    if (fs_fd == -1)
    {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    uint16_t metadata = 0x0000;
    read(fs_fd, &metadata, sizeof(metadata));
    int fat_block = metadata >> 8;
    int block_size = metadata & 255;
    // uint16_t *fat_temp = mmap(NULL, fat_block*block_size-4, PROT_READ | PROT_WRITE, MAP_SHARED, fs_fd, 0);
    // printf("%x\n",&fat_temp);
    printf(" fat block %x\n", fat_block);
    printf(" block_size %x\n", block_size);
    switch (block_size)
    {
    case 0:
        sb.Block_size = 256;
        break;
    case 1:
        sb.Block_size = 512;
        break;
    case 2:
        sb.Block_size = 1024;
    case 3:
        sb.Block_size = 2048;
        break;
    case 4:
        sb.Block_size = 4096;
        break;
    default:
        printf("Invalid block size configuration.\n");
        return -1;
    }
    sb.fd = fs_fd;
    sb.num_FATBlocks = fat_block;
    sb.num_data_Blocks = sb.Block_size * fat_block / 2 - 1;
    sb.DBoffset = fat_block * sb.Block_size;

    /// rdsize wrong!!
    sb.rdsize = 0;

    // read root directory data into my data structure
    // fc->NUM_FAT_BYTES + curr_block*(fc->BLOCK_SIZE)
    int fat_block_num = sb.Block_size * fat_block / 2;
    sb.FAT_tablesize = fat_block_num;
    // Allocate memory for array
    int fat_size = fat_block * sb.Block_size;
    FAT_block = (int16_t *)malloc(fat_size);
    lseek(fs_fd, 0, SEEK_SET);
    for (int j = 0; j < fat_block_num; j += 1)
    {
        read(fs_fd, &FAT_block[j], 2);
    }

    int next = 1;
    char entry[64]; ////64 byte

    int dir_size = sb.Block_size / 64;
    rd = malloc(fat_block_num * sizeof(struct Root_directory_entry));
    // printf("first filename: %s \n", rd[0].name);
    int i = 0;
    do
    {
        // printf("find next: %d\n", next);
        lseek(fs_fd, sb.DBoffset + (next - 1) * sb.Block_size, SEEK_SET);
        read(fs_fd, entry, 64);

        // for (int i=0;i<strlen(entry);i+=1)
        // printf("%s",entry);
        // printf("offset:%x\n", sb.DBoffset + (next - 1) * sb.Block_size);

        // printf("dirsize %d\n",dir_size);
        while (1)
        {
            i += 1;
            if (!check_allzero(entry))
            {
                sb.rdsize += 1;

                // for (int i = 0; i < 64; i++)
                // {
                //     printf("%02x ", (unsigned char)entry[i]);
                // }

                // printf("\n");
                memmove(&rd[i - 1], entry, 64);
            }

            // printf("filename: %s \n", rd[i - 1].name);
            if (i % dir_size == 0)
                break;
            read(fs_fd, entry, 64);
        }
        next = FAT_block[next];
        // printf("next:%d\n", next);

    } while (next != -1);
    return 0;
}

int f_open(const char *fname, int mode)
{
    char fname_copy[strlen(fname) + 1];
    strcpy(fname_copy, fname);
    printf("congtain file %s\n", fname_copy);
    int fd = contain_file(fname_copy);
    printf("congtain file %d\n", fd);
    // if (fd == -1)
    // {
    //     perror("no such file");
    //     return -1;
    // }
    int fmode = mode;
    if (fmode == 0) /// F_WRITE
    {

        char *to_touch[32];
        to_touch[0] = "touch";
        to_touch[1] = fname_copy;
        if (fd == -1)
        {
            touch(to_touch, 1);
            fd=contain_file(fname_copy);
        }
        else
        {
            // printf("here\n");
            rm(fname_copy, 1);
            // printf("after remove\n");
            touch(to_touch, 1);
        }
        chmod1(fname_copy, "6");
        rd[fd].cursor = 0;
    }
    else if (fmode == 1) // F_READ
    {
        if (fd == -1)
        {
            perror("F_READ: file does not exist");
            return -1;
        }
        chmod1(fname_copy, "4");
        rd[fd].cursor = 0;
    }
    else /// F_APPEND
    {
        char *to_touch[32];
        to_touch[0] = "touch";
        to_touch[1] = fname_copy;

        if (fd == -1)
            touch(to_touch, 1);

        chmod1(fname_copy, "6");
        rd[fd].cursor = rd[fd].size;
    }
    printf("fd is :%d\n", fd);
    return fd;
}
int f_read(int fd, int n, char *buf)
{
    // char *data=malloc(n*sizeof(char));
    if (fd==f_STDIN_FILENO)
    {
        int r = read(STDIN_FILENO, buf, n);
        return r;
    }
    // if (rd[fd].perm == 0 || rd[fd].perm == 2)
    // {
    //     perror("no permssion to read");
    //     return -1;
    // }
    int cur = rd[fd].firstBlock;
    int cur_cursor = rd[fd].cursor;
    while (cur_cursor > sb.Block_size)
    {
        cur_cursor -= sb.Block_size;
        cur = FAT_block[cur];
    }

    int r = 0;
    int sizeleft = n;
    do
    {

        if (FAT_block[cur] == -1)
        {
            int datasize = (sizeleft % sb.Block_size == 0) ? sb.Block_size : sizeleft;
            // char data[datasize];
            lseek(sb.fd, sb.DBoffset + (cur - 1) * sb.Block_size, SEEK_SET);
            int r1 = read(sb.fd, buf+r, datasize);
            r += r1;
            // printf("data of f1: %s: %d\n", data, datasize);
            // data_write(dest1, data, datasize);
        }
        else
        {
            //char data[sb.Block_size];
            lseek(sb.fd, sb.DBoffset + (cur - 1) * sb.Block_size, SEEK_SET);
            int r2 = read(sb.fd, buf+r, sb.Block_size);
            r += r2;
            // data_write(dest1, data, sb.Block_size);
            sizeleft -= sb.Block_size;
        }
        cur = FAT_block[cur];
        // printf("%d\n", cur);

    } while (cur != -1 && sizeleft > 0);

    rd[fd].cursor += n;
    return r;
}
#define F_SEEK_SET 0
#define F_SEEK_CUR 1
#define F_SEEK_END 2
int f_write(int fd, const char *str, int n){
    
    
    if (fd==f_STDOUT_FILENO)
    {
        int w=write(STDERR_FILENO,str,n);
        return w;
    }
    if (contain_file(rd[fd].name)!=-1) {
        // if(rd[fd].perm==4 ||rd[fd].perm==5||rd[fd].perm==0 ){
        //     perror("read only or not open\n");
        //     return -1;
        // }
        int avai_block = 0;
        
        // find last block
        if (rd[fd].size == 0)
        {
            for (int i = 0; i < sb.FAT_tablesize; i += 1)
            {

                //printf("FAT: %d\n", FAT_block[i]);
                if (FAT_block[i] == 0)
                {
                    avai_block = i;
                    break;
                }
            }

            /// FAT_block update
            FAT_block[avai_block] = 0xFFFF;
            /// update fat block in file
            lseek(sb.fd, 2 * (avai_block), SEEK_SET);
            uint16_t buf = 0xFFFF;
            write(sb.fd, &buf, 2);
            rd[fd].firstBlock = avai_block;
        }
        int last_bsize = 0;
        last_bsize = sb.Block_size-rd[fd].cursor % sb.Block_size; 
        int cur=rd[fd].firstBlock;
        int cur_cursor=rd[fd].cursor;
        printf("cursor %d\n",rd[fd].cursor);
        while(cur_cursor>sb.Block_size)
            {
                cur_cursor-=sb.Block_size;
                cur=FAT_block[cur];
            }
        lseek(sb.fd, sb.DBoffset + (cur - 1) * sb.Block_size, SEEK_SET);
        lseek(sb.fd,cur_cursor , SEEK_CUR);
        int datasize = n;

        if (last_bsize > datasize)
        {
            // printf("%d", datasize);
            // printf("%s", data);
            write(sb.fd, str, datasize);
            
            if(rd[fd].cursor+datasize>rd[fd].size){
                printf("size %d\n",rd[fd].size);
                rd[fd].size =rd[fd].cursor+datasize;
            }
            rd[fd].cursor+=datasize;
            rd[fd].mtime = time(NULL);
            update_dir_entry(&rd[fd], 0);
        }
        else
        {

            write(sb.fd, str, last_bsize);
            rd[fd].mtime = time(NULL);
            update_dir_entry(&rd[fd], 0);
            
            if(rd[fd].cursor+datasize>rd[fd].size){
                rd[fd].size =rd[fd].cursor+datasize;
            }
            rd[fd].cursor+=datasize;
            printf("check fwrite, cursor: %d, datasize:%d\n",rd[fd].cursor,datasize);
            datasize -= last_bsize;
            const char *ptr = str;
            while (datasize > 0)
            {
                int block = find_available_block();
                FAT_block[cur] = block;
                FAT_block[block] = 0xFFFF;
                update_fat(cur, block);
                cur = block;
                /// write to data
                lseek(sb.fd, sb.DBoffset + (block - 1) * sb.Block_size, SEEK_SET);
                ptr += last_bsize;
                printf("%s\n", ptr);
                if (datasize > sb.Block_size)
                {
                    write(sb.fd, ptr, sb.Block_size);
                    datasize -= sb.Block_size;
                }
                else
                {
                    write(sb.fd, ptr, datasize);
                    datasize = 0;
                }
            }
        }
    } else {
        perror("file does not exist\n");
        return -1;
    }
    printf("rd[fd].cursor:%d,rd[fd].size:%d\n",rd[fd].cursor,rd[fd].size);
    return n;
}

int f_close(int fd)
{
    if (contain_file(rd[fd].name)!=-1) {
        f_unlink(rd[fd].name);
    } else {
        perror("can not close\n");
        return -1;
    }
    return 0;
}
int f_lseek(int fd, int offset, int whence){
    if(contain_file(rd[fd].name)==-1){
        perror("can not lseek\n");
        return -1;
    }
    if (whence == F_SEEK_SET)
    {
        rd[fd].cursor = offset;
    }
    else if (whence == F_SEEK_CUR)
    {
        rd[fd].cursor += offset;
    }
    else if (whence == F_SEEK_END)
    {
        rd[fd].cursor = rd[fd].size;
    }
    return 0;
}

int f_unlink(const char *fname)
{
    char fname_copy[strlen(fname) + 1];
    strcpy(fname_copy, fname);
    int fd=contain_file(fname_copy);
    if (fd != -1) {
        char fname_copy[strlen(fname) + 1];
        strcpy(fname_copy, fname);
        rm(fname_copy,1);
    }else{
        return -1;
    }
    return 0;
}

int f_ls(const char *filename){
    if (filename != NULL){
        char fname_copy[strlen(filename) + 1];
        strcpy(fname_copy, filename);
        int i=contain_file(fname_copy);
        printf("%d ", rd[i].firstBlock);
        struct tm *time = localtime(&(rd[i].mtime));
        int month = time->tm_mon + 1;
        int day = time->tm_mday;
        int hour = time->tm_hour;
        int minute = time->tm_min;
        char monthChar[4];
        if (month == 1)
        {
            strcpy(monthChar, "Jan");
        }
        else if (month == 2)
        {
            strcpy(monthChar, "Feb");
        }
        else if (month == 3)
        {
            strcpy(monthChar, "Mar");
        }
        else if (month == 4)
        {
            strcpy(monthChar, "Apr");
        }
        else if (month == 5)
        {
            strcpy(monthChar, "May");
        }
        else if (month == 6)
        {
            strcpy(monthChar, "Jun");
        }
        else if (month == 7)
        {
            strcpy(monthChar, "Jul");
        }
        else if (month == 8)
        {
            strcpy(monthChar, "Aug");
        }
        else if (month == 9)
        {
            strcpy(monthChar, "Sep");
        }
        else if (month == 10)
        {
            strcpy(monthChar, "Oct");
        }
        else if (month == 11)
        {
            strcpy(monthChar, "Nov");
        }
        else if (month == 12)
        {
            strcpy(monthChar, "Dec");
        }
        if (rd[i].perm == 0)
        {
            printf("%s ", "---");
        }
        else if (rd[i].perm == 2)
        {
            printf("%s ", "--w");
        }
        else if (rd[i].perm == 4)
        {
            printf("%s ", "--r");
        }
        else if (rd[i].perm == 5)
        {
            printf("%s ", "-xr");
        }
        else if (rd[i].perm == 6)
        {
            printf("%s ", "-rw");
        }
        else if (rd[i].perm == 7)
        {
            printf("%s ", "rwx");
        }
        printf("%d ", rd[i].size);
        printf("%s ", monthChar);
        printf("%02d ", day);
        printf("%02d:%02d ", hour, minute);
        printf("%s\n", rd[i].name);
    }
    else{
        int i = 0;
        int j = 0;
        while (j < sb.rdsize)
        {
            if (check_allzero(rd[i].name))
            {
                i += 1;
                continue;
            }
            else
                j += 1;

            printf("%d ", rd[i].firstBlock);
            struct tm *time = localtime(&(rd[i].mtime));

            int month = time->tm_mon + 1;
            int day = time->tm_mday;
            int hour = time->tm_hour;
            int minute = time->tm_min;
            char monthChar[4];
            if (month == 1)
            {
                strcpy(monthChar, "Jan");
            }
            else if (month == 2)
            {
                strcpy(monthChar, "Feb");
            }
            else if (month == 3)
            {
                strcpy(monthChar, "Mar");
            }
            else if (month == 4)
            {
                strcpy(monthChar, "Apr");
            }
            else if (month == 5)
            {
                strcpy(monthChar, "May");
            }
            else if (month == 6)
            {
                strcpy(monthChar, "Jun");
            }
            else if (month == 7)
            {
                strcpy(monthChar, "Jul");
            }
            else if (month == 8)
            {
                strcpy(monthChar, "Aug");
            }
            else if (month == 9)
            {
                strcpy(monthChar, "Sep");
            }
            else if (month == 10)
            {
                strcpy(monthChar, "Oct");
            }
            else if (month == 11)
            {
                strcpy(monthChar, "Nov");
            }
            else if (month == 12)
            {
                strcpy(monthChar, "Dec");
            }
            if (rd[i].perm == 0)
            {
                printf("%s ", "---");
            }
            else if (rd[i].perm == 2)
            {
                printf("%s ", "--w");
            }
            else if (rd[i].perm == 4)
            {
                printf("%s ", "--r");
            }
            else if (rd[i].perm == 5)
            {
                printf("%s ", "-xr");
            }
            else if (rd[i].perm == 6)
            {
                printf("%s ", "-rw");
            }
            else if (rd[i].perm == 7)
            {
                printf("%s ", "rwx");
            }
            printf("%d ", rd[i].size);
            printf("%s ", monthChar);
            printf("%02d ", day);
            printf("%02d:%02d ", hour, minute);
            printf("%s\n", rd[i].name);
            i += 1;
        }
    }
    p_exit();
    return 0;
}


int f_rm(char **file,int filenum){
    
    // printf("rm size: %d\n",filenum);
    // for (int i=0;i<filenum;i+=1)
    //     printf("rm : %s\n",file[i]);
    for (int i=1;i<filenum;i+=1)
        f_unlink(file[i]);
    p_exit();
    return 0;
}
int f_rm1(char **file,int filenum){
    
    // printf("rm size: %d\n",filenum);
    // for (int i=0;i<filenum;i+=1)
    //     printf("rm : %s\n",file[i]);
    for (int i=1;i<filenum;i+=1)
        f_unlink(file[i]);
    return 0;
}
int f_mv(char *source, char *dest){
    printf("soucre: %s\n",source);
    printf("dest: %s\n",dest);
    int fi = contain_file(source);
    if (fi == -1)
    {
        perror("Source does not exist\n");
        return -1;
    }
    f_copy1(source, dest);
    char *to_rm[32];
    to_rm[0] = "rm";
    to_rm[1] = source;
    f_rm(to_rm,2);
    p_exit();
    return 0;
}

int f_copy(char *source, char *dest1)
{
    printf("soucre: %s\n",source);
    printf("dest: %s\n",dest1);
    int fi = contain_file(source);
    if (fi == -1)
    {
        perror("Source does not exist\n");
        p_exit();
        return -1;
    }
    //int cur = rd[fi].firstBlock;
    //printf("first block of f1: %s, %d\n", rd[fi].name, cur);
    int dest = contain_file(dest1);
    char *fn = dest1;
    // printf("file %s\n", fn);
    char *to_touch[32];
    to_touch[0] = "touch";
    to_touch[1] = fn;
    //printf("ddd: \n");
    //printf("dest: %d\n",dest);
    if (dest == -1)
    {
        f_touch1(to_touch);
        dest=contain_file(dest1);
    }
    else
    {   printf("%d\n",rd[dest].size);
        if (rd[dest].size != 0)
            {   
                char *to_rm[32];
                to_rm[0] = "rm";
                to_rm[1] = dest1;
                f_rm1(to_rm,2); 
                printf("finished rm\n");
                f_touch1(to_touch);
            }
    }
    printf("ddd: \n");
    int sizeleft = rd[fi].size;
    printf("dad %d\n",sizeleft);
    f_lseek(fi,0,0);
    f_lseek(dest,0,0);
    char data[sizeleft];
    f_read(fi,sizeleft,data);
    f_write(dest,data,sizeleft);
    p_exit();
    return 0;
}

int f_copy1(char *source, char *dest1)
{
    printf("soucre: %s\n",source);
    printf("dest: %s\n",dest1);
    int fi = contain_file(source);
    if (fi == -1)
    {
        perror("Source does not exist\n");
        p_exit();
        return -1;
    }
    //int cur = rd[fi].firstBlock;
    //printf("first block of f1: %s, %d\n", rd[fi].name, cur);
    int dest = contain_file(dest1);
    char *fn = dest1;
    // printf("file %s\n", fn);
    char *to_touch[32];
    to_touch[0] = "touch";
    to_touch[1] = fn;
    //printf("ddd: \n");
    //printf("dest: %d\n",dest);
    if (dest == -1)
    {
        f_touch1(to_touch);
        dest=contain_file(dest1);
    }
    else
    {   printf("%d\n",rd[dest].size);
        if (rd[dest].size != 0)
            {   
                char *to_rm[32];
                to_rm[0] = "rm";
                to_rm[1] = dest1;
                f_rm1(to_rm,2); 
                // printf("finished rm\n");
                f_touch1(to_touch);
            }
    }
    printf("ddd: \n");
    int sizeleft = rd[fi].size;
    printf("dad %d\n",sizeleft);
    f_lseek(fi,0,0);
    f_lseek(dest,0,0);
    char data[sizeleft];
    f_read(fi,sizeleft,data);
    f_write(dest,data,sizeleft);
    return 0;
}
int f_touch(char* file_name[]){
    int i = 1;
    
    // while (file_name[i]!=NULL)
    //     {
    //         printf("f[%d]\n",i);
    //         printf("%s\n",file_name[i]);
    //         i+=1;

    //     }
    
    int fsize=0;
    printf("size: %d\n",fsize);
    while(file_name[fsize]!=NULL)
        fsize+=1;
    while (i < (fsize))
    {
        int fileidx = contain_file(file_name[i]);
        printf("contain file, %d\n", fileidx);
        if (fileidx != -1) /// check if the file already exist(check rd)
        {
            time_t current_time = time(NULL);
            uint64_t time_value = current_time;
            memcpy(&rd[fileidx].mtime, &time_value, 8);
            update_dir_entry(&rd[fileidx], 0);
        }  
        else
        {
            create_newfile(file_name[i]);
        }
        i += 1;
    }
    p_exit();
    return 0;

}
int f_touch1(char* file_name[]){
    int i = 1;
    
    // while (file_name[i]!=NULL)
    //     {
    //         printf("f[%d]\n",i);
    //         printf("%s\n",file_name[i]);
    //         i+=1;

    //     }
    
    int fsize=0;
    printf("size: %d\n",fsize);
    while(file_name[fsize]!=NULL)
        fsize+=1;
    while (i < (fsize))
    {
        int fileidx = contain_file(file_name[i]);
        printf("contain file, %d\n", fileidx);
        if (fileidx != -1) /// check if the file already exist(check rd)
        {
            time_t current_time = time(NULL);
            uint64_t time_value = current_time;
            memcpy(&rd[fileidx].mtime, &time_value, 8);
            update_dir_entry(&rd[fileidx], 0);
        }  
        else
        {
            create_newfile(file_name[i]);
        }
        i += 1;
    }
    return 0;

}
int f_unmount()
{
    close(sb.fd);
    free(rd);
    free(FAT_block);
    return 0;
}
int f_chmod(char *filename, char *perm){
    int perm1 = atoi(perm);
    int fi = contain_file(filename);
    if (fi == -1)
    {
        perror("Source does not exist\n");
        return -3;
    }
    rd[fi].perm = perm1;
    update_dir_entry(&rd[fi], 0);
    p_exit();
    return 0;
}
// /// start main function!!!!!!! //////////////////////
// int c = 0;
// int pid;
// int pid1;
// int jobid = 0;
// char *stringcpy(char *destination, const char *source)
// {
//     // return if no memory is allocated to the destination
//     if (destination == NULL)
//     {
//         return NULL;
//     }

//     // take a pointer pointing to the beginning of the destination string
//     char *ptr = destination;

//     // copy the C-string pointed by source into the array
//     // pointed by destination
//     while (*source != '\0' && *source != '&')
//     {
//         *destination = *source;
//         destination++;
//         source++;
//     }

//     // include the terminating null character
//     *destination = '\0';

//     // the destination is returned by standard `strcpy()`
//     return ptr;
// }
// struct bgNode
// {
//     int self_pid;
//     int self_pgid;
//     int jid;
//     int state;
//     int groupsize;
//     char *cm;
//     struct bgNode *next;
// };
// struct bgNode *currentjob;
// struct jobQueue
// {
//     struct bgNode *front, *rear;
//     int q_size;
// };
// struct bgNode *creatNode(int p, int pg, char *c, int a, int state)
// {
//     struct bgNode *temp = (struct bgNode *)malloc(sizeof(struct bgNode));

//     temp->self_pid = p;
//     temp->self_pgid = pg;
//     temp->jid = jobid += 1;
//     temp->cm = (char *)malloc(sizeof(char) * (strlen(c) + 1));
//     temp->groupsize = a;
//     temp->state = state;
//     stringcpy(temp->cm, c);

//     temp->next = NULL;

//     return temp;
// }
// struct jobQueue *createQueue()
// {
//     struct jobQueue *q = (struct jobQueue *)malloc(sizeof(struct jobQueue));
//     // fprintf(stderr,"malloc queue\n");
//     q->front = q->rear = NULL;
//     q->q_size = 0;
//     return q;
// }
// void enQueue(struct jobQueue *q, int p, int pg, char *c, int a, int state)
// {
//     // Create a new LL node
//     if (state == 0)
//     {

//         fprintf(stderr, "Running:");

//         fprintf(stderr, " pid: %s\n", c);
//         //     int a =0;
//         // while(c[a]!=NULL)
//         // {fprintf(stderr," %s",c[a]); a+=1;}
//         // fprintf(stderr,"\n");
//         //
//     }
//     struct bgNode *temp = creatNode(p, pg, c, a, state);

//     // fprintf(stderr,"here\n");
//     //  If queue is empty, then new node is front and rear
//     //  both
//     q->q_size += 1;
//     if (q->rear == NULL)
//     {
//         q->front = q->rear = temp;
//         return;
//     }
//     // Add the new node at the end of queue and change rear
//     q->rear->next = temp;
//     q->rear = temp;
//     // fprintf(stderr,"enqu\n");
// }
// void deQueue(struct jobQueue *q)
// {
//     // If queue is empty, return NULL.
//     if (q->front == NULL)
//         return;

//     // Store previous front and move front one node ahead
//     struct bgNode *temp = q->front;
//     q->q_size -= 1;
//     if (temp->next != NULL)
//     {
//         q->front = q->front->next;
//     }
//     else
//     {
//         // fprintf(stderr,"ff\n");
//         q->front = NULL;
//         q->rear = NULL;
//     }
//     free(temp->cm);
//     // //fprintf(stderr,"free node->cm\n");
//     free(temp);
//     // fprintf(stderr,"free node\n");
//     //  If front becomes NULL, then change rear also as NULL
// }
// void removeQueue(struct jobQueue *q, int pg)
// {
//     // If queue is empty, return NULL.

//     if (q->front == NULL)
//         return;

//     // Store previous front and move front one node ahead

//     struct bgNode *parent = q->front;
//     // fprintf(stderr,"parent %d\n",parent->jid);
//     if (parent->self_pgid == pg)
//     {
//         deQueue(q);
//         // fprintf(stderr,"here\n");
//         return;
//     }
//     if (parent->next == NULL)
//         return;
//     struct bgNode *temp = parent->next;
//     while (temp != NULL)
//     {
//         // fprintf(stderr,"next %d\n",temp->jid);
//         if (temp->self_pgid == pg)
//         {
//             break;
//         }

//         parent = temp;
//         temp = temp->next;
//     }
//     // fprintf(stderr,"final %d\n",temp->jid);
//     parent->next = temp->next;
//     if (temp == q->rear)
//     {
//         q->rear = parent;
//     }
//     q->q_size -= 1;
//     // If front becomes NULL, then change rear also as NULL
//     if (q->front == NULL)
//         q->rear = NULL;

//     free(temp->cm);
//     // fprintf(stderr,"free remove\n");
//     free(temp);
//     // fprintf(stderr,"free remove\n");
// }
// void freeq(struct jobQueue *q)
// {
//     if (q->front == NULL)
//     {
//         free(q);
//         // fprintf(stderr,"free q\n");
//         return;
//     }

//     // Store previous front and move front one node ahead
//     while (q->front != NULL)
//     {
//         deQueue(q);
//     }
//     free(q);
//     // fprintf(stderr,"free q\n");
// }
// char *stringcpy(char *destination, const char *source);
// void sig_handler(int signo);
// void INThandler(int signo)
// {
//     if (signo == SIGINT)
//     {
//         c = 1;

//         write(STDERR_FILENO, "\n", 2);
//         write(STDERR_FILENO, PROMPT, strlen(PROMPT) + 1);
//         signal(SIGINT, INThandler);
//     }
// }
// void sigtstp_handler(int sig)
// {
//     // pid_t pid = fgpid(jobs);
//     // //check for valid pid
//     // if (pid != 0) {
//     //     kill(-pid, sig);
//     // }

//     write(STDERR_FILENO, "\n", 2);
//     write(STDERR_FILENO, PROMPT, strlen(PROMPT) + 1);
//     signal(SIGTSTP, sigtstp_handler);
// }

// int back;
// int check_finished(struct jobQueue *q, int asyn)
// {
//     // fprintf(stderr,"qsize: %d\n",q->q_size);
//     if (q->q_size == 0)
//         return 0;

//     int status;
//     struct bgNode *n = q->front;
//     // struct bgNode * temp;
//     int wpid;
//     // start
//     // fprintf(stderr,"checkfinished\n");
//     int s = -1;
//     while (n != NULL)
//     {
//         fprintf(stderr, "exiy\n");
//         wpid = waitpid(-(n->self_pgid), &status, WNOHANG | WUNTRACED);
//         if (wpid < 0)
//         {

//             int p = n->self_pgid;
//             fprintf(stderr, "Finished:");
//             fprintf(stderr, " %s", n->cm);

//             removeQueue(q, p);
//         }
//         while (wpid > 0)
//         {
//             if (WIFEXITED(status))
//             { // fprintf(stderr,"exiy\n");
//                 s = 0;
//                 n->groupsize -= 1;
//             }
//             else if (WIFSIGNALED(status))
//             {
//             }
//             else if (WIFSTOPPED(status))
//             {
//                 n->state = 1;
//                 // fprintf(stderr,"current %s\n", currentjob->cm[0]);
//                 fprintf(stderr, "Stopped:");
//                 fprintf(stderr, " %s", n->cm);
//                 // fprintf(stderr, "\n");
//             }

//             else if (WIFCONTINUED(status))
//             {
//                 fprintf(stderr, "bg continued\n");
//             }

//             wpid = waitpid(-(n->self_pgid), &status, WNOHANG | WUNTRACED);
//         }

//         int p = n->self_pgid;
//         if (s == 0)
//         {
//             int goup = n->groupsize;
//             if (goup == 0)
//             {
//                 fprintf(stderr, "Finished:");
//                 fprintf(stderr, " %s", n->cm);
//                 // write(STDERR_FILENO, "\n", 2);
//                 // write(STDERR_FILENO, PROMPT, strlen(PROMPT) + 1);
//             }
//             // fprintf(stderr, "\n");}
//             n = n->next;
//             if (goup == 0)
//                 removeQueue(q, p);
//         }
//         else
//             n = n->next;
//     }

//     // fprintf(stderr,"queue size: %d\n",q->q_size);

//     return 1;
// }
// void check_redirection(struct parsed_command *cmd)
// {
//     if (cmd->stdin_file != NULL)
//     {
//         int new_stdin = open(cmd->stdin_file, O_RDONLY, 0644);
//         if (new_stdin < 0)
//         {
//             perror("Invalid standard input redirect: No such file or directory");
//             exit(EXIT_FAILURE);
//         }
//         int dup2Ret = dup2(new_stdin, STDIN_FILENO);
//         if (dup2Ret == -1)
//         {
//             perror("Error in redirecting the output.");
//             exit(EXIT_FAILURE);
//         }
//     }
//     if (cmd->stdout_file != NULL)
//     {
//         int new_stdout = open(cmd->stdout_file, O_WRONLY | O_TRUNC | O_CREAT, 0644);
//         if (new_stdout < 0)
//         {
//             perror("Invalid standard output redirect: No such file or directory");
//             exit(EXIT_FAILURE);
//         }
//         int dup2Ret1 = dup2(new_stdout, STDOUT_FILENO);
//         if (dup2Ret1 == -1)
//         {
//             perror("Error in redirecting the output.");
//             exit(EXIT_FAILURE);
//         }
//     }
// }
// struct bgNode *search_byjid(int j, struct jobQueue *q)
// {
//     if (q->q_size == 0)
//         return NULL;
//     struct bgNode *n = q->front;
//     while (n != NULL)
//     {
//         if (n->jid == j)
//             return n;
//         else
//         {
//             if (n->next != NULL)
//                 n = n->next;
//             else
//                 break;
//         }
//     }
//     return NULL;
// }
// void bg(char **comd, struct jobQueue *q)
// {
//     if (q->q_size == 0)
//         return;
//     struct bgNode *n;

//     if (comd[1] != NULL)
//     {
//         n = search_byjid(atoi(comd[1]), q);
//     }
//     else
//     {
//         // fprintf(stderr,"try %s\n",currentjob->cm);
//         n = currentjob;
//     }
//     if (n == NULL)
//     {
//         perror("no such job\n");
//         return;
//     }
//     if (n->state == 0)
//     {
//         perror("already running");
//         return;
//     }

//     // fprintf(stderr,"try\n");
//     n->state = 0;
//     kill(-n->self_pgid, SIGCONT);

//     fprintf(stderr, "Running:");

//     fprintf(stderr, " %s", n->cm);
//     // fprintf(stderr,"\n");
// }
// void fg(char **comd, struct jobQueue *q)
// {
//     if (q->q_size == 0)
//         return;
//     struct bgNode *n;

//     if (comd[1] != NULL)
//     {
//         n = search_byjid(atoi(comd[1]), q);
//     }
//     else
//     {
//         // fprintf(stderr,"try %s\n",currentjob->cm[1]);
//         n = currentjob;
//     }
//     if (n == NULL)
//     {
//         perror("no such job\n");
//         return;
//     }

//     int status;
//     int wpid;
//     int s = -1;
//     //

//     // fprintf(stderr,"\n");
//     signal(SIGTTOU, SIG_IGN);
//     tcsetpgrp(STDIN_FILENO, n->self_pgid);
//     if (n->state == 1)
//     {
//         kill(-n->self_pgid, SIGCONT);
//         fprintf(stderr, "Restarting:");
//         n->state = 0;
//     }
//     fprintf(stderr, " %s", n->cm);
//     wpid = waitpid(-(n->self_pgid), &status, WUNTRACED);
//     while (wpid > 0)
//     {
//         if (WIFEXITED(status))
//         {
//             fprintf(stderr, "finished\n");
//             s = 0;
//         }
//         else if (WIFSIGNALED(status))
//         {
//             s = 1;
//         }
//         else if (WIFSTOPPED(status))
//         {
//             s = 2;
//             n->state = 1;

//             signal(SIGTSTP, sigtstp_handler);
//             break;
//         }
//         else if (WIFCONTINUED(status))
//         {
//         }
//         wpid = waitpid(-(n->self_pgid), &status, WUNTRACED);
//         fprintf(stderr, "error : %d\n", errno);
//     }

//     tcsetpgrp(STDIN_FILENO, getpgid(0));
//     if (s == 0 || s == 1)
//     {
//         if (s == 0)
//         {
//             fprintf(stderr, "\nFinished:");
//             fprintf(stderr, " %s", n->cm);
//         }
//         removeQueue(q, n->self_pgid);
//     }
//     else if (s == 2)
//     {
//         fprintf(stderr, "\nStopped:");
//         fprintf(stderr, " %s", n->cm);
//         kill(-n->self_pgid, SIGTSTP);
//     }
//     // fprintf(stderr,"Sted:\n");
// }
// /// jobs no complete!!!!
// void jobs(struct jobQueue *q)
// {
//     if (q->q_size == 0)
//         return;
//     struct bgNode *n = q->front;

//     while (n != NULL)
//     {
//         fprintf(stderr, "[%d]", n->jid);
//         fprintf(stderr, " %s", n->cm);
//         char *state;
//         if (n->state == 0)
//             state = "running";
//         else
//             state = "stopped";
//         fprintf(stderr, " (%s)\n", state);
//         n = n->next;
//     }
// }
// void check_current(struct jobQueue *q)
// {

//     int fl = 0;
//     if (q->q_size == 0)
//         return;
//     // fprintf(stderr,"h\n");
//     struct bgNode *n = q->front;
//     // fprintf(stderr,"checkcurrent\n");
//     while (n != NULL)
//     {
//         if (n->state == 1)
//         {
//             currentjob = n;
//             fl = 1;
//             // fprintf(stderr, "current job %s\n",currentjob->cm[0]);
//         }
//         n = n->next;
//     }
//     if (fl == 0)
//     {
//         currentjob = q->rear;
//     }
//     // fprintf(stderr, "current job %s\n",currentjob->cm[0]);
// }

// struct jobQueue *gq;
// void sigchld_handler(int sig)
// {
//     // printf("change\n");
//     fprintf(stderr, "check\n");
//     check_finished(gq, 1);
// }
// int main(int argc, char *argv[])
// {

//     int status = 0;
//     struct jobQueue *q = createQueue();
//     gq = q;
//     int pgid;
//     char command[INPUT_SIZE];
//     char *b = command;
//     struct parsed_command *cmd;
//     struct sigaction sa;
//     sa.sa_handler = sigchld_handler;
//     sa.sa_flags = SA_RESTART;
//     sigemptyset(&sa.sa_mask);

//     if (argc > 1)
//     {
//         if (sigaction(SIGCHLD, &sa, NULL) == -1)
//         {
//             perror("sigaction");
//             exit(1);
//         }
//     }
//     // printf("argv %d\n",isatty(0));
//     //  if (argc==1)
//     //      printf("ee\n");
//     while (1)
//     {

//         // start excution
//         if (signal(SIGINT, INThandler) == SIG_ERR)
//         {
//             perror("Unable to catch SIGINT\n");
//         }
//         signal(SIGTSTP, sigtstp_handler);

//         for (int p = 0; p < INPUT_SIZE; p++)
//         {
//             command[p] = 0;
//         }
//         // fprintf(stderr,"whatis pid now: %d\n",pid);

//         // fprintf(stderr,"whatis pid now: %d\n",pid);
//         if (isatty(0) == 1)
//             write(STDOUT_FILENO, PROMPT, strlen(PROMPT) + 1);
//         size_t si = (size_t)INPUT_SIZE;

//         int r = getline(&b, &si, stdin);
//         if (argc == 1)
//             check_finished(q, 0);
//         // fprintf(stderr,"whatis q now: %d\n",q->q_size);
//         check_current(q);
//         if (r <= 1)
//         {
//             if (r == 1)
//                 continue;

//             break;
//         }
//         // start parse
//         int pa = parse_command(command, &cmd);
//         for (int i = 0; i < r; i++)
//         {
//             if (command[i] == '&')
//             {
//                 command[i] = ' ';
//             }
//         }
//         if (pa < 0)
//         {

//             perror("invalid");
//             continue;
//         }
//         if (pa > 0)
//         {

//             perror("invalid");
//             continue;
//         }
//         if (strcmp(cmd->commands[0][0], "bg") == 0)
//         {
//             bg(cmd->commands[0], q);
//             free(cmd);
//             continue;
//         }
//         if (strcmp(cmd->commands[0][0], "fg") == 0)
//         {
//             fg(cmd->commands[0], q);
//             free(cmd);
//             continue;
//         }
//         if (strcmp(cmd->commands[0][0], "jobs") == 0)
//         {
//             jobs(q);
//             free(cmd);
//             continue;
//         }

//         // if (strcmp(cmd->commands[0][0], "unmount") == 0)
//         // {
//         //     unmount();
//         //     free(cmd);
//         //     continue;
//         // }
//         //         if (strcmp(cmd->commands[0][0], "cat") == 0)
//         // {
//         //     int i = 1;
//         //     while (cmd->commands[0][i] != NULL)
//         //     {
//         //         if (cmd->commands[0][i][0] == '-')
//         //             break;
//         //         i += 1;
//         //     }
//         //     printf("size %d\n", i - 1);
//         //     // return 0;
//         //     cat(cmd->commands[0], i);
//         //     free(cmd);
//         //     continue;
//         // }
//         // if (strcmp(cmd->commands[0][0], "cp") == 0)
//         // {
//         //     if (strcmp(cmd->commands[0][1], "-h") == 0)
//         //     {
//         //         printf("D\n");
//         //         copy(cmd->commands[0][2], cmd->commands[0][3], true, false);
//         //         printf("finished\n");
//         //         free(cmd);
//         //         continue;
//         //     }
//         //     else if (strcmp(cmd->commands[0][2], "-h") == 0)
//         //     {
//         //         copy(cmd->commands[0][1], cmd->commands[0][3], false, true);
//         //         free(cmd);
//         //         continue;
//         //     }
//         //     else
//         //     {
//         //         printf("E\n");
//         //         copy(cmd->commands[0][1], cmd->commands[0][2], false, false);
//         //         printf("finished\n");
//         //         free(cmd);
//         //         continue;
//         //     }
//         // }

//         // if (strcmp(cmd->commands[0][0], "print_status") == 0)
//         // {
//         //     print_status();
//         //     free(cmd);
//         //     continue;
//         // }
//         // if (strcmp(cmd->commands[0][0], "mkfs") == 0)
//         // {
//         //     // printf("size %lu\n",sizeof(cmd->commands[0][1]));
//         //     // return 1;
//         //     //  if (sizeof(cmd->commands[0][1])!=4)
//         //     //      {
//         //     //          perror("mkfs command invalid");
//         //     //          exit(EXIT_FAILURE);
//         //     //      }
//         //     mkfs(cmd->commands[0][1], atoi(cmd->commands[0][2]), atoi(cmd->commands[0][3]));
//         //     free(cmd);
//         //     continue;
//         // }
//         // if (strcmp(cmd->commands[0][0], "mount") == 0)
//         // {

//         //     mount(cmd->commands[0][1]);
//         //     free(cmd);
//         //     continue;
//         // }
//         if (strcmp(cmd->commands[0][0], "touch") == 0)
//         {
//             int i = 0;
//             while (cmd->commands[0][i] != NULL)
//                 i += 1;
//             printf("size %d\n", i);
//             touch(cmd->commands[0], i - 1);
//             free(cmd);
//             continue;
//         }
//         if (strcmp(cmd->commands[0][0], "rm") == 0)
//         {
//             rm(cmd->commands[0][1], 1);
//             free(cmd);
//             continue;
//         }
//         if (strcmp(cmd->commands[0][0], "chmod") == 0)
//         {
//             chmod1(cmd->commands[0][1], cmd->commands[0][2]);
//             free(cmd);
//             continue;
//         }
//         // if (strcmp(cmd->commands[0][0], "mv") == 0)
//         // {
//         //     mv(cmd->commands[0][1], cmd->commands[0][2]);
//         //     free(cmd);
//         //     continue;
//         // }
//         if (strcmp(cmd->commands[0][0], "fatfs_init") == 0)
//         {
//             char *fat="minfs";
//             fatfs_init(fat);
//             int i=f_open("f1",0);
//             int i_f3=f_open("f3",0);
//             //int i_f2=f_open("f2",0);
//             printf("%d\n",i);
//             char *to_touch[32];
//             to_touch[0] = "touch";
//             // to_touch[1] = "f2";
//             // f_touch(to_touch,1);
//             // to_touch[0] = "touch";

//             to_touch[1] = "f3";
//             f_touch(to_touch,1);
//             printf("after touch: %d\n",rd[i_f3].cursor);
//             f_copy("f2","f3");
//             printf("after copy: %d\n",rd[i_f3].cursor);
//             f_ls(NULL);
//             char* file_[10]={"cat","f2"};
//             f_cat(file_,2);
//             char* file_1[10]={"cat","f3"};
//             f_cat(file_1,2);
//             free(cmd);
//             continue;
//         }
//         if (strcmp(cmd->commands[0][0], "f_open") == 0)
//         {
//             f_open(cmd->commands[0][1], atoi(cmd->commands[0][2]));
//             free(cmd);
//             continue;
//         }
//         if (strcmp(cmd->commands[0][0], "f_read") == 0)
//         {
//             char *data = malloc(atoi(cmd->commands[0][2]) * sizeof(char));
//             f_read(atoi(cmd->commands[0][1]), atoi(cmd->commands[0][2]), data);
//             printf("%s\n", data);
//             free(data);
//             free(cmd);
//             continue;
//         }
//         // if (strcmp(cmd->commands[0][0], "ls") == 0)
//         // {
//         //     ls();
//         //     free(cmd);
//         //     continue;
//         // }
//         int fd[cmd->num_commands - 1][2];
//         for (int j = 0; j < cmd->num_commands - 1; j += 1)
//         {
//             if (pipe(fd[j]) < 0)
//                 perror("pipe error");
//         }
//         int fg_children[cmd->num_commands];

//         for (int ind_cmd = 0; ind_cmd < cmd->num_commands; ind_cmd += 1)
//         {

//             pid = fork();
//             if (pid == -1)
//             {
//                 perror("fork");
//                 free(cmd);
//                 exit(EXIT_FAILURE);
//             }
//             if (pid == 0)
//             { ////child
//                 // fprintf(stderr,"grandchild in for\n");
//                 check_redirection(cmd);
//                 if (ind_cmd != 0) /// not first time
//                 {

//                     int ret1 = dup2(fd[ind_cmd - 1][0], STDIN_FILENO);
//                     if (ret1 < 0)
//                     {
//                         perror("in_dup2");
//                         free(cmd);
//                     }
//                 }
//                 // printf("buhuiba\n");
//                 if (ind_cmd != (cmd->num_commands - 1))
//                 { /// not last time
//                     // fprintf( stderr, " ne %d,i: %d,command: %s\n",ind_cmd,i,cmd->commands[ind_cmd][0]);
//                     close(fd[ind_cmd][0]);
//                     int ret = dup2(fd[ind_cmd][1], STDOUT_FILENO);
//                     if (ret < 0)
//                         perror("dup2");
//                 }

//                 for (int j = 0; j < cmd->num_commands - 1; j += 1)
//                 {
//                     close(fd[j][1]);
//                     close(fd[j][0]);
//                 }
//                 // fprintf(stderr,"pid: %d child's process group id is now %d\n", (int)getpid(),(int) getpgrp());
//                 // fprintf(stderr,"children\n");
//                 // signal(SIGTTIN, SIG_DFL);
//                 int e = execvp(cmd->commands[ind_cmd][0], cmd->commands[ind_cmd]);

//                 if (e < 0)
//                     perror("exe");
//             }
//             if (ind_cmd == 0)
//                 pgid = pid;
//             setpgid(pid, pgid);
//             fg_children[ind_cmd] = pid;
//             // fprintf(stderr,"grandparent in for\n");
//         }
//         for (int i = 0; i < cmd->num_commands - 1; i += 1)
//         {
//             close(fd[i][1]);
//             close(fd[i][0]);
//         }

//         // int waitp;
//         //  signal(SIGTTIN, SIG_DFL);
//         //  this is in parent process
//         //  s =0, exit, s=1, stoped, s=2, signaled;
//         int s = 0;
//         // fprintf(stderr,"pid: %d, pgid: %d\n",fg_children[ind_cmd],getpgid(fg_children[ind_cmd]));

//         if (!cmd->is_background)
//         {
//             // fprintf(stderr,"parent in for\n");
//             //  if (strcmp(cmd->commands[0][0],"kill")!=0)
//             //  {
//             //      if (sigprocmask(SIG_BLOCK, &new_mask, &old_mask) < 0) {
//             //          perror("sigprocmask");

//             //             }
//             // }
//             // else{
//             //     fprintf(stderr,"kill\n");
//             // }
//             signal(SIGTTOU, SIG_IGN);
//             tcsetpgrp(STDIN_FILENO, fg_children[0]);

//             while (waitpid(-fg_children[0], &status, WUNTRACED) > 0)
//             {

//                 if (WIFEXITED(status))
//                 {
//                 }
//                 else if (WIFSIGNALED(status))
//                 {
//                     // fprintf(stderr, "killed by signal %d\n", WTERMSIG(status));
//                 }
//                 else if (WIFSTOPPED(status))
//                 {
//                     // fprintf(stderr,"stop fg\n");
//                     kill(-fg_children[0], SIGTSTP);
//                     signal(SIGTSTP, sigtstp_handler);
//                     s = 1;
//                     break;
//                 }
//                 else if (WIFCONTINUED(status))
//                 {
//                     // fprintf(stderr, "continued\n");
//                 }
//             }
//             tcsetpgrp(STDIN_FILENO, getpgid(0));
//             //         if (strcmp(cmd->commands[0][0],"kill")!=0){
//             //         if (sigprocmask(SIG_SETMASK, &old_mask, NULL) < 0) {
//             //     perror("sigprocmask");
//             //     return 1;
//             // }}
//         }

//         else
//         {
//             // fprintf(stderr,"bg %s\n",command);
//             enQueue(q, fg_children[0], fg_children[0], command, cmd->num_commands, 0);
//             // fprintf(stderr,"qsize:%d\n",q->q_size);
//         }

//         if (s == 1)
//         {
//             fprintf(stderr, "\nStopped:");
//             fprintf(stderr, " %s", command);
//             enQueue(q, fg_children[0], fg_children[0], command, cmd->num_commands, 1);
//         }

//         free(cmd);
//         // fprintf(stderr,"freecommand:\n");
//     }
//     freeq(q);
// }