#pragma once
#include <atomic>
#include <thread>

namespace mic_in
{
    struct ctx_t
    {
        std::thread         th;
        std::atomic< bool > stop{ false };
    };
    int  start( ctx_t& ctx, uint16_t port, const char* device_name );
    void stop( ctx_t& ctx );
}  // namespace mic_in