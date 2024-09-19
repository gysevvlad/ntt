#include "ntt/defs.h"

#include "./queue.h"

#include <criterion/criterion.h>

typedef struct test_node {
  struct ntt_node node;
  int val;
} test_node_t;

test_node_t *make_node(int val) {
  test_node_t *node = malloc(sizeof(struct test_node));
  node->val = val;
  return node;
}

Test(ntt_queue, pop) {
  ntt_queue_t queue;
  ntt_queue_init(&queue);
  int last;
  struct ntt_node *node = ntt_queue_pop_front(&queue, &last);
  cr_expect_null(node);
  cr_expect(last);
}

Test(ntt_queue, pop_pop) {
  ntt_queue_t queue;
  ntt_queue_init(&queue);
  cr_assert(ntt_queue_empty(&queue));

  int last;
  ntt_node_t *node = ntt_queue_pop_front(&queue, &last);
  cr_expect_null(node);
  cr_expect(last);

  node = ntt_queue_pop_front(&queue, &last);
  cr_expect_null(node);
  cr_expect(last);
}

Test(ntt_queue, push_pop) {
  ntt_queue_t queue;
  ntt_queue_init(&queue);
  cr_assert(ntt_queue_empty(&queue));

  int first;
  ntt_queue_push_back(&queue, &make_node(1)->node, &first);
  cr_assert(!ntt_queue_empty(&queue));
  cr_assert(first);

  int last;
  ntt_node_t *node = ntt_queue_pop_front(&queue, &last);
  cr_assert_not_null(node);
  cr_assert(last);
  cr_assert(ntt_queue_empty(&queue));
  cr_assert(ntt_container_of(node, test_node_t, node)->val == 1);
}

Test(ntt_queue, push_push_pop) {
  ntt_queue_t queue;
  ntt_queue_init(&queue);
  cr_assert(ntt_queue_empty(&queue));

  int first;
  ntt_queue_push_back(&queue, &make_node(1)->node, &first);
  cr_assert(!ntt_queue_empty(&queue));
  cr_assert(first);

  ntt_queue_push_back(&queue, &make_node(2)->node, &first);
  cr_assert(!ntt_queue_empty(&queue));
  cr_assert(!first);

  int last;
  ntt_node_t *node = ntt_queue_pop_front(&queue, &last);
  cr_assert_not_null(node);
  cr_assert(!last);
  cr_assert(!ntt_queue_empty(&queue));
  cr_assert(ntt_container_of(node, test_node_t, node)->val == 1);
}

Test(ntt_queue, push_push_pop_pop) {
  ntt_queue_t queue;
  ntt_queue_init(&queue);
  cr_assert(ntt_queue_empty(&queue));

  int first;
  ntt_queue_push_back(&queue, &make_node(1)->node, &first);
  cr_assert(!ntt_queue_empty(&queue));
  cr_assert(first);

  ntt_queue_push_back(&queue, &make_node(2)->node, &first);
  cr_assert(!ntt_queue_empty(&queue));
  cr_assert(!first);

  int last;
  ntt_node_t *node = ntt_queue_pop_front(&queue, &last);
  cr_assert_not_null(node);
  cr_assert(!last);
  cr_assert(!ntt_queue_empty(&queue));
  cr_assert(ntt_container_of(node, test_node_t, node)->val == 1);

  node = ntt_queue_pop_front(&queue, &last);
  cr_assert_not_null(node);
  cr_assert(last);
  cr_assert(ntt_queue_empty(&queue));
  cr_assert(ntt_container_of(node, test_node_t, node)->val == 2);
}
