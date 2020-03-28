//
// Created by root on 2020/3/21.
//

#ifndef ELFSIGN_ELF_32_H
#define ELFSIGN_ELF_32_H

#include <elf.h>
#include <stdio.h>
#include <apue.h>
#include <stdbool.h>
#include <openssl/sha.h>

typedef struct {
    long int size;
    char *path;
    Elf32_Ehdr ehdr;
    Elf32_Shdr shstrtabhdr;
    char *shstrtab;
    unsigned char digest[SHA_DIGEST_LENGTH];
    unsigned char sign[256];
} Elf32;


bool IsELF32(const char *file);

void SetElf32Path(Elf32 *elf32, const char *path);

bool GetEhdr32(Elf32 *elf32);

bool Getshstrtabhdr32(Elf32 *elf32);

bool Getshstrtab32(Elf32 *elf32);

int GetFileSize32(Elf32 *elf32);

bool HashText32(Elf32 *elf32);

void Destract32(Elf32 *elf32);

#endif //ELFSIGN_ELF_32_H
