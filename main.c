#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#define LEN_BOOT 446
#define LEN_PART 16
#define NUM_PARTS 4
#define BEGIN_PART_LBA 8
#define LEN_PART_LBA 4

#define errorf(fmt, ...) fprintf(stderr, "\033[0;31merror:\033[0m "fmt , ##__VA_ARGS__)
#define debugf(fmt, ...) printf("\033[0;32mdebug:\033[0m " fmt, ##__VA_ARGS__)
/* comment to turn on debug
#undef debugf
#define debugf(...)
//*/

bool read_bytes(FILE *f, unsigned int offset, void *buf, unsigned int length)
{
    if(fseek(f, offset, SEEK_SET != 0)) {
        errorf("error seeking\n");
        return false;
    }

    if(fgets(buf, length, f) == NULL) {
        errorf("error reading file\n");
        return false;
    }

    return true;
}

#define READ_BITS_DECL(NUM_BITS) \
bool read_i##NUM_BITS(FILE *f, unsigned int offset, uint##NUM_BITS##_t *result)\
{\
    char buf[NUM_BITS/8 + 1];\
    if(!read_bytes(f, offset, buf, sizeof(buf))) {\
        return false;\
    }\
    *result = 0;\
    for(int i = NUM_BITS/8 - 1; i >= 0; i--) {\
        *result = *result << 8;\
        *result = *result + (unsigned char)buf[i];\
    }\
    return true;\
}

READ_BITS_DECL(8)
READ_BITS_DECL(16)
READ_BITS_DECL(32)
READ_BITS_DECL(64)

#define SIG 0xAA55
bool check_sig(FILE *f)
{
    uint16_t sig;
    if(!read_i16(f, LEN_BOOT + LEN_PART*NUM_PARTS, &sig)) return false;
    if(sig != SIG) {
        errorf("sanity check failed: %04x\n", sig);
    }
    return sig == SIG;
}

#define BYTES_PER_SECTOR 512
bool check_bps(FILE *f)
{
    uint16_t bps;
    if(!read_i16(f, 0x0B, &bps)) return false;
    if(bps != BYTES_PER_SECTOR) {
        errorf("bps is %u, should be %u\n", bps, BYTES_PER_SECTOR);
    }
    return bps == BYTES_PER_SECTOR;
}

#define NUM_FATS 2
bool check_num_fats(FILE *f)
{
    uint8_t num;
    if(!read_i8(f, 0x10, &num)) return false;
    if(num != NUM_FATS) {
        errorf("num FATs is %u, should be %u\n", num, NUM_FATS);
    }
    return num == NUM_FATS;
}

int main(int argc, char *argv[])
{
    char *filename = argc < 2 ? "fat.ima" : argv[1];

    FILE *f = fopen(filename, "r");

    if(f == NULL) {
        errorf("couldn't open file '%s'\n", filename);
        return 1;
    }

    if(!check_sig(f) || !check_bps(f) || !check_num_fats(f)) return 1;

    uint16_t RsvdSecCnt; //reserved sector count
    if(!read_i16(f, 0x0E, &RsvdSecCnt)) return 1;

    uint16_t FATz16; //sectors per FAT
    if(!read_i16(f, 0x16, &FATz16)) return 1;

    uint8_t SecPerClus; //sectors per cluster
    if(!read_i8(f, 0x0D, &SecPerClus)) return 1;

    uint16_t RootEntCnt;
    if(!read_i16(f, 0x11, &RootEntCnt)) return 1;

    uint32_t TotSec32; //total size
    if(!read_i32(f, 0x20, &TotSec32)) return 1;

    uint32_t FirstFATSector = RsvdSecCnt;
    uint32_t FirstRootSector = FirstFATSector + NUM_FATS*FATz16;
    uint32_t RootDirectorySectors = RootEntCnt*32/512;
    uint32_t FirstDataSector = FirstRootSector + RootDirectorySectors;
    uint32_t SectorCount = TotSec32 - FirstDataSector;

    for(int i = 0; true; i++) {
        uint32_t offset = FirstRootSector*BYTES_PER_SECTOR + i*32;
        uint8_t first_byte;
        read_i8(f, offset, &first_byte);
        if(first_byte == 0) break;

        uint8_t attrib;
        read_i8(f, offset + 0x0B, &attrib);
        if((attrib & 0xF) == 0xF) {
            printf("long filename\n");
            continue;
        }

        char name[12];
        read_bytes(f, offset, name, sizeof(name));
        printf("%s\n", name);
    }

    uint32_t x;
    char y[2] = "\x00\x02";
    memcpy(&x, y, 2);
    printf("\n\nx: %u\n", x);

    fclose(f);

    return 0;
}
