# Actor Requirements

The Actor provides the functionality of running a user-provided function in a new thread and keeping a pair of zmq
sockets for communication between the parent thread and the child thread. It synchronizes the start of the function
execution in its start method and the stop of the thread in its stop method.

## Requirements

### API

- The constructor should take as parameter only the zmq context to be used in the sockets.
- A start method that takes the user-defined function as parameter (any callable object, like a function, lambda or functor).
- The callable object should have the signature taking a zmq socket reference and returning bool, true on success and
  false on failure.
- The user-provided function should be responsible for sending a success signal as soon as possible after its initialization, indicating that it is ready to start processing messages.
- The caller thread will be blocked in the start method while waiting for the success signal.
- The user-provided function should be responsible for processing a stop request message and returning to
  indicate that it has finished processing messages and is ready to be stopped.
- After sending the success signal, the user-provided function should only return when receiving the stop request message.
- A method to return the parent socket so it can be used by the owner of the actor object to communicate with the user-provided function.
- A method to stop the thread with an optional timeout parameter (0 means not blocking, negative means blocking indefinitely). It should return true on success and false on failure.
- A method to check if the thread was already started.
- A method to check if the thread was already stopped.
- If the application uses the actor parent socket to process messages and receives any signal from the actor, it should destruct the actor object or call the stop method with a timeout of 0 to avoid blocking.

### Expected Behavior

#### Constructor

The constructor should:

- Create a pair of zmq::sockets of type pair, one for the parent side (actor object itself) and other for the child.
- Perform the bind of parent socket to an automatic self generated address that should be unique for each instance.
- Store the sockets for later use.

#### Start Method

The start method should:

- Take the user-defined function as parameter.
- Connect the child socket to the parent's bound address.
- Start a new thread through a private execute method which calls the user-provided function and does extra management tasks.
- Block while waiting for the success or failure signal sent from the user-provided function or the execute method.
- Rethrow the saved exception (by the execute method), or throw a specific exception if there is no saved exception, on a failure signal.
- Throw if the thread was already started.

#### Execute Method

The private execute method should:

- Call the user-provided function with the child socket.
- Monitor its execution. If it returns false or throws an exception, it should send a failure signal. On true, it should send a success signal.
- Save a catched exception in a member variable of the actor class, so it can be rethrown by the start method if it is waiting for the signal (the user-provided function failed during its initialization before sending the success signal).
- Close the child socket when the user-provided function finishes its execution, regardless of whether it was successful or not, so the stop signal from the stop method fails to be sent indicating that the child thread has already finished.

#### Stop Method

The stop method should:

- Send a stop request message and then wait for a signal from the user-provided function indicating that it has finished processing messages and is ready to be stopped.
- Clean up the parent socket and return true for success.
- The stop request message should not be blocking so the signal waiting is skipped if the stop request could not be sent (e.g. if the user-provided function has already finished and closed the socket). Dough, the cleanup should still be performed and return true for success.
- The signal waiting should use the timeout parameter so it is possible to avoid blocking forever if the user-provided function does not respond.
- On timeout, the cleanup should still be performed, but return false for failure.
- Discussion:
  - A timeout opens space for leaving a zombie thread running forever (or until the user-defined function finishes its "hard work" and processes the stop request), but at least does not block the parent thread indefinitely.
  - On the other hand, if it does block while waiting for the signal, it could block forever if the application processing messages from the actor threw away any received signal before calling the stop method. It should have called the destructor instead of the stop method in this case. The following scenario would be a corner case: the user-provided function returns, sending the success or failure signal; the parent application receives the signal but throws it away; the stop method is called before the child thread closes the socket, so the stop request is sent and it waits for a signal that will never arrive.
  - Ideally, the user-provided function should never return without receiving the stop rquest, so the parent socket only receives the signal while executing the stop method.

#### Destructor

The destructor should:

- Call the stop method with a timeout of 0 to avoid blocking.

### Synchronization Scenarios

From the above logic, we can derive the following scenarios:

#### Normal Execution

1. The constructor is called, creating the zmq sockets.
2. The start method is called, which connects the child socket and starts the helper function in a new thread.
3. The helper function calls the user-provided function with the child socket.
4. The user-provided function sends a success signal to the parent socket.
5. The start method receives the success signal and completes successfully.
6. The destructor is called, sending a stop request message to the child socket.
7. The user-provided function processes the stop request and returns true.
8. The helper function sends the success signal for the stop request.
9. The destructor receives the success signal and cleans up the parent socket.

#### Failure During Start

1. The constructor is called, creating the zmq sockets.
2. The start method is called, which connects the child socket and starts the helper function in a new thread.
3. The helper function calls the user-provided function with the child socket.
4. The user-provided function fails to start and throws an exception or returns false.
5. The helper function catches the exception or the false return value and sends a failure signal to the parent socket.
6. The start method receives the failure signal and rethrows the saved exception or throws a specific exception.
7. No desctructor call occurs since the constructor throws.

#### Failure During Execution

1. The constructor is called, creating the zmq sockets.
2. The start method is called, which connects the child socket and starts the helper function in a new thread.
3. The helper function calls the user-provided function with the child socket.
4. The user-provided function processes messages and then throws an exception or returns false.
5. The helper function catches the exception or the false return value, sends a failure signal to the parent socket and
closes the child socket.
6. The destructor is called, sending a stop request message to the child socket.
7. The send fails and the destructor skips the signal waiting.
8. The destructor cleans up the parent socket.

#### Destructor Called Concurrently with Failure During Execution

1. The constructor is called, creating the zmq sockets.
2. The start method is called, which connects the child socket and starts the helper function in a new thread.
3. The helper function calls the user-provided function with the child socket.
4. The user-provided function processes messages and then throws an exception or returns false.
5. The helper function catches the exception or the false return value, sends a failure signal to the parent socket.
6. The destructor is called, sending a stop request message to the child socket. The message is sent successfully
   because the child socket is still open, but is not processed.
7. The helper function closes the child socket.
8. The destructor receives the failure signal and cleans up the parent socket.
