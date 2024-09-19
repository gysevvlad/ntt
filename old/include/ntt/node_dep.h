#pragma once

#include "ntt/debug.h"
#include "ntt/defs.h"

#include <urcu/wfcqueue.h>

EXTERN_START

struct ntt_forward_node {
  struct cds_wfcq_node node;
  DEBUG_FIELD(bool, initialized);
};

EXTERN_STOP
