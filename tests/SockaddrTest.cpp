#include "ntt/sockaddr.h"
#include "ntt/ntt.hpp"
#include "ntt/view.h"

#include <boost/asio.hpp>
#include <gtest/gtest.h>

class SockaddrTest : public testing::Test { };

TEST_F(SockaddrTest, CommonIpv6)
{
    auto addr = ntt_sockaddr_make_from_view(
        ntt_view_from_cstr("[fe80::1ff:fe23:4567:890a]:1234"));
    ASSERT_NE(addr, nullptr);
    ASSERT_EQ(ntt_sockaddr_formatted_size(addr), 31);
    std::string buffer;
    ASSERT_EQ(std::string { "[fe80::1ff:fe23:4567:890a]:1234" }, ntt::to_string(addr));
    ntt_sockaddr_release(addr);
}

TEST_F(SockaddrTest, CommonIpv4)
{
    auto addr = ntt_sockaddr_make_from_view(ntt_view_from_cstr("0.0.0.1:1234"));
    ASSERT_NE(addr, nullptr);
    ASSERT_EQ(ntt_sockaddr_formatted_size(addr), 12);
    std::string buffer;
    ASSERT_EQ(std::string { "0.0.0.1:1234" }, ntt::to_string(addr));
    ntt_sockaddr_release(addr);
}

TEST_F(SockaddrTest, CommonUds)
{
    auto addr = ntt_sockaddr_make_from_view(
        ntt_view_from_cstr("/tmp/9Lq7BNBnBycd6nxy.socket"));
    ASSERT_NE(addr, nullptr);
    ASSERT_EQ(ntt_sockaddr_formatted_size(addr), 28);
    std::string buffer;
    ASSERT_EQ(std::string { "/tmp/9Lq7BNBnBycd6nxy.socket" }, ntt::to_string(addr));
    ntt_sockaddr_release(addr);
}

TEST_F(SockaddrTest, BindSocketUds)
{
    auto addr = ntt_sockaddr_make_from_view(
        ntt_view_from_cstr("/tmp/9Lq7BNBnBycd6nxy.socket"));
    ASSERT_TRUE(ntt_create_and_bind_socket(addr) == 1);
    ntt_sockaddr_release(addr);
}
TEST_F(SockaddrTest, BindSocketIpv4)
{
    auto addr = ntt_sockaddr_make_from_view(
        ntt_view_from_cstr("0.0.0.0:43222"));
    ASSERT_TRUE(ntt_create_and_bind_socket(addr) == 1);
    ntt_sockaddr_release(addr);
}
TEST_F(SockaddrTest, BindSocketIpv6)
{
    auto addr = ntt_sockaddr_make_from_view(
        ntt_view_from_cstr("[::]:49156"));
    ASSERT_TRUE(ntt_create_and_bind_socket(addr) == 1);
    ntt_sockaddr_release(addr);
}