#include "Engine/VoiceManager.h"
#include "DSP/Utilities/DspMath.h"
#include <algorithm>
#include <cmath>

namespace cray
{
    void VoiceManager::prepare(double sampleRate) noexcept
    {
        sampleRate_ = (sampleRate > 1.0) ? sampleRate : 44100.0;
        for (auto& s : slots_)
        {
            s.synthVoice.prepare(sampleRate_);
            s.sampleVoice.prepare(sampleRate_);
        }
        resetAllVoices();
    }

    void VoiceManager::resetAllVoices() noexcept
    {
        for (auto& s : slots_)
        {
            s.synthVoice.reset();
            s.sampleVoice.reset();
            s.inUse = false;
            s.keyHeld = false;
            s.sustainHeld = false;
            s.stealing = false;
            s.midiNote = -1;
        }
        heldCount_ = 0;
        hasLastMonoNote_ = false;
        sustainPedalDown_ = false;
        activeSynthCount_ = 0;
        activeSampleCount_ = 0;
        activeTotalCount_ = 0;
    }

    void VoiceManager::setPerformanceParams(const PerformanceParams& perf,
                                            const SynthGlobalParams& synth,
                                            const SampleGlobalParams& sample) noexcept
    {
        perf_ = perf;
        perf_.maxPolyphony = std::clamp(perf_.maxPolyphony, 1, ProductInfo::kMaxPolyphony);
        synthParams_ = synth;
        sampleParams_ = sample;

        for (auto& s : slots_)
        {
            if (s.inUse)
            {
                s.synthVoice.updateEnvelopes(synthParams_);
                s.sampleVoice.updateEnvelopes(sampleParams_);
            }
        }
    }

    void VoiceManager::configureGlide(VoiceSlot& slot, float startNote, float targetNote, bool hadPreviousNote) noexcept
    {
        slot.targetNote = targetNote;
        const bool shouldGlide = (perf_.glideMs > 0.5f)
                              && (!perf_.glideLegatoOnly || hadPreviousNote);

        if (!shouldGlide || std::abs(targetNote - startNote) < 1.0e-4f)
        {
            slot.currentNote = targetNote;
            slot.glideStepPerSample = 0.0f;
            return;
        }

        slot.currentNote = startNote;
        const float samples = std::max(1.0f, (perf_.glideMs * 0.001f) * static_cast<float>(sampleRate_));
        slot.glideStepPerSample = (targetNote - startNote) / samples;
    }

    int VoiceManager::findFreeOrStealSlot() noexcept
    {
        int activeNonStealing = 0;
        int firstIdleIdx = -1;

        for (int i = 0; i < kTotalVoiceSlots; ++i)
        {
            auto& s = slots_[static_cast<size_t>(i)];
            if (s.inUse && !s.isSounding())
            {
                s.inUse = false;
                s.stealing = false;
                s.midiNote = -1;
            }

            if (!s.inUse)
            {
                if (firstIdleIdx < 0)
                    firstIdleIdx = i;
            }
            else if (!s.stealing)
            {
                ++activeNonStealing;
            }
        }

        if (activeNonStealing >= perf_.maxPolyphony)
        {
            // Pick best candidate to fast-kill: prefer releasing & quietest, otherwise oldest
            int bestIdx = -1;
            bool bestIsReleasing = false;
            float bestLevel = 999.0f;
            uint64_t oldestOrder = UINT64_MAX;

            for (int i = 0; i < kTotalVoiceSlots; ++i)
            {
                const auto& s = slots_[static_cast<size_t>(i)];
                if (!s.inUse || s.stealing)
                    continue;

                const bool rel = s.isReleasing();
                const float lvl = s.maxEnvLevel();

                if (bestIdx < 0)
                {
                    bestIdx = i;
                    bestIsReleasing = rel;
                    bestLevel = lvl;
                    oldestOrder = s.ageOrder;
                    continue;
                }

                if (rel && !bestIsReleasing)
                {
                    bestIdx = i;
                    bestIsReleasing = true;
                    bestLevel = lvl;
                    oldestOrder = s.ageOrder;
                }
                else if (rel == bestIsReleasing)
                {
                    if ((rel && lvl < bestLevel) || (!rel && s.ageOrder < oldestOrder))
                    {
                        bestIdx = i;
                        bestLevel = lvl;
                        oldestOrder = s.ageOrder;
                    }
                }
            }

            if (bestIdx >= 0)
            {
                auto& victim = slots_[static_cast<size_t>(bestIdx)];
                victim.stealing = true;
                victim.keyHeld = false;
                victim.sustainHeld = false;
                victim.synthVoice.fastKill();
                victim.sampleVoice.fastKill();
            }
        }

        if (firstIdleIdx >= 0)
            return firstIdleIdx;

        // Emergency fallback if all 136 slots are busy (including steal slots): reuse quietest slot directly
        int fallbackIdx = 0;
        float lowestLvl = 999.0f;
        for (int i = 0; i < kTotalVoiceSlots; ++i)
        {
            const float lvl = slots_[static_cast<size_t>(i)].maxEnvLevel();
            if (lvl < lowestLvl)
            {
                lowestLvl = lvl;
                fallbackIdx = i;
            }
        }
        return fallbackIdx;
    }

    void VoiceManager::noteOn(int midiNote, float velocity01, const CompiledSampleInstrument* sampleInstrument) noexcept
    {
        const float vel = DspMath::clamp(velocity01, 0.01f, 1.0f);
        const bool isMonoOrLegato = (perf_.voiceMode != VoicePlayMode::Poly);

        if (isMonoOrLegato)
        {
            const bool hadHeld = (heldCount_ > 0);
            // Push/update note stack
            for (int i = 0; i < heldCount_; ++i)
            {
                if (heldNotes_[static_cast<size_t>(i)].note == midiNote)
                {
                    for (int j = i; j < heldCount_ - 1; ++j)
                        heldNotes_[static_cast<size_t>(j)] = heldNotes_[static_cast<size_t>(j + 1)];
                    --heldCount_;
                    break;
                }
            }
            if (heldCount_ >= kMaxHeldNotes)
            {
                for (int j = 0; j < kMaxHeldNotes - 1; ++j)
                    heldNotes_[static_cast<size_t>(j)] = heldNotes_[static_cast<size_t>(j + 1)];
                heldCount_ = kMaxHeldNotes - 1;
            }
            heldNotes_[static_cast<size_t>(heldCount_++)] = { midiNote, vel };

            // Find active mono voice slot if any
            int monoIdx = -1;
            for (int i = 0; i < kTotalVoiceSlots; ++i)
            {
                if (slots_[static_cast<size_t>(i)].inUse && !slots_[static_cast<size_t>(i)].stealing)
                {
                    monoIdx = i;
                    break;
                }
            }

            if (monoIdx < 0)
                monoIdx = findFreeOrStealSlot();

            auto& slot = slots_[static_cast<size_t>(monoIdx)];
            const bool isLegatoTransition = (perf_.voiceMode == VoicePlayMode::Legato) && hadHeld && slot.isSounding();
            const float startNote = hasLastMonoNote_ ? lastMonoNote_ : static_cast<float>(midiNote);

            slot.inUse = true;
            slot.keyHeld = true;
            slot.sustainHeld = false;
            slot.stealing = false;
            slot.midiNote = midiNote;
            slot.ageOrder = ++globalOrderCounter_;
            configureGlide(slot, startNote, static_cast<float>(midiNote), hadHeld);

            lastMonoNote_ = static_cast<float>(midiNote);
            hasLastMonoNote_ = true;

            const uint32_t seed = static_cast<uint32_t>(globalOrderCounter_ * 2654435761u) ^ static_cast<uint32_t>(midiNote);
            if (perf_.engineMode != EngineMode::SampleOnly)
            {
                slot.synthVoice.updateEnvelopes(synthParams_);
                slot.synthVoice.startNote(midiNote, vel, slot.currentNote, isLegatoTransition, seed);
            }
            if (perf_.engineMode != EngineMode::SynthOnly)
            {
                slot.sampleVoice.startNote(sampleInstrument, midiNote, vel, isLegatoTransition);
            }
            return;
        }

        // Poly mode
        const int idx = findFreeOrStealSlot();
        auto& slot = slots_[static_cast<size_t>(idx)];
        const float startNote = hasLastMonoNote_ ? lastMonoNote_ : static_cast<float>(midiNote);

        slot.inUse = true;
        slot.keyHeld = true;
        slot.sustainHeld = false;
        slot.stealing = false;
        slot.midiNote = midiNote;
        slot.ageOrder = ++globalOrderCounter_;
        configureGlide(slot, startNote, static_cast<float>(midiNote), hasLastMonoNote_);

        lastMonoNote_ = static_cast<float>(midiNote);
        hasLastMonoNote_ = true;

        const uint32_t seed = static_cast<uint32_t>(globalOrderCounter_ * 2654435761u) ^ static_cast<uint32_t>(midiNote);
        if (perf_.engineMode != EngineMode::SampleOnly)
        {
            slot.synthVoice.reset();
            slot.synthVoice.updateEnvelopes(synthParams_);
            slot.synthVoice.startNote(midiNote, vel, slot.currentNote, false, seed);
        }
        else
        {
            slot.synthVoice.reset();
        }

        if (perf_.engineMode != EngineMode::SynthOnly)
        {
            slot.sampleVoice.startNote(sampleInstrument, midiNote, vel, false);
        }
        else
        {
            slot.sampleVoice.reset();
        }
    }

    void VoiceManager::noteOff(int midiNote) noexcept
    {
        if (perf_.voiceMode != VoicePlayMode::Poly)
        {
            // Remove from held note stack
            for (int i = 0; i < heldCount_; ++i)
            {
                if (heldNotes_[static_cast<size_t>(i)].note == midiNote)
                {
                    for (int j = i; j < heldCount_ - 1; ++j)
                        heldNotes_[static_cast<size_t>(j)] = heldNotes_[static_cast<size_t>(j + 1)];
                    --heldCount_;
                    break;
                }
            }

            for (auto& slot : slots_)
            {
                if (!slot.inUse || slot.stealing)
                    continue;

                if (heldCount_ > 0)
                {
                    // Return smoothly to previous held note
                    const auto prev = heldNotes_[static_cast<size_t>(heldCount_ - 1)];
                    if (slot.midiNote != prev.note)
                    {
                        configureGlide(slot, slot.currentNote, static_cast<float>(prev.note), true);
                        slot.midiNote = prev.note;
                        lastMonoNote_ = static_cast<float>(prev.note);
                    }
                }
                else
                {
                    slot.keyHeld = false;
                    if (sustainPedalDown_)
                    {
                        slot.sustainHeld = true;
                    }
                    else
                    {
                        slot.synthVoice.releaseNote();
                        slot.sampleVoice.releaseNote();
                    }
                }
            }
            return;
        }

        for (auto& slot : slots_)
        {
            if (slot.inUse && !slot.stealing && slot.keyHeld && slot.midiNote == midiNote)
            {
                slot.keyHeld = false;
                if (sustainPedalDown_)
                {
                    slot.sustainHeld = true;
                }
                else
                {
                    slot.synthVoice.releaseNote();
                    slot.sampleVoice.releaseNote();
                }
            }
        }
    }

    void VoiceManager::setSustainPedal(bool down) noexcept
    {
        sustainPedalDown_ = down;
        if (!down)
        {
            for (auto& slot : slots_)
            {
                if (slot.inUse && !slot.stealing && slot.sustainHeld && !slot.keyHeld)
                {
                    slot.sustainHeld = false;
                    slot.synthVoice.releaseNote();
                    slot.sampleVoice.releaseNote();
                }
            }
        }
    }

    void VoiceManager::setPitchBend(float normalizedMinus1To1) noexcept
    {
        pitchBendNorm_ = DspMath::clamp(normalizedMinus1To1, -1.0f, 1.0f);
    }

    void VoiceManager::setModWheel(float value01) noexcept
    {
        modWheel_ = DspMath::clamp(value01, 0.0f, 1.0f);
    }

    void VoiceManager::setAftertouch(float value01) noexcept
    {
        aftertouch_ = DspMath::clamp(value01, 0.0f, 1.0f);
    }

    void VoiceManager::allNotesOff() noexcept
    {
        heldCount_ = 0;
        sustainPedalDown_ = false;
        for (auto& slot : slots_)
        {
            if (slot.inUse)
            {
                slot.keyHeld = false;
                slot.sustainHeld = false;
                slot.synthVoice.releaseNote();
                slot.sampleVoice.releaseNote();
            }
        }
    }

    void VoiceManager::renderSample(const ModMatrix& modMatrix,
                                    float lfo1Val,
                                    float lfo2Val,
                                    const ModOffsets& globalModOffsets,
                                    float& outLeft,
                                    float& outRight) noexcept
    {
        float sumL = 0.0f;
        float sumR = 0.0f;
        int sCount = 0;
        int mCount = 0;
        int tCount = 0;

        const float pbSemis = pitchBendNorm_ * static_cast<float>(perf_.pitchBendRange);
        const bool  runSynth  = (perf_.engineMode != EngineMode::SampleOnly) && (perf_.synthMix > 0.0005f);
        const bool  runSample = (perf_.engineMode != EngineMode::SynthOnly) && (perf_.sampleMix > 0.0005f);

        for (auto& slot : slots_)
        {
            if (!slot.inUse)
                continue;

            // Advance glide
            if (slot.glideStepPerSample != 0.0f)
            {
                slot.currentNote += slot.glideStepPerSample;
                if ((slot.glideStepPerSample > 0.0f && slot.currentNote >= slot.targetNote) ||
                    (slot.glideStepPerSample < 0.0f && slot.currentNote <= slot.targetNote))
                {
                    slot.currentNote = slot.targetNote;
                    slot.glideStepPerSample = 0.0f;
                }
            }

            bool voiceStillSounding = false;

            if (runSynth && slot.synthVoice.isActive())
            {
                float sL = 0.0f, sR = 0.0f;
                slot.synthVoice.renderSample(synthParams_, modMatrix, slot.currentNote, pbSemis,
                                             lfo1Val, lfo2Val, modWheel_, aftertouch_, sL, sR);
                sumL += sL * perf_.synthMix;
                sumR += sR * perf_.synthMix;
                if (slot.synthVoice.isActive())
                {
                    ++sCount;
                    voiceStillSounding = true;
                }
            }

            if (runSample && slot.sampleVoice.isActive())
            {
                float mL = 0.0f, mR = 0.0f;
                ModInputContext sampleCtx;
                sampleCtx.lfo1 = lfo1Val;
                sampleCtx.lfo2 = lfo2Val;
                sampleCtx.ampEnv = slot.sampleVoice.getEnvLevel();
                sampleCtx.filterEnv = slot.sampleVoice.getFilterEnvLevel();
                sampleCtx.modWheel = modWheel_;
                sampleCtx.velocity = slot.sampleVoice.getVelocity();
                sampleCtx.keyTrack = (slot.currentNote - 60.0f) / 60.0f;
                sampleCtx.pitchBend = pbSemis / 24.0f;
                sampleCtx.aftertouch = aftertouch_;
                ModOffsets sampleOffsets{};
                modMatrix.evaluate(sampleCtx, sampleOffsets);
                auto sampleParams = sampleParams_;
                sampleParams.lfo1Value = lfo1Val;
                slot.sampleVoice.renderSample(sampleParams, slot.currentNote, pbSemis, sampleOffsets, mL, mR);
                sumL += mL * perf_.sampleMix;
                sumR += mR * perf_.sampleMix;
                if (slot.sampleVoice.isActive())
                {
                    ++mCount;
                    voiceStillSounding = true;
                }
            }

            if (voiceStillSounding)
            {
                ++tCount;
            }
            else
            {
                slot.inUse = false;
                slot.stealing = false;
                slot.midiNote = -1;
                slot.sampleVoice.reset();
            }
        }

        activeSynthCount_  = sCount;
        activeSampleCount_ = mCount;
        activeTotalCount_  = tCount;

        outLeft  = sumL;
        outRight = sumR;
    }
}
