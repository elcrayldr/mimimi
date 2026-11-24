#include "Engine/AudioEngine.h"
#include "Common/ParameterIDs.h"
#include "DSP/Utilities/DspMath.h"
#include <algorithm>
#include <cmath>

namespace cray
{
    namespace
    {
        float loadParam(const juce::AudioProcessorValueTreeState& apvts, const juce::String& id, float fallback = 0.0f) noexcept
        {
            if (auto* p = apvts.getRawParameterValue(id))
                return p->load(std::memory_order_relaxed);
            return fallback;
        }
    }

    void AudioEngine::prepare(double sampleRate, int /*samplesPerBlock*/)
    {
        sampleRate_ = (sampleRate > 1.0) ? sampleRate : 44100.0;
        voiceManager_.prepare(sampleRate_);
        effectsChain_.prepare(sampleRate_);
        for (auto& lfo : lfos_)
            lfo.prepare(sampleRate_);
        masterGainSmoother_.prepare(sampleRate_, 0.015);
        masterGainSmoother_.reset(DspMath::dbToGain(-3.0f));
    }

    void AudioEngine::reset()
    {
        voiceManager_.resetAllVoices();
        effectsChain_.reset();
        for (auto& lfo : lfos_)
            lfo.reset();
        uiMidiQueue_.clear();
    }

    void AudioEngine::setSampleInstrument(std::shared_ptr<const CompiledSampleInstrument> newInstrument)
    {
        // Publication and reclamation can originate from different non-audio threads.
        const std::lock_guard<std::mutex> ownershipLock(instrumentOwnerMutex_);
        if (currentInstrumentOwner_ != nullptr)
            retiredInstruments_.push_back(currentInstrumentOwner_);

        currentInstrumentOwner_ = std::move(newInstrument);
        activeInstrumentPtr_.store(currentInstrumentOwner_.get(), std::memory_order_release);
    }

    void AudioEngine::collectGarbage()
    {
        const std::lock_guard<std::mutex> ownershipLock(instrumentOwnerMutex_);
        const auto hazard = audioHazardPtr_.load(std::memory_order_seq_cst);
        retiredInstruments_.erase(
            std::remove_if(retiredInstruments_.begin(), retiredInstruments_.end(),
                [hazard](const std::shared_ptr<const CompiledSampleInstrument>& inst)
                {
                    return inst == nullptr
                        || (inst.get() != hazard
                            && inst->audioRefCount.load(std::memory_order_acquire) <= 0);
                }),
            retiredInstruments_.end());
    }

    void AudioEngine::pushUiMidiEvent(int midiNote, float velocity01, bool isNoteOn) noexcept
    {
        AuditionEvent ev;
        ev.midiNote = std::clamp(midiNote, 0, 127);
        ev.velocity = DspMath::clamp(velocity01, 0.01f, 1.0f);
        ev.isNoteOn = isNoteOn;
        uiMidiQueue_.push(ev);
    }

    void AudioEngine::syncParameters(const juce::AudioProcessorValueTreeState& apvts, double hostBpm) noexcept
    {
        PerformanceParams perf;
        perf.engineMode      = static_cast<EngineMode>(static_cast<int>(loadParam(apvts, ParamIDs::engineMode, 2.0f)));
        perf.synthMix        = loadParam(apvts, ParamIDs::synthMix, 0.85f);
        perf.sampleMix       = loadParam(apvts, ParamIDs::sampleMix, 0.85f);
        perf.voiceMode       = static_cast<VoicePlayMode>(static_cast<int>(loadParam(apvts, ParamIDs::voiceMode, 0.0f)));
        perf.maxPolyphony    = static_cast<int>(loadParam(apvts, ParamIDs::polyphony, 32.0f));
        perf.glideMs         = loadParam(apvts, ParamIDs::glideMs, 0.0f);
        perf.glideLegatoOnly = loadParam(apvts, ParamIDs::glideLegatoOnly, 0.0f) >= 0.5f;
        perf.pitchBendRange  = static_cast<int>(loadParam(apvts, ParamIDs::pitchBendRange, 2.0f));

        SynthGlobalParams sp;
        for (int i = 0; i < ProductInfo::kNumOscillators; ++i)
        {
            auto& op = sp.oscs[static_cast<size_t>(i)];
            op.enabled   = loadParam(apvts, ParamIDs::oscEnabled(i), i == 0 ? 1.0f : 0.0f) >= 0.5f;
            op.waveform  = static_cast<OscWaveform>(static_cast<int>(loadParam(apvts, ParamIDs::oscWave(i), 1.0f)));
            op.level     = loadParam(apvts, ParamIDs::oscLevel(i), 0.5f);
            op.pan       = loadParam(apvts, ParamIDs::oscPan(i), 0.0f);
            op.pitchSemi = static_cast<int>(loadParam(apvts, ParamIDs::oscPitch(i), 0.0f));
            op.fineCents = loadParam(apvts, ParamIDs::oscFine(i), 0.0f);
            op.unison    = static_cast<int>(loadParam(apvts, ParamIDs::oscUnison(i), 1.0f));
            op.detune    = loadParam(apvts, ParamIDs::oscDetune(i), 0.25f);
            op.spread    = loadParam(apvts, ParamIDs::oscSpread(i), 0.5f);
        }

        sp.ampAttack       = loadParam(apvts, ParamIDs::ampAttack, 0.006f);
        sp.ampDecay        = loadParam(apvts, ParamIDs::ampDecay, 0.30f);
        sp.ampSustain      = loadParam(apvts, ParamIDs::ampSustain, 0.80f);
        sp.ampRelease      = loadParam(apvts, ParamIDs::ampRelease, 0.25f);

        sp.filterEnabled   = loadParam(apvts, ParamIDs::filterEnabled, 1.0f) >= 0.5f;
        sp.filterType      = static_cast<SvfFilterType>(static_cast<int>(loadParam(apvts, ParamIDs::filterType, 1.0f)));
        sp.filterCutoffHz  = loadParam(apvts, ParamIDs::filterCutoff, 14000.0f);
        sp.filterResonance = loadParam(apvts, ParamIDs::filterResonance, 0.15f);
        sp.filterKeyTrack  = loadParam(apvts, ParamIDs::filterKeyTrack, 0.25f);
        sp.filterEnvAmount = loadParam(apvts, ParamIDs::filterEnvAmount, 0.35f);

        sp.filterAttack    = loadParam(apvts, ParamIDs::filterAttack, 0.01f);
        sp.filterDecay     = loadParam(apvts, ParamIDs::filterDecay, 0.40f);
        sp.filterSustain   = loadParam(apvts, ParamIDs::filterSustain, 0.35f);
        sp.filterRelease   = loadParam(apvts, ParamIDs::filterRelease, 0.35f);

        sp.velSensitivity  = loadParam(apvts, ParamIDs::velocitySensitivity, 0.75f);
        sp.lfo1ToPitch     = loadParam(apvts, ParamIDs::lfo1ToPitch, 0.0f);
        sp.lfo1ToCutoff    = loadParam(apvts, ParamIDs::lfo1ToCutoff, 0.0f);
        sp.lfo1ToAmp       = loadParam(apvts, ParamIDs::lfo1ToAmp, 0.0f);

        SampleGlobalParams smp;
        smp.gainLinear     = DspMath::dbToGain(loadParam(apvts, ParamIDs::sampleGainDb, 0.0f));
        smp.pan            = loadParam(apvts, ParamIDs::samplePan, 0.0f);
        smp.pitchSemis     = static_cast<int>(loadParam(apvts, ParamIDs::samplePitch, 0.0f));
        smp.fineCents      = loadParam(apvts, ParamIDs::sampleFine, 0.0f);
        smp.velSensitivity = sp.velSensitivity;

        voiceManager_.setPerformanceParams(perf, sp, smp);

        // Mod Matrix
        for (int s = 0; s < ProductInfo::kNumModSlots; ++s)
        {
            const auto src = static_cast<ModSource>(static_cast<int>(loadParam(apvts, ParamIDs::modSource(s), 0.0f)));
            const auto dst = static_cast<ModDest>(static_cast<int>(loadParam(apvts, ParamIDs::modDest(s), 0.0f)));
            const float amt = loadParam(apvts, ParamIDs::modAmount(s), 0.0f);
            modMatrix_.setSlot(s, src, dst, amt);
        }

        // LFOs (note: global ModOffsets can modulate Lfo1Rate/Lfo2Rate on next block)
        for (int i = 0; i < ProductInfo::kNumLfos; ++i)
        {
            const auto wave = static_cast<LfoWaveform>(static_cast<int>(loadParam(apvts, ParamIDs::lfoWave(i), 0.0f)));
            const float rate = loadParam(apvts, ParamIDs::lfoRateHz(i), 2.0f);
            const bool sync  = loadParam(apvts, ParamIDs::lfoSync(i), 0.0f) >= 0.5f;
            const auto div   = static_cast<TempoSyncDivision>(static_cast<int>(loadParam(apvts, ParamIDs::lfoSyncDiv(i), 4.0f)));
            const float dep  = loadParam(apvts, ParamIDs::lfoDepth(i), 1.0f);
            const bool retr  = loadParam(apvts, ParamIDs::lfoRetrigger(i), 0.0f) >= 0.5f;
            lfos_[static_cast<size_t>(i)].setParameters(wave, rate, sync, div, hostBpm, dep, retr);
        }

        const float masterDb = loadParam(apvts, ParamIDs::masterGainDb, -3.0f);
        masterGainSmoother_.setTarget(DspMath::dbToGain(masterDb));
    }

    void AudioEngine::processBlock(juce::AudioBuffer<float>& buffer,
                                   juce::MidiBuffer& midiMessages,
                                   const juce::AudioProcessorValueTreeState& apvts,
                                   double hostBpm,
                                   AudioTelemetry& telemetry)
    {
        const int numSamples = buffer.getNumSamples();
        if (numSamples <= 0 || buffer.getNumChannels() <= 0)
            return;

        buffer.clear();
        syncParameters(apvts, hostBpm);

        // Acquire a hazard before dereferencing the published raw pointer. The message
        // thread may retire snapshots concurrently; collectGarbage() must not free the
        // selected snapshot until this block has either pinned it in a voice or finished.
        const CompiledSampleInstrument* sampleInst = nullptr;
        do
        {
            sampleInst = activeInstrumentPtr_.load(std::memory_order_seq_cst);
            audioHazardPtr_.store(sampleInst, std::memory_order_seq_cst);
        }
        while (sampleInst != activeInstrumentPtr_.load(std::memory_order_seq_cst));

        // Drain UI Virtual Keyboard / Sample Audition queue at start of block
        AuditionEvent uiEv;
        while (uiMidiQueue_.pop(uiEv))
        {
            if (uiEv.isNoteOn)
            {
                for (auto& lfo : lfos_) lfo.retrigger();
                voiceManager_.noteOn(uiEv.midiNote, uiEv.velocity, sampleInst);
                telemetry.midiActivityCounter.fetch_add(1, std::memory_order_relaxed);
                telemetry.lastTriggeredMidiNote.store(uiEv.midiNote, std::memory_order_relaxed);
            }
            else
            {
                voiceManager_.noteOff(uiEv.midiNote);
            }
        }

        // Evaluate global mod offsets for effects & sample engine
        ModInputContext globalCtx;
        globalCtx.lfo1       = lfos_[0].getCurrentValue();
        globalCtx.lfo2       = lfos_[1].getCurrentValue();
        globalCtx.modWheel   = modWheelCache_;
        globalCtx.pitchBend  = pitchBendCache_;
        globalCtx.aftertouch = aftertouchCache_;

        ModOffsets globalModOffsets{};
        modMatrix_.evaluate(globalCtx, globalModOffsets);
        effectsChain_.updateFromParameters(apvts, hostBpm, globalModOffsets);

        float* left  = buffer.getWritePointer(0);
        float* right = (buffer.getNumChannels() > 1) ? buffer.getWritePointer(1) : left;

        auto midiIt = midiMessages.cbegin();
        const auto midiEnd = midiMessages.cend();

        for (int sampleIdx = 0; sampleIdx < numSamples; ++sampleIdx)
        {
            while (midiIt != midiEnd && (*midiIt).samplePosition <= sampleIdx)
            {
                const auto msg = (*midiIt).getMessage();
                if (msg.isNoteOn())
                {
                    for (auto& lfo : lfos_) lfo.retrigger();
                    voiceManager_.noteOn(msg.getNoteNumber(), msg.getFloatVelocity(), sampleInst);
                    telemetry.midiActivityCounter.fetch_add(1, std::memory_order_relaxed);
                    telemetry.lastTriggeredMidiNote.store(msg.getNoteNumber(), std::memory_order_relaxed);
                }
                else if (msg.isNoteOff())
                {
                    voiceManager_.noteOff(msg.getNoteNumber());
                }
                else if (msg.isSustainPedalOn())
                {
                    voiceManager_.setSustainPedal(true);
                }
                else if (msg.isSustainPedalOff())
                {
                    voiceManager_.setSustainPedal(false);
                }
                else if (msg.isPitchWheel())
                {
                    pitchBendCache_ = (static_cast<float>(msg.getPitchWheelValue()) - 8192.0f) / 8192.0f;
                    voiceManager_.setPitchBend(pitchBendCache_);
                }
                else if (msg.isController() && msg.getControllerNumber() == 1)
                {
                    modWheelCache_ = static_cast<float>(msg.getControllerValue()) / 127.0f;
                    voiceManager_.setModWheel(modWheelCache_);
                }
                else if (msg.isChannelPressure())
                {
                    aftertouchCache_ = static_cast<float>(msg.getChannelPressureValue()) / 127.0f;
                    voiceManager_.setAftertouch(aftertouchCache_);
                }
                else if (msg.isAllNotesOff() || msg.isAllSoundOff())
                {
                    voiceManager_.allNotesOff();
                }

                ++midiIt;
            }

            const float lfo1Val = lfos_[0].processSample();
            const float lfo2Val = lfos_[1].processSample();

            float sL = 0.0f;
            float sR = 0.0f;
            voiceManager_.renderSample(modMatrix_, lfo1Val, lfo2Val, globalModOffsets, sL, sR);

            left[sampleIdx]  = sL;
            if (buffer.getNumChannels() > 1)
                right[sampleIdx] = sR;
        }

        // Apply stereo effects chain
        effectsChain_.processBlock(left, right, numSamples);

        // Apply smoothed master gain + telemetry peak metering
        float maxL = 0.0f;
        float maxR = 0.0f;
        bool clipped = false;

        for (int i = 0; i < numSamples; ++i)
        {
            const float g = masterGainSmoother_.getNext();
            float outL = left[i] * g;
            float outR = (buffer.getNumChannels() > 1) ? (right[i] * g) : outL;

            if (std::abs(outL) > 1.0f || std::abs(outR) > 1.0f)
                clipped = true;

            // Gentle safety ceiling so DAW channels are never blasted by pathological resonance+gain
            outL = DspMath::clamp(outL, -1.5f, 1.5f);
            outR = DspMath::clamp(outR, -1.5f, 1.5f);

            left[i] = outL;
            if (buffer.getNumChannels() > 1)
                right[i] = outR;

            maxL = std::max(maxL, std::abs(outL));
            maxR = std::max(maxR, std::abs(outR));
        }

        // Atomic telemetry update (max-hold until UI reads/decays)
        float prevL = telemetry.peakLeft.load(std::memory_order_relaxed);
        if (maxL > prevL) telemetry.peakLeft.store(maxL, std::memory_order_relaxed);

        float prevR = telemetry.peakRight.load(std::memory_order_relaxed);
        if (maxR > prevR) telemetry.peakRight.store(maxR, std::memory_order_relaxed);

        if (clipped) telemetry.clippingDetected.store(true, std::memory_order_relaxed);

        telemetry.activeSynthVoices.store(voiceManager_.getActiveSynthVoices(), std::memory_order_relaxed);
        telemetry.activeSampleVoices.store(voiceManager_.getActiveSampleVoices(), std::memory_order_relaxed);
        telemetry.totalVoices.store(voiceManager_.getTotalActiveVoices(), std::memory_order_relaxed);

        // All newly started SampleVoice instances have now taken their own reference.
        audioHazardPtr_.store(nullptr, std::memory_order_seq_cst);
    }
}
