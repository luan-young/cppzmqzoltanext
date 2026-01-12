#include "cppzmqzoltanext/helpers.h"

#include <cerrno>

namespace zmqzext {

zmq::send_result_t send_retry_on_eintr(zmq::socket_t& socket, zmq::const_buffer const& buf,
                                       zmq::send_flags flags /* = zmq::send_flags::none*/) {
    zmq::send_result_t result;
    while (true) {
        try {
            result = socket.send(buf, flags);
            break;
        } catch (zmq::error_t const& e) {
            if (e.num() == EINTR) {
                continue;
            }
            throw;
        }
    }
    return result;
}

zmq::send_result_t send_retry_on_eintr(zmq::socket_t& socket, zmq::message_t& msg,
                                       zmq::send_flags flags /* = zmq::send_flags::none*/) {
    zmq::send_result_t result;
    while (true) {
        try {
            result = socket.send(msg, flags);
            break;
        } catch (zmq::error_t const& e) {
            if (e.num() == EINTR) {
                continue;
            }
            throw;
        }
    }
    return result;
}

zmq::send_result_t send_retry_on_eintr(zmq::socket_t& socket, zmq::message_t&& msg,
                                       zmq::send_flags flags /* = zmq::send_flags::none*/) {
    return send_retry_on_eintr(socket, msg, flags);
}

zmq::recv_buffer_result_t recv_retry_on_eintr(zmq::socket_t& socket, zmq::mutable_buffer const& buf,
                                              zmq::recv_flags flags /*= zmq::recv_flags::none*/) {
    zmq::recv_buffer_result_t result;
    while (true) {
        try {
            result = socket.recv(buf, flags);
            break;
        } catch (zmq::error_t const& e) {
            if (e.num() == EINTR) {
                continue;
            }
            throw;
        }
    }
    return result;
}

zmq::recv_result_t recv_retry_on_eintr(zmq::socket_t& socket, zmq::message_t& msg,
                                       zmq::recv_flags flags /*= zmq::recv_flags::none*/) {
    zmq::recv_result_t result;
    while (true) {
        try {
            result = socket.recv(msg, flags);
            break;
        } catch (zmq::error_t const& e) {
            if (e.num() == EINTR) {
                continue;
            }
            throw;
        }
    }
    return result;
}

}  // namespace zmqzext
