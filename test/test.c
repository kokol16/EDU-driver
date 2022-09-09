#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#define DMA_BASE 0x40000

int edu_calculate_factorial(int val){
        int reg = 0, fd = 0;

        fd = open("/dev/edu", O_RDWR);

        // Write factorial to calculate
        lseek(fd, 0x08, SEEK_SET);
        write(fd, &val, 4);

        lseek(fd, 0x20, SEEK_SET);
        while(read(fd, &reg, 4) == 4 && reg == 0x01) { ; /* Spin */}

        lseek(fd, 0x08, SEEK_SET);
        read(fd, &reg, 4);

        lseek(fd, 0x00, SEEK_SET);

        close(fd);

        return reg;
}

int edu_get_device_id(void){
        return 1;
}

int edu_get_vendor_id(void){
        return 1;
}

int edu_get_id(void){
        return 1;
}

int store(int address, void *buffer, int length){
        int fd = 0;

        fd = open("/dev/edu", O_RDWR);

        lseek(fd, DMA_BASE + address, SEEK_SET);
        write(fd, buffer, length);

        close(fd);

        return 0;
}

int load(int address, void *buffer, int length){
        int fd = 0;

        fd = open("/dev/edu", O_RDWR);

        lseek(fd, DMA_BASE + address, SEEK_SET);
        read(fd, buffer, length);

        close(fd);

        return 0;
}

int main(void){
        printf("Factorial: %d\n", edu_calculate_factorial(8));
        char buffer[] = "Hello World";
        char *b = (char *)malloc(256);
        memset(b, 0, 256);
        store(0, buffer, sizeof(buffer));
        load(0, b, sizeof(buffer));
        printf("Stored %x : %s\nLoaded %x : %s\n", buffer, buffer, b, b);
        
        char buffer1[] = "Wat";
        memset(b, 0, 256);
        store(0, buffer1, sizeof(buffer1));
        load(0, b, sizeof(buffer1));
        printf("Stored %x : %s\nLoaded %x : %s\n", buffer1, buffer1, b, b);

        return 0;
}
