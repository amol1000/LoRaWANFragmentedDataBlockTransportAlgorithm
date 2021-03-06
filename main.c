#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "frag.h"

void putbuf(uint8_t *buf, int len);
void put_bool_buf(uint8_t *buf, int len);
void frag_encobj_log(frag_enc_t *encobj);

void frag_generate_valid_data_map(uint8_t *map, int len, float r);

int flash_write(uint32_t addr, uint8_t *buf, uint32_t len);
int flash_read(uint32_t addr, uint8_t *buf, uint32_t len);


/* brief::
M == The  initial  data  block  that  needs  to  be  transported  must
     first  be  fragmented  into  M  data 417fragments  of arbitrary  but  equal  length
N == bytes to be sent
*/
#define FRAG_NB                 (10) // data block will be divided into 10 fragments
#define FRAG_SIZE               (30) // each fragment size will be 10 bytes
// thus data block size is 10 * 10 == 100
#define FRAG_CR                 (FRAG_NB - 6) // basically M/N
#define FRAG_PER                (0.25) // changes the lost packet count
#define FRAG_TOLERENCE          (10 + FRAG_NB * (FRAG_PER + 0.05))//0.05
#define LOOP_TIMES              (1)
#define DEBUG

frag_enc_t encobj;
//uint8_t enc_dt[FRAG_NB * FRAG_SIZE]; // 100 bytes
uint8_t enc_buf[FRAG_NB * FRAG_SIZE + FRAG_CR * FRAG_SIZE + FRAG_NB * FRAG_CR]; // //100 + 20 * 10 + 20 * 10 == 500 bytes

frag_dec_t decobj;
uint8_t dec_buf[(FRAG_NB + FRAG_CR) * FRAG_SIZE + 1024*1024];
uint8_t dec_flash_buf[(FRAG_NB + FRAG_CR) * FRAG_SIZE + 1024*1024];

int process()
{
    int i, ret, len;
    uint8_t *dynamic_valid_data_map;

    srand(time(0));
    srand(0);

    for (i = 0; i < FRAG_NB * FRAG_SIZE; i++) {
        enc_buf[i] = i;
    }

    encobj.dt = enc_buf;
    encobj.maxlen = sizeof(enc_buf);
    ret = frag_enc(&encobj, enc_buf, FRAG_NB * FRAG_SIZE, FRAG_SIZE, FRAG_CR);
    printf("enc ret %d, maxlen %d\n", ret, encobj.maxlen);
    frag_encobj_log(&encobj);

    dynamic_valid_data_map = (uint8_t *)malloc(encobj.num + encobj.cr);
    frag_generate_valid_data_map(dynamic_valid_data_map, encobj.num + encobj.cr, FRAG_PER);

    printf("\n\n-------------------\n");
    decobj.cfg.dt = dec_buf;
    decobj.cfg.maxlen = sizeof(dec_buf);
    decobj.cfg.nb = FRAG_NB;
    decobj.cfg.size = FRAG_SIZE;
    decobj.cfg.tolerence = FRAG_TOLERENCE;
    decobj.cfg.frd_func = flash_read;
    decobj.cfg.fwr_func = flash_write;
    len = frag_dec_init(&decobj);
    printf("memory cost: %d, nb %d, size %d, tol %d\n",
           len,
           decobj.cfg.nb,
           decobj.cfg.size,
           decobj.cfg.tolerence);

    printf("encobj.line addr : %x\n", encobj.line);
    printf("decobj cfg size is %d\n", decobj.cfg.size);
    printf("encobj.num is : %d & encobj.cr is %d\n", encobj.num, encobj.cr );
    printf("last data dec can access is at addr %x\n", encobj.line + ((encobj.num + encobj.cr) - 1) * decobj.cfg.size);
#if 1
    for (i = 0; i < (encobj.num + encobj.cr); i++) {
        if (dynamic_valid_data_map[i] == 1) {
            printf("Index %d lost, addr would have been %x \n", i, encobj.line + i * decobj.cfg.size);
            continue;
        }
        printf(" addr access is :: %x\n",encobj.line + i * decobj.cfg.size );
        ret = frag_dec(&decobj, i + 1, encobj.line + i * decobj.cfg.size, decobj.cfg.size);
        if (ret == FRAG_DEC_ONGOING) {
            //printf("\n");
        } else if (ret >= 0) {
            printf("dec complete (reconstruct %d packets)\n", ret);
            break;
        } else {
            printf("dec error %d\n", ret);
            break;
        }
    }
#else
    for (i = 0; i < (encobj.num+1); i++) {
        printf("trying to decode %d index\n", i);
        if (dynamic_valid_data_map[i] == 1) {
            printf("Index %d lost\n", i);
            continue;
        }
        ret = frag_dec(&decobj, i + 1, encobj.line + i * decobj.cfg.size, decobj.cfg.size);
        if (ret == FRAG_DEC_ONGOING) {
            //printf("\n");
        } else if (ret >= 0) {
            printf("dec complete (reconstruct %d packets)\n", ret);
            break;
        } else {
            printf("dec error %d\n", ret);
            break;
        }
    }
    for (i = 0; i < (encobj.num); i++) {
        if (dynamic_valid_data_map[i] == 1) {
            printf("Index %d lost\n", i);
            continue;
        }
        ret = frag_dec(&decobj, i + 1, encobj.line + i * decobj.cfg.size, decobj.cfg.size);
        if (ret == FRAG_DEC_ONGOING) {
            //printf("\n");
        } else if (ret >= 0) {
            printf("dec complete (reconstruct %d packets)\n", ret);
            break;
        } else {
            printf("dec error %d\n", ret);
            break;
        }
    }
#endif




#if 1
    frag_dec_log(&decobj);
#endif
    if (memcmp(dec_flash_buf, enc_buf, FRAG_NB * FRAG_SIZE) == 0) {
        printf("decode ok (lost: %d)\n", decobj.lost_frm_count);
    } else {
        printf("decode ng (lost: %d)\n", decobj.lost_frm_count);
    }
    return 0;
}

void test(void)
{
    int x, y, m;
    int oft;
    m = 10;
    for (y = 0; y < m; y++) {
        for (x = 0; x < m; x++) {
            oft = m2t_map(x, y, m);
            if (oft >= 0) {
                printf("(%d, %d) -> %d\n", x, y, oft);
            }
        }
    }
    exit(0);
}

int main()
{
    int i;

    //test();

    clock_t t1 = clock();
    for (i = 0; i < LOOP_TIMES; i++) {
        process();
    }
    fprintf(stderr, "CPU time used %.2f ms\n", 1000.0*(clock()-t1)/CLOCKS_PER_SEC/LOOP_TIMES);
    fprintf(stderr, "Total %.2f ms\n", 1000.0*(clock()-t1)/CLOCKS_PER_SEC);
    return 0;
}

int flash_write(uint32_t addr, uint8_t *buf, uint32_t len)
{
    memcpy(dec_flash_buf + addr, buf, len);
    return 0;
}

int flash_read(uint32_t addr, uint8_t *buf, uint32_t len)
{
    memcpy(buf, dec_flash_buf + addr, len);
    return 0;
}

void putbuf(uint8_t *buf, int len)
{
    int i;
    for (i = 0; i < len; i++) {
        printf("%02X ", buf[i]);
    }
    printf("\n");
}

void put_bool_buf(uint8_t *buf, int len)
{
    int i;
    for (i = 0; i < len; i++) {
        printf("%d ", buf[i]);
    }
    printf("\n");
}

void frag_encobj_log(frag_enc_t *encobj)
{

    int i;

    printf("addr of uncoded blocks : %x\n", encobj->line);
    printf("uncoded blocks:\n");
    for (i = 0; i < encobj->num * encobj->unit; i += encobj->unit) {
        printf(" %x:\t", encobj->line + i);
        putbuf(encobj->line + i, encobj->unit);
    }

    printf("\naddr of coded blocks : %x\n", encobj->rline);
    printf("coded blocks:\n");
    for (i = 0; i < encobj->cr * encobj->unit; i += encobj->unit) {
        printf(" %x:\t", encobj->rline + i);
        putbuf(encobj->rline + i, encobj->unit);
    }

    printf("\n addr of mline blocks : %x\n", encobj->mline);
    printf("\nmatrix line:\n");
    for (i = 0; i < encobj->num * encobj->cr; i += encobj->num) {
        put_bool_buf(encobj->mline + i, encobj->num);
    }
}

void frag_generate_valid_data_map(uint8_t *map, int len, float r)
{
    int lost, index;

    lost = (int)(len * r);
    printf("len is %d\n", len);
    printf("r is %f\n", r);
    printf("lost is %d\n", lost);
    memset(map, 0, len);
    while (lost--) {
        while (1) {
            index = rand() % len;
            if (map[index] == 0) {
                map[index] = 1;
                break;
            }
        }
    }
}

