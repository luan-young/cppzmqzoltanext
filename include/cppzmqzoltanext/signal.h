#pragma once

#include <array>
#include <optional>
#include <zmq.hpp>

namespace zmqzext {

/**
 * @brief Class representing actor signals for inter-thread communication
 */
class signal_t {
public:
    /**
     * @brief Enumeration of possible signal types
     */
    enum class type_t : uint8_t {
        success = 1,
        failure = 2,
        stop = 3,
    };

    /**
     * @brief Get the type of the signal
     * @return The signal type
     */
    type_t type() const { return _type; }

    /**
     * @brief Check if this signal is a success signal
     * @return true if success signal, false otherwise
     */
    bool is_success() const { return _type == type_t::success; }

    /**
     * @brief Check if this signal is a failure signal
     * @return true if failure signal, false otherwise
     */
    bool is_failure() const { return _type == type_t::failure; }

    /**
     * @brief Check if this signal is a stop signal
     * @return true if stop signal, false otherwise
     */
    bool is_stop() const { return _type == type_t::stop; }

    /**
     * @brief Create a success signal message
     * @return The ZMQ message containing the success signal
     */
    static zmq::message_t create_success();

    /**
     * @brief Create a failure signal message
     * @return The ZMQ message containing the failure signal
     */
    static zmq::message_t create_failure();

    /**
     * @brief Create a stop signal message
     * @return The ZMQ message containing the stop signal
     */
    static zmq::message_t create_stop();

    /**
     * @brief Check if a ZMQ message contains a valid signal
     * @param msg The message to check
     * @return std::optional<signal_t> The signal if valid, std::nullopt
     * otherwise
     */
    static std::optional<signal_t> check_signal(const zmq::message_t& msg);

private:
    explicit signal_t(type_t type) : _type(type) {}
    type_t _type;
};

}  // namespace zmqzext
