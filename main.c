#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "huffman.h"
#include "archive.h"

void print_help(void) {
    printf("\n***-------- HELP ---------***\n\n");
    printf("archive -a archive.harc input_file1 input_file2 ...\n");
    printf("archive -x archive.harc file1 file2 ... -o out1 out2 ...\n");
    printf("archive -d archive.harc file1 file2 ...\n");
    printf("archive -l archive.harc\n");
    printf("archive -t archive.harc\n");
    printf("archive -h\n");
    printf("\n***------Enjoy using it!------***\n");
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_help();
        return 1;
    }
    if (argv[1][0] != '-' || strlen(argv[1]) != 2) {
        fprintf(stderr, "Invalid mode format.\n");
        print_help();
        return 1;
    }
    char mode = argv[1][1];

    if (mode == 'a') {
        if (argc < 4) {
            fprintf(stderr, "Error: expected archive and input files.\n");
            return 1;
        }
        clock_t start = clock();
        add_files(argv[2], argc - 3, &argv[3]);
        clock_t end = clock();
        double time_spent = (double)(end - start) / CLOCKS_PER_SEC;
        printf("Compressed into archive '%s'\n", argv[2]);
        printf("Compression time: %.4f sec\n", time_spent);

    } else if (mode == 'x') {
        if (argc < 6) {
            fprintf(stderr, "Too few arguments for extraction.\n");
            return 1;
        }
        char* archive = argv[2];
        int o_index = -1;
        for (int i = 3; i < argc; ++i) {
            if (strcmp(argv[i], "-o") == 0) {
                o_index = i;
                break;
            }
        }
        if (o_index == -1 || o_index == 3 || o_index == argc - 1) {
            fprintf(stderr, "Usage: harc -x archive.harc file1 file2 ... -o out1 out2 ...\n");
            return 1;
        }
        int target_count = o_index - 3;
        int output_count = argc - o_index - 1;
        if (target_count != output_count) {
            fprintf(stderr, "Number of input and output files must match.\n");
            return 1;
        }
        char** targets = &argv[3];
        char** outputs = &argv[o_index + 1];
        extract_files(archive, target_count, targets, outputs);

    } else if (mode == 'd') {
        if (argc < 4) {
            fprintf(stderr, "Usage: harc -d archive.harc file1 file2 ...\n");
            return 1;
        }
        remove_files(argv[2], argc - 3, &argv[3]);

    } else if (mode == 'l') {
        if (argc != 3) {
            fprintf(stderr, "Usage: harc -l archive.harc\n");
            return 1;
        }
        FILE* arc = fopen(argv[2], "rb");
        if (!arc) {
            perror("fopen");
            return 1;
        }
        list_files(arc);
        fclose(arc);

    } else if (mode == 't') {
        if (argc != 3) {
            fprintf(stderr, "Usage: harc -t archive.harc\n");
            return 1;
        }
        test_archive(argv[2]);

    } else if (mode == 'h') {
        print_help();
    } else {
        fprintf(stderr, "Unknown mode: %c\n", mode);
        print_help();
        return 1;
    }
    return 0;
}
