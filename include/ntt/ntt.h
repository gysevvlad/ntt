#pragma once

typedef void(ntt_ws_started_callback)();
typedef void(ntt_ws_txt_msg_callback)();
typedef void(ntt_ws_bin_msg_callback)();
typedef void(ntt_ws_stopped_callback)();

struct ntt_ws_handler {
  ntt_ws_started_callback *ws_started_callback;
  ntt_ws_bin_msg_callback *ws_bin_msg_callback;
  ntt_ws_txt_msg_callback *ws_txt_msg_callback;
  ntt_ws_stopped_callback *ws_stopped_callback;
};

typedef void(ntt_tcp_started_callback)();
typedef void(ntt_tcp_txt_msg_callback)();
typedef void(ntt_tcp_bin_msg_callback)();
typedef void(ntt_tcp_stopped_callback)();

struct ntt_tcp_handler {
  ntt_tcp_started_callback *tcp_started_callback;
  ntt_tcp_bin_msg_callback *tcp_bin_msg_callback;
  ntt_tcp_txt_msg_callback *tcp_txt_msg_callback;
  ntt_tcp_stopped_callback *tcp_stopped_callback;
};


