#pragma once
#include <stddef.h>
#include <stdbool.h>

bool File_ReadAllBytes(const char* path, unsigned char** out_data, size_t* out_size);
void File_Free(void* p);
