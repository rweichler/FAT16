#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define DEBUG_ON //comment this out to turn off debug mode

#define errorf(fmt, ...) fprintf(stderr, "\033[0;31m<error>\033[0m "fmt , ##__VA_ARGS__)
#ifdef DEBUG_ON
#define debugf(fmt, ...) printf("\033[0;32m<debug>\033[0m " fmt, ##__VA_ARGS__)
#else
#define debugf(fmt, ...)
#endif
#define colorf(color, fmt, ...) printf("\033[0;" color "m" fmt "\033[0m", ##__VA_ARGS__)
#define purplef(fmt, ...) colorf("35", fmt, ##__VA_ARGS__)
#define redf(fmt, ...) colorf("31", fmt, ##__VA_ARGS__)
#define greenf(fmt, ...) colorf("32", fmt, ##__VA_ARGS__)

bool read_bytes(FILE *f, unsigned int offset, void *buf, unsigned int length)
{
    if(fseek(f, offset, SEEK_SET != 0)) {
        errorf("error seeking\n");
        return false;
    }

    if(length != fread(buf, 1, length, f) && ferror(f)) {
        errorf("error reading file\n");
        return false;
    }

    return true;
}

#define LEN_BOOT 446
#define LEN_PART 16
#define NUM_PARTS 4
#define BEGIN_PART_LBA 8
#define LEN_PART_LBA 4
#define SIG 0xAA55
bool check_sig(FILE *f)
{
    uint16_t sig;
    if(!read_bytes(f, LEN_BOOT + LEN_PART*NUM_PARTS, &sig, sizeof(sig)))
        return false;
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
    uint16_t RsvdSecCnt;

    uint8_t NumFATs;
    uint16_t RootEntCnt;
    uint16_t TotSec16;
    uint8_t Media;
    uint16_t FATSz16; //sectors per FAT
    uint16_t SecPerTrk;
    uint16_t NumHeads;
    uint32_t HiddSec;

    uint32_t TotSec32; //total size
};
uint32_t BPB_FAT_addr(struct BPB_t *bpb)
{
    return bpb->RsvdSecCnt * bpb->BytsPerSec;
}
uint32_t BPB_Root_addr(struct BPB_t *bpb)
{
    return BPB_FAT_addr(bpb) + bpb->NumFATs * bpb->FATSz16 * bpb->BytsPerSec;
}
uint32_t BPB_Data_addr(struct BPB_t *bpb)
{
    return BPB_Root_addr(bpb) + bpb->RootEntCnt * 32;
                                //size of root
}
uint32_t BPB_Data_Sector_count(struct BPB_t *bpb)
{
    return bpb->TotSec32 - BPB_Data_addr(bpb) / bpb->BytsPerSec;
}

struct dir_t {
    char Name[11];
    uint8_t Attr;
    uint8_t NTRes;
    uint8_t CrtTimeTenth;
    uint16_t CrtTime;

    uint16_t CrtDate;
    uint16_t LstAccDate;
    uint16_t FstClusHI;
    uint16_t WrtTime;
    uint16_t WrtDate;
    uint16_t FstClusLO;
    uint32_t FileSize;
};
enum dirattr_t {
    DIR_ATTR_READONLY = 1 << 0,
    DIR_ATTR_HIDDEN   = 1 << 1,
    DIR_ATTR_SYSTEM   = 1 << 2,
    DIR_ATTR_VOLUMEID = 1 << 3,
    DIR_ATTR_DIRECTORY= 1 << 4,
    DIR_ATTR_ARCHIVE  = 1 << 5,
    DIR_ATTR_LFN      = 0xF
};
#pragma pack(pop)

uint16_t get_next_cluster(FILE *f, struct BPB_t *bpb, uint16_t cluster)
{
    uint16_t result;
    read_bytes(f, BPB_FAT_addr(bpb) + cluster*2, &result, sizeof(result));
    return result;
}

#define MIN(a, b) ((a) < (b) ? (a) : (b))
void print_cluster(FILE *f, struct BPB_t *bpb, struct dir_t *dir)
{
    const uint32_t cluster_size = bpb->BytsPerSec * bpb->SecPerClus;

    for(uint16_t c = dir->FstClusLO; c < 0xFFF8; c = get_next_cluster(f, bpb, c)) {
        char buf[cluster_size];
        //FAT16 spec says that you have to subtract 2 from the cluster number to get the actual data
        //so weird. but whatevs....
        read_bytes(f, BPB_Data_addr(bpb) + (c - 2) * cluster_size, buf, sizeof(buf));

        printf("%.*s", (int)(sizeof(buf)), buf);
    }
}

int main(int argc, char *argv[])
{
    char *filename = argc < 2 ? "fat.ima" : argv[1];

    FILE *f = fopen(filename, "r");

    if(f == NULL) {
        errorf("couldn't open file '%s'\n", filename);
        return 1;
    }

    if(!check_sig(f))
        return 1;

    struct BPB_t bpb;
    read_bytes(f, 0x0, &bpb, sizeof(bpb));

    if(bpb.BytsPerSec != 512 || bpb.NumFATs != 2)
        return 1;

    greenf( "FAT addr: 0x%x\n"
            "Root addr: 0x%x\n"
            "Data addr: 0x%x\n",
            BPB_FAT_addr(&bpb),
            BPB_Root_addr(&bpb),
            BPB_Data_addr(&bpb)
          );

    for(int i = 0; true; i++) {
        struct dir_t dir;
        uint32_t offset = BPB_Root_addr(&bpb) + i*32;
        read_bytes(f, offset, &dir, sizeof(dir));

        if(dir.Name[0] == 0)
            break;


        purplef("\n%.*s", (int)(sizeof(dir.Name)/sizeof(char)), dir.Name);
        const char *type;

        if(dir.Attr & DIR_ATTR_LFN)
            purplef("<LONG FILENAME>\n");
        else if(dir.Attr & DIR_ATTR_DIRECTORY)
            purplef("<DIRECTORY>\n");
        else {
            purplef("<FILE>\n");

            if(i == 0xF)
                print_cluster(f, &bpb, &dir);
            else
                printf("<omitting print>\n");
        }
    }

    fclose(f);

    return 0;
}

