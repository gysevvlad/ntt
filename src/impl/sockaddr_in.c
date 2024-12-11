#include "ntt/impl/sockaddr_in.h"

#include "ntt/char.h"
#include "ntt/impl/in_addr.h"
#include "ntt/util.h"

int ntt_sockaddr_in_from_view(struct sockaddr_in* addr, ntt_view_t view)
{
    ntt_view_t addr_view = view;
    ntt_view_t port_view = ntt_view_from_literal("0");

    ntt_view_split_by_char(view, ':', &addr_view, &port_view);

    unsigned short port_value;
    if (!ntt_unsigned_short_from_view(&port_value, port_view)) {
        return 0;
    }

    struct in_addr addr_value;
    if (!ntt_in_addr_from_view(&addr_value, addr_view)) {
        return 0;
    }

    addr->sin_family = AF_INET;
    addr->sin_port = htons(port_value);
    addr->sin_addr = addr_value;
    return 1;
}

size_t ntt_sockaddr_in_formatted_size(struct sockaddr_in* self)
{
    size_t len = 0;
    len += ntt_in_addr_formatted_size(&self->sin_addr);
    len += 1;
    len += ntt_unsigned_short_formatted_size(ntohs(self->sin_port));
    return len;
}

ntt_char_span_t ntt_sockaddr_in_format_to(struct sockaddr_in* self,
    ntt_char_span_t buffer)
{
    buffer = ntt_in_addr_format_to(&self->sin_addr, buffer);
    buffer = ntt_char_format_to(':', buffer);
    buffer = ntt_unsigned_short_format_to(ntohs(self->sin_port), buffer);
    return buffer;
}
