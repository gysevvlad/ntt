#include "./list.h"

#include <gtest/gtest.h>

struct test_node {
    ntt_node_t node;
    int v;
};

TEST(ListTest, Init)
{
    ntt_list_t list;
    ntt_list_init(&list);

    ASSERT_EQ(list.sentinel.next, &list.sentinel);
    ASSERT_EQ(list.sentinel.prev, &list.sentinel);

    ASSERT_EQ(ntt_list_empty(&list), 1);
}

TEST(ListTest, PushBack)
{
    ntt_list_t list;
    ntt_list_init(&list);

    test_node n1 {};
    n1.v = 1;
    ntt_list_push_back(&list, &n1.node);

    ASSERT_EQ(list.sentinel.next, &n1.node);
    ASSERT_EQ(list.sentinel.prev, &n1.node);
    ASSERT_EQ(n1.node.next, &list.sentinel);
    ASSERT_EQ(n1.node.prev, &list.sentinel);
}

TEST(ListTest, PushBackPushBack)
{
    ntt_list_t list;
    ntt_list_init(&list);

    test_node n1 {};
    n1.v = 1;
    ntt_list_push_back(&list, &n1.node);

    test_node n2 {};
    n2.v = 2;
    ntt_list_push_back(&list, &n2.node);

    ASSERT_EQ(list.sentinel.next, &n1.node);
    ASSERT_EQ(n1.node.next, &n2.node);
    ASSERT_EQ(n2.node.next, &list.sentinel);

    ASSERT_EQ(list.sentinel.prev, &n2.node);
    ASSERT_EQ(n2.node.prev, &n1.node);
    ASSERT_EQ(n1.node.prev, &list.sentinel);
}

TEST(ListTest, PushBackPushBackNext)
{
    ntt_list_t list;
    ntt_list_init(&list);

    test_node n1 {};
    n1.v = 1;
    ntt_list_push_back(&list, &n1.node);

    ASSERT_EQ(ntt_list_front(&list), &n1.node);

    test_node n2 {};
    n2.v = 2;
    ntt_list_push_back(&list, &n2.node);

    ASSERT_EQ(ntt_list_front(&list), &n1.node);
    ASSERT_EQ(ntt_list_next(&list), &n2.node);
    ASSERT_EQ(ntt_list_next(&list), nullptr);

    ASSERT_EQ(list.sentinel.next, &list.sentinel);
    ASSERT_EQ(list.sentinel.prev, &list.sentinel);
}
