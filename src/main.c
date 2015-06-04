#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "macros.h"
#include "structs.h"

//BPB functions

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

bool read_bytes(FILE *f, unsigned int offset, void *buf, unsigned int length)
{
    if(fseek(f, offset, SEEK_SET != 0)) {
        errorf("error seeking\n");
        return false;
    }

    if(fread(buf, 1, length, f) != length && ferror(f)) {
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

    for(int i = 0; i < bpb.RootEntCnt; i++) {
        struct dir_t dir;
        uint32_t offset = BPB_Root_addr(&bpb) + i*32;
        read_bytes(f, offset, &dir, sizeof(dir));

        if(dir.Name[0] == 0) {
            break;
        } else if(dir.Name[0] == 0xE5) {
            printf("<unused space>\n");
            continue;
        }


        purplef("\n%.*s", (int)(sizeof(dir.Name)/sizeof(char)), dir.Name);
        const char *type;

        if(dir.Attr & DIR_ATTR_LFN) {
            purplef("<LONG FILENAME>\n");
        } else if(dir.Attr & DIR_ATTR_DIRECTORY) {
            purplef("<DIRECTORY>\n");
        } else {
            purplef("<FILE>\n");

            //if(i == 0xF)
            //    print_cluster(f, &bpb, &dir);
            //else
                printf("<omitting print>\n");
        }
    }

    fclose(f);

    return 0;
}

