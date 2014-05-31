Qt Dispatcher
============

A Dispatcher object (similar to the one in WPF) for the Qt Framework, written in C++11.

This class can be used to invoke functions (including function objects and lambdas) on a specific Qt thread.
It's primary purpose is to be able, from background worker threads, to cause actions to be performed on the UI thread which dispatched them.

A good example is when you want to notify the UI thread of some progress your background thread has made.
If you tried to do that from a different thread, the behavior, in most cases, would be undefined, since (especially in multi-core systems) you'd be getting in the way of the UI thread doing whatever it needs to do, like dispatching events, drawing, etc.

So here's how to use this class:

```c++
QObject* someWidget = GetSomeUIWidget();  
Dispatcher dispatcher(someWidget); // now the dispatcher works on the thread Qt assigns to someWidget  
dispatcher.invoke([=] { doSomething(); }); // will be invoked on someWidget's thread,
                                           // and block this thread until the call
                                           // on the other thread is completed
```

The `Invoke` function blocks the calling thread until the operation has been performed on the widget's thread.
It may also return values, as in the following example:

```int i = dispatcher.invoke([] { return 123; });```

If the returned value is not a primitive, then it will either be copied or moved to the calling thread, depending on which constructor is available for its class.

`invoke` will also propagate any exceptions thrown in the other thread right into the calling thread, so you may wrap the call to `invoke` with a `try/catch` if you wish.
If you know what you're doing, there's no problem dispatching a lambda which captures state from the calling thread. Since `invoke` blocks, any existing state in the thread's stack will be intact while running the function on the destination thread.

There is also an alternative `FireAndForget` function, which does not block, and hence does not run any function which returns a value. This can also be useful sometimes.
