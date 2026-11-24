#include "Library/InstrumentDefinition.h"
#include "DSP/Utilities/DspMath.h"
#include <algorithm>
#include <cmath>

namespace cray
{
    InstrumentDefinition::InstrumentDefinition()
    {
        id = juce::Uuid().toString();
        for (int i = 0; i < ProductInfo::kInitialSampleLayers; ++i)
        {
            LayerConfig lc;
            lc.name = "Layer " + juce::String(i + 1);
            layers.push_back(std::move(lc));
        }
    }

    void InstrumentDefinition::ensureLayerExists(int layerIndex)
    {
        if (layerIndex < 0)
            return;

        while (static_cast<int>(layers.size()) <= layerIndex)
        {
            LayerConfig lc;
            lc.name = "Layer " + juce::String(static_cast<int>(layers.size()) + 1);
            layers.push_back(std::move(lc));
        }
    }

    CompileInstrumentReport InstrumentDefinition::compile(SampleDecoder& decoder, const juce::File& baseDir) const
    {
        CompileInstrumentReport report;
        auto compiled = std::make_shared<CompiledSampleInstrument>();
        compiled->instrumentId   = id;
        compiled->instrumentName = name;

        for (const auto& lc : layers)
        {
            compiled->layerEnabled.push_back(lc.enabled);
            const float safeLayerGain = std::isfinite(lc.gain)
                ? DspMath::clamp(lc.gain, 0.0f, 1.5f)
                : 1.0f;
            compiled->layerGain.push_back(safeLayerGain);
        }

        for (const auto& spec : zones)
        {
            juce::File sampleFile(spec.sourceFilePath);
            if (!sampleFile.existsAsFile() && baseDir.isDirectory())
            {
                const auto relCandidate = baseDir.getChildFile(spec.sourceFilePath);
                if (relCandidate.existsAsFile())
                    sampleFile = relCandidate;
            }

            const auto dec = decoder.loadSample(sampleFile);
            if (!dec.success || dec.buffer == nullptr)
            {
                if (dec.isMissing)
                    report.missingFiles.addIfNotAlreadyThere(spec.sourceFilePath);
                else
                    report.errors.add(dec.errorMessage);
                continue;
            }

            const int totalFrames = dec.buffer->numSamples;
            if (totalFrames < 2)
                continue;

            CompiledSampleZone cz;
            cz.zoneId          = spec.zoneId;
            cz.sample          = dec.buffer;
            cz.layerIndex      = spec.layerIndex;
            cz.rootNote        = std::clamp(spec.rootNote, 0, 127);
            cz.lowKey          = std::clamp(spec.lowKey, 0, 127);
            cz.highKey         = std::clamp(std::max(spec.lowKey, spec.highKey), 0, 127);
            cz.lowVelocity     = std::clamp(spec.lowVelocity, 1, 127);
            cz.highVelocity    = std::clamp(std::max(spec.lowVelocity, spec.highVelocity), 1, 127);
            cz.pitchSemitones  = std::clamp(spec.pitchSemitones, -48, 48);
            cz.fineTuneCents   = std::isfinite(spec.fineTuneCents)
                ? DspMath::clamp(spec.fineTuneCents, -100.0f, 100.0f)
                : 0.0f;
            cz.reverse         = spec.reverse;

            cz.startSample     = std::clamp(spec.startSample, 0, totalFrames - 2);
            const int rawEnd   = (spec.endSample > 0) ? spec.endSample : (totalFrames - 1);
            cz.endSample       = std::clamp(rawEnd, cz.startSample + 1, totalFrames - 1);

            cz.loopEnabled     = spec.loopEnabled;
            cz.loopStartSample = std::clamp(spec.loopStartSample, cz.startSample, cz.endSample - 2);
            const int rawLoopE = (spec.loopEndSample > 0) ? spec.loopEndSample : cz.endSample;
            cz.loopEndSample   = std::clamp(rawLoopE, cz.loopStartSample + 2, cz.endSample);
            cz.loopCrossfade   = std::clamp(spec.loopCrossfade, 0, totalFrames / 2);

            cz.fadeInSamples   = std::clamp(spec.fadeInSamples, 0, totalFrames);
            cz.fadeOutSamples  = std::clamp(spec.fadeOutSamples, 0, totalFrames);

            const float totalGainDb = (std::isfinite(spec.gainDb) ? spec.gainDb : 0.0f)
                                    + (std::isfinite(spec.normalizeGainDb) ? spec.normalizeGainDb : 0.0f);
            cz.gainLinear      = DspMath::dbToGain(DspMath::clamp(totalGainDb, -96.0f, 24.0f));
            cz.pan             = std::isfinite(spec.pan)
                ? DspMath::clamp(spec.pan, -1.0f, 1.0f)
                : 0.0f;

            cz.attackSec       = std::isfinite(spec.attackSec)
                ? DspMath::clamp(spec.attackSec, 0.0005f, 8.0f) : 0.005f;
            cz.decaySec        = std::isfinite(spec.decaySec)
                ? DspMath::clamp(spec.decaySec, 0.001f, 10.0f) : 0.25f;
            cz.sustainLevel    = std::isfinite(spec.sustainLevel)
                ? DspMath::clamp(spec.sustainLevel, 0.0f, 1.0f) : 1.0f;
            cz.releaseSec      = std::isfinite(spec.releaseSec)
                ? DspMath::clamp(spec.releaseSec, 0.001f, 12.0f) : 0.20f;

            cz.filterEnabled   = spec.filterEnabled;
            cz.filterType      = spec.filterType;
            cz.filterCutoffHz  = std::isfinite(spec.filterCutoffHz)
                ? DspMath::clamp(spec.filterCutoffHz, 20.0f, 20000.0f) : 18000.0f;
            cz.filterResonance = std::isfinite(spec.filterResonance)
                ? DspMath::clamp(spec.filterResonance, 0.0f, 0.96f) : 0.1f;

            compiled->zones.push_back(std::move(cz));
        }

        report.compiled = compiled;
        return report;
    }

    juce::var InstrumentDefinition::toJson(const juce::File& relativeToDir) const
    {
        auto* root = new juce::DynamicObject();
        root->setProperty("schemaVersion", ProductInfo::kStateSchemaVersion);
        root->setProperty("id", id);
        root->setProperty("name", name);
        root->setProperty("author", author);
        root->setProperty("category", categoryToString(category));
        root->setProperty("tags", tags);
        root->setProperty("isFactory", isFactory);

        juce::Array<juce::var> layerArr;
        for (const auto& l : layers)
        {
            auto* lo = new juce::DynamicObject();
            lo->setProperty("name", l.name);
            lo->setProperty("enabled", l.enabled);
            lo->setProperty("gain", l.gain);
            layerArr.add(juce::var(lo));
        }
        root->setProperty("layers", layerArr);

        juce::Array<juce::var> zoneArr;
        for (const auto& z : zones)
        {
            auto* zo = new juce::DynamicObject();
            zo->setProperty("zoneId", z.zoneId);

            juce::String storedPath = z.sourceFilePath;
            if (relativeToDir.isDirectory() && juce::File::isAbsolutePath(storedPath))
            {
                const juce::File f(storedPath);
                if (f.isAChildOf(relativeToDir))
                    storedPath = f.getRelativePathFrom(relativeToDir).replaceCharacter('\\', '/');
            }
            zo->setProperty("sourceFilePath", storedPath);
            zo->setProperty("displayName", z.displayName);
            zo->setProperty("layerIndex", z.layerIndex);
            zo->setProperty("rootNote", z.rootNote);
            zo->setProperty("lowKey", z.lowKey);
            zo->setProperty("highKey", z.highKey);
            zo->setProperty("lowVelocity", z.lowVelocity);
            zo->setProperty("highVelocity", z.highVelocity);
            zo->setProperty("pitchSemitones", z.pitchSemitones);
            zo->setProperty("fineTuneCents", z.fineTuneCents);
            zo->setProperty("reverse", z.reverse);
            zo->setProperty("startSample", z.startSample);
            zo->setProperty("endSample", z.endSample);
            zo->setProperty("loopEnabled", z.loopEnabled);
            zo->setProperty("loopStartSample", z.loopStartSample);
            zo->setProperty("loopEndSample", z.loopEndSample);
            zo->setProperty("loopCrossfade", z.loopCrossfade);
            zo->setProperty("fadeInSamples", z.fadeInSamples);
            zo->setProperty("fadeOutSamples", z.fadeOutSamples);
            zo->setProperty("gainDb", z.gainDb);
            zo->setProperty("normalizeGainDb", z.normalizeGainDb);
            zo->setProperty("pan", z.pan);
            zo->setProperty("attackSec", z.attackSec);
            zo->setProperty("decaySec", z.decaySec);
            zo->setProperty("sustainLevel", z.sustainLevel);
            zo->setProperty("releaseSec", z.releaseSec);
            zo->setProperty("filterEnabled", z.filterEnabled);
            zo->setProperty("filterType", static_cast<int>(z.filterType));
            zo->setProperty("filterCutoffHz", z.filterCutoffHz);
            zo->setProperty("filterResonance", z.filterResonance);
            zoneArr.add(juce::var(zo));
        }
        root->setProperty("zones", zoneArr);

        return juce::var(root);
    }

    bool InstrumentDefinition::fromJson(const juce::var& json, InstrumentDefinition& outDef, const juce::File& baseDir)
    {
        auto* root = json.getDynamicObject();
        if (root == nullptr)
            return false;

        outDef = InstrumentDefinition{};
        if (root->hasProperty("id"))       outDef.id       = root->getProperty("id").toString();
        if (root->hasProperty("name"))     outDef.name     = root->getProperty("name").toString();
        if (root->hasProperty("author"))   outDef.author   = root->getProperty("author").toString();
        if (root->hasProperty("category")) outDef.category = categoryFromString(root->getProperty("category").toString());
        if (root->hasProperty("tags"))     outDef.tags     = root->getProperty("tags").toString();
        if (root->hasProperty("isFactory"))outDef.isFactory = static_cast<bool>(root->getProperty("isFactory"));

        if (auto* layerArr = root->getProperty("layers").getArray())
        {
            outDef.layers.clear();
            for (const auto& lv : *layerArr)
            {
                if (auto* lo = lv.getDynamicObject())
                {
                    LayerConfig lc;
                    lc.name    = lo->getProperty("name").toString();
                    lc.enabled = lo->hasProperty("enabled") ? static_cast<bool>(lo->getProperty("enabled")) : true;
                    lc.gain    = lo->hasProperty("gain") ? static_cast<float>(lo->getProperty("gain")) : 1.0f;
                    outDef.layers.push_back(std::move(lc));
                }
            }
        }

        if (auto* zoneArr = root->getProperty("zones").getArray())
        {
            outDef.zones.clear();
            for (const auto& zv : *zoneArr)
            {
                auto* zo = zv.getDynamicObject();
                if (zo == nullptr)
                    continue;

                SampleZoneSpec z;
                z.zoneId = zo->getProperty("zoneId").toString();
                if (z.zoneId.isEmpty()) z.zoneId = juce::Uuid().toString();

                juce::String path = zo->getProperty("sourceFilePath").toString();
                if (!juce::File::isAbsolutePath(path) && baseDir.isDirectory())
                    path = baseDir.getChildFile(path).getFullPathName();
                z.sourceFilePath    = path;
                z.missingSourceFile = !juce::File(path).existsAsFile();

                z.displayName       = zo->getProperty("displayName").toString();
                z.layerIndex        = static_cast<int>(zo->getProperty("layerIndex"));
                z.rootNote          = zo->hasProperty("rootNote") ? static_cast<int>(zo->getProperty("rootNote")) : 60;
                z.lowKey            = zo->hasProperty("lowKey") ? static_cast<int>(zo->getProperty("lowKey")) : 0;
                z.highKey           = zo->hasProperty("highKey") ? static_cast<int>(zo->getProperty("highKey")) : 127;
                z.lowVelocity       = zo->hasProperty("lowVelocity") ? static_cast<int>(zo->getProperty("lowVelocity")) : 1;
                z.highVelocity      = zo->hasProperty("highVelocity") ? static_cast<int>(zo->getProperty("highVelocity")) : 127;
                z.pitchSemitones    = static_cast<int>(zo->getProperty("pitchSemitones"));
                z.fineTuneCents     = static_cast<float>(zo->getProperty("fineTuneCents"));
                z.reverse           = static_cast<bool>(zo->getProperty("reverse"));
                z.startSample       = static_cast<int>(zo->getProperty("startSample"));
                z.endSample         = static_cast<int>(zo->getProperty("endSample"));
                z.loopEnabled       = static_cast<bool>(zo->getProperty("loopEnabled"));
                z.loopStartSample   = static_cast<int>(zo->getProperty("loopStartSample"));
                z.loopEndSample     = static_cast<int>(zo->getProperty("loopEndSample"));
                z.loopCrossfade     = zo->hasProperty("loopCrossfade") ? static_cast<int>(zo->getProperty("loopCrossfade")) : 256;
                z.fadeInSamples     = static_cast<int>(zo->getProperty("fadeInSamples"));
                z.fadeOutSamples    = static_cast<int>(zo->getProperty("fadeOutSamples"));
                z.gainDb            = static_cast<float>(zo->getProperty("gainDb"));
                z.normalizeGainDb   = static_cast<float>(zo->getProperty("normalizeGainDb"));
                z.pan               = static_cast<float>(zo->getProperty("pan"));
                z.attackSec         = zo->hasProperty("attackSec") ? static_cast<float>(zo->getProperty("attackSec")) : 0.005f;
                z.decaySec          = zo->hasProperty("decaySec") ? static_cast<float>(zo->getProperty("decaySec")) : 0.25f;
                z.sustainLevel      = zo->hasProperty("sustainLevel") ? static_cast<float>(zo->getProperty("sustainLevel")) : 1.0f;
                z.releaseSec        = zo->hasProperty("releaseSec") ? static_cast<float>(zo->getProperty("releaseSec")) : 0.20f;
                z.filterEnabled     = static_cast<bool>(zo->getProperty("filterEnabled"));
                z.filterType        = static_cast<SvfFilterType>(static_cast<int>(zo->getProperty("filterType")));
                z.filterCutoffHz    = zo->hasProperty("filterCutoffHz") ? static_cast<float>(zo->getProperty("filterCutoffHz")) : 18000.0f;
                z.filterResonance   = zo->hasProperty("filterResonance") ? static_cast<float>(zo->getProperty("filterResonance")) : 0.1f;

                outDef.ensureLayerExists(z.layerIndex);
                outDef.zones.push_back(std::move(z));
            }
        }

        return true;
    }

    bool InstrumentDefinition::saveToFile(const juce::File& file, bool useRelativeSamplePaths) const
    {
        file.getParentDirectory().createDirectory();
        const auto jsonVar = toJson(useRelativeSamplePaths ? file.getParentDirectory() : juce::File());
        return file.replaceWithText(juce::JSON::toString(jsonVar, false));
    }

    bool InstrumentDefinition::loadFromFile(const juce::File& file, InstrumentDefinition& outDef)
    {
        if (!file.existsAsFile())
            return false;

        const auto parsed = juce::JSON::parse(file);
        return fromJson(parsed, outDef, file.getParentDirectory());
    }
}
