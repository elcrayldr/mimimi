#include "Library/PresetManager.h"
#include "Common/ProductInfo.h"

namespace cray
{
    PresetManager::PresetManager(SampleDecoder& decoder, LibraryManager& library)
        : decoder_(decoder), library_(library)
    {
    }

    bool PresetManager::savePresetToFile(const juce::File& targetFile,
                                         const PresetMetadata& meta,
                                         juce::AudioProcessorValueTreeState& apvts,
                                         const InstrumentDefinition& instrument)
    {
        // Protect Factory Library from accidental overwrites
        if (targetFile.isAChildOf(library_.getFactoryLibraryRoot()))
            return false;

        auto* root = new juce::DynamicObject();
        root->setProperty("schemaVersion", ProductInfo::kStateSchemaVersion);
        root->setProperty("name", meta.name);
        root->setProperty("author", meta.author);
        root->setProperty("category", categoryToString(meta.category));
        root->setProperty("bank", meta.bank);
        root->setProperty("tags", meta.tags);
        root->setProperty("isFactory", false);

        const auto stateTree = apvts.copyState();
        if (auto xml = stateTree.createXml())
            root->setProperty("apvtsXml", xml->toString());

        root->setProperty("instrument", instrument.toJson(targetFile.getParentDirectory()));

        targetFile.getParentDirectory().createDirectory();
        const bool written = targetFile.replaceWithText(juce::JSON::toString(juce::var(root), false));
        if (written)
        {
            LibraryItem item;
            item.id            = "user::preset::" + juce::File::createLegalFileName(meta.name);
            item.name          = meta.name;
            item.filePath      = targetFile.getFullPathName();
            item.category      = meta.category;
            item.bank          = meta.bank;
            item.author        = meta.author;
            item.tags          = meta.tags;
            item.type          = LibraryItemType::Preset;
            item.isFactory     = false;
            item.fileSizeBytes = targetFile.getSize();
            library_.registerOrUpdateUserItem(item);
        }
        return written;
    }

    PresetLoadResult PresetManager::loadPresetFromFile(const juce::File& file,
                                                       juce::AudioProcessorValueTreeState& apvts)
    {
        PresetLoadResult res;
        if (!file.existsAsFile())
        {
            res.statusMessage = "Preset file not found: " + file.getFullPathName();
            return res;
        }

        const auto parsed = juce::JSON::parse(file);
        auto* root = parsed.getDynamicObject();
        if (root == nullptr)
        {
            res.statusMessage = "Invalid preset format: " + file.getFileName();
            return res;
        }

        res.metadata.name      = root->getProperty("name").toString();
        res.metadata.author    = root->getProperty("author").toString();
        res.metadata.category  = categoryFromString(root->getProperty("category").toString());
        res.metadata.bank      = root->getProperty("bank").toString();
        res.metadata.tags      = root->getProperty("tags").toString();
        res.metadata.isFactory = static_cast<bool>(root->getProperty("isFactory"));

        const auto xmlStr = root->getProperty("apvtsXml").toString();
        if (xmlStr.isNotEmpty())
        {
            if (auto xml = juce::XmlDocument::parse(xmlStr))
            {
                auto tree = juce::ValueTree::fromXml(*xml);
                if (tree.isValid() && tree.hasType(apvts.state.getType()))
                    apvts.replaceState(tree);
            }
        }

        if (root->hasProperty("instrument"))
        {
            InstrumentDefinition::fromJson(root->getProperty("instrument"), res.instrument, file.getParentDirectory());
            for (const auto& z : res.instrument.zones)
            {
                if (z.missingSourceFile || !juce::File(z.sourceFilePath).existsAsFile())
                    res.missingSamples.addIfNotAlreadyThere(z.sourceFilePath);
            }
        }

        res.success = true;
        res.statusMessage = res.missingSamples.isEmpty()
            ? ("Loaded preset: " + res.metadata.name)
            : ("Loaded preset with " + juce::String(res.missingSamples.size()) + " missing sample(s).");
        return res;
    }

    bool PresetManager::exportPortablePack(const juce::File& packFile,
                                           const PresetMetadata& meta,
                                           juce::AudioProcessorValueTreeState& apvts,
                                           const InstrumentDefinition& instrument)
    {
        const auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                 .getChildFile("cray_pack_" + juce::Uuid().toString());
        tempDir.createDirectory();
        const auto samplesDir = tempDir.getChildFile("Samples");
        samplesDir.createDirectory();

        // Clone instrument & copy referenced samples into Samples/ with relative paths
        InstrumentDefinition portableInst = instrument;
        for (auto& z : portableInst.zones)
        {
            const juce::File src(z.sourceFilePath);
            if (src.existsAsFile())
            {
                const auto dest = samplesDir.getNonexistentChildFile(src.getFileNameWithoutExtension(),
                                                                     src.getFileExtension(), false);
                if (src.copyFileTo(dest))
                    z.sourceFilePath = "Samples/" + dest.getFileName();
            }
        }

        auto* root = new juce::DynamicObject();
        root->setProperty("schemaVersion", ProductInfo::kStateSchemaVersion);
        root->setProperty("name", meta.name);
        root->setProperty("author", meta.author);
        root->setProperty("category", categoryToString(meta.category));
        root->setProperty("bank", meta.bank);
        root->setProperty("tags", meta.tags);
        root->setProperty("isFactory", false);

        if (auto xml = apvts.copyState().createXml())
            root->setProperty("apvtsXml", xml->toString());

        root->setProperty("instrument", portableInst.toJson(tempDir));

        const auto presetTempFile = tempDir.getChildFile("preset" + juce::String(ProductInfo::kPresetExtension));
        presetTempFile.replaceWithText(juce::JSON::toString(juce::var(root), false));

        juce::ZipFile::Builder zipBuilder;
        zipBuilder.addFile(presetTempFile, 6, presetTempFile.getFileName());

        const auto copiedSamples = samplesDir.findChildFiles(juce::File::findFiles, false);
        for (const auto& sf : copiedSamples)
            zipBuilder.addFile(sf, 6, "Samples/" + sf.getFileName());

        packFile.getParentDirectory().createDirectory();
        if (packFile.existsAsFile())
            packFile.deleteFile();

        juce::FileOutputStream outStream(packFile);
        const bool ok = outStream.openedOk() && zipBuilder.writeToStream(outStream, nullptr);
        tempDir.deleteRecursively();
        return ok;
    }

    PresetLoadResult PresetManager::importPortablePack(const juce::File& packFile,
                                                       juce::AudioProcessorValueTreeState& apvts,
                                                       const juce::File& extractSamplesDir)
    {
        PresetLoadResult res;
        if (!packFile.existsAsFile())
        {
            res.statusMessage = "Pack file not found.";
            return res;
        }

        juce::ZipFile zip(packFile);
        if (zip.getNumEntries() <= 0)
        {
            res.statusMessage = "Invalid or empty .craypack archive.";
            return res;
        }

        const auto targetDir = extractSamplesDir.getChildFile(
            juce::File::createLegalFileName(packFile.getFileNameWithoutExtension()));
        targetDir.createDirectory();

        const auto uncompressResult = zip.uncompressTo(targetDir, true);
        if (uncompressResult.failed())
        {
            res.statusMessage = "Failed extracting pack: " + uncompressResult.getErrorMessage();
            return res;
        }

        const auto presetFile = targetDir.getChildFile("preset" + juce::String(ProductInfo::kPresetExtension));
        if (!presetFile.existsAsFile())
        {
            res.statusMessage = "No preset manifest found inside pack.";
            return res;
        }

        res = loadPresetFromFile(presetFile, apvts);
        if (res.success)
        {
            // Also register in User Library Presets
            const auto userPresetCopy = library_.getUserLibraryRoot()
                .getChildFile("Presets")
                .getChildFile(juce::File::createLegalFileName(res.metadata.name) + ProductInfo::kPresetExtension);
            savePresetToFile(userPresetCopy, res.metadata, apvts, res.instrument);
        }
        return res;
    }

    int PresetManager::relinkMissingSamples(InstrumentDefinition& instrument, const juce::File& searchDirectory)
    {
        if (!searchDirectory.isDirectory())
            return 0;

        int relinked = 0;
        const auto candidates = searchDirectory.findChildFiles(juce::File::findFiles, true, "*");

        for (auto& z : instrument.zones)
        {
            if (juce::File(z.sourceFilePath).existsAsFile())
            {
                z.missingSourceFile = false;
                continue;
            }

            const auto missingName = juce::File(z.sourceFilePath).getFileName();
            for (const auto& cand : candidates)
            {
                if (cand.getFileName().equalsIgnoreCase(missingName))
                {
                    z.sourceFilePath = cand.getFullPathName();
                    z.missingSourceFile = false;
                    ++relinked;
                    break;
                }
            }
        }
        return relinked;
    }
}
