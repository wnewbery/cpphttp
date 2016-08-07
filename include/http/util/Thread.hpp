#pragma once
#include <string>
#ifdef _WIN32
#include <Windows.h>
namespace http
{
    inline void set_thread_name(const std::string &str)
    {
        const DWORD MS_VC_EXCEPTION = 0x406D1388;
#pragma pack(push,8)
        struct ThreadInfo
        {
            DWORD dwType; // Must be 0x1000.
            LPCSTR szName; // Pointer to name (in user addr space).
            DWORD dwThreadID; // Thread ID (-1=caller thread).
            DWORD dwFlags; // Reserved for future use, must be zero.
        };
#pragma pack(pop)
        DWORD threadId = GetCurrentThreadId();
        ThreadInfo info = { 0x1000, str.c_str(), threadId, 0 };
#pragma warning(push)
#pragma warning(disable: 6320 6322)
        __try {
            RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
        }
#pragma warning(pop)
    }
}
#else
#include <pthread.h>
namespace http
{
    inline void set_thread_name(const std::string &str)
    {
        auto str2 = str.substr(0, 15);
        pthread_setname_np(pthread_self(), str2.c_str());
    }
}
#endif
