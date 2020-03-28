//
// Created by root on 2020/3/21.
//

#ifndef ELFSIGN_ELF_64_H
#define ELFSIGN_ELF_64_H

#include <elf.h>
#include <stdio.h>
#include <apue.h>
#include <stdbool.h>
#include <openssl/sha.h>

typedef struct {
    long int size;
    char *path;
    Elf64_Ehdr ehdr;
    Elf64_Shdr shstrtabhdr;
    char *shstrtab;
    unsigned char digest[SHA_DIGEST_LENGTH];
    unsigned char sign[256];
} Elf64;


bool IsELF64(const char *file);

void SetElf64Path(Elf64 *elf64, const char *path);

bool GetEhdr64(Elf64 *elf64);

bool Getshstrtabhdr64(Elf64 *elf64);

bool Getshstrtab64(Elf64 *elf64);

int GetFileSize64(Elf64 *elf64);

bool HashText64(Elf64 *elf64);

void Destract64(Elf64 *elf64);

#endif //ELFSIGN_ELF_64_H
