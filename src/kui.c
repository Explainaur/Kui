#define pr_fmt(fmt) "Kui: " fmt

#include <linux/slab.h>
#include <linux/ftrace.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/binfmts.h>
#include <linux/linkage.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/kallsyms.h>

#include "../include/sign.h"
#include "../include/config.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("dyf");
MODULE_DESCRIPTION("Verifying the ELF's sign");
MODULE_VERSION("0.1");


#define USE_FENTRY_OFFSET 0

/**
 * struct ftrace_hook - describes a single hook to install
 *
 * @name:     name of the function to hook
 *
 * @function: pointer to the function to execute instead
 *
 * @original: pointer to the location where to save a pointer
 *            to the original function
 *
 * @address:  kernel address of the function entry
 *
 * @ops:      ftrace_ops state for this function hook
 *
 * The user should fill in only &name, &hook, &orig fields.
 * Other fields are considered implementation details.
 */
struct ftrace_hook {
    const char *name;
    void *function;
    void *original;

    unsigned long address;
    struct ftrace_ops ops;
};

static int fh_resolve_hook_address(struct ftrace_hook *hook)
{
    hook->address = kallsyms_lookup_name(hook->name);

    if (!hook->address) {
        pr_debug("unresolved symbol: %s\n", hook->name);
        return -ENOENT;
    }

#if USE_FENTRY_OFFSET
    *((unsigned long*) hook->original) = hook->address + MCOUNT_INSN_SIZE;
#else
    *((unsigned long*) hook->original) = hook->address;
#endif

    return 0;
}

static void notrace fh_ftrace_thunk(unsigned long ip, unsigned long parent_ip,
                                    struct ftrace_ops *ops, struct pt_regs *regs)
{
    struct ftrace_hook *hook = container_of(ops, struct ftrace_hook, ops);

#if USE_FENTRY_OFFSET
    regs->ip = (unsigned long) hook->function;
#else
    if (!within_module(parent_ip, THIS_MODULE))
        regs->ip = (unsigned long) hook->function;
#endif
}

/**
 * fh_install_hooks() - register and enable a single hook
 * @hook: a hook to install
 *
 * Returns: zero on success, negative error code otherwise.
 */
int fh_install_hook(struct ftrace_hook *hook)
{
    int err;

    err = fh_resolve_hook_address(hook);
    if (err)
        return err;

    /*
     * We're going to modify %rip register so we'll need IPMODIFY flag
     * and SAVE_REGS as its prerequisite. ftrace's anti-recursion guard
     * is useless if we change %rip so disable it with RECURSION_SAFE.
     * We'll perform our own checks for trace function reentry.
     */
    hook->ops.func = fh_ftrace_thunk;
    hook->ops.flags = FTRACE_OPS_FL_SAVE_REGS
                      | FTRACE_OPS_FL_RECURSION_SAFE
                      | FTRACE_OPS_FL_IPMODIFY;

    err = ftrace_set_filter_ip(&hook->ops, hook->address, 0, 0);
    if (err) {
        pr_debug("ftrace_set_filter_ip() failed: %d\n", err);
        return err;
    }

    err = register_ftrace_function(&hook->ops);
    if (err) {
        pr_debug("register_ftrace_function() failed: %d\n", err);
        ftrace_set_filter_ip(&hook->ops, hook->address, 1, 0);
        return err;
    }

    return 0;
}

/**
 * fh_remove_hooks() - disable and unregister a single hook
 * @hook: a hook to remove
 */
void fh_remove_hook(struct ftrace_hook *hook)
{
    int err;

    err = unregister_ftrace_function(&hook->ops);
    if (err) {
        pr_debug("unregister_ftrace_function() failed: %d\n", err);
    }

    err = ftrace_set_filter_ip(&hook->ops, hook->address, 1, 0);
    if (err) {
        pr_debug("ftrace_set_filter_ip() failed: %d\n", err);
    }
}

/**
 * fh_install_hooks() - register and enable multiple hooks
 * @hooks: array of hooks to install
 * @count: number of hooks to install
 *
 * If some hooks fail to install then all hooks will be removed.
 *
 * Returns: zero on success, negative error code otherwise.
 */
int fh_install_hooks(struct ftrace_hook *hooks, size_t count)
{
    int err;
    size_t i;

    for (i = 0; i < count; i++) {
        err = fh_install_hook(&hooks[i]);
        if (err)
            goto error;
    }

    return 0;

    error:
    while (i != 0) {
        fh_remove_hook(&hooks[--i]);
    }

    return err;
}

/**
 * fh_remove_hooks() - disable and unregister multiple hooks
 * @hooks: array of hooks to remove
 * @count: number of hooks to remove
 */
void fh_remove_hooks(struct ftrace_hook *hooks, size_t count)
{
    size_t i;

    for (i = 0; i < count; i++)
        fh_remove_hook(&hooks[i]);
}

#ifndef CONFIG_X86_64
#error Currently only x86_64 architecture is supported
#endif

#if defined(CONFIG_X86_64) && (LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0))
#define PTREGS_SYSCALL_STUBS 1
#endif

/*
 * Tail call optimization can interfere with recursion detection based on
 * return address on the stack. Disable it to avoid machine hangups.
 */
#if !USE_FENTRY_OFFSET
#pragma GCC optimize("-fno-optimize-sibling-calls")
#endif

static char* checkBinary(const char* filename) {
    const char * pos;
    const char *dir[] = {"/usr/sbin", "/usr/bin", 
                "/usr/local/bin", "/bin",
                "/sbin", "/usr", "/lib", "ELFSign"};
    for (int i=0; i < ARRAY_SIZE(dir); i++) {
        pos = strstr(filename, dir[i]);
        if (pos != NULL) return true;
    }
    return false;
}

static char* get_absolute_path(const struct path * f_path) {
    char *absolutePath;
    char *retAddr = NULL;
    absolutePath = kmalloc(512, GFP_KERNEL);
    memset(absolutePath, 0, 512);
    if (f_path == NULL) {
        pr_err("f_path is NULL");
        kfree(absolutePath);
        return NULL;
    }
    retAddr = d_path(f_path, absolutePath, 512);
    if (IS_ERR(retAddr)) {
        pr_err("Get Path Error");
        kfree(absolutePath);
        return NULL;
    }
    return retAddr;
}

// static int (*real_load_elf_binary)(struct linux_binprm *bprm);

static  int (*real_sys_execve)(struct linux_binprm *bprm);

static int fh_load_elf_binary(struct linux_binprm *bprm) {
    int ret = 0;
    bool isSysBin, checkResult;
    const char * filename = bprm->filename;
    struct file *fd;
    pr_info("%s", filename);

    isSysBin = checkBinary(filename);
    if (isSysBin) {
        pr_info("System ELF %s, pass", filename);
        ret = real_sys_execve(bprm);
        pr_info("%s(%p) = %d\n", __func__, bprm, ret);
        return ret;
    }

    filename = get_absolute_path(&(bprm->file->f_path));
    pr_info("Normal ELF: %s, checking!", filename);

    fd = file_open(filename, O_RDONLY, 0);
    if (fd != NULL) {
        pr_info("File %s exisit", filename);
        file_close(fd);
        checkResult = CheckSign("/sbin/check", filename);
        if (checkResult != 1) {
            pr_info("Sign check failed, will not execute");
            return 0;
        } else {
            pr_info("Sign check success!");
        }
    } else {
        pr_info("Can't found ELF file");
    }

    ret = real_sys_execve(bprm);
    pr_info("%s(%p) = %d\n", __func__, bprm, ret);

    return 0;
}


/*
 * x86_64 kernels have a special naming convention for syscall entry points in newer kernels.
 * That's what you end up with if an architecture has 3 (three) ABIs for system calls.
 */
#ifdef PTREGS_SYSCALL_STUBS
#define SYSCALL_NAME(name) ("__x64_" name)
#else
#define SYSCALL_NAME(name) (name)
#endif

#define HOOK(_name, _function, _original)       \
        {                                       \
                .name = SYSCALL_NAME(_name),    \
                .function = (_function),        \
                .original = (_original),        \
        }

static struct ftrace_hook demo_hooks[] = {
//        HOOK("sys_clone",  fh_sys_clone,  &real_sys_clone),
        // HOOK("sys_execve", fh_sys_execve, &real_sys_execve),
        HOOK("load_elf_binary", fh_load_elf_binary, &real_sys_execve),
        
};

static int fh_init(void)
{
    int err;

    // Make sign dir
    make_dir("/tmp/sign/");    

    err = fh_install_hooks(demo_hooks, ARRAY_SIZE(demo_hooks));
    if (err)
        return err;

    pr_info("module loaded\n");

    return 0;
}
module_init(fh_init);

static void fh_exit(void)
{
    fh_remove_hooks(demo_hooks, ARRAY_SIZE(demo_hooks));

    // Remove sign dir
    rm_dir("/tmp/sign/");

    pr_info("module unloaded\n");
}
module_exit(fh_exit);
