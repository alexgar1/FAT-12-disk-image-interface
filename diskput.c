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
#define MAXSECTORS 2847

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

int findFreeCluster(char *fat, int totalsectors) {
    for (int i = 2; i < totalsectors; i++) {
        int entry = fat[i * 3 / 2];
        if (i % 2 == 0) {
            entry |= (fat[(i * 3 / 2) + 1] & 0x0F) << 8;
        } else {
            entry = (entry >> 4) | (fat[(i * 3 / 2) + 1] << 4);
        }
        if (entry == 0x000) {
            return i;
        }
    }
    return -1;
}

void writeFAT(char *fat, int cluster, int value) {
    int offset = cluster * 3 / 2;
    if (cluster % 2 == 0) {
        fat[offset] = value & 0xFF;
        fat[offset + 1] = (fat[offset + 1] & 0xF0) | ((value >> 8) & 0x0F);
    } else {
        fat[offset] = (fat[offset] & 0x0F) | ((value << 4) & 0xF0);
        fat[offset + 1] = (value >> 4) & 0xFF;
    }
}

int findFreeEntry(char *rootdir) {
    for (int i = 0; i < MAXROOTDIR; i++) {
        if (rootdir[i * ENTRYSIZE] == 0x00 || rootdir[i * ENTRYSIZE] == 0xE5) {
            return i;
        }
    }
    return -1;
}


// recursively navigate file tree
int listdirs(char *start, char *map, char *curdir, char **spot){
	char nextdir[strlen(curdir)+1];
	strcpy(nextdir, curdir);
	char *token = strtok(nextdir, "/"); // parses first directory

	

	 // this directory entry is free and all the remaining directory entries in this directory are also free
	if (start[0] == 0){
		printf("The directory not found");
		return 1;
	}
	
	int logical = start[26] + (start[27]<<8);
	if (logical == 0 || logical == 1) return 1;
	

    //int size = (start[28] & 0xFF) + ((start[29] & 0xFF) << 8) + ((start[30] & 0xFF) << 16) + ((start[31] & 0xFF) << 24);
	char* name = getStr(start, 13, 0);
	int type = '0';
    if((start[11] & 0x10) == 0x10) type = 'D'; // determine if directory exists

	name = getFileName(start); // get file name with extension
	
	
	if (token!=NULL){ 
		if (strcmp(name, token) == 0){
			if (type == 'D'){
				char *remaining = strstr(curdir, token) + strlen(token);

				
				char *sub = map + SECTSIZE * (logical + 31) + 64; // find subdir location

				if (strcmp(remaining, "/") == 0){
					//printf("Found correct directory\n");
					*spot = start;
					return 0;
				}
				return listdirs(sub, map, remaining, spot);  // recursion on next sect and sub dir

			}
		}
	}
	

	free(name);
	listdirs(start+32, map, curdir, spot); // recursion only next sect
	
    return 1;
}

void writeDirectoryEntry(char *entry, char *filename, int first_cluster, int filesize) {
    memset(entry, ' ', 11);
    char *dot = strrchr(filename, '.');
    if (dot != NULL) {
        memcpy(entry, filename, dot - filename);
        memcpy(entry + 8, dot + 1, 3);
    } else {
        memcpy(entry, filename, strlen(filename));
    }
    entry[11] = 0x20;
    entry[26] = first_cluster & 0xFF;
    entry[27] = (first_cluster >> 8) & 0xFF;
    entry[28] = filesize & 0xFF;
    entry[29] = (filesize >> 8) & 0xFF;
    entry[30] = (filesize >> 16) & 0xFF;
    entry[31] = (filesize >> 24) & 0xFF;
}

//split the path into directory and file name
void splitPath(const char *path, char **dir, char **file) {
    char *lastSlash = strrchr(path, '/');
    if (lastSlash != NULL) {

        *dir = malloc(lastSlash - path + 2);
        *file = malloc(strlen(lastSlash));


        strncpy(*dir, path, lastSlash - path + 1);
        (*dir)[lastSlash - path + 1] = '\0';


        strcpy(*file, lastSlash + 1);
    } else {

        *dir = NULL;
        *file = strdup(path);
    }
}



int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <disk image> <file name>\n", argv[0]);
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
	const char *path = argv[2];
    char *dir = NULL;
    char *file = NULL;

    splitPath(path, &dir, &file);

	FILE *towrite = fopen(file, "rb");
	if (!towrite){
		printf("File not found\n");
		return 1;
	}

	char *spot = NULL;

	// recursive search
	if (dir!=NULL){
		if (listdirs(p+SECTSIZE*19, p, dir, &spot)){
			printf("The directory not found\n");

			free(dir);
			free(file);

			munmap(p, sb.st_size);
			close(fd);
			return 1;
		}
	} else {
		spot = p + SECTSIZE * 19;
	}

	fseek(towrite, 0, SEEK_END);
    int filesize = ftell(towrite);
    fseek(towrite, 0, SEEK_SET);

	if (filesize > sb.st_size) {
        fprintf(stderr, "No enough free space in the disk image.\n");
        fclose(towrite);
        free(dir);
        free(file);
        munmap(p, sb.st_size);
        close(fd);
        return 1;
    }

    char *direntry = spot;
    int freeentry = -1;
    for (int i = 0; i < MAXROOTDIR; i++) {
        if (direntry[i * ENTRYSIZE] == 0x00 || direntry[i * ENTRYSIZE] == 0xE5) {
            freeentry = i;
            break;
        }
    }
	if (freeentry < 0) {
        fprintf(stderr, "No enough free space in the disk image.\n");
        fclose(towrite);
        free(dir);
        free(file);
        munmap(p, sb.st_size);
        close(fd);
        return 1;
    }

	char *fat = p + SECTSIZE;
    int freecluster = findFreeCluster(fat, MAXSECTORS);
	char *dataarea = p + SECTSIZE * (31 + freecluster);
    fread(dataarea, 1, filesize, towrite);
    fclose(towrite);

    writeFAT(fat, freecluster, 0xFFF);
    writeDirectoryEntry(spot + freeentry*ENTRYSIZE, file, freecluster, filesize);

	free(dir);
	free(file);
	munmap(p, sb.st_size);
	close(fd);
	return 0;
}

