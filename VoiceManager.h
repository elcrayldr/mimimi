#pragma once

#include "Common/ProductInfo.h"
#include "Engine/SynthEngine.h"
#include "Engine/SampleEngine.h"
#include <array>
#include <cstdint>

namespace cray
{
    enum class EngineMode : int
    {
        SynthOnly = 0,
        SampleOnly,
        Hybrid
    };

    enum class VoicePlayMode : int
    {
        Poly = 0,
        Mono,
        Legato
    };

    struct PerformanceParams
    {
        EngineMode    engineMode      = EngineMode::Hybrid;
        float         synthMix        = 0.85f;
        float         sampleMix       = 0.85f;
        VoicePlayMode voiceMode       = VoicePlayMode::Poly;
        int           maxPolyphony    = 32;
        float         glideMs         = 0.0f;
        bool          glideLegatoOnly = false;
        int           pitchBendRange  = 2;
    };

    /**
     * Polyphonic Voice Manager supporting up to 128 simultaneous voices (+8 micro-fade steal slots),
     * Poly / Mono / Legato modes with held-note stack, Glide/Portamento, Sustain Pedal, Pitch Bend,
     * Mod Wheel, and click-free Voice Stealing.
     */
    class VoiceManager
    {
    public:
        static constexpr int kStealHeadroomSlots = 8;
        static constexpr int kTotalVoiceSlots    = ProductInfo::kMaxPolyphony + kStealHeadroomSlots;

        void prepare(double sampleRate) noexcept;
        void resetAllVoices() noexcept;

        void noteOn(int midiNote, float velocity01, const CompiledSampleInstrument* sampleInstrument) noexcept;
        void noteOff(int midiNote) noexcept;
        void setSustainPedal(bool down) noexcept;
        void setPitchBend(float normalizedMinus1To1) noexcept;
        void setModWheel(float value01) noexcept;
        void setAftertouch(float value01) noexcept;
        void allNotesOff() noexcept;

        void setPerformanceParams(const PerformanceParams& perf,
                                  const SynthGlobalParams& synth,
                                  const SampleGlobalParams& sample) noexcept;

        void renderSample(const ModMatrix& modMatrix,
                          float lfo1Val,
                          float lfo2Val,
                          const ModOffsets& globalModOffsets,
                          float& outLeft,
                          float& outRight) noexcept;

        int getActiveSynthVoices() const noexcept { return activeSynthCount_; }
        int getActiveSampleVoices() const noexcept { return activeSampleCount_; }
        int getTotalActiveVoices() const noexcept { return activeTotalCount_; }

    private:
        struct VoiceSlot
        {
            SynthVoice  synthVoice;
            SampleVoice sampleVoice;

            bool     inUse          = false;
            bool     keyHeld        = false;
            bool     sustainHeld    = false;
            bool     stealing       = false;
            int      midiNote       = -1;
            uint64_t ageOrder       = 0;

            float    currentNote    = 60.0f;
            float    targetNote     = 60.0f;
            float    glideStepPerSample = 0.0f;

            bool isSounding() const noexcept
            {
                return synthVoice.isActive() || sampleVoice.isActive();
            }

            float maxEnvLevel() const noexcept
            {
                return std::max(synthVoice.getEnvLevel(), sampleVoice.getEnvLevel());
            }

            bool isReleasing() const noexcept
            {
                const bool sAct = synthVoice.isActive();
                const bool mAct = sampleVoice.isActive();
                if (!sAct && !mAct) return true;
                return (!sAct || synthVoice.isReleasing()) && (!mAct || sampleVoice.isReleasing());
            }
        };

        int  findFreeOrStealSlot() noexcept;
        void configureGlide(VoiceSlot& slot, float startNote, float targetNote, bool hadPreviousNote) noexcept;

        double                          sampleRate_ = 44100.0;
        std::array<VoiceSlot, kTotalVoiceSlots> slots_;

        PerformanceParams  perf_{};
        SynthGlobalParams  synthParams_{};
        SampleGlobalParams sampleParams_{};

        // Mono/Legato held-note stack
        static constexpr int kMaxHeldNotes = 32;
        struct HeldNote { int note = -1; float velocity = 0.8f; };
        std::array<HeldNote, kMaxHeldNotes> heldNotes_{};
        int      heldCount_        = 0;
        float    lastMonoNote_     = 60.0f;
        bool     hasLastMonoNote_  = false;

        bool     sustainPedalDown_ = false;
        float    pitchBendNorm_    = 0.0f;
        float    modWheel_         = 0.0f;
        float    aftertouch_       = 0.0f;
        uint64_t globalOrderCounter_ = 0;

        int      activeSynthCount_  = 0;
        int      activeSampleCount_ = 0;
        int      activeTotalCount_  = 0;
    };
}
