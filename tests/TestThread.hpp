#pragma once
#include <functional>
#include <thread>
#include <typeinfo>
#include <boost/test/unit_test.hpp>
#ifdef _WIN32
#   include <Windows.h>
#else
#   include <pthread.h>
#   include <signal.h> // pthread_kill
#endif
/**Thread wrapper that forcefully kills the child rather than calling abort on failure.*/
class TestThread : public std::thread
{
public:
    template<class F>
    static std::function<void()> wrap(F func)
    {
        return [func]() -> void
        {
            try
            {
                func();
            }
            catch (const std::exception &e)
            {
                BOOST_ERROR(std::string("Unexpected exception: ") + typeid(e).name() + " " + e.what());
            }
        };
    }

    TestThread() : std::thread() {}
    template<class F>
    TestThread(F func) : std::thread(wrap(func)) {}

    TestThread& operator = (TestThread &&mv)
    {
        std::thread::operator=(std::move(mv));
        return *this;
    }
    ~TestThread()
    {
        if (joinable())
        {
            auto &&handle = native_handle();
#ifdef _WIN32
            if (WaitForSingleObject(handle, 1000) == WAIT_OBJECT_0)
                join();
            else
            {
                BOOST_ERROR("Forcefully terminating test child thread");
                TerminateThread(handle, (DWORD)-1);
                join();
            }
#else
            timespec time = { 1, 0 };
            void *retval;
            if (pthread_timedjoin_np(handle, &retval, &time) == 0)
                detach();
            else
            {
                BOOST_ERROR("Forgetting test child thread");
                //pthread_cancel(handle);
                detach();
            }
#endif
        }
    }
};
