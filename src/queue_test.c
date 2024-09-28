#include "ntt/defs.h"

#include "./queue.h"

#include <criterion/criterion.h>
#include <criterion/internal/assert.h>

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
  cr_expect(queue.node.next == &queue.node);
  cr_expect(queue.node.prev == &queue.node);
}

Test(ntt_queue, empty) {
  ntt_queue_t queue;
  ntt_queue_init(&queue);
  cr_expect(ntt_queue_empty(&queue));
  cr_expect(queue.node.next == &queue.node);
  cr_expect(queue.node.prev == &queue.node);
}

Test(ntt_queue, push) {
  ntt_queue_t queue;
  ntt_queue_init(&queue);

  test_node_t *test_node = make_node(1);

  int first;
  ntt_queue_push_back(&queue, &test_node->node, &first);
  cr_expect(first);
  cr_expect(!ntt_queue_empty(&queue));
  cr_expect(queue.node.next == &test_node->node);
  cr_expect(queue.node.prev == &test_node->node);
  cr_expect(test_node->node.next == &queue.node);
  cr_expect(test_node->node.prev == &queue.node);
}

Test(ntt_queue, push_pop) {
  ntt_queue_t queue;
  ntt_queue_init(&queue);

  test_node_t *test_node = make_node(1);

  int first;
  ntt_queue_push_back(&queue, &test_node->node, &first);

  int last;
  ntt_node_t *node = ntt_queue_pop_front(&queue, &last);
  cr_assert_not_null(node);
  cr_expect(queue.node.next == &queue.node);
  cr_expect(queue.node.prev == &queue.node);
  cr_expect(last);
  cr_expect(ntt_queue_empty(&queue));
  cr_assert(ntt_container_of(node, test_node_t, node)->val == 1);
}

Test(ntt_queue, push_push) {
  ntt_queue_t queue;
  ntt_queue_init(&queue);

  test_node_t *first_node = make_node(1);
  test_node_t *second_node = make_node(2);

  int first;
  ntt_queue_push_back(&queue, &first_node->node, &first);

  ntt_queue_push_back(&queue, &second_node->node, &first);
  cr_expect(!first);
  cr_expect(!ntt_queue_empty(&queue));
  cr_expect(queue.node.next == &second_node->node);
  cr_expect(second_node->node.next == &first_node->node);
  cr_expect(first_node->node.next == &queue.node);
  cr_expect(queue.node.prev == &first_node->node);
  cr_expect(first_node->node.prev == &second_node->node);
  cr_expect(second_node->node.prev == &queue.node);
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

Test(ntt_queue, push_push_pop) {
  ntt_queue_t queue;
  ntt_queue_init(&queue);
  cr_assert(ntt_queue_empty(&queue));

  test_node_t *first_node = make_node(1);
  test_node_t *second_node = make_node(2);

  int first;
  ntt_queue_push_back(&queue, &first_node->node, &first);
  ntt_queue_push_back(&queue, &second_node->node, &first);
  ntt_node_t *node = ntt_queue_pop_front(&queue, &first);
  cr_expect(&first_node->node == node);
  cr_expect(!first);
  cr_expect(!ntt_queue_empty(&queue));
  cr_expect(queue.node.next == &second_node->node);
  cr_expect(second_node->node.next == &queue.node);
  cr_expect(queue.node.prev == &second_node->node);
  cr_expect(second_node->node.prev == &queue.node);
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

Test(ntt_queue, push_pop_push_pop) {
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

  ntt_queue_push_back(&queue, &make_node(2)->node, &first);
  cr_assert(!ntt_queue_empty(&queue));
  cr_assert(first);

  node = ntt_queue_pop_front(&queue, &last);
  cr_assert_not_null(node);
  cr_assert(last);
  cr_assert(ntt_queue_empty(&queue));
  cr_assert(ntt_container_of(node, test_node_t, node)->val == 2);
}
