//
// Created by root on 2020/3/26.
//


#include "../include/elf_32.h"

bool IsELF32(const char *file) {
    int ret;
    loff_t pos;
    unsigned char ident[EI_NIDENT];
    struct file *fd = file_open(file, O_RDONLY, 0);
    if (!fd) {
        pr_err("Can not open file %s", file);
        return false;
    }
    pos = fd->f_pos;
    ret = kernel_read(fd, ident, EI_NIDENT, &pos);
    file_close(fd);
    if (ret != EI_NIDENT) {
        pr_err("Read ELF magic failed!");
        return false;
    }
    if (ident[0] == 0x7f && ident[1] == 'E' && ident[2] == 'L' && ident[3] == 'F') {
        if (ident[4] == 1)
            return true;
        else
            return false;
    } else {
        return false;
    }
}


void SetElf32Path(Elf32 *elf32, const char *path) {
    int len = strlen(path);
    elf32->path = (char *) kmalloc(len, GFP_KERNEL);
    strcpy(elf32->path, path);
}

bool GetEhdr32(Elf32 *elf32) {
    if (elf32->path == NULL) {
        pr_err("ELF file not set");
        return false;
    }
    struct file *fd = file_open(elf32->path, O_RDONLY, 0);
    if (!fd) {
        pr_err("Can not open file %s", elf32->path);
        return false;
    }
    loff_t pos = fd->f_pos;
    int ret = kernel_read(fd, &elf32->ehdr, sizeof(Elf32_Ehdr), &pos);
    file_close(fd);
    if (ret != sizeof(Elf32_Ehdr)) {
        pr_err("Read ELF Header failed");
        return false;
    }
    return true;
}

bool Getshstrtabhdr32(Elf32 *elf32) {
    int offset = 0;
    if (elf32->path == NULL) {
        pr_err("ELF file not set");
        return false;
    }
    struct file *fd = file_open(elf32->path, O_RDONLY, 0);
    if (!fd) {
        pr_err("Can not open file %s", elf32->path);
        return false;
    }
    offset = elf32->ehdr.e_shoff + elf32->ehdr.e_shentsize * elf32->ehdr.e_shstrndx;
    vfs_llseek(fd, offset, SEEK_SET);
    loff_t pos = fd->f_pos;
    int ret = kernel_read(fd, &elf32->shstrtabhdr, sizeof(Elf32_Shdr), &pos);
    file_close(fd);
    if (ret != sizeof(Elf32_Shdr)) {
        pr_err("Read Section Header Table failed");
        return false;
    }
    return true;
}

bool Getshstrtab32(Elf32 *elf32) {
    if (elf32->path == NULL) {
        pr_err("ELF file not set");
        return false;
    }
    struct file *fd = file_open(elf32->path, O_RDONLY, 0);
    if (!fd) {
        pr_err("Can not open file %s", elf32->path);
        return false;
    }
    elf32->shstrtab = (char *) kmalloc(elf32->shstrtabhdr.sh_size, GFP_KERNEL);
    vfs_llseek(fd, elf32->shstrtabhdr.sh_offset, SEEK_SET);
    loff_t pos = fd->f_pos;
    int ret = kernel_read(fd, elf32->shstrtab, elf32->shstrtabhdr.sh_size, &pos);
    file_close(fd);
    if (ret != elf32->shstrtabhdr.sh_size) {
        pr_err("Read shstrtab Section failed");
        return false;
    }
    return true;
}

// Get orign file size
int GetFileSize32(Elf32 *elf32) {
    mm_segment_t fs;
    struct kstat stat;
    if (!elf32->path) {
        pr_err("ELF file not set");
        return -1;
    }

    fs = get_fs();
    set_fs(KERNEL_DS);
    vfs_stat(elf32->path, &stat);
    set_fs(fs);
    pr_info("stat size %lld", stat.size);
    elf32->size = stat.size;
    return elf32->size;
}

bool HashText32(Elf32 *elf32) {
    Elf32_Shdr tmp;
    int textOffset;
    char name[20];
    Elf32_Off sectionHeaderTable = elf32->ehdr.e_shoff;

    struct file *fd = file_open(elf32->path, O_RDONLY, 0);
    if (!fd) {
        pr_err("Open file %s failed", elf32->path);
        return false;
    }

    // Find the .text section header item
    vfs_llseek(fd, sectionHeaderTable, SEEK_SET);
    loff_t pos = fd->f_pos;
    do {
        int ret = kernel_read(fd, &tmp, sizeof(Elf32_Shdr), &pos);
        if (ret != sizeof(Elf32_Shdr)) {
            pr_err("Read section header failed");
            return false;
        }
        strcpy(name, elf32->shstrtab + tmp.sh_name);
       pr_info("Section name is %s", name);
    } while (strcmp(name, ".text"));
    if (strcmp(name, ".text")) {
        pr_err("Not found .text section");
        return false;
    }

    char *text = (char *) kmalloc(tmp.sh_size, GFP_KERNEL);
    textOffset = tmp.sh_offset;
    vfs_llseek(fd, textOffset, SEEK_SET);
    pos = fd->f_pos;
    int ret = kernel_read(fd, text, tmp.sh_size, &pos);
    file_close(fd);
    if (ret != tmp.sh_size) {
        pr_err("Read .text section failed");
        return false;
    }

    SHA1(elf32->digest, text, tmp.sh_size);

    kfree(text);
    return true;
}

void Destract32(Elf32 *elf32) {
    if (elf32->path != NULL) {
        kfree(elf32->path);
    }
    if (elf32->shstrtab != NULL) {
        kfree(elf32->shstrtab);
    }
}












