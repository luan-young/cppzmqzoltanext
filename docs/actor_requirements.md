# Actor Requirements

The Actor provides the functionality of running a user-provided function in a new thread and keeping a pair of zmq
sockets for communication between the parent thread and the child thread. It synchronizes the start of the function
execution in its constructor and the stop of the thread in its destructor.

## Requirements

### API

- The constructor should take as parameters the zmq context to be used in the sockets and the user-defined function (any
  callable object, like a function, lambda or functor).
- The callable object should have the signature taking a zmq socket reference and returning bool, true on success and
  false on failure.
- The user-provided function should be responsible for sending a success signal after its initialization and it is ready for processing messages.
- The caller thread will be blocked in the class constructor while waiting for the success signal.
- The user-provided function should be responsible for processing a stop request message and returning to
  indicate that it has finished processing messages and is ready to be stopped.
- A method to return the parent socket so it can be used by the owner of the actor object.
- A method to stop the thread even before the destructor is called.
- A method to check if the thread was already stopped.

### Expected Behavior

- The constructor should create a pair of zmq::sockets of type pair, one for the parent side of the class (i.e. actor
  object itself) and the other for the child (i.e. the function that is run in the thread).
- The parent socket should perform the bind to an automatic self generated address that should be unique for each actor
  instance.
- The child socket should connect to the address provided by the parent socket.
- The child socket should be passed to the user-provided function.
- The constructor should start in a new thread a helper function which should call the user-provided function and do
  extra management tasks.
- The constructor should block until the user-provided function is fully synchronized.
- The start synchronization involves the constructor waiting for a signal from the user-provided function
  indicating that it is ready to start processing messages.
- It is responsibility of the user-provided function to send a success signal as soon as possible.
- The helper function should call the user-provided function and monitor its execution. If it returns false or throws
  an exception, it should send a failure signal. On true, it should send a success signal.
- The helper function should also save a catched exception in a member variable of the actor class, so it can be rethrown
  by the constructor if it is waiting the signal (the user-provided function failed to start).
- The constructor should rethrow the saved exception on a failure signal or throw a specific exception if there is no
  saved exception.
- The helper function should also close the child socket when the user-provided function finishes its execution,
  regardless of whether it was successful or not, so the destructor stop message fails to be sent indicating that the
  child thread has finished.
- The destructor should synchronize the stop of the user-provided function and then clean up the parent socket.
- The stop synchronization involves the destructor sending a stop request message and then waiting for a signal from the
  user-provided function indicating that it has finished processing messages and is ready to be stopped.
- The stop request message should not be blocking so the signal waiting is skipped if the stop request could not
  be sent (e.g. if the user-provided function has already finished and closed the socket).
- The signal waiting should have a timeout to avoid blocking forever if the user-provided function does not respond.
  (discussion: this opens space for leaving a zombie thread running forever without any apparent warning to the
  destructor caller as it can't throw. On the other hand, if it does block while waiting for the signal, it would be mandatory that the application processing messages from the actor does not throw away any received signal and calls an
  appropriate method to stop the actor before destroying it. This would be a burden for the user of the class. The
  following scenario would be a corner case: the user-provided function returns or throws, sending the failure signal;
  the parent application receives the signal but throws it away; the destructor is called before the child thread closes
  the socket, so the stop request is sent and the destructor waits for a signal that will never arrive. The user of the class should be aware of this scenario and avoid throwing away any signal received from the actor).
- The stop method should do

### Synchronization Scenarios

From the above logic, we can derive the following scenarios:

#### Normal Execution

1. The constructor is called, creating the zmq sockets and starting the helper function in a new thread.
2. The helper function calls the user-provided function with the child socket.
3. The user-provided function sends a success signal to the parent socket.
4. The constructor receives the success signal and completes successfully.
5. The destructor is called, sending a stop request message to the child socket.
6. The user-provided function processes the stop request and returns true.
7. The helper function sends the success signal for the stop request.
8. The destructor receives the success signal and cleans up the parent socket.

#### Failure During Start

1. The constructor is called, creating the zmq sockets and starting the helper function in a new thread.
2. The helper function calls the user-provided function with the child socket.
3. The user-provided function fails to start and throws an exception or returns false.
4. The helper function catches the exception or the false return value and sends a failure signal to the parent socket.
5. The constructor receives the failure signal and rethrows the saved exception or throws a specific exception.
6. No desctructor call occurs since the constructor throws.

#### Failure During Execution

1. The constructor is called, creating the zmq sockets and starting the helper function in a new thread.
2. The helper function calls the user-provided function with the child socket.
3. The user-provided function processes messages and then throws an exception or returns false.
4. The helper function catches the exception or the false return value, sends a failure signal to the parent socket and
closes the child socket.
5. The destructor is called, sending a stop request message to the child socket.
6. The send fails and the destructor skips the signal waiting.
7. The destructor cleans up the parent socket.

#### Destructor Called Concurrently with Failure During Execution

1. The constructor is called, creating the zmq sockets and starting the helper function in a new thread.
2. The helper function calls the user-provided function with the child socket.
3. The user-provided function processes messages and then throws an exception or returns false.
4. The helper function catches the exception or the false return value, sends a failure signal to the parent socket.
5. The destructor is called, sending a stop request message to the child socket. The message is sent successfully
   because the child socket is still open, but is not processed.
6. The helper function closes the child socket.
7. The destructor receives the failure signal and cleans up the parent socket.

## Modificar a lógica para ter uma função de start, onde é feita a sincronizção e para fazer parte do fluxo normal chamar a função de stop. O destrutor até deve mandar o sinal de stop, mas nunca deve bloquear esperando. A função de stop pode ter uma parâmetro para ser bloqueante, mas somente no caso do envio do sinal de stop ter sucedido.
