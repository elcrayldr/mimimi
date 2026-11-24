#include "Library/SampleEditorModel.h"
#include "DSP/Utilities/DspMath.h"
#include <algorithm>
#include <cmath>

namespace cray
{
    SampleEditorModel::SampleEditorModel(SampleDecoder& decoder, InstrumentDefinition& activeInstrument)
        : decoder_(decoder), instrument_(activeInstrument)
    {
    }

    void SampleEditorModel::setSelectedZoneIndex(int index)
    {
        if (instrument_.zones.empty())
        {
            selectedZoneIndex_ = -1;
            return;
        }
        selectedZoneIndex_ = std::clamp(index, 0, static_cast<int>(instrument_.zones.size()) - 1);
    }

    SampleZoneSpec* SampleEditorModel::getSelectedZone()
    {
        if (selectedZoneIndex_ < 0 || selectedZoneIndex_ >= static_cast<int>(instrument_.zones.size()))
            return nullptr;
        return &instrument_.zones[static_cast<size_t>(selectedZoneIndex_)];
    }

    const SampleZoneSpec* SampleEditorModel::getSelectedZone() const
    {
        if (selectedZoneIndex_ < 0 || selectedZoneIndex_ >= static_cast<int>(instrument_.zones.size()))
            return nullptr;
        return &instrument_.zones[static_cast<size_t>(selectedZoneIndex_)];
    }

    std::shared_ptr<const SampleBuffer> SampleEditorModel::getSelectedSampleBuffer() const
    {
        const auto* zone = getSelectedZone();
        if (zone == nullptr)
            return nullptr;

        const auto res = decoder_.loadSample(juce::File(zone->sourceFilePath));
        return res.success ? res.buffer : nullptr;
    }

    void SampleEditorModel::notifyModified()
    {
        if (onInstrumentModified)
            onInstrumentModified();
    }

    void SampleEditorModel::setStartEnd(int startSample, int endSample)
    {
        auto* z = getSelectedZone();
        if (z == nullptr) return;

        auto buf = getSelectedSampleBuffer();
        const int maxSample = (buf != nullptr) ? (buf->numSamples - 1) : std::max(1, endSample);

        z->startSample = std::clamp(startSample, 0, std::max(0, maxSample - 1));
        z->endSample   = std::clamp(endSample, z->startSample + 1, std::max(z->startSample + 1, maxSample));

        if (z->loopStartSample < z->startSample) z->loopStartSample = z->startSample;
        if (z->loopEndSample > z->endSample || z->loopEndSample <= z->loopStartSample)
            z->loopEndSample = z->endSample;

        notifyModified();
    }

    void SampleEditorModel::setLoopEnabled(bool enabled)
    {
        auto* z = getSelectedZone();
        if (z == nullptr) return;
        z->loopEnabled = enabled;
        if (enabled && z->loopEndSample <= z->loopStartSample)
        {
            auto buf = getSelectedSampleBuffer();
            const int endS = (z->endSample > 0) ? z->endSample : (buf != nullptr ? buf->numSamples - 1 : 1);
            z->loopStartSample = z->startSample;
            z->loopEndSample   = endS;
        }
        notifyModified();
    }

    void SampleEditorModel::setLoopRange(int loopStartSample, int loopEndSample, int crossfadeSamples)
    {
        auto* z = getSelectedZone();
        if (z == nullptr) return;

        auto buf = getSelectedSampleBuffer();
        const int maxSample = (buf != nullptr) ? (buf->numSamples - 1) : std::max(2, loopEndSample);
        const int effEnd    = (z->endSample > 0) ? std::min(z->endSample, maxSample) : maxSample;

        z->loopStartSample = std::clamp(loopStartSample, z->startSample, std::max(z->startSample, effEnd - 2));
        z->loopEndSample   = std::clamp(loopEndSample, z->loopStartSample + 2, std::max(z->loopStartSample + 2, effEnd));
        z->loopCrossfade   = std::max(0, crossfadeSamples);
        notifyModified();
    }

    void SampleEditorModel::setReverse(bool reverse)
    {
        auto* z = getSelectedZone();
        if (z == nullptr) return;
        z->reverse = reverse;
        notifyModified();
    }

    void SampleEditorModel::setGainDb(float gainDb)
    {
        auto* z = getSelectedZone();
        if (z == nullptr) return;
        z->gainDb = DspMath::clamp(gainDb, -36.0f, 24.0f);
        notifyModified();
    }

    void SampleEditorModel::normalize(int regionStartSample, int regionEndSample, float targetCeilingDb)
    {
        auto* z = getSelectedZone();
        if (z == nullptr) return;

        auto buf = getSelectedSampleBuffer();
        if (buf == nullptr || buf->numSamples <= 0)
            return;

        const int rStart = (regionStartSample >= 0)
            ? std::clamp(regionStartSample, 0, buf->numSamples - 1)
            : std::clamp(z->startSample, 0, buf->numSamples - 1);

        const int defaultEnd = (z->endSample > 0) ? z->endSample : (buf->numSamples - 1);
        const int rEnd = (regionEndSample > rStart)
            ? std::clamp(regionEndSample, rStart + 1, buf->numSamples)
            : std::clamp(defaultEnd + 1, rStart + 1, buf->numSamples);

        float peak = 0.0f;
        for (int ch = 0; ch < buf->numChannels; ++ch)
            peak = std::max(peak, buf->audio.getMagnitude(ch, rStart, rEnd - rStart));

        if (peak > 1.0e-6f)
        {
            const float currentPeakDb = DspMath::gainToDb(peak);
            z->normalizeGainDb = DspMath::clamp(targetCeilingDb - currentPeakDb, -24.0f, 36.0f);
            notifyModified();
        }
    }

    void SampleEditorModel::setPitchAndFine(int pitchSemitones, float fineTuneCents)
    {
        auto* z = getSelectedZone();
        if (z == nullptr) return;
        z->pitchSemitones = std::clamp(pitchSemitones, -36, 36);
        z->fineTuneCents  = DspMath::clamp(fineTuneCents, -100.0f, 100.0f);
        notifyModified();
    }

    void SampleEditorModel::setPan(float panMinus1To1)
    {
        auto* z = getSelectedZone();
        if (z == nullptr) return;
        z->pan = DspMath::clamp(panMinus1To1, -1.0f, 1.0f);
        notifyModified();
    }

    void SampleEditorModel::setFades(int fadeInSamples, int fadeOutSamples)
    {
        auto* z = getSelectedZone();
        if (z == nullptr) return;
        z->fadeInSamples  = std::max(0, fadeInSamples);
        z->fadeOutSamples = std::max(0, fadeOutSamples);
        notifyModified();
    }

    void SampleEditorModel::setAdsr(float attackSec, float decaySec, float sustainLevel, float releaseSec)
    {
        auto* z = getSelectedZone();
        if (z == nullptr) return;
        z->attackSec    = DspMath::clamp(attackSec, 0.001f, 8.0f);
        z->decaySec     = DspMath::clamp(decaySec, 0.002f, 10.0f);
        z->sustainLevel = DspMath::clamp(sustainLevel, 0.0f, 1.0f);
        z->releaseSec   = DspMath::clamp(releaseSec, 0.002f, 12.0f);
        notifyModified();
    }

    void SampleEditorModel::setFilter(bool enabled, SvfFilterType type, float cutoffHz, float resonance01)
    {
        auto* z = getSelectedZone();
        if (z == nullptr) return;
        z->filterEnabled   = enabled;
        z->filterType      = type;
        z->filterCutoffHz  = DspMath::clamp(cutoffHz, 20.0f, 20000.0f);
        z->filterResonance = DspMath::clamp(resonance01, 0.0f, 0.96f);
        notifyModified();
    }
}
