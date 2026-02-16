#include "nes/util/file.h"
#include <stdio.h>
#include <stdlib.h>

bool File_ReadAllBytes(const char* path, unsigned char** out_data, size_t* out_size)
{
    if (!out_data || !out_size) return false;
    *out_data = NULL;
    *out_size = 0;

    FILE* f = fopen(path, "rb");
    if (!f) return false;

    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return false; }
    long len = ftell(f);
    if (len < 0) { fclose(f); return false; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return false; }

    unsigned char* buf = (unsigned char*)malloc((size_t)len);
    if (!buf) { fclose(f); return false; }

    size_t rd = fread(buf, 1, (size_t)len, f);
    fclose(f);

    if (rd != (size_t)len) {
        free(buf);
        return false;
    }

    *out_data = buf;
    *out_size = (size_t)len;
    return true;
}

void File_Free(void* p)
{
    free(p);
}
