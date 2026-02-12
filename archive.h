//
// Created by danil on 22.04.2025.
//
#include <stdio.h>

#ifndef ARCHIVE_H
#define ARCHIVE_H
void add_files(char* arc_name, int file_count, char* file_names);
void extract_files(const char* archive_name, int target_count, char* targets[], char* outputs[]);
void list_files(FILE* archive);
void test_archive(const char* archive_name);
void remove_files(char* arc_name, int rm_count, char* remove_list[]);
#endif //ARCHIVE_H
