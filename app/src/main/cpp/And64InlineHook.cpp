#include "And64InlineHook.h"
#include <dlfcn.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#define PAGE_SIZE 4096
#define PAGE_ALIGN(x) ((uintptr_t)(x) & ~(PAGE_SIZE - 1))
static struct { void* addr; uint32_t code[4]; bool used; } g_backups[A64_MAX_BACKUPS] = {};
static int Protect(void* addr, size_t sz, int prot) { return mprotect((void*)PAGE_ALIGN((uintptr_t)addr), PAGE_SIZE, prot); }
static void WriteBranch(uint32_t* addr, uintptr_t target) {
    int64_t off = ((int64_t)target - (int64_t)addr) >> 2;
    addr[0] = 0x14000000 | (off & 0x03FFFFFF);
}
void A64HookFunction(void* target, void* newFunc, void** original) {
    if (!target || !newFunc || !original) return;
    *original = target;
    int slot = -1;
    for (int i = 0; i < A64_MAX_BACKUPS; i++) { if (!g_backups[i].used) { slot = i; break; } }
    if (slot == -1) return;
    g_backups[slot].addr = target;
    g_backups[slot].used = true;
    memcpy(g_backups[slot].code, target, 16);
    Protect(target, 16, PROT_READ | PROT_WRITE | PROT_EXEC);
    WriteBranch((uint32_t*)target, (uintptr_t)newFunc);
    __builtin___clear_cache((char*)target, (char*)target + 16);
    Protect(target, 16, PROT_READ | PROT_EXEC);
}
