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

#pragma pack(push, 1)
struct BPB_t {
    uint8_t jmpBoot[3];
    char OEMName[8];
    uint16_t BytsPerSec;
    uint8_t SecPerClus;
    uint16_t RsvdSecCnt; //reserved sector count

    uint8_t NumFATs;
    uint16_t RootEntCnt;
    uint16_t TotSec16;
    uint8_t Media;
    uint16_t FATSz16; //sectors per FAT
    uint16_t SecPerTrk; //sectors per cluster
    uint16_t NumHeads;
    uint32_t HiddSec;

    uint32_t TotSec32; //total size


    char null;
};
#pragma pack(pop)

int main(int argc, char *argv[])
{
    char *filename = argc < 2 ? "fat.ima" : argv[1];

    FILE *f = fopen(filename, "r");

    if(f == NULL) {
        errorf("couldn't open file '%s'\n", filename);
        return 1;
    }

    if(!check_sig(f)) return 1;

    struct BPB_t bpb;
    read_bytes(f, 0x0, &bpb, sizeof(bpb));

    if(bpb.BytsPerSec != 512 || bpb.NumFATs != 2) return 1;

    uint32_t FirstFATSector = bpb.RsvdSecCnt;
    uint32_t FirstRootSector = FirstFATSector + bpb.NumFATs*bpb.FATSz16;
    uint32_t RootDirectorySectors = bpb.RootEntCnt*32/512;
    uint32_t FirstDataSector = FirstRootSector + RootDirectorySectors;
    uint32_t SectorCount = bpb.TotSec32 - FirstDataSector;

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

    fclose(f);

    return 0;
}
