//
// Created by root on 2020/3/26.
//

#ifndef KUI_ELF_32_H
#define KUI_ELF_32_H

#include <elf.h>
#include <kernel.h>
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

bool AddSectionHeader32(Elf32 *elf32);

bool CreateSignSection32(Elf32 *elf32, Elf32_Shdr *signSection);

bool AddSectionName32(Elf32 *elf32);

bool UpdateShstrtabSize32(Elf32 *elf32);

bool UpdateShnum32(Elf32 *elf32);

bool HashText32(Elf32 *elf32);

void Destract32(Elf32 *elf32);

#endif //ELFSIGN_ELF_32_H


#endif //KUI_ELF_32_H
