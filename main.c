#include <assert.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
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

int exists(char* path){
    if (!access(path, F_OK)){
        return 1;
    }
    return 0;
}

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
        tmp_sign[i&1] += imei_in[i];
    assert((tmp_sign[0]==imei_in[10])&&(tmp_sign[1]==imei_in[11]));
}

void encode_imei(unsigned char* imei_in, unsigned char* imei_out){
    char out_mask[8] = {0xAB, 0xA0, 0x6F, 0x2F, 0x1F, 0x1E, 0x9A, 0x45};
    for(int i = 0; i < IMEI_ENCODED_SIZE-4; i++){
        imei_out[i] = imei_in[i*2]-'0';
        if(i!=7)
            imei_out[i] += (imei_in[i*2+1]-'0')<<4;
        imei_out[i] = imei_out[i] ^ out_mask[i];
    }
    imei_out[8] = 0xf3;
    imei_out[9] = 0xf3;
    imei_out[10] = 0;
    imei_out[11] = 0;
    for(int i = 0; i < IMEI_ENCODED_SIZE-2; i++){
        imei_out[10+(i&1)] += imei_out[i];
    }
}

int get_db_version(int dbfd){
    switch (fsize(dbfd)) {
        case DBVER1_SIZE:
            return DBVER1;
            break;
        case DBVER2_SIZE:
            return DBVER2;
            break;
        default:
            return -1;
            break;
    }
}

void read_imei(int dbfd, int dbversion){
    unsigned char encoded_imei[IMEI_ENCODED_SIZE];
    unsigned char out_imei[IMEI_SIZE];
    printf("DB Version: %d\n", dbversion);
    for(int imei_num=0; imei_num<2; imei_num++){
        read(dbfd, encoded_imei, sizeof(encoded_imei));
        decode_imei(encoded_imei, out_imei);
        printf("IMEI %d: %s\n", imei_num+1, out_imei);
    }
}

void write_imei(int dbfd, int dbversion, int argc, char** argv){
    int imei2 = 0;
    if(argc<4){
        printf("Usage: %s w <DB NAME> <IMEI1 123456789012345> [IMEI2 123456789012345]\n", argv[0]);
        return;
    }
    if (argc==5){
        imei2 = 1;
    }
    if (strlen(argv[3])!=15){
        printf("Usage: %s w <DB NAME> <IMEI1 123456789012345> [IMEI2 123456789012345]\n", argv[0]);
        return;
    }
    if (imei2 && strlen(argv[4])!=15){
        printf("Usage: %s w <DB NAME> <IMEI1 123456789012345> [IMEI2 123456789012345]\n", argv[0]);
        return;
    }
    unsigned int dbsize = DBVER1_SIZE;
    if(dbversion==DBVER1) dbsize=DBVER1_SIZE;
    else if(dbversion==DBVER2) dbsize=DBVER2_SIZE;
    unsigned char db[dbsize];
    read(dbfd, db, dbsize);
    lseek(dbfd, 0, SEEK_SET);
    unsigned char imei_out[IMEI_ENCODED_SIZE];
    encode_imei((unsigned char*) argv[3], imei_out);
    memcpy(db, imei_out, sizeof(imei_out));
    if(imei2){
        encode_imei((unsigned char*) argv[4], imei_out);
        memcpy(db+IMEI_ENCODED_SIZE, imei_out, sizeof(imei_out));
    }
    write(dbfd, db, sizeof(db));
}

void usage(char* name){
    printf(
        "Usage: %s <r|w|c> <DB NAME>\n",
    name);
}

int main(int argc, char** argv){
    if (argc < 3){
        usage(argv[0]);
        return 1;
    }

    int dbfd = -1;
    int dbversion = -1;
    switch (argv[1][0]) {
        case 'r':
            if(!exists(argv[2])){
                usage(argv[0]);
                return 1;
            }
            dbfd = open(argv[2], O_RDONLY);
            dbversion = get_db_version(dbfd);
            read_imei(dbfd, dbversion);
            break;
        case 'w':
            if(!exists(argv[2])){
                usage(argv[0]);
                return 1;
            }
            dbfd = open(argv[2], O_RDWR);
            dbversion = get_db_version(dbfd);
            write_imei(dbfd, dbversion, argc, argv);
            break;
        case 'c':
            printf("Not implemented!\n");
            return 1;
            break;
        default:
            break;
    }

    if (dbfd>-1) close(dbfd);
    return 0;
}