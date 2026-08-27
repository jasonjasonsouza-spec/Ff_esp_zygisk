#pragma once
#include <cstdint>
#ifdef __cplusplus
extern "C" {
#endif
#define A64_MAX_BACKUPS 256
void A64HookFunction(void* target, void* newFunc, void** original);
#ifdef __cplusplus
}
#endif
