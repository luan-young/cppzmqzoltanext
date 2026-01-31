# CppZmqZoltanExt

**CppZmqZoltanExt** is a extension library for [cppzmq](https://github.com/zeromq/cppzmq), the modern C++ binding for ZeroMQ (ZMQ). It provides high-level abstractions and utilities for building robust, concurrent, and event-driven applications with ZeroMQ.

**CppZmqZoltanExt** aims to offer a subset of the well-stablished features found in [czmq](https://github.com/zeromq/czmq), but in a modern C++ interface build on top of cppzmq, simplifying common patterns in ZeroMQ-based applications, such as interrupt handling, socket polling, event loops, and the actor model for concurrent programming.

The design is mostly based on the wonderful work done in czmq and its concepts, although the implementation is entirely new and idiomatic C++. It is also important to mention that the actor model implementation uses some concepts from the [zmqpp](https://github.com/zeromq/zmqpp), another C++ binding for ZeroMQ.

Note

This library is a side project in beta version currently under active development and may undergo significant changes. While it is functional for many use cases, users should be aware that APIs and features may evolve as the library matures. Contributions and feedback are welcome to help shape its future direction.

## Features Overview

### Interrupt Handling

The interrupt handling module provides signal management for SIGINT (Ctrl+C) and SIGTERM termination signals. It establishes a mechanism for detecting and responding to interrupt signals, enabling clean application shutdown without abrupt termination.

- **Signal Handler Installation**: Register signal handlers for SIGINT and SIGTERM
- **Atomic State Tracking**: Interrupt state is tracked via an atomic flag
- **Integration**: Works seamlessly with the Poller and Event Loop for responsive shutdown behavior
- **Thread-Safe Monitoring**: Allows applications to safely detect interruption requests from multiple threads

### Poller

The Poller provides efficient monitoring of multiple ZeroMQ sockets simultaneously. It wraps ZMQ's native polling mechanism with an intuitive C++ API, allowing your application to react to socket events without busy-waiting or managing complex threading logic.

- **Multi-Socket Monitoring**: Add and remove sockets dynamically for event monitoring
- **Flexible Waiting**: Wait for a single socket to become ready or check all registered sockets
- **Configurable Timeouts**: Control how long the poller waits for socket events
- **Interrupt Awareness**: Automatically checks for interrupt signals during polling operations
- **Termination Detection**: Detect when the application should shut down

### Event Loop

The Event Loop combines socket polling with timer management to create a complete reactive event-driven architecture. It monitors registered sockets for read readiness and executes scheduled timers, enabling event-driven applications that respond to both socket messages and time-based events.

- **Socket Event Handling**: Register callbacks that fire when sockets become ready for receiving
- **Timer Management**: Schedule one-shot and recurring timer events with flexible callback handlers
- **Event-Driven Architecture**: Process both socket read and timer events in a single unified loop
- **Graceful Shutdown**: Integrate with interrupt handling for clean application termination
- **Cross-Platform Support**: Configurable interrupt checking intervals for reliable behavior on all platforms

### Actor Pattern

The actor pattern provides a powerful abstraction for concurrent programming, enabling isolated execution units that communicate exclusively through message passing. Each actor runs in its own thread with its own socket pair, eliminating shared memory and making concurrent programs easier to reason about and maintain.

- **Isolated Execution**: Each actor runs independently in its own thread
- **Message-Based Communication**: Parent and child threads communicate via ZeroMQ PAIR sockets
- **Synchronized Initialization**: The parent thread blocks until the actor confirms successful initialization
- **Exception Propagation**: Exceptions during actor initialization are safely propagated to the parent thread
- **Graceful Termination**: Coordinated shutdown protocol ensures clean resource cleanup
- **Memory Safety**: Minimal shared state between threads reduces concurrency bugs

## Example Usage

This example demonstrates a complete application using the core features of CppZmqZoltanExt:

- Interrupt handling for graceful shutdown
- Actor pattern for concurrent processing
- Event loop with socket and timer event handling

```cpp
#include <cppzmqzoltanext/actor.h>
#include <cppzmqzoltanext/helpers.h>
#include <cppzmqzoltanext/interrupt.h>
#include <cppzmqzoltanext/loop.h>
#include <cppzmqzoltanext/signal.h>

#include <chrono>
#include <functional>
#include <iostream>
#include <string>
#include <zmq.hpp>

using namespace zmqzext;

/**
 * Actor child socket handler
 *
 */
bool actor_socket_handler(loop_t& loop, zmq::socket_ref socket) {
    try {
        // Receive message from parent
        zmq::message_t msg;
        auto result = recv_retry_on_eintr(socket, msg, zmq::recv_flags::dontwait);
        if (!result) {
            return true;  // Continue loop
        }

        auto signal = signal_t::check_signal(msg);
        if (signal && signal->is_stop()) {
            return false;  // Exit loop
        }

        std::cout << "[Actor] Received: " << msg.to_string_view() << std::endl;

        // Echo the message back to parent
        send_retry_on_eintr(socket, msg, zmq::send_flags::none);
    } catch (...) {
        // Ignore exceptions and continue, but usually should communicate them to parent
    }

    return true;  // Continue loop
}

/**
 * Actor user function that runs in a separate thread.
 * Creates its own event loop to handle incoming requests and stop signals.
 */
bool actor_runner(zmq::socket_t& socket) {
    zmq::context_t actor_context;
    loop_t loop;

    std::cout << "[Actor] Started" << std::endl;

    // Register the actor socket to receive messages from parent
    loop.add(socket, actor_socket_handler);

    // Send success signal to parent
    send_retry_on_eintr(socket, signal_t::create_success(), zmq::send_flags::none);

    // Run the event loop
    try {
        loop.run(false);  // Non-interruptible mode, so the actor is stopped only by parent stop signal
    } catch (...) {
        // As the application exceptions were already caught in the socket handler,
        // the exceptions left here should be only ZMQ related. So, we still could
        // try to comunicate the failure to parent and wait to be stopped or return
        // if that fails.
    }

    std::cout << "[Actor] Finished" << std::endl;

    return false;
}

bool parent_api_socket_handler(loop_t& loop, zmq::socket_ref socket, zmq::socket_ref actor_socket) {
    try {
        zmq::message_t msg;

        // Receive echoed message from actor
        auto result = socket.recv(msg, zmq::recv_flags::dontwait);
        if (!result) {
            return false;  // Exit loop
        }

        std::cout << "[Main] Received request. Delivering it to actor: " << msg.to_string_view() << std::endl;

        // Forward the message to the actor
        auto send_result = actor_socket.send(msg, zmq::send_flags::none);
        if (!send_result) {
            return false;  // Exit loop
        }

        // Send reply back to API client
        auto reply_msg = zmq::message_t{std::string{"Ok"}};
        send_result = socket.send(reply_msg, zmq::send_flags::none);
        if (!send_result) {
            return false;  // Exit loop
        }
    } catch (...) {
        return false;  // Exit loop
    }
    return true;  // Continue loop
}

bool parent_actors_socket_handler(loop_t& loop, zmq::socket_ref socket) {
    try {
        zmq::message_t msg;

        // Receive echoed message from actor
        auto result = socket.recv(msg, zmq::recv_flags::dontwait);
        if (!result) {
            return false;  // Exit loop
        }

        std::cout << "[Main] Received from actor: " << msg.to_string_view() << std::endl;
    } catch (...) {
        return false;  // Exit loop
    }
    return true;  // Continue loop
}

int main() {
    std::cout << "[Main] Starting application" << std::endl;

    // Install interrupt handler for graceful shutdown
    install_interrupt_handler();

    try {
        // Create ZMQ context
        zmq::context_t context;

        // Create and start the actor
        std::cout << "[Main] Creating and starting actor" << std::endl;
        actor_t actor(context);
        actor.start(actor_runner);

        // Create a REP socket for request-reply communication
        zmq::socket_t rep_socket(context, zmq::socket_type::rep);
        rep_socket.bind("tcp://127.0.0.1:5555");

        // Create the main event loop
        loop_t loop;

        // Register actor socket handler
        loop.add(actor.socket(), parent_actors_socket_handler);

        // Register REP socket handler
        loop.add(rep_socket, std::bind(parent_api_socket_handler, std::placeholders::_1, std::placeholders::_2,
                                       zmq::socket_ref(actor.socket())));

        // Register a timer to print status every 2 seconds
        loop.add_timer(std::chrono::milliseconds(2000),
                       0,  // Infinite occurrences
                       [](loop_t&, timer_id_t) {
                           std::cout << "[Main] Timer event - application is running" << std::endl;
                           return true;  // Recurring timer
                       });

        std::cout << "[Main] Running loop" << std::endl;

        // Run the event loop
        // The loop will continue until:
        // - A handler returns false
        // - An interrupt signal (Ctrl+C) is received
        // On Windows, we must assure the loop will check for interrupts on a regular basis
        // On linux, the loop will be interrupted by signals automatically when waiting on zmq::poll
        loop.run(true, std::chrono::milliseconds{500});  // interruptible mode with 500ms check interval

        std::cout << "[Main] Loop finished" << std::endl;

        // No need to stop the actor explicitly, as its destructor will handle it
        std::cout << "[Main] Stopping actor" << std::endl;
    } catch (...) {
        return 1;
    }

    std::cout << "[Main] Actor stopped" << std::endl;
    std::cout << "[Main] Application finished" << std::endl;

    return 0;
}

```

## Documentation

For detailed documentation, please refer to the [CppZmqZoltanExt Documentation](https://github.com/luan-young/cppzmqzoltanext/).

Additionally, you can explore the examples provided in the `examples` directory of the repository, build and run them.

## Building and Installing

### Dependencies

#### libzmq

1. Build and install libzmq from source by cmake.

```console
$ git clone https://github.com/zeromq/libzmq.git
$ cd libzmq
$ mkdir build
$ cmake -B build -DCMAKE_BUILD_TYPE=Release
$ cmake --build build
$ sudo cmake --install build
```

2. Optionally, you can install libzmq using your system's package manager.

See detailed instructions in the [libzmq](https://github.com/zeromq/libzmq) repository.

#### cppzmq

Build and install cppzmq from source by cmake.

```console
$ git clone https://github.com/zeromq/cppzmq.git
$ cd cppzmq
$ mkdir build
$ cmake -B build -DCMAKE_BUILD_TYPE=Release -DCPPZMQ_BUILD_TESTS=OFF
$ cmake --build build
$ sudo cmake --install build
```

See detailed instructions in the [cppzmq](https://github.com/zeromq/cppzmq) repository.

### CppZmqZoltanExt

Build and install CppZmqZoltanExt from source by cmake.

```console
$ git clone https://github.com/luan-young/cppzmqzoltanext.git
$ cd cppzmqzoltanext
$ mkdir build
$ cmake -B build -DCMAKE_BUILD_TYPE=Release -DCZZE_BUILD_TESTS=ON
$ cmake --build build
$ sudo cmake --install build
```

Run the tests (if built):

```console
$ ctest --test-dir build
```

### Using CppZmqZoltanExt in Your CMake Project

To use CppZmqZoltanExt in your CMake project, you can use the following snippet in your `CMakeLists.txt`:

```cmake
find_package(cppzmqzoltanext REQUIRED)
target_link_libraries(your_target PRIVATE cppzmqzoltanext::cppzmqzoltanext)
```

## Contributing

Contributions are welcome! As this is an early-stage project, your feedback and contributions can help shape its development.

Please feel free to submit issues and pull requests on the [GitHub repository](https://github.com/luan-young/cppzmqzoltanext).

## Licensing

CppZmqZoltanExt is licensed under the MIT License. See the [LICENSE](https://github.com/luan-young/cppzmqzoltanext/blob/master/LICENSE) file for details.
