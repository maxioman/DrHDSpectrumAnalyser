#pragma once

#include <AudioToolbox/AudioToolbox.h>
#include <atomic>
#include <cmath>

namespace DrHD {

class ToneGenerator {
public:
    static ToneGenerator& instance();

    void start();
    void stop();

    void setMuted(bool muted) { muted_ = muted; }
    bool isMuted() const { return muted_; }

    void setVolume(float vol) { baseVolume_ = vol; }
    float getVolume() const { return baseVolume_; }

    void updateSignal(float snrDb, bool carrierLocked);

private:
    ToneGenerator();
    ~ToneGenerator();

    static void audioCallback(void* custom_data, AudioQueueRef queue, AudioQueueBufferRef buffer);
    void fillBuffer(AudioQueueBufferRef buffer);

    AudioQueueRef queue_ = nullptr;
    AudioQueueBufferRef buffers_[3];
    AudioStreamBasicDescription format_;

    std::atomic<bool> isRunning_{false};
    std::atomic<bool> muted_{false};
    std::atomic<float> targetFrequency_{800.0f};
    std::atomic<float> baseVolume_{0.15f};
    std::atomic<float> volume_{0.15f};
    std::atomic<bool> carrierLocked_{false};

    double phase_ = 0.0;
    double pulsePhase_ = 0.0;
    static constexpr int SAMPLE_RATE = 44100;
};

} // namespace DrHD
