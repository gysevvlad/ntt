#include "ntt/sockaddr.h"
#include "ntt/ntt.hpp"

#include <boost/asio.hpp>
#include <gtest/gtest.h>

class SockaddrTest : public testing::Test { };

TEST_F(SockaddrTest, CommonIpv6)
{
    auto* addr = ntt_sockaddr_create_from_ipv6_and_port("fe80::1ff:fe23:4567:890a", 1234);
    ASSERT_NE(addr, nullptr);
    ASSERT_EQ(ntt_sockaddr_format_to(addr, nullptr, 0), 31);
    ASSERT_EQ(std::string { "[fe80::1ff:fe23:4567:890a]:1234" }, ntt::to_string(addr));
    ntt_sockaddr_delete(addr);
}

TEST_F(SockaddrTest, CommonIpv4)
{
    auto* addr = ntt_sockaddr_create_from_ipv4_and_port("0.0.0.1", 1234);
    ASSERT_NE(addr, nullptr);
    ASSERT_EQ(ntt_sockaddr_format_to(addr, nullptr, 0), 12);
    ASSERT_EQ(std::string { "0.0.0.1:1234" }, ntt::to_string(addr));
    ntt_sockaddr_delete(addr);
}
