#include <stdio.h>
#include <stdlib.h>
#include "huffman.h"
#include <string.h>

typedef struct {
    char name[256];
    int name_len;
    int orig_size;
    int comp_size;
    int offset;
} FileEntry;

void write_header(FILE* archive, FileEntry* entries, int file_count) {
    fwrite("HARC", 1, 4, archive);
    fwrite(&file_count, sizeof(int), 1, archive);

    for (int i = 0; i < file_count; i++) {
        int name_len = entries[i].name_len;
        fwrite(&name_len, 1, 1, archive);
        fwrite(entries[i].name, 1, name_len, archive);
        fwrite(&entries[i].orig_size, sizeof(int), 1, archive);
        fwrite(&entries[i].comp_size, sizeof(int), 1, archive);
        fwrite(&entries[i].offset, sizeof(int), 1, archive);
    }
}

FileEntry *read_header(FILE*archive, int *out_count) {
    char indentify[4];
    fread(indentify, 1, 4, archive);
    if (memcmp(indentify, "HARC", 4) != 0) {
        fprintf(stderr, "Not a valid HARC archive\n");
        return NULL;
    }


    int count;
    if (fread(&count, sizeof(int), 1, archive) != 1 || count < 0 || count > 1000) {
        fprintf(stderr, "Invalid file count\n");
        return NULL;
    }

    FileEntry *entries = malloc(sizeof(FileEntry) * count);
    for (int i = 0; i < count; i++) {
        int name_len;
        fread(&name_len, 1, 1, archive);
        fread(entries[i].name, 1, name_len, archive);
        entries[i].name[name_len] = '\0';
        entries[i].name_len = name_len;
        fread(&entries[i].orig_size, sizeof(int), 1, archive);
        fread(&entries[i].comp_size, sizeof(int), 1, archive);
        fread(&entries[i].offset, sizeof(int), 1, archive);
    }
    *out_count = count;
    return entries;
}
void list_files(FILE* archive) {
    int count;
    FileEntry *entries = read_header(archive, &count);
    if (!entries) {
        printf("Error to scan files\n");
        return;
    }
    printf("Archive has %u file(s):\n", count);
    for (int i = 0; i < count; i++) {
        printf("[%u] %s (%u bytes) (%u compressed bytes) %u\n", i+1, entries[i].name, entries[i].orig_size, entries[i].comp_size, entries[i].orig_size);
    }
    free(entries);
}


void add_files(char* arc_name, int file_count, char** file_names) {

    FILE* temp_data = fopen("tempdata.tmp", "wb+");
    if (!temp_data) {
        perror("tmpfile");
        return;
    }

    FileEntry *entries = malloc(sizeof(FileEntry) * file_count);

    int offset = 0;
    for (int i = 0; i < file_count; i++) {
        FILE *input = fopen(file_names[i], "rb");
        if (input == NULL) {
            fprintf(stderr, "Couldn't open file: %s\n", file_names[i]);
            continue;
        }
        fseek(input, 0, SEEK_END);
        int orig_size = ftell(input);
        rewind(input);

        FILE* compressed = fopen("compressed.tmp", "wb+");
        if (compressed == NULL) {
            perror("tmpfile compressed");
            fclose(input);
            continue;
        }
        huffman_compress(input, compressed);
        fseek(compressed, 0, SEEK_END);
        int compressed_size = ftell(compressed);

        rewind(compressed);
        
        entries[i].name_len = strlen(file_names[i]);
        strncpy(entries[i].name, file_names[i], 256);
        entries[i].orig_size = orig_size;
        entries[i].comp_size = compressed_size;
        entries[i].offset = offset;

        for (int j = 0; j < compressed_size; j++) {
            int c = fgetc(compressed);
            fputc(c, temp_data);
        }
        offset += compressed_size;
        fclose(input);
        fclose(compressed);
        remove("compressed.tmp");
    }


    FILE*archive = fopen(arc_name, "wb");
    if (!archive) {
        perror("fopen archive");
        free(entries);
        fclose(temp_data);
        return;
    }

    write_header(archive, entries, file_count);
    rewind(temp_data);

    int byte;
    while ((byte = fgetc(temp_data)) != EOF) {
        fputc(byte, archive);
    }
    fclose(temp_data);
    fclose(archive);
    remove("tempdata.tmp");
    free(entries);
}

void extract_files(const char* archive_name, int target_count, char* targets[], char* outputs[]) {

    FILE* archive = fopen(archive_name, "rb");
    if (!archive) {
        perror("fopen archive");
        return;
    }

    int count;
    FileEntry* entries = read_header(archive, &count);
    long data_start = ftell(archive);
    if (!entries) {
        fclose(archive);
        return;
    }

    for (int t = 0; t < target_count; t++) { 
        int found = 0;
        for (int i = 0; i < count; i++) {
            if (strcmp(entries[i].name, targets[t]) == 0) {
                found = 1;
                fseek(archive, data_start + entries[i].offset, SEEK_SET);

                FILE* out = fopen(outputs[t], "wb");
                if (!out) {
                    perror("fopen output");
                    break;
                }
                huffman_decompress(archive, out);
                fclose(out);
                printf("Extracted: %s -> %s\n", targets[t], outputs[t]);
                break;
            }
        }
        if (!found) {
            fprintf(stderr, "File not found in archive: %s\n", targets[t]);
        }
    }
    free(entries);
    fclose(archive);
}



void remove_files(char* arc_name, int rm_count, char* remove_list[]) {
    FILE*archive = fopen(arc_name, "rb");
    if (!archive) {
        perror("fopen archive");
        return;
    }

    int count;
    FileEntry* entries = read_header(archive, &count);
    if (!entries) {
        fclose(archive);
        return;
    }

    int* found_flags = calloc(rm_count, sizeof(int));
    FileEntry* new_entries = malloc(sizeof(FileEntry) * count);

    long header_end = ftell(archive);

    int new_count = 0, offset = 0;

    FILE* temp_data = fopen("tempdata.tmp", "wb+");
    for  (int i = 0; i < count; i++) {
        int to_remove = 0;
        for (int r = 0; r < rm_count; r++) {
            if (strcmp(entries[i].name, remove_list[r]) == 0) {
                to_remove = 1;
                found_flags[r] = 1;
                break;
            }
        }
        if (to_remove) continue;
        
        fseek(archive, header_end + entries[i].offset, SEEK_SET);

        char* buffer = malloc(entries[i].comp_size);
        fread(buffer, 1, entries[i].comp_size, archive);
        fwrite(buffer, 1, entries[i].comp_size, temp_data);
        free(buffer);

        new_entries[new_count] = entries[i];
        new_entries[new_count].offset = offset;
        offset += entries[i].comp_size;
        new_count++;
    }
    fclose(archive);
    archive = fopen(arc_name, "wb");
    if (!archive) {
        perror("fopen archive (write)");
        free(entries);
        free(new_entries);
        fclose(temp_data);
        return;
    }

    write_header(archive, new_entries, new_count);
    rewind(temp_data);

    int byte;
    while ((byte = fgetc(temp_data)) != EOF) {
        fputc(byte, archive);
    }

    fclose(temp_data);
    fclose(archive);
    free(entries);
    free(new_entries);
    remove("testfile.tmp");

    
    int count_removed = 0;
    for (int r = 0; r < rm_count; r++) {
        if (!found_flags[r]) {
            fprintf(stderr, "File '%s' not found in archive and couldn't be removed.\n", remove_list[r]);
        }else {
            count_removed += 1;
        }
    }
    if (count_removed > 0) {
        printf("Files removed successfully.\n");
    }
}


void test_archive(const char* archive_name) {
    FILE* archive = fopen(archive_name, "rb");
    if (!archive) {
        perror("fopen archive");
        return;
    }
    int count;
    FileEntry* entries = read_header(archive, &count);
    if (!entries) {
        fclose(archive);
        return;
    }
    long header_size = 4 + sizeof(int);

    for (int i = 0; i < count; i++) {
        header_size += 1 + entries[i].name_len + sizeof(int) * 3;
    }


    for (int i = 0; i < count; ++i) {
        fseek(archive, header_size + entries[i].offset, SEEK_SET);
        FILE* temp = fopen("testfile.tmp", "wb+");
        if (!temp) {
            perror("tmpfile");
            break;
        }
        printf("Checking file: %s... ", entries[i].name);

        long before = ftell(archive);
        huffman_decompress(archive, temp);
        long after = ftell(archive);
        fclose(temp);
        remove("testfile.tmp");
        int actual_size = after - before;
        if (actual_size == entries[i].comp_size) {
            printf("ok\n");
        } else {
            printf("corrupted\n");
        }
    }
    free(entries);
    fclose(archive);
}