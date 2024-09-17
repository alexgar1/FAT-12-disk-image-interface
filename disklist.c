#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>

#define SECTSIZE 512
#define ENTRYSIZE 32
#define MAXROOTDIR 224

#define timeOffset 14 
#define dateOffset 16 


char* getStr(void *map, int size, int off) {
    char* buff = malloc(sizeof(unsigned char)*size);
    memcpy(buff, map + off, size);
    return buff;
}

char* getFileName(char *start) {
	// gets file name and cats extension with .

    char* name = malloc(13);
    if (name == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }

    strncpy(name, start, 8);

	// remove spaces
    for (int i = 7; i >= 0; i--) {
        if (name[i] == ' ') {
            name[i] = '\0';
        } else {
            break;
        }
    }

    if (start[8] != ' ') {  // add extension
        strcat(name, ".");
        strncat(name, start + 8, 3);
        name[12] = '\0';
    }

    return name;
}


char* datetime (char * directory_entry_startPos){

	// code from brightspace modified slightly
	
	int time, date;
	int day, month, year, hours, minutes;
	
	time = *(unsigned short *)(directory_entry_startPos + timeOffset);
	date = *(unsigned short *)(directory_entry_startPos + dateOffset);
	
	//the year is stored as a value since 1980
	//the year is stored in the high seven bits
	year = ((date & 0xFE00) >> 9) + 1980;
	//the month is stored in the middle four bits
	month = (date & 0x1E0) >> 5;
	//the day is stored in the low five bits
	day = (date & 0x1F);
	
	//the hours are stored in the high five bits
	hours = (time & 0xF800) >> 11;
	//the minutes are stored in the middle 6 bits
	minutes = (time & 0x7E0) >> 5;
	
	char *formattedTime = (char *)malloc(19 * sizeof(char));
    if (formattedTime == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }
	snprintf(formattedTime, 19, "%2d/%d/%4d %02d:%02d", day, month, year, hours, minutes);
	
	return formattedTime;	
}



int listdirs(char *start, char *map, char *curdir){
	// recursively navigate file tree

	if (strcmp(curdir, "0") != 0){
		printf("%s\n", curdir);
		printf("==================\n");
	}

	 // this directory entry is free and all the remaining directory entries in this directory are also free
	if (start[0] == 0) return 0;
	
	int logical = start[26] + (start[27]<<8);
	if (logical == 0 || logical == 1) return listdirs(start + 32, map, "0");

    int size = (start[28] & 0xFF) + ((start[29] & 0xFF) << 8) + ((start[30] & 0xFF) << 16) + ((start[31] & 0xFF) << 24);
	int type = 'F';
	char* name = getStr(start, 13, 0);
	char *date;
    
    if((start[11] & 0x10) == 0x10) type = 'D'; // if subdirectory

	date = datetime(start + 8*(type=='D')); // get date (directories have last write date instead of creation)
	name = getFileName(start); // get file name with extension
	
	printf("%c %10d %20s %s\n", type, size, name, date);
	if (type == 'D'){
		char *sub = map + SECTSIZE * (logical + 31) + 64; // find subdir location
		return listdirs(start+32, map, "0") + listdirs(sub, map, name);  // recursion on next sect and sub dir
	}
	free(name);
	free(date);
	listdirs(start+32, map, "0"); // recursion only next sect
	
    return 0;
}

void label(void *map, char *label){
	// gets disk label

	int found = 0;

	for (int i=0; i<MAXROOTDIR; i++){
		char *entry = getStr(map, 12, SECTSIZE * 19 + i * ENTRYSIZE);
		if (entry[11] == 0x08){
			memcpy(label, entry, 12);
			free(entry);
			label[11] = '\0';
			found = 1;
		}
	}
	if (found){
		printf("%s\n", label);
		return;
	}
	printf("Disk has no label\n");

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

    char root[12];
    label(p, root);
	printf("==================\n");
    listdirs(p+SECTSIZE*19, p, "0"); // begin recursive search

	munmap(p, sb.st_size);
	close(fd);
	return 0;
}

