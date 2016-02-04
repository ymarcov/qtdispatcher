// The MIT License (MIT)
//
// Copyright (c) 2016 Yam Marcovic
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
#pragma once

#include <QObject>
#include <QMetaObject>
#include <QThread>
#include <QVariant>
#include <exception>
#include <functional>
#include <memory>
#include <type_traits>

namespace ymarcov {
namespace qt {

class Dispatcher : public QObject
{
    Q_OBJECT

public:
    struct Task
    {
        template <typename Result>
        static Task create(typename std::enable_if<!std::is_void<Result>::value &&
            !std::is_reference<Result>::value, std::function<Result()>>::type f)
        {
            return Task([=](void** result, std::exception_ptr& e)
            {
                try
                {
                    *result = new typename std::decay<decltype(f())>::type(f());
                }
                catch (...)
                {
                    e = std::current_exception();
                }
            });
        }

        template <typename Result>
        static Task create(typename std::enable_if<!std::is_void<Result>::value &&
            std::is_reference<Result>::value, std::function<Result()>>::type f)
        {
            return Task([=](void** result, std::exception_ptr& e)
            {
                try
                {
                    *result = const_cast<typename std::decay<decltype(f())>::type*>(&f());
                }
                catch (...)
                {
                    e = std::current_exception();
                }
            });
        }

        template <typename Result>
        static Task create(typename std::enable_if<std::is_void<Result>::value, std::function<void()>>::type f)
        {
            return Task([=](void**, std::exception_ptr& e)
            {
                try
                {
                    f();
                }
                catch (...)
                {
                    e = std::current_exception();
                }
            });
        }

        static Task createFireAndForget(std::function<void()> f)
        {
            return Task([=](void**, std::exception_ptr&)
            {
                f();
            });
        }

        Task() // QVariant needs a default constructor in case it fails to convert
        {}

        Task(std::function<void(void**, std::exception_ptr&)> shim)
            : _shim(std::move(shim))
            , _result(nullptr)
        {}

        void operator()()
        {
            _shim(&_result, _exception);
        }

        template <typename Result>
        typename std::enable_if<!std::is_void<Result>::value && std::is_reference<Result>::value, Result>::type get()
        {
            if (_exception)
                std::rethrow_exception(_exception);

            return std::forward<Result>(*static_cast<typename std::decay<Result>::type*>(_result));
        }

        template <typename Result>
        typename std::enable_if<!std::is_void<Result>::value && !std::is_reference<Result>::value, Result>::type get()
        {
            typedef typename std::decay<Result>::type RawType;

            std::unique_ptr<RawType> ptr(static_cast<RawType*>(_result));

            if (_exception)
                std::rethrow_exception(_exception);
            
            return std::move(*ptr);
        }

        template <typename Result>
        typename std::enable_if<std::is_void<Result>::value, void>::type get()
        {
            if (_exception)
                std::rethrow_exception(_exception);
        }

        std::function<void(void**, std::exception_ptr&)> _shim;
        void* _result;
        std::exception_ptr _exception;
    };

    Dispatcher(QObject* parent = nullptr)
        : QObject(parent)
    {}

    // Runs a function and returns its possible value.
    // If the function throws an exception, it will be re-thrown here.
    template <typename Func>
    auto invoke(Func&& f) const -> decltype(f())
    {
        typedef decltype(f()) Result;

        Task task = Task::create<Result>(std::function<Result()>(std::forward<Func>(f)));
        QMetaObject::invokeMethod(
            const_cast<Dispatcher*>(this),
            "dispatch",
            QThread::currentThread() == thread() ? Qt::DirectConnection : Qt::BlockingQueuedConnection,
            Q_ARG(QVariant, QVariant::fromValue(reinterpret_cast<intptr_t>(&task)))
        );
        return task.get<Result>();
    }

    // Runs a function on a the dispatcher's thread.
    // Exceptions are caught by the dispatcher's message loop.
    template <typename Action>
    void fireAndForget(Action&& f) const
    {
        Task task = Task::createFireAndForget(std::function<void()>(std::forward<Action>(f)));
        QMetaObject::invokeMethod(
            const_cast<Dispatcher*>(this),
            "dispatch",
            Qt::QueuedConnection,
            Q_ARG(QVariant, QVariant::fromValue(task))
        );
    }

private:
    Q_INVOKABLE void dispatch(QVariant var)
    {
        if (var.canConvert<intptr_t>()) // blocking connection, pointer to callable on diff thread
            reinterpret_cast<Task*>(var.value<intptr_t>())->operator()();
        else if (var.canConvert<Task>()) // non blocking connection, callable was copied
            var.value<Task>()();
    }
};

} // namespace qt
} // namespace ymarcov

Q_DECLARE_METATYPE(ymarcov::qt::Dispatcher::Task);
