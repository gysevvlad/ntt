#pragma once

#include "ntt/impl/list.h"
#include "ntt/impl/node.h"
#include "ntt/impl/reader.h"

#include <pthread.h>
#include <stdatomic.h>

struct ntt_session_backend {
    ntt_pool_t* pool;
    int write_fd;
    int read_fd;
    ntt_reader_t reader;
};

