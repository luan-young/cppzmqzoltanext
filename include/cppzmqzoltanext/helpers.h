#pragma once

#include <zmq.hpp>

#include "cppzmqzoltanext/czze_export.h"

namespace zmqzext {

CZZE_EXPORT zmq::send_result_t send_retry_on_eintr(zmq::socket_t& socket, zmq::const_buffer const& buf,
                                                   zmq::send_flags flags = zmq::send_flags::none);

CZZE_EXPORT zmq::send_result_t send_retry_on_eintr(zmq::socket_t& socket, zmq::message_t& msg,
                                                   zmq::send_flags flags = zmq::send_flags::none);

CZZE_EXPORT zmq::send_result_t send_retry_on_eintr(zmq::socket_t& socket, zmq::message_t&& msg,
                                                   zmq::send_flags flags = zmq::send_flags::none);

CZZE_EXPORT zmq::recv_buffer_result_t recv_retry_on_eintr(zmq::socket_t& socket, zmq::mutable_buffer const& buf,
                                                          zmq::recv_flags flags = zmq::recv_flags::none);

CZZE_EXPORT zmq::recv_result_t recv_retry_on_eintr(zmq::socket_t& socket, zmq::message_t& msg,
                                                   zmq::recv_flags flags = zmq::recv_flags::none);

}  // namespace zmqzext
