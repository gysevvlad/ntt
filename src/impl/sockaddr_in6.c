#include "ntt/impl/sockaddr_in6.h"

#include "ntt/char.h"
#include "ntt/cstr.h"
#include "ntt/impl/in6_addr.h"
#include "ntt/util.h"

#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <string.h>

int ntt_sockaddr_in6_from_view(struct sockaddr_in6 *addr, ntt_view_t view) {
  ntt_view_t head_view = view;
  ntt_view_t port_view = ntt_view_from_literal("0");

  if (view.len != 0 && view.str[0] == '[') {
    head_view.len -= 1;
    head_view.str += 1;
    if (!ntt_view_split_by_char(head_view, ']', &head_view, &port_view)) {
      return 0;
    }
    if (port_view.len != 0) {
      if (port_view.str[0] != ':') {
        return 0;
      }
      port_view.len -= 1;
      port_view.str += 1;
    } else {
      port_view.len = sizeof("0") - 1;
      port_view.str = "0";
    }
  }

  {
    // check that it's ipv6 address
    ntt_view_t addr_view = head_view;
    if (addr_view.len > 0 && addr_view.str[0] == '[') {
      addr_view.len -= 1;
      addr_view.str += 1;
    }
    if (addr_view.len > 0 && addr_view.str[addr_view.len - 1] == ']') {
      addr_view.len -= 1;
    }
    ntt_view_t zone_view = ntt_view_from_literal("");
    ntt_view_split_by_char(head_view, '%', &addr_view, &zone_view);
    struct in6_addr temp;
    if (!ntt_in6_addr_from_view(&temp, addr_view)) {
      return 0;
    }
  }

  struct addrinfo hints;
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_INET6;

  char *addr_str = ntt_cstr_from_view(head_view);
  char *port_str = ntt_cstr_from_view(port_view);
  struct addrinfo *result;
  int rc = getaddrinfo(addr_str, port_str, &hints, &result);
  ntt_cstr_free(port_str);
  ntt_cstr_free(addr_str);
  if (rc != 0) {
    return 0;
  }
  if (result->ai_family != AF_INET6) {
    return 0;
  }
  *addr = *(struct sockaddr_in6 *)(result->ai_addr);
  freeaddrinfo(result);
  return 1;
}

size_t ntt_sockaddr_in6_formatted_size(struct sockaddr_in6 *self) {
  size_t len = 0;
  len += 1; // '['
  len += ntt_in6_addr_formatted_size(&self->sin6_addr);
  len += 1; // ']'
  len += 1; // ':'
  len += ntt_unsigned_short_formatted_size(ntohs(self->sin6_port));
  return len;
}

ntt_char_span_t ntt_sockaddr_in6_format_to(struct sockaddr_in6 *self,
                                           ntt_char_span_t buffer) {
  buffer = ntt_char_format_to('[', buffer);
  buffer = ntt_in6_addr_format_to(&self->sin6_addr, buffer);
  buffer = ntt_char_format_to(']', buffer);
  buffer = ntt_char_format_to(':', buffer);
  buffer = ntt_unsigned_short_format_to(ntohs(self->sin6_port), buffer);
  return buffer;
}
