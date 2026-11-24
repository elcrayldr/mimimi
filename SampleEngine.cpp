#include "Engine/SampleEngine.h"
#include "DSP/Utilities/DspMath.h"
#include <algorithm>
#include <cmath>

namespace cray
{
    void SampleVoice::prepare(double sampleRate) noexcept
    {
        sampleRate_ = (sampleRate > 1.0) ? sampleRate : 44100.0;
        ampEnv_.prepare(sampleRate_);
        filterEnv_.prepare(sampleRate_);
        globalFilter_.prepare(sampleRate_);
        for (auto& p : players_)
        {
            p.env.prepare(sampleRate_);
            p.filter.prepare(sampleRate_);
        }
        reset();
    }

    void SampleVoice::reset() noexcept
    {
        ampEnv_.reset();
        filterEnv_.reset();
        globalFilter_.reset();
        globalFilterParamsValid_ = false;
        lastGlobalCutoff_ = 0.0f;
        lastGlobalResonance_ = 0.0f;
        for (auto& p : players_)
        {
            p.active = false;
            p.zone = nullptr;
            p.playhead = 0.0;
            p.samplesRendered = 0;
            p.env.reset();
            p.filter.reset();
            p.filterParamsValid = false;
            p.lastCutoffMod = 0.0f;
            p.lastResonanceMod = 0.0f;
            p.lastFilterType = SvfFilterType::LowPass12;
        }
        releasePinnedInstrument();
    }

    void SampleVoice::releasePinnedInstrument() noexcept
    {
        if (pinnedInstrument_ != nullptr)
        {
            pinnedInstrument_->releaseAudio();
            pinnedInstrument_ = nullptr;
        }
    }

    void SampleVoice::startNote(const CompiledSampleInstrument* instrument,
                                int midiNote,
                                float velocity01,
                                bool isLegato) noexcept
    {
        if (isLegato && isActive())
        {
            velocity_ = DspMath::clamp(velocity01, 0.01f, 1.0f);
            return;
        }

        reset();

        if (instrument == nullptr || instrument->zones.empty())
            return;

        pinnedInstrument_ = instrument;
        pinnedInstrument_->retainAudio();
        velocity_ = DspMath::clamp(velocity01, 0.01f, 1.0f);
        const int vel127 = std::clamp(static_cast<int>(std::round(velocity_ * 127.0f)), 1, 127);

        int slotIdx = 0;
        for (const auto& zone : instrument->zones)
        {
            if (slotIdx >= kMaxLayersPerVoice)
                break;

            if (zone.layerIndex >= 0 && static_cast<size_t>(zone.layerIndex) < instrument->layerEnabled.size())
            {
                if (!instrument->layerEnabled[static_cast<size_t>(zone.layerIndex)])
                    continue;
            }

            if (!zone.matchesNote(midiNote, vel127))
                continue;

            auto& p = players_[static_cast<size_t>(slotIdx)];
            p.zone = &zone;
            p.active = true;
            p.samplesRendered = 0;

            const int startS = std::clamp(zone.startSample, 0, zone.sample->numSamples - 2);
            const int endS   = std::clamp(zone.endSample, startS + 1, zone.sample->numSamples - 1);
            p.playhead = zone.reverse ? static_cast<double>(endS) : static_cast<double>(startS);

            p.env.setParameters(zone.attackSec, zone.decaySec, zone.sustainLevel, zone.releaseSec);
            p.env.noteOn();

            p.filter.reset();
            p.filterParamsValid = false;

            float lGain = 1.0f;
            if (zone.layerIndex >= 0 && static_cast<size_t>(zone.layerIndex) < instrument->layerGain.size())
                lGain = instrument->layerGain[static_cast<size_t>(zone.layerIndex)];
            layerGains_[slotIdx] = lGain;

            ++slotIdx;
        }

        if (slotIdx == 0)
            releasePinnedInstrument();
        else
        {
            ampEnv_.noteOn();
            filterEnv_.noteOn();
            globalFilter_.reset();
            globalFilterParamsValid_ = false;
        }
    }

    void SampleVoice::updateEnvelopes(const SampleGlobalParams& params) noexcept
    {
        ampEnv_.setParameters(params.ampAttack, params.ampDecay, params.ampSustain, params.ampRelease);
        filterEnv_.setParameters(params.filterAttack, params.filterDecay, params.filterSustain, params.filterRelease);
    }

    void SampleVoice::releaseNote() noexcept
    {
        ampEnv_.noteOff();
        filterEnv_.noteOff();
        for (auto& p : players_)
        {
            if (p.active)
                p.env.noteOff();
        }
    }

    void SampleVoice::fastKill() noexcept
    {
        ampEnv_.fastKill(0.004f);
        filterEnv_.fastKill(0.004f);
        for (auto& p : players_)
        {
            if (p.active)
                p.env.fastKill(0.004f);
        }
    }

    bool SampleVoice::isActive() const noexcept
    {
        if (!ampEnv_.isActive())
            return false;
        for (const auto& p : players_)
            if (p.active)
                return true;
        return false;
    }

    bool SampleVoice::isReleasing() const noexcept
    {
        if (ampEnv_.isActive() && !ampEnv_.isReleasing())
            return false;
        bool anyActive = false;
        for (const auto& p : players_)
        {
            if (p.active)
            {
                anyActive = true;
                if (!p.env.isReleasing())
                    return false;
            }
        }
        return anyActive;
    }

    float SampleVoice::getEnvLevel() const noexcept
    {
        float maxLevel = ampEnv_.getCurrentLevel();
        for (const auto& p : players_)
            if (p.active)
                maxLevel = std::max(maxLevel, p.env.getCurrentLevel() * ampEnv_.getCurrentLevel());
        return maxLevel;
    }

    float SampleVoice::readZoneSample(const SampleBuffer& buf, int channel, double pos) noexcept
    {
        const int n = buf.numSamples;
        if (n <= 0)
            return 0.0f;

        const int ch = std::clamp(channel, 0, buf.numChannels - 1);
        const float* data = buf.audio.getReadPointer(ch);

        const int i0 = static_cast<int>(std::floor(pos));
        const float frac = static_cast<float>(pos - static_cast<double>(i0));

        const int im1 = std::clamp(i0 - 1, 0, n - 1);
        const int ic0 = std::clamp(i0,     0, n - 1);
        const int i1  = std::clamp(i0 + 1, 0, n - 1);
        const int i2  = std::clamp(i0 + 2, 0, n - 1);

        return DspMath::cubicHermite(data[im1], data[ic0], data[i1], data[i2], frac);
    }

    void SampleVoice::renderSample(const SampleGlobalParams& globalParams,
                                   float currentNoteFloat,
                                   float pitchBendSemis,
                                   const ModOffsets& modOffsets,
                                   float& outLeft,
                                   float& outRight) noexcept
    {
        float sumL = 0.0f;
        float sumR = 0.0f;
        bool anyStillActive = false;

        if (!ampEnv_.isActive())
        {
            for (auto& player : players_)
                player.active = false;
            releasePinnedInstrument();
            outLeft = outRight = 0.0f;
            return;
        }

        const float globalAmpEnv = ampEnv_.getNextSample();
        const float globalFilterEnv = filterEnv_.getNextSample();
        const float velGain = (1.0f - globalParams.velSensitivity) + globalParams.velSensitivity * velocity_;
        const float matrixPitchSemis = modOffsets[static_cast<size_t>(ModDest::MasterPitch)] * 12.0f;

        for (int idx = 0; idx < kMaxLayersPerVoice; ++idx)
        {
            auto& p = players_[static_cast<size_t>(idx)];
            if (!p.active || p.zone == nullptr || p.zone->sample == nullptr)
                continue;

            const auto& z   = *p.zone;
            const auto& buf = *z.sample;

            const float envVal = p.env.getNextSample();
            if (!p.env.isActive())
            {
                p.active = false;
                continue;
            }

            const int effStart = std::clamp(z.startSample, 0, buf.numSamples - 2);
            const int effEnd   = std::clamp(z.endSample, effStart + 1, buf.numSamples - 1);

            if ((!z.reverse && p.playhead >= static_cast<double>(effEnd)) ||
                (z.reverse && p.playhead <= static_cast<double>(effStart)))
            {
                p.active = false;
                p.env.reset();
                continue;
            }

            float sampleL = readZoneSample(buf, 0, p.playhead);
            float sampleR = (buf.numChannels > 1) ? readZoneSample(buf, 1, p.playhead) : sampleL;

            // Smooth loop crossfade near loop end (forward playback)
            if (z.loopEnabled && !z.reverse)
            {
                const int lStart = std::clamp(z.loopStartSample, effStart, effEnd - 2);
                const int lEnd   = std::clamp(z.loopEndSample, lStart + 2, effEnd);
                const int xfade  = std::min(z.loopCrossfade, (lEnd - lStart) / 2);

                if (xfade > 4 && p.playhead >= static_cast<double>(lEnd - xfade) && lStart >= xfade)
                {
                    const double distFromXfadeStart = p.playhead - static_cast<double>(lEnd - xfade);
                    const float  alpha = DspMath::clamp(static_cast<float>(distFromXfadeStart / static_cast<double>(xfade)), 0.0f, 1.0f);
                    const double preLoopPos = static_cast<double>(lStart - xfade) + distFromXfadeStart;

                    const float altL = readZoneSample(buf, 0, preLoopPos);
                    const float altR = (buf.numChannels > 1) ? readZoneSample(buf, 1, preLoopPos) : altL;

                    sampleL = sampleL * (1.0f - alpha) + altL * alpha;
                    sampleR = sampleR * (1.0f - alpha) + altR * alpha;
                }
            }

            // Non-destructive Fade In & Fade Out
            float fadeGain = 1.0f;
            if (z.fadeInSamples > 0)
            {
                const double distFromEntry = z.reverse
                    ? (static_cast<double>(effEnd) - p.playhead)
                    : (p.playhead - static_cast<double>(effStart));
                if (distFromEntry < static_cast<double>(z.fadeInSamples))
                    fadeGain *= DspMath::clamp(static_cast<float>(distFromEntry / static_cast<double>(z.fadeInSamples)), 0.0f, 1.0f);
            }
            if (z.fadeOutSamples > 0 && !z.loopEnabled)
            {
                const double distToExit = z.reverse
                    ? (p.playhead - static_cast<double>(effStart))
                    : (static_cast<double>(effEnd) - p.playhead);
                if (distToExit < static_cast<double>(z.fadeOutSamples))
                    fadeGain *= DspMath::clamp(static_cast<float>(distToExit / static_cast<double>(z.fadeOutSamples)), 0.0f, 1.0f);
            }

            if (z.filterEnabled)
            {
                const float cutoffModInput = modOffsets[static_cast<size_t>(ModDest::FilterCutoff)];
                const float resonanceModInput = modOffsets[static_cast<size_t>(ModDest::FilterResonance)];

                if (!p.filterParamsValid
                    || cutoffModInput != p.lastCutoffMod
                    || resonanceModInput != p.lastResonanceMod
                    || z.filterType != p.lastFilterType)
                {
                    const float cutoffMod = z.filterCutoffHz * std::pow(2.0f, cutoffModInput * 4.0f);
                    const float resMod = DspMath::clamp(
                        z.filterResonance + resonanceModInput * 0.4f, 0.0f, 0.95f);

                    p.filter.setParameters(z.filterType, cutoffMod, resMod);
                    p.lastCutoffMod = cutoffModInput;
                    p.lastResonanceMod = resonanceModInput;
                    p.lastFilterType = z.filterType;
                    p.filterParamsValid = true;
                }

                p.filter.processStereo(sampleL, sampleR);
            }

            float panL = 1.0f, panR = 1.0f;
            const float combinedPan = DspMath::clamp(z.pan, -1.0f, 1.0f);
            DspMath::equalPowerPan(combinedPan, panL, panR);

            const float totalGain = z.gainLinear
                                  * layerGains_[idx]
                                  * globalParams.gainLinear
                                  * envVal
                                  * velGain
                                  * fadeGain
                                  * DspMath::clamp(1.0f + modOffsets[static_cast<size_t>(ModDest::AmpLevel)]
                                                       + (globalParams.lfo1ToAmp * globalParams.lfo1Value), 0.0f, 1.5f)
                                  * 1.4142f; // preserve unity gain at center pan

            sumL += sampleL * panL * totalGain;
            sumR += sampleR * panR * totalGain;

            // Advance playhead with resampling + pitch transposition
            const double srRatio = buf.sampleRate / sampleRate_;
            const double totalSemiOffset =
                (static_cast<double>(currentNoteFloat) - static_cast<double>(z.rootNote))
                + static_cast<double>(z.pitchSemitones + globalParams.pitchSemis)
                + (static_cast<double>(z.fineTuneCents + globalParams.fineCents) * 0.01)
                + static_cast<double>(pitchBendSemis + matrixPitchSemis
                                      + (globalParams.lfo1ToPitch * globalParams.lfo1Value * 2.0f));

            const double step = srRatio * DspMath::semitonesToRatioDouble(totalSemiOffset);
            if (z.reverse)
            {
                p.playhead -= step;
                if (z.loopEnabled)
                {
                    const int lStart = std::clamp(z.loopStartSample, effStart, effEnd - 2);
                    const int lEnd   = std::clamp(z.loopEndSample, lStart + 2, effEnd);
                    if (p.playhead < static_cast<double>(lStart))
                        p.playhead += static_cast<double>(lEnd - lStart);
                }
                else if (p.playhead <= static_cast<double>(effStart))
                {
                    p.active = false;
                    p.env.reset();
                    continue;
                }
            }
            else
            {
                p.playhead += step;
                if (z.loopEnabled)
                {
                    const int lStart = std::clamp(z.loopStartSample, effStart, effEnd - 2);
                    const int lEnd   = std::clamp(z.loopEndSample, lStart + 2, effEnd);
                    if (p.playhead >= static_cast<double>(lEnd))
                        p.playhead -= static_cast<double>(lEnd - lStart);
                }
                else if (p.playhead >= static_cast<double>(effEnd))
                {
                    p.active = false;
                    p.env.reset();
                    continue;
                }
            }

            ++p.samplesRendered;
            anyStillActive = true;
        }

        if (!anyStillActive)
        {
            releasePinnedInstrument();
            outLeft = outRight = 0.0f;
            return;
        }

        if (globalParams.filterEnabled)
        {
            const float keyTrackOctaves = ((currentNoteFloat - 60.0f) / 12.0f) * globalParams.filterKeyTrack;
            const float envOctaves = globalFilterEnv * globalParams.filterEnvAmount * 5.0f;
            const float lfoOctaves = globalParams.lfo1Value * globalParams.lfo1ToCutoff * 3.0f;
            const float matrixOctaves = modOffsets[static_cast<size_t>(ModDest::FilterCutoff)] * 5.0f;
            const float cutoff = DspMath::clamp(globalParams.filterCutoffHz
                                   * std::pow(2.0f, keyTrackOctaves + envOctaves + lfoOctaves + matrixOctaves),
                                   20.0f, static_cast<float>(sampleRate_ * 0.45));
            const float resonance = DspMath::clamp(globalParams.filterResonance
                                   + modOffsets[static_cast<size_t>(ModDest::FilterResonance)] * 0.5f,
                                   0.0f, 0.96f);
            if (!globalFilterParamsValid_ || cutoff != lastGlobalCutoff_ || resonance != lastGlobalResonance_)
            {
                globalFilter_.setParameters(globalParams.filterType, cutoff, resonance);
                lastGlobalCutoff_ = cutoff;
                lastGlobalResonance_ = resonance;
                globalFilterParamsValid_ = true;
            }
            globalFilter_.processStereo(sumL, sumR);
        }

        const float globalPan = DspMath::clamp(globalParams.pan
                                  + modOffsets[static_cast<size_t>(ModDest::Pan)], -1.0f, 1.0f);
        float globalPanL = 1.0f, globalPanR = 1.0f;
        DspMath::equalPowerPan(globalPan, globalPanL, globalPanR);
        outLeft = sumL * globalPanL * globalAmpEnv;
        outRight = sumR * globalPanR * globalAmpEnv;
    }
}
