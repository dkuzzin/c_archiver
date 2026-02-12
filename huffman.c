#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "bit_work.h"

#define byte_size 256

typedef struct list_s {
    int prob, symb;
    struct list_s *left, *right;
} HuffNode;

typedef struct data_s {
    int count;
    int symbol;
} Freq;

static void free_tree(HuffNode *n) {
    if (!n) return;
    free_tree(n->left);
    free_tree(n->right);
    free(n);
}

static int cmp_data(const void *a, const void *b) {
    Freq *da = (Freq *)a, *db = (Freq *)b;
    return (da->count == db->count) ? da->symbol - db->symbol : -(da->count - db->count);
}

static int cmp_list(const void *a, const void *b) {
    HuffNode *na = *(HuffNode **)a, *nb = *(HuffNode **)b;
    return (na->prob == nb->prob) ? na->symb - nb->symb : -(na->prob - nb->prob);
}

static Freq *count_frequencies(FILE *input, int *uniq) {
    Freq *freqs = calloc(byte_size, sizeof(Freq));
    unsigned char byte;

    while (fread(&byte, 1, 1, input) == 1) {
        if (freqs[byte].count == 0) {
            (*uniq)++;
            freqs[byte].symbol = byte;
        }
        freqs[byte].count++;
    }

    qsort(freqs, byte_size, sizeof(Freq), cmp_data);
    rewind(input);
    return freqs;
}

static HuffNode *build_huffman_tree(Freq *freqs, int uniq) {
    HuffNode *nodes[byte_size];
    int size = 0;

    for (int i = 0; i < byte_size; ++i) {//Листья дерева
        if (freqs[i].count > 0) {
            HuffNode *node = malloc(sizeof(HuffNode));
            node->prob = freqs[i].count;
            node->symb = freqs[i].symbol;
            node->left = node->right = NULL;
            nodes[size++] = node;
        }
    }

    qsort(nodes, size, sizeof(HuffNode *), cmp_list);

    while (size > 1) {

        HuffNode *left = nodes[size - 2], *right = nodes[size - 1];

        HuffNode *parent = malloc(sizeof(HuffNode));
        parent->prob = left->prob + right->prob;
        parent->symb = 0;
        parent->left = left;
        parent->right = right;
        size--;

        int pos = 0;
        while (pos < size && nodes[pos]->prob > parent->prob) pos++;
        for (int i = size; i > pos; i--)
            nodes[i] = nodes[i - 1];
        nodes[pos] = parent;
    }

    return size == 1 ? nodes[0] : NULL;
}


static void build_codes(HuffNode *node, char *code, int depth, char *codes[byte_size]) {
    if (!node) return;
    if (!node->left && !node->right) {
        code[depth] = '\0';
        codes[node->symb] = strdup(code);
        return;
    }
    code[depth] = '0'; build_codes(node->left, code, depth + 1, codes);
    code[depth] = '1'; build_codes(node->right, code, depth + 1, codes);
}

static void write_tree(HuffNode *node, Bit *b, FILE *out) {
    if (!node) return;

    if (!node->left && !node->right) {
        write_bit(b, 1, out);
        for (int i = 7; i >= 0; i--)
            write_bit(b, (node->symb >> i) & 1, out);
    } else {
        write_bit(b, 0, out);
        write_tree(node->left, b, out);
        write_tree(node->right, b, out);
    }
}


void huffman_compress(FILE *input, FILE *output) {
    int uniq = 0;
    Freq *freqs = count_frequencies(input, &uniq);
    HuffNode *tree = build_huffman_tree(freqs, uniq);

    fwrite("HARC", 1, 4, output);

    fseek(input, 0, SEEK_END);
    int filesize = ftell(input);
    rewind(input);
    for (int i = 0; i < 4; ++i)
        fputc((filesize >> (8 * i)) & 0xFF, output);
    fputc(uniq & 0xFF, output);


    Bit b = {0, 0};
    write_tree(tree, &b, output);
    flush_bits(&b, output);

    char *codes[byte_size] = {0}, tmp[byte_size] = {0};
    build_codes(tree, tmp, 0, codes);

    if (uniq == 1 && codes[tree->symb][0] == '\0') {
        free(codes[tree->symb]);
        codes[tree->symb] = strdup("0");
    }

    unsigned char byte;
    while (fread(&byte, 1, 1, input) == 1) {
        char *s = codes[byte];
        for (int i = 0; s[i]; ++i)
            write_bit(&b, s[i] == '1', output);
    }
    flush_bits(&b, output);

    for (int i = 0; i < byte_size; ++i)
        free(codes[i]);
    free(freqs);
}



static HuffNode *read_tree(Bit *r, FILE *in) {
    int b = read_bit(r, in);
    if (b == 1) {
        int sym = 0;
        for (int i = 7; i >= 0; --i)
            sym |= read_bit(r, in) << i;
        HuffNode *leaf = malloc(sizeof(HuffNode));
        leaf->symb = sym;
        leaf->left = leaf->right = NULL;
        return leaf;
    } else {
        HuffNode *n = malloc(sizeof(HuffNode));
        n->symb = -1;
        n->left = read_tree(r, in);
        n->right = read_tree(r, in);
        return n;
    }
}

void huffman_decompress(FILE *input, FILE *output) {
    char magic[4];
    fread(magic, 1, 4, input);
    if (memcmp(magic, "HARC", 4) != 0) {
        fprintf(stderr, "Invalid HARC archive\n");
        exit(1);
    }

    uint32_t length = 0;
    for (int i = 0; i < 4; ++i)
        length |= (uint32_t)fgetc(input) << (8 * i);
    fgetc(input);

    Bit r = {0, 0};
    HuffNode *tree = read_tree(&r, input);
    r.count = 0;

    if (!tree->left && !tree->right) {
        for (uint32_t i = 0; i < length; ++i)
            fputc(tree->symb, output);
    } else {
        HuffNode *cur = tree;
        for (uint32_t got = 0; got < length;) {
            int bit = read_bit(&r, input);
            cur = bit ? cur->right : cur->left;
            if (!cur->left && !cur->right) {
                fputc(cur->symb, output);
                cur = tree;
                ++got;
            }
        }
    }

    free_tree(tree);
}
