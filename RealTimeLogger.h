#pragma once

#include <atomic>
#include <cstdint>

namespace cray
{
    /**
     * Lock-free audio telemetry block updated by the audio thread and polled by the UI timer.
     */
    struct AudioTelemetry
    {
        std::atomic<float> peakLeft{0.0f};
        std::atomic<float> peakRight{0.0f};
        std::atomic<int>   activeSynthVoices{0};
        std::atomic<int>   activeSampleVoices{0};
        std::atomic<int>   totalVoices{0};
        std::atomic<float> cpuLoadEstimate{0.0f};
        std::atomic<uint64_t> midiActivityCounter{0};
        std::atomic<bool>  clippingDetected{false};
        std::atomic<int>   lastTriggeredMidiNote{-1};

        void resetPeaks() noexcept
        {
            peakLeft.store(0.0f, std::memory_order_relaxed);
            peakRight.store(0.0f, std::memory_order_relaxed);
            clippingDetected.store(false, std::memory_order_relaxed);
        }
    };
}
