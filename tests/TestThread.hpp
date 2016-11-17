#pragma once
#include <thread>
#include <boost/test/unit_test.hpp>
#include <Windows.h>
/**Thread wrapper that forcefully kills the child rather than calling abort on failure.*/
class TestThread : public std::thread
{
public:
    template<class F>
    TestThread(F func)
        : std::thread([func]() -> void {
        try
        {
            func();
        }
        catch (const std::exception &e)
        {
            BOOST_ERROR(std::string("Unexpected exception: ") + e.what());
        }
    })
    {}
    ~TestThread()
    {
        if (joinable())
        {
            auto handle = native_handle();
            if (WaitForSingleObject(handle, 1000) != WAIT_OBJECT_0)
            {
                BOOST_ERROR("Forcefully terminating test child thread");
                TerminateThread(handle, (DWORD)-1);
            }
            join();
        }
    }
};
