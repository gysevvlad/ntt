#include "ntt/impl/in_addr.h"

#include <assert.h>
#include <string.h>

#include <arpa/inet.h>

int ntt_in_addr_from_view(struct in_addr* addr, ntt_view_t str)
{
    assert(addr != NULL);
    if (str.len < INET_ADDRSTRLEN) {
        char buffer[INET_ADDRSTRLEN];
        memcpy(buffer, str.str, str.len);
        buffer[str.len] = '\0';
        if (inet_pton(AF_INET, buffer, addr) == 1) {
            return 1;
        }
    }
    return 0;
}

size_t ntt_in_addr_formatted_size(struct in_addr* addr)
{
    assert(addr != NULL);
    char buffer[INET_ADDRSTRLEN];
    const char* dst = inet_ntop(AF_INET, addr, buffer, INET_ADDRSTRLEN);
    assert(dst != NULL);
    return strlen(buffer);
}

ntt_char_span_t ntt_in_addr_format_to(struct in_addr* addr,
    ntt_char_span_t buffer)
{
    assert(addr != NULL);
    char temp[INET_ADDRSTRLEN];
    const char* dst = inet_ntop(AF_INET, addr, temp, INET_ADDRSTRLEN);
    assert(dst != NULL);
    size_t len = strlen(dst);
    if (buffer.len < len) {
        len = buffer.len;
    }
    size_t i;
    for (i = 0; i < len; ++i) {
        buffer.ptr[i] = dst[i];
    }
    buffer.len = buffer.len - len;
    buffer.ptr = buffer.ptr + len;
    return buffer;
}
