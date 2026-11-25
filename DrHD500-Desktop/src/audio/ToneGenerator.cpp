#include "ToneGenerator.hpp"
#include <algorithm>

namespace DrHD {

ToneGenerator& ToneGenerator::instance() {
    static ToneGenerator generator;
    return generator;
}

ToneGenerator::ToneGenerator() {
    format_.mSampleRate       = SAMPLE_RATE;
    format_.mFormatID         = kAudioFormatLinearPCM;
    format_.mFormatFlags      = kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;
    format_.mBitsPerChannel   = 16;
    format_.mChannelsPerFrame = 1;
    format_.mBytesPerFrame    = 2;
    format_.mFramesPerPacket  = 1;
    format_.mBytesPerPacket   = 2;
    format_.mReserved         = 0;
}

ToneGenerator::~ToneGenerator() {
    stop();
}

void ToneGenerator::start() {
    if (isRunning_) return;

    OSStatus status = AudioQueueNewOutput(&format_, audioCallback, this, nullptr, nullptr, 0, &queue_);
    if (status != noErr) return;

    isRunning_ = true;

    // Allocate 3 buffers of 50ms each
    uint32_t bufferByteSize = SAMPLE_RATE * 2 * 0.05; // 50ms of 16-bit mono
    for (int i = 0; i < 3; ++i) {
        AudioQueueAllocateBuffer(queue_, bufferByteSize, &buffers_[i]);
        fillBuffer(buffers_[i]);
        AudioQueueEnqueueBuffer(queue_, buffers_[i], 0, nullptr);
    }

    AudioQueueStart(queue_, nullptr);
}

void ToneGenerator::stop() {
    if (!isRunning_) return;
    isRunning_ = false;

    if (queue_) {
        AudioQueueStop(queue_, true);
        for (int i = 0; i < 3; ++i) {
            AudioQueueFreeBuffer(queue_, buffers_[i]);
        }
        AudioQueueDispose(queue_, true);
        queue_ = nullptr;
    }
}

void ToneGenerator::updateSignal(float snrDb, bool carrierLocked) {
    carrierLocked_ = carrierLocked;
    if (snrDb <= 0.0f) {
        volume_ = 0.0f;
    } else {
        volume_ = baseVolume_.load();
        // Map 0 to 20 dB SNR -> 500 Hz to 2400 Hz
        float freq = 500.0f + (snrDb / 20.0f) * 1900.0f;
        targetFrequency_ = std::max(400.0f, std::min(2800.0f, freq));
    }
}

void ToneGenerator::audioCallback(void* custom_data, AudioQueueRef queue, AudioQueueBufferRef buffer) {
    auto* self = static_cast<ToneGenerator*>(custom_data);
    if (!self->isRunning_) return;

    self->fillBuffer(buffer);
    AudioQueueEnqueueBuffer(queue, buffer, 0, nullptr);
}

void ToneGenerator::fillBuffer(AudioQueueBufferRef buffer) {
    int16_t* samples = static_cast<int16_t*>(buffer->mAudioData);
    uint32_t numSamples = buffer->mAudioDataBytesCapacity / sizeof(int16_t);
    buffer->mAudioDataByteSize = buffer->mAudioDataBytesCapacity;

    if (muted_ || volume_ <= 0.001f) {
        std::fill(samples, samples + numSamples, 0);
        return;
    }

    double freq = targetFrequency_.load();
    double vol = volume_.load() * 32767.0;
    bool locked = carrierLocked_.load();

    // Pulse rate: if locked, fast beep (8 Hz), if searching slow beep (2 Hz)
    double pulseRate = locked ? 8.0 : 2.5;

    for (uint32_t i = 0; i < numSamples; ++i) {
        pulsePhase_ += pulseRate / SAMPLE_RATE;
        if (pulsePhase_ >= 1.0) pulsePhase_ -= 1.0;

        // 60% on, 40% off duty cycle
        double pulseGate = (pulsePhase_ < 0.6) ? 1.0 : 0.0;

        phase_ += freq / SAMPLE_RATE;
        if (phase_ >= 1.0) phase_ -= 1.0;

        double sample = std::sin(phase_ * 2.0 * M_PI) * vol * pulseGate;
        samples[i] = static_cast<int16_t>(sample);
    }
}

} // namespace DrHD
