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



void writeBufferToFile(const char *filename, const char *buffer, int size) {
    
    // write buffer to file
    FILE *file = fopen(filename, "wb");
    if (file == NULL) {
        fprintf(stderr, "Error: Could not open file %s\n", filename);
        exit(1);
    }
    
    size_t written = fwrite(buffer, sizeof(char), size, file);
    if (written != size) {
        fprintf(stderr, "Error: Could not write entire buffer to file %s\n", filename);
    }

    fclose(file);
}



int searchroot(char *p, char *filename){
    char *entry = p+SECTSIZE*19;
    
    int logical, size;
    for (int i=0; i<MAXROOTDIR; i++){
        char *start = entry + i*ENTRYSIZE;

        if (start[0] == 0){
            printf("File not found\n");
            break;
        }

        char* name = getFileName(start);


        if (strcmp(filename, name) == 0){
            logical = start[26] + (start[27] << 8);
            size = (start[28] & 0xFF) + ((start[29] & 0xFF) << 8) + ((start[30] & 0xFF) << 16) + ((start[31] & 0xFF) << 24);
            writeBufferToFile(filename, p + SECTSIZE * (logical + 31), size);
            return 0;
        }
    }

    return 1;
}


int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <disk image> <file name>\n", argv[0]);
        return 1;
    }

	int fd;
	struct stat sb;
    char *filename = argv[2];

	fd = open(argv[1], O_RDWR);
	fstat(fd, &sb);

	char * p = mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0); // p points to the starting pos of your mapped memory
	if (p == MAP_FAILED) {
		printf("Error: failed to map memory\n");
		exit(1);
	}

    searchroot(p, filename); // begin recursive search

	munmap(p, sb.st_size);
	close(fd);
	return 0;
}
