# Actor Requirements

The Actor provides the functionality of running a user-provided function in a new thread and keeping a pair of zmq
sockets for communication between the parent thread and the child thread. It synchronizes the start of the function
execution in its start method and the stop of the thread in its stop method.

## Requirements

### API

- The constructor must take as parameter only the zmq context to be used in the sockets.
- A start method that takes the user-defined function as parameter (any callable object, like a function, lambda or functor).
- The callable object must have the signature taking a zmq socket reference and returning bool, true on success and
  false on failure.
- The user-provided function must be responsible for sending a success signal as soon as possible after its initialization, indicating that it is ready to start processing messages.
- The caller thread will be blocked in the start method while waiting for the success signal.
- The user-provided function must be responsible for processing a stop request message and returning to
  indicate that it has finished processing messages and is ready to be stopped.
- After sending the success signal, the user-provided function must only return when receiving the stop request message.
- A method to return the parent socket so it can be used by the owner of the actor object to communicate with the user-provided function.
- A method to stop the thread with an optional timeout parameter (0 means not blocking, negative means blocking indefinitely). It must return true on success and false on failure.
- A method to check if the thread was already started.
- A method to check if the thread was already stopped.
- If the application uses the actor parent socket to process messages and receives any signal from the actor, it must destruct the actor object or call the stop method with a timeout of 0 to avoid blocking.

### Expected Behavior

#### Constructor

The constructor must:

- Create a pair of zmq::sockets of type pair, one for the parent side (actor object itself) and other for the child.
- Perform the bind of parent socket to an automatic self generated address that must be unique for each instance.
- Connect the child socket to the parent's bound address.
- Store the sockets for later use.

#### Start Method

The start method must:

- Take the user-defined function as parameter.
- Start a new thread through a private execute method which calls the user-provided function and does extra management tasks.
- Block while waiting for the success or failure signal sent from the user-provided function or the execute method.
- Rethrow the saved exception (by the execute method), or throw a specific exception if there is no saved exception, on a failure signal.
- Throw if the thread was already started.

#### Execute Method

The private execute method must:

- Call the user-provided function with the child socket.
- Monitor its execution. If it returns false or throws an exception, it must send a failure signal. On true, it must send a success signal.
- Save a catched exception in a member variable of the actor class, so it can be rethrown by the start method if it is waiting for the signal (the user-provided function failed during its initialization before sending the success signal).
- Close the child socket when the user-provided function finishes its execution, regardless of whether it was successful or not, so the stop signal from the stop method fails to be sent indicating that the child thread has already finished.

#### Stop Method

The stop method must:

- Return false if the thread was not started or was already stopped.
- Send a stop request message and then wait for a signal from the user-provided function indicating that it has finished processing messages and is ready to be stopped.
- Clean up the parent socket and return true for success.
- The stop request message must not be blocking so the signal waiting is skipped if the stop request could not be sent (e.g. if the user-provided function has already finished and closed the socket). Dough, the cleanup must still be performed and return true for success.
- The signal waiting must use the timeout parameter so it is possible to avoid blocking forever if the user-provided function does not respond.
- On timeout, the cleanup must still be performed, but return false for failure.
- Discussion:
  - A timeout opens space for leaving a zombie thread running forever (or until the user-defined function finishes its "hard work" and processes the stop request), but at least does not block the parent thread indefinitely.
  - On the other hand, if it does block while waiting for the signal, it could block forever if the application processing messages from the actor threw away any received signal before calling the stop method. It must have called the destructor instead of the stop method in this case. The following scenario would be a corner case: the user-provided function returns, sending the success or failure signal; the parent application receives the signal but throws it away; the stop method is called before the child thread closes the socket, so the stop request is sent and it waits for a signal that will never arrive.
  - Ideally, the user-provided function must never return without receiving the stop rquest, so the parent socket only receives the signal while executing the stop method.

#### Destructor

The destructor must:

- Call the stop method with a timeout to avoid blocking forever.

### Synchronization Scenarios

From the above logic, we can derive the following scenarios:

#### Normal Execution

1. The constructor is called, creating and connecting the zmq sockets.
2. The start method is called, which starts the execute method in a new thread.
3. The execute method calls the user-provided function with the child socket.
4. The user-provided function sends a success signal to the parent socket.
5. The start method receives the success signal and completes successfully.
6. The stop method is called, sending a stop request message to the child socket.
7. The user-provided function processes the stop request and returns true.
8. The execute method sends a success signal and closes the child socket.
9. The stop method receives the success signal and cleans up the parent socket.
10. The destructor is called, calling stop with a non-zero timeout, which just returns since the actor was already stopped.

#### Failure During Start

1. The constructor is called, creating and connecting the zmq sockets.
2. The start method is called, which starts the execute method in a new thread.
3. The execute method calls the user-provided function with the child socket.
4. The user-provided function fails to start and throws an exception or returns false.
5. The execute method catches the exception or the false return value, sends a failure signal and closes the child socket.
6. The start method receives the failure signal and rethrows the saved exception or throws a specific exception.
7. The destructor is called, calling stop with a non-zero timeout, which just returns since the actor was never started.

#### Failure During Execution

1. The constructor is called, creating and connecting the zmq sockets.
2. The start method is called, which starts the execute method in a new thread.
3. The execute method calls the user-provided function with the child socket.
4. The user-provided function sends a success signal to the parent socket.
5. The start method receives the success signal and completes successfully.
6. The user-provided function encounters an error and returns false.
7. The execute method sends a failure signal and closes the child socket.
8. The parent application receives the failure signal while processing messages from the actor and destroys the actor.
9. The stop method is called with a non-zero timeout via destructor, attempts to send a stop request but fails since socket is closed.
10. The stop method skips waiting for signal and just cleans up the parent socket.

#### Concurrent Stop and Failure

1. The constructor is called, creating and connecting the zmq sockets.
2. The start method is called, which starts the execute method in a new thread.
3. The execute method calls the user-provided function with the child socket.
4. The user-provided function sends a success signal to the parent socket.
5. The start method receives the success signal and completes successfully.
6. The user-provided function encounters an error and returns false.
7. The execute method sends a failure signal and is about to close the child socket.
8. The parent application receives the failure signal while processing messages from the actor and destroys the actor.
9. The stop method is called with a non-zero timeout via destructor just before the execute method closes the socket.
10. The stop request is sent successfully but will never be processed.
11. The stop method times out waiting for the signal, cleans up the parent socket and returns.

#### Discarding Actor Signals Causes Blocking Forever in Stop

1. The constructor is called, creating and connecting the zmq sockets.
2. The start method is called, which starts the execute method in a new thread.
3. The execute method calls the user-provided function with the child socket.
4. The user-provided function sends a success signal to the parent socket.
5. The start method receives the success signal and completes successfully.
6. The user-provided function encounters an error and returns false.
7. The execute method sends a failure signal and is about to close the child socket.
8. The parent application receives the failure signal while processing messages from the actor and discards the signal.
9. The stop method is called with a negative timeout just before the execute method closes the socket.
10. The stop request is sent successfully but will never be processed.
11. The stop method blocks forever waiting for the signal.

#### Stop Method Timeout

1. The constructor is called, creating and connecting the zmq sockets.
2. The start method is called, which starts the execute method in a new thread.
3. The execute method calls the user-provided function with the child socket.
4. The user-provided function sends a success signal to the parent socket.
5. The start method receives the success signal and completes successfully.
6. The stop method is called with a non-zero timeout, sending a stop request message to the child socket.
7. The user-provided function is busy and doesn't process the stop request within the timeout period.
8. The stop method times out waiting for the signal, cleans up the parent socket and returns false.
9. The user-provided function continues running as a "zombie" thread until it eventually processes the stop request.
