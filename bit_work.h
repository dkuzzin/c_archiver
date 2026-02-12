//
// Created by danil on 23.04.2025.
//

#ifndef BIT_WORK_H
#define BIT_WORK_H
typedef struct bit_s {
    unsigned char buffer;
    int count;
} Bit;
void write_bit(Bit *b, int bit, FILE *out);
void flush_bits(Bit *b, FILE *out);
int read_bit(Bit *r, FILE *in);
#endif //BIT_WORK_H
