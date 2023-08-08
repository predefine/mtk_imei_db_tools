#include <assert.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#define DB_SIGN           48
#define IMEI_SIZE         15
#define IMEI_ENCODED_SIZE 12
#define DBVER1_SIZE       IMEI_ENCODED_SIZE*2
#define DBVER2_SIZE       DB_SIGN*2+DBVER1_SIZE

enum DBVER {
    DBVER1=1,
    DBVER2=2
};

size_t fsize(int fd){
    struct stat fst;
    fstat(fd, &fst);
    return fst.st_size;
}

void decode_imei(unsigned char* imei_in, unsigned char* imei_out){
    char out_mask[8] = {0xAB, 0xA0, 0x6F, 0x2F, 0x1F, 0x1E, 0x9A, 0x45};
    for(int i = 0; i < IMEI_ENCODED_SIZE-4; i++){
        unsigned char imei_decoded = imei_in[i]^out_mask[i];
        imei_out[i*2]=(imei_decoded%16)+'0';
        if(i!=7)
            imei_out[i*2+1]=(imei_decoded>>4)+'0';
    }
    unsigned char tmp_sign[2]={0,0};
    for (int i = 0; i < IMEI_ENCODED_SIZE-2; i++)
    {
        if (i & 1)
        {
            tmp_sign[1] += imei_in[i];
        }
        else
        {
            tmp_sign[0] += imei_in[i];
        }
    }
    assert((tmp_sign[0]==imei_in[10])&&(tmp_sign[1]==imei_in[11]));
}

int main(){
    int dbfd = open("MP0B_001", O_RDWR);
    unsigned char encoded_imei[IMEI_ENCODED_SIZE];
    unsigned char out_imei[IMEI_SIZE];
    int dbversion = 0;
    switch (fsize(dbfd)) {
        case DBVER1_SIZE:
            dbversion = DBVER1;
            break;
        case DBVER2_SIZE:
            dbversion = DBVER2;
            break;
        default:
            assert(0);
            break;
    }

    printf("DB Version: %d\n", dbversion);
    for(int imei_num=0; imei_num<2; imei_num++){
        read(dbfd, encoded_imei, sizeof(encoded_imei));
        decode_imei(encoded_imei, out_imei);
        printf("IMEI %d: %s\n", imei_num+1, out_imei);
    }
    
    close(dbfd);
    return 0;
}