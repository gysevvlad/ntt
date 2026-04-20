#include "./task.h"
#include "./malloc.h"

ntt_task_t* ntt_task_create(
    ntt_task_cb_t* task_cb)
{
    ntt_task_node_t* task_node = ntt_task_create_inl(task_cb, NULL);
    return &task_node->payload;
}

void ntt_task_svc(
    ntt_task_t* self)
{
    return ntt_task_svc_inl(self);
}

void ntt_task_destroy(
    ntt_task_t* self)
{
    return ntt_task_destroy_inl(self);
}
