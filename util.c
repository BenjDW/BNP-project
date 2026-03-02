#include "include/util.h"
#include <string.h>

size_t ft_strlcpy(char *dst, const char *src, size_t dstsize)
{
    size_t src_len;
    size_t i;

    if (!dst || !src)
        return 0;

    src_len = strlen(src);

    if (dstsize != 0) {
        i = 0;
        while (i + 1 < dstsize && src[i] != '\0') {
            dst[i] = src[i];
            i++;
        }
        dst[i] = '\0';
    }

    return src_len;
}

