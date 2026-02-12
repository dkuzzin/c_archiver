#include <stdio.h>

typedef struct bit_s {
    unsigned char buffer;
    int count;
} Bit;

void write_bit(Bit *b, int bit, FILE *out) {
    b->buffer |= (bit & 1) << (7 - b->count);
    b->count++;
    if (b->count == 8) {
        fputc(b->buffer, out);
        b->buffer = 0;
        b->count = 0;
    }
}

void flush_bits(Bit *b, FILE *out) {
    while (b->count > 0 && b->count < 8)
        write_bit(b, 0, out);
}
int read_bit(Bit *r, FILE *in) {
    if (r->count == 0) {
        if (fread(&r->buffer, 1, 1, in) != 1) {
            fprintf(stderr, "Unexpected EOF\n");
            return 0;
        }
        r->count = 8;
    }
    return (r->buffer >> --r->count) & 1;
}