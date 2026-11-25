#pragma once

#include "Library/InstrumentDefinition.h"
#include "Library/SampleDecoder.h"
#include <functional>

namespace cray
{
    /**
     * Non-destructive visual Sample Editor controller.
     * Edits parameters on a selected SampleZoneSpec without ever touching the source audio file,
     * and notifies the audio engine immediately for real-time auditioning.
     */
    class SampleEditorModel
    {
    public:
        SampleEditorModel(SampleDecoder& decoder, InstrumentDefinition& activeInstrument);

        void setSelectedZoneIndex(int index);
        int  getSelectedZoneIndex() const noexcept { return selectedZoneIndex_; }

        SampleZoneSpec*       getSelectedZone();
        const SampleZoneSpec* getSelectedZone() const;

        std::shared_ptr<const SampleBuffer> getSelectedSampleBuffer() const;

        void setStartEnd(int startSample, int endSample);
        void setLoopEnabled(bool enabled);
        void setLoopRange(int loopStartSample, int loopEndSample, int crossfadeSamples = 256);
        void setReverse(bool reverse);
        void setGainDb(float gainDb);
        void normalize(int regionStartSample = -1, int regionEndSample = -1, float targetCeilingDb = -0.3f);
        void setRootNote(int midiNote);
        void setPitchAndFine(int pitchSemitones, float fineTuneCents);
        void setPan(float panMinus1To1);
        void setFades(int fadeInSamples, int fadeOutSamples);
        void setAdsr(float attackSec, float decaySec, float sustainLevel, float releaseSec);
        void setFilter(bool enabled, SvfFilterType type, float cutoffHz, float resonance01);

        /** Invoked immediately after any non-destructive edit so audio playback updates live. */
        std::function<void()> onInstrumentModified;

    private:
        void notifyModified();

        SampleDecoder&        decoder_;
        InstrumentDefinition& instrument_;
        int                   selectedZoneIndex_ = 0;
    };
}
