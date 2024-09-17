#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <stdint.h>




#define BYTESPERSECTOFF 11
#define BYTESPERSECT 2
#define FATSIZE 3
#define ENTRYSIZE 32
#define SECTSIZE 512
#define MAXROOTDIR 224
#define NAMELEN 8
#define NAMEOFF 3





char* getStr(void *map, int size, int off) {
    char* buff = malloc(sizeof(unsigned char)*size);
    memcpy(buff, map + off, size);
    return buff;
}

uint16_t getInt(void *map, int size, int off){
	uint16_t num;
	memcpy(&num, map + off, sizeof(num));
	return num;
}

void label(void *map){
	char label[12];
	int found = 0;

	for (int i=0; i<MAXROOTDIR; i++){
		char *entry = getStr(map, 12, SECTSIZE * 19 + i * ENTRYSIZE);
		if (entry[11] == 0x08){
			memcpy(label, entry, 12);
			label[11] = '\0';
			found = 1;
		}
	}
	if (found){
		printf("Label of the disk: %s\n", label);
		return;
	}
	printf("Disk has no label\n");

}

void freeSize(void *map){
	uint16_t bpersect = getInt(map, BYTESPERSECT, BYTESPERSECTOFF);
    
    uint32_t freeclust = 0;
	for (int i=2; i<2849; ++i){
		uint16_t entry = getInt(map, 2, SECTSIZE + i*3/2);
		if (i%2==0){
			entry = entry & 0x0fff;
		} else {
			entry = entry >> 4;
		}
		freeclust+=!entry;
	}
    printf("Free size of the disk: %u bytes\n", freeclust * bpersect);

}

int countfiles(char *start, char *map){
	int count = 0;

	if (start[0] == 0){ // this directory entry is free and all the remaining directory entries in this directory are also free
		return count;
	}

	int logical = start[26] + (start[27]<<8);

	if (start[11] == 0x0F || start[11] & 0x08 || (start[0]&0xFF) == 0xE5 || logical == 0 || logical == 1){
		return count + countfiles(start+32, map);
	} 
	else if((start[11] & 0x10) == 0x10){ // if subdirectory
		char *sub = map + SECTSIZE * (logical + 31) + 64;
		return count + countfiles(start+32, map) + countfiles(sub, map);
	}
	else {
		return ++count + countfiles(start+32, map);
	}
}


int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <disk image>\n", argv[0]);
        return 1;
    }

	int fd;
	struct stat sb;

	fd = open(argv[1], O_RDWR);
	fstat(fd, &sb);

	char * p = mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0); // p points to the starting pos of your mapped memory
	if (p == MAP_FAILED) {
		printf("Error: failed to map memory\n");
		exit(1);
	}

    printf("OS Name: %s\n", getStr(p, NAMELEN, NAMEOFF));
	label(p);
	printf("Total size of the disk: %lu\n", (uint64_t)sb.st_size);
	freeSize(p);

	printf("==============\n");
	printf("The number of files in the disk %d\n", countfiles(p+SECTSIZE*19, p));
	printf("==============\n");

	printf("Number of FAT copies: %d\n", p[16]);
	printf("Sectors per FAT: %d\n", p[22] + (p[23] << 8));
    

	munmap(p, sb.st_size); // the modifed the memory data would be mapped to the disk image
	close(fd);
	return 0;
}


