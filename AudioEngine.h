#pragma once

#include "Common/LockFreeQueue.h"
#include "Common/RealTimeLogger.h"
#include "Engine/VoiceManager.h"
#include "DSP/Effects/EffectsChain.h"
#include "DSP/Modulation/LfoEngine.h"
#include "DSP/Modulation/ModMatrix.h"
#include "DSP/Utilities/ParameterSmoother.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

namespace cray
{
    /**
     * Top-level real-time audio engine owned by PluginProcessor.
     * Coordinates VoiceManager, dual LFOs, ModMatrix, EffectsChain,
     * lock-free sample instrument snapshot publishing, and MIDI UI audition queue.
     */
    class AudioEngine
    {
    public:
        struct AuditionEvent
        {
            int   midiNote = 60;
            float velocity = 0.85f;
            bool  isNoteOn = true;
        };

        void prepare(double sampleRate, int samplesPerBlock);
        void reset();

        /** Publish a new compiled sample instrument from message/worker thread (lock-free). */
        void setSampleInstrument(std::shared_ptr<const CompiledSampleInstrument> newInstrument);

        /** Reap retired sample instruments whose audioRefCount has reached 0 (call from message thread timer). */
        void collectGarbage();

        /** Push a note event from GUI virtual keyboard or Sample Editor audition button. */
        void pushUiMidiEvent(int midiNote, float velocity01, bool isNoteOn) noexcept;

        /** Process a full audio+MIDI block (real-time safe: zero locks, zero heap allocations). */
        void processBlock(juce::AudioBuffer<float>& buffer,
                          juce::MidiBuffer& midiMessages,
                          const juce::AudioProcessorValueTreeState& apvts,
                          double hostBpm,
                          AudioTelemetry& telemetry);

        const EffectsChain& getEffectsChain() const noexcept { return effectsChain_; }

    private:
        void syncParameters(const juce::AudioProcessorValueTreeState& apvts, double hostBpm) noexcept;

        double       sampleRate_   = 44100.0;
        VoiceManager voiceManager_;
        EffectsChain effectsChain_;
        ModMatrix    modMatrix_;
        std::array<LfoEngine, ProductInfo::kNumLfos> lfos_;
        OnePoleSmoother masterGainSmoother_;

        float modWheelCache_   = 0.0f;
        float pitchBendCache_  = 0.0f;
        float aftertouchCache_ = 0.0f;

        // Lock-free atomic pointer + message-thread retention list
        std::atomic<const CompiledSampleInstrument*> activeInstrumentPtr_{ nullptr };
        // Hazard pointer protects the load-to-retain window at the start of an audio block.
        // A voice then takes its own audioRefCount pin before this block-level hazard is cleared.
        std::atomic<const CompiledSampleInstrument*> audioHazardPtr_{ nullptr };
        // Serialises message/worker-thread publication against timer-thread reclamation.
        // The audio thread never takes this mutex; it uses the hazard pointer above.
        std::mutex instrumentOwnerMutex_;
        std::shared_ptr<const CompiledSampleInstrument> currentInstrumentOwner_;
        std::vector<std::shared_ptr<const CompiledSampleInstrument>> retiredInstruments_;

        LockFreeSpscQueue<AuditionEvent, 256> uiMidiQueue_;
    };
}
