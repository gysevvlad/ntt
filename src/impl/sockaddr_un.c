#include "ntt/impl/sockaddr_un.h"

#include <sys/socket.h>

#include <string.h>

int ntt_sockaddr_un_from_view(struct sockaddr_un* self, ntt_view_t view)
{
    if (0 < view.len && view.len < 108 && view.str[0] == '/') {
        self->sun_family = AF_UNIX;
        memcpy(self->sun_path, view.str, view.len);
        self->sun_path[view.len] = '\0';
        return 1;
    }
    return 0;
}

size_t ntt_sockaddr_un_formatted_size(struct sockaddr_un* self)
{
    return strlen(self->sun_path);
}

ntt_char_span_t ntt_sockaddr_un_format_to(struct sockaddr_un* self,
    ntt_char_span_t buffer)
{
    int len = strlen(self->sun_path);
    if (len > buffer.len) {
        len = buffer.len;
    }
    memcpy(buffer.ptr, self->sun_path, len);
    buffer.len -= len;
    buffer.ptr += len;
    return buffer;
}
