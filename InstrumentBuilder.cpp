#include "Library/InstrumentBuilder.h"
#include "Common/ProductInfo.h"
#include <algorithm>
#include <array>
#include <cctype>

namespace cray
{
    InstrumentBuilder::InstrumentBuilder(SampleDecoder& decoder, LibraryManager& library)
        : decoder_(decoder), library_(library)
    {
    }

    juce::String InstrumentBuilder::midiNoteToName(int midiNote)
    {
        static constexpr std::array<const char*, 12> kNames = {
            "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
        };
        const int clamped = std::clamp(midiNote, 0, 127);
        const int octave  = clamped / 12; // FL Studio convention: MIDI 60 is C5.
        return juce::String(kNames[static_cast<size_t>(clamped % 12)]) + juce::String(octave);
    }

    std::optional<int> InstrumentBuilder::parseMidiNoteFromName(const juce::String& filename)
    {
        const auto base = juce::File(filename).getFileNameWithoutExtension().toStdString();
        const int len = static_cast<int>(base.size());

        auto noteLetterToSemitone = [](char c) -> int
        {
            switch (std::toupper(static_cast<unsigned char>(c)))
            {
                case 'C': return 0;
                case 'D': return 2;
                case 'E': return 4;
                case 'F': return 5;
                case 'G': return 7;
                case 'A': return 9;
                case 'B': return 11;
                default:  return -1;
            }
        };

        // Scan right-to-left so suffixes like "808_Lead_F#3" match F#3 before any earlier letters
        for (int i = len - 1; i >= 0; --i)
        {
            const int semiBase = noteLetterToSemitone(base[static_cast<size_t>(i)]);
            if (semiBase < 0)
                continue;

            // Require separator or start of string before note letter so words like "BASS" or "LEAD" don't false-match
            if (i > 0)
            {
                const char prev = base[static_cast<size_t>(i - 1)];
                if (std::isalpha(static_cast<unsigned char>(prev)))
                    continue;
            }

            int pos = i + 1;
            int accidental = 0;
            if (pos < len && (base[static_cast<size_t>(pos)] == '#' || base[static_cast<size_t>(pos)] == 'b'))
            {
                accidental = (base[static_cast<size_t>(pos)] == '#') ? 1 : -1;
                ++pos;
            }

            if (pos >= len)
                continue;

            bool neg = false;
            if (base[static_cast<size_t>(pos)] == '-')
            {
                neg = true;
                ++pos;
            }

            if (pos >= len || !std::isdigit(static_cast<unsigned char>(base[static_cast<size_t>(pos)])))
                continue;

            const int octaveDigit = base[static_cast<size_t>(pos)] - '0';
            // Ensure it isn't followed immediately by another digit (e.g. "C808")
            if (pos + 1 < len && std::isdigit(static_cast<unsigned char>(base[static_cast<size_t>(pos + 1)])))
                continue;

            const int octave = neg ? -octaveDigit : octaveDigit;
            const int midi   = octave * 12 + semiBase + accidental; // Parse filenames using FL Studio octave labels.
            if (midi >= 0 && midi <= 127)
                return midi;
        }

        // Fallback: look for explicit 3-digit or separator-bounded MIDI note number 021..108
        for (int i = len - 1; i >= 0; --i)
        {
            if (!std::isdigit(static_cast<unsigned char>(base[static_cast<size_t>(i)])))
                continue;

            int start = i;
            while (start > 0 && std::isdigit(static_cast<unsigned char>(base[static_cast<size_t>(start - 1)])))
                --start;

            const int digits = i - start + 1;
            if (digits == 3 || (digits == 2 && (start == 0 || !std::isalnum(static_cast<unsigned char>(base[static_cast<size_t>(start - 1)])))))
            {
                const int val = std::stoi(base.substr(static_cast<size_t>(start), static_cast<size_t>(digits)));
                if (val >= 21 && val <= 108)
                    return val;
            }
            i = start;
        }

        return std::nullopt;
    }

    int InstrumentBuilder::addSamples(InstrumentDefinition& draft,
                                      const juce::Array<juce::File>& files,
                                      int targetLayer,
                                      bool autoDetectRootNotes)
    {
        draft.ensureLayerExists(targetLayer);
        int addedCount = 0;

        for (const auto& f : files)
        {
            if (!f.existsAsFile() || !decoder_.isSupportedAudioFile(f))
                continue;

            // Do not decode here: buildInstrument() compiles each sample once.
            // The previous eager decode was discarded immediately because the decoder
            // cache is weak, forcing a second full decode during instrument compilation.
            SampleZoneSpec spec;
            spec.zoneId         = juce::Uuid().toString();
            spec.sourceFilePath = f.getFullPathName();
            spec.displayName    = f.getFileNameWithoutExtension();
            spec.layerIndex     = std::max(0, targetLayer);
            spec.startSample    = 0;
            spec.endSample      = 0; // 0 = full sample length at compile time
            spec.loopStartSample = 0;
            spec.loopEndSample   = 0; // 0 = compiled zone end

            if (autoDetectRootNotes)
            {
                if (auto detected = parseMidiNoteFromName(f.getFileName()))
                    spec.rootNote = *detected;
            }

            draft.zones.push_back(std::move(spec));
            ++addedCount;
        }

        return addedCount;
    }

    void InstrumentBuilder::autoMapZonesByRootNote(InstrumentDefinition& draft, int layerIndex)
    {
        const int numLayers = static_cast<int>(draft.layers.size());
        const int startL = (layerIndex >= 0) ? layerIndex : 0;
        const int endL   = (layerIndex >= 0) ? layerIndex : (numLayers - 1);

        for (int l = startL; l <= endL; ++l)
        {
            std::vector<size_t> indices;
            for (size_t i = 0; i < draft.zones.size(); ++i)
            {
                if (draft.zones[i].layerIndex == l)
                    indices.push_back(i);
            }

            if (indices.empty())
                continue;

            std::stable_sort(indices.begin(), indices.end(),
                [&](size_t a, size_t b)
                {
                    return draft.zones[a].rootNote < draft.zones[b].rootNote;
                });

            // Group zones sharing the same root note. Velocity-split samples at one
            // pitch must receive the same key range; otherwise auto-map would split
            // identical roots across different keyboard regions and make layers vanish.
            std::vector<std::vector<size_t>> rootGroups;
            for (const auto index : indices)
            {
                if (rootGroups.empty()
                    || draft.zones[rootGroups.back().front()].rootNote != draft.zones[index].rootNote)
                    rootGroups.emplace_back();

                rootGroups.back().push_back(index);
            }

            for (size_t groupIndex = 0; groupIndex < rootGroups.size(); ++groupIndex)
            {
                const int root = draft.zones[rootGroups[groupIndex].front()].rootNote;
                const int low = (groupIndex == 0)
                    ? 0
                    : (draft.zones[rootGroups[groupIndex - 1].front()].rootNote + root) / 2 + 1;
                const int high = (groupIndex + 1 == rootGroups.size())
                    ? 127
                    : (root + draft.zones[rootGroups[groupIndex + 1].front()].rootNote) / 2;

                for (const auto zoneIndex : rootGroups[groupIndex])
                {
                    auto& zone = draft.zones[zoneIndex];
                    zone.lowKey = std::clamp(low, 0, 127);
                    zone.highKey = std::clamp(high, zone.lowKey, 127);
                }
            }
        }
    }

    BuildInstrumentResult InstrumentBuilder::buildInstrument(InstrumentDefinition& draft, bool saveToUserLibrary)
    {
        BuildInstrumentResult res;

        if (draft.zones.empty())
        {
            res.statusMessage = "Cannot build instrument: no sample zones added.";
            return res;
        }

        if (draft.name.trim().isEmpty())
            draft.name = "Custom Urban Instrument";

        if (draft.id.isEmpty())
            draft.id = juce::Uuid().toString();

        res.compileReport = draft.compile(decoder_);
        if (res.compileReport.compiled == nullptr || res.compileReport.compiled->zones.empty())
        {
            res.statusMessage = "Build failed: no valid playable sample zones could be decoded.";
            if (!res.compileReport.errors.isEmpty())
                res.statusMessage += " " + res.compileReport.errors[0];
            if (!res.compileReport.missingFiles.isEmpty())
                res.statusMessage += " Missing: " + res.compileReport.missingFiles.joinIntoString(", ");
            return res;
        }

        if (saveToUserLibrary && !draft.isFactory)
        {
            const auto safeName = juce::File::createLegalFileName(draft.name);
            const auto instDir  = library_.getUserLibraryRoot().getChildFile("Instruments");
            instDir.createDirectory();
            const auto outFile  = instDir.getChildFile(safeName + ProductInfo::kInstrumentExtension);

            if (!draft.saveToFile(outFile, false))
            {
                res.statusMessage = "Compiled instrument in memory, but failed writing file: " + outFile.getFullPathName();
                return res;
            }

            res.savedInstrumentFile = outFile;

            LibraryItem libItem;
            libItem.id            = "user::inst::" + draft.id;
            libItem.name          = draft.name;
            libItem.filePath      = outFile.getFullPathName();
            libItem.category      = draft.category;
            libItem.bank          = "User";
            libItem.author        = draft.author;
            libItem.tags          = draft.tags;
            libItem.type          = LibraryItemType::Instrument;
            libItem.isFactory     = false;
            libItem.fileSizeBytes = outFile.getSize();
            library_.registerOrUpdateUserItem(libItem);
        }

        res.success = true;
        res.statusMessage = "Built '" + draft.name + "' ("
                          + juce::String(static_cast<int>(res.compileReport.compiled->zones.size()))
                          + " playable zones).";
        if (!res.compileReport.missingFiles.isEmpty())
        {
            res.statusMessage += " Warning: " + juce::String(res.compileReport.missingFiles.size())
                               + " missing sample(s).";
        }
        if (!res.compileReport.errors.isEmpty())
        {
            res.statusMessage += " Skipped " + juce::String(res.compileReport.errors.size())
                               + " invalid sample(s). " + res.compileReport.errors[0];
        }
        return res;
    }
}
