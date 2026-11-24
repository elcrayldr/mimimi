#include "Library/LibraryManager.h"
#include "Common/ProductInfo.h"
#include <algorithm>
#if JUCE_WINDOWS
 #include <windows.h>
#endif

namespace cray
{
    LibraryManager::LibraryManager(SampleDecoder& decoder)
        : decoder_(decoder)
    {
    }

    juce::File LibraryManager::resolveDefaultFactoryRoot()
    {
#if JUCE_WINDOWS
        // In a VST3 host, currentExecutableFile points to the DAW, not this plugin DLL.
        static const int moduleAnchor = 0;
        HMODULE moduleHandle = nullptr;
        if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               reinterpret_cast<LPCWSTR>(&moduleAnchor),
                               &moduleHandle) != 0)
        {
            wchar_t modulePath[32768] {};
            const auto length = GetModuleFileNameW(moduleHandle, modulePath,
                                                    static_cast<DWORD>(std::size(modulePath)));
            if (length > 0 && length < std::size(modulePath))
            {
                const auto moduleDir = juce::File(juce::String::fromUTF16(
                    reinterpret_cast<const juce::juce_wchar*>(modulePath))).getParentDirectory();

                // VST3 resources are stored in the bundle's Contents/Resources directory.
                if (moduleDir.getFileName().equalsIgnoreCase("x86_64-win"))
                {
                    const auto bundleResources = moduleDir.getParentDirectory().getChildFile("Resources");
                    if (bundleResources.isDirectory())
                        return bundleResources;
                }

                // Standalone resources are stored beside the executable.
                const auto siblingResources = moduleDir.getChildFile("Resources");
                if (siblingResources.isDirectory())
                    return siblingResources;
            }
        }
#endif
        const auto exeDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
        const auto siblingRes = exeDir.getChildFile("Resources");
        if (siblingRes.isDirectory())
            return siblingRes;

        const auto parentRes = exeDir.getParentDirectory().getChildFile("Resources");
        if (parentRes.isDirectory())
            return parentRes;

        return juce::File::getSpecialLocation(juce::File::commonApplicationDataDirectory)
            .getChildFile("CRAY LDR")
            .getChildFile("CRAY LDR URBAN INSTRUMENTS")
            .getChildFile("FactoryLibrary");
    }

    juce::File LibraryManager::resolveDefaultConfigFile()
    {
        return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile("CRAY LDR")
            .getChildFile("CRAY LDR URBAN INSTRUMENTS")
            .getChildFile("library_config.json");
    }

    void LibraryManager::initialize(const juce::File& factoryRootDir, const juce::File& userConfigFile)
    {
        factoryRoot_ = factoryRootDir;
        configFile_  = userConfigFile;

        userRoot_ = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                        .getChildFile("CRAY LDR")
                        .getChildFile("Urban Instruments User Library");

        loadConfig();
        if (!userRoot_.isDirectory())
            userRoot_.createDirectory();

        userRoot_.getChildFile("Presets").createDirectory();
        userRoot_.getChildFile("Instruments").createDirectory();
        userRoot_.getChildFile("Samples").createDirectory();

        rescanAll();
    }

    void LibraryManager::setUserLibraryRoot(const juce::File& newRoot)
    {
        if (newRoot == juce::File())
            return;

        userRoot_ = newRoot;
        userRoot_.createDirectory();
        userRoot_.getChildFile("Presets").createDirectory();
        userRoot_.getChildFile("Instruments").createDirectory();
        userRoot_.getChildFile("Samples").createDirectory();
        saveConfig();
        rescanUser();
    }

    void LibraryManager::loadConfig()
    {
        if (!configFile_.existsAsFile())
            return;

        const auto parsed = juce::JSON::parse(configFile_);
        if (auto* obj = parsed.getDynamicObject())
        {
            const auto pathStr = obj->getProperty("userLibraryRoot").toString();
            if (pathStr.isNotEmpty())
                userRoot_ = juce::File(pathStr);

            if (auto* favArr = obj->getProperty("factoryFavorites").getArray())
            {
                factoryFavoriteIds_.clear();
                for (const auto& v : *favArr)
                    factoryFavoriteIds_.addIfNotAlreadyThere(v.toString());
            }
        }
    }

    void LibraryManager::saveConfig() const
    {
        if (configFile_ == juce::File())
            return;

        configFile_.getParentDirectory().createDirectory();
        auto* obj = new juce::DynamicObject();
        obj->setProperty("userLibraryRoot", userRoot_.getFullPathName());

        juce::Array<juce::var> favArr;
        for (const auto& id : factoryFavoriteIds_)
            favArr.add(id);
        obj->setProperty("factoryFavorites", favArr);

        configFile_.replaceWithText(juce::JSON::toString(juce::var(obj), false));
    }

    void LibraryManager::loadUserIndex()
    {
        userItems_.clear();
        userFolders_.clear();

        const auto indexFile = userRoot_.getChildFile("user_library_index.json");
        if (!indexFile.existsAsFile())
            return;

        const auto parsed = juce::JSON::parse(indexFile);
        auto* obj = parsed.getDynamicObject();
        if (obj == nullptr)
            return;

        if (auto* folders = obj->getProperty("folders").getArray())
        {
            for (const auto& f : *folders)
                userFolders_.addIfNotAlreadyThere(f.toString());
        }

        if (auto* items = obj->getProperty("items").getArray())
        {
            for (const auto& v : *items)
            {
                auto* itemObj = v.getDynamicObject();
                if (itemObj == nullptr)
                    continue;

                LibraryItem item;
                item.id            = itemObj->getProperty("id").toString();
                item.name          = itemObj->getProperty("name").toString();
                item.filePath      = itemObj->getProperty("filePath").toString();
                item.virtualFolder = itemObj->getProperty("virtualFolder").toString();
                item.category      = categoryFromString(itemObj->getProperty("category").toString());
                item.bank          = itemObj->getProperty("bank").toString();
                item.author        = itemObj->getProperty("author").toString();
                item.tags          = itemObj->getProperty("tags").toString();
                item.type          = static_cast<LibraryItemType>(static_cast<int>(itemObj->getProperty("type")));
                item.isFactory     = false;
                item.isFavorite    = static_cast<bool>(itemObj->getProperty("isFavorite"));
                item.contentHash   = itemObj->getProperty("contentHash").toString();
                item.fileSizeBytes = static_cast<juce::int64>(itemObj->getProperty("fileSizeBytes"));
                item.sampleRate    = static_cast<double>(itemObj->getProperty("sampleRate"));
                item.channels      = static_cast<int>(itemObj->getProperty("channels"));
                item.durationSec   = static_cast<double>(itemObj->getProperty("durationSec"));
                item.isMissing     = !juce::File(item.filePath).existsAsFile();

                if (item.id.isNotEmpty())
                    userItems_.push_back(std::move(item));
            }
        }
    }

    void LibraryManager::saveUserIndex() const
    {
        if (userRoot_ == juce::File())
            return;

        userRoot_.createDirectory();
        auto* rootObj = new juce::DynamicObject();

        juce::Array<juce::var> folderArr;
        for (const auto& f : userFolders_)
            folderArr.add(f);
        rootObj->setProperty("folders", folderArr);

        juce::Array<juce::var> itemArr;
        for (const auto& item : userItems_)
        {
            auto* o = new juce::DynamicObject();
            o->setProperty("id", item.id);
            o->setProperty("name", item.name);
            o->setProperty("filePath", item.filePath);
            o->setProperty("virtualFolder", item.virtualFolder);
            o->setProperty("category", categoryToString(item.category));
            o->setProperty("bank", item.bank);
            o->setProperty("author", item.author);
            o->setProperty("tags", item.tags);
            o->setProperty("type", static_cast<int>(item.type));
            o->setProperty("isFavorite", item.isFavorite);
            o->setProperty("contentHash", item.contentHash);
            o->setProperty("fileSizeBytes", item.fileSizeBytes);
            o->setProperty("sampleRate", item.sampleRate);
            o->setProperty("channels", item.channels);
            o->setProperty("durationSec", item.durationSec);
            itemArr.add(juce::var(o));
        }
        rootObj->setProperty("items", itemArr);

        userRoot_.getChildFile("user_library_index.json")
            .replaceWithText(juce::JSON::toString(juce::var(rootObj), false));
    }

    void LibraryManager::rescanAll()
    {
        rescanFactory();
        rescanUser();
    }

    void LibraryManager::scanDirectoryForFactoryItems(const juce::File& dir, LibraryItemType type, SoundCategory fallbackCat)
    {
        if (!dir.isDirectory())
            return;

        const auto files = dir.findChildFiles(juce::File::findFiles, true, "*");
        for (const auto& f : files)
        {
            const auto ext = f.getFileExtension().toLowerCase();
            if (type == LibraryItemType::Preset && ext != ProductInfo::kPresetExtension)
                continue;
            if (type == LibraryItemType::Instrument && ext != ProductInfo::kInstrumentExtension)
                continue;
            if (type == LibraryItemType::Sample && !decoder_.isSupportedAudioFile(f))
                continue;

            LibraryItem item;
            item.filePath  = f.getFullPathName();
            item.id        = "factory::" + f.getRelativePathFrom(factoryRoot_).replaceCharacter('\\', '/');
            item.name      = f.getFileNameWithoutExtension();
            item.isFactory = true;
            item.isFavorite = factoryFavoriteIds_.contains(item.id);
            item.type      = type;
            item.bank      = "Factory";
            item.author    = ProductInfo::kManufacturer;
            item.fileSizeBytes = f.getSize();

            const auto parentName = f.getParentDirectory().getFileName();
            item.category = categoryFromString(parentName);
            if (parentName.isEmpty() || !categoryToString(item.category).equalsIgnoreCase(parentName))
                item.category = fallbackCat;

            if (type == LibraryItemType::Preset || type == LibraryItemType::Instrument)
            {
                const auto parsed = juce::JSON::parse(f);
                if (auto* obj = parsed.getDynamicObject())
                {
                    if (obj->hasProperty("name"))     item.name     = obj->getProperty("name").toString();
                    if (obj->hasProperty("author"))   item.author   = obj->getProperty("author").toString();
                    if (obj->hasProperty("category")) item.category = categoryFromString(obj->getProperty("category").toString());
                    if (obj->hasProperty("tags"))     item.tags     = obj->getProperty("tags").toString();
                }
            }

            factoryItems_.push_back(std::move(item));
        }
    }

    void LibraryManager::rescanFactory()
    {
        factoryItems_.clear();
        if (!factoryRoot_.isDirectory())
            return;

        scanDirectoryForFactoryItems(factoryRoot_.getChildFile("FactoryPresets"), LibraryItemType::Preset, SoundCategory::PlucksAndBells);
        scanDirectoryForFactoryItems(factoryRoot_.getChildFile("FactoryPresets"), LibraryItemType::Instrument, SoundCategory::KeysAndPianos);
        scanDirectoryForFactoryItems(factoryRoot_.getChildFile("FactorySamples"), LibraryItemType::Sample, SoundCategory::DrumsAndPercussion);
    }

    void LibraryManager::scanDirectoryForUserItems(const juce::File& dir, LibraryItemType type)
    {
        if (!dir.isDirectory())
            return;

        const auto files = dir.findChildFiles(juce::File::findFiles, true, "*");
        for (const auto& f : files)
        {
            const auto ext = f.getFileExtension().toLowerCase();
            if (type == LibraryItemType::Preset && ext != ProductInfo::kPresetExtension)
                continue;
            if (type == LibraryItemType::Instrument && ext != ProductInfo::kInstrumentExtension)
                continue;
            if (type == LibraryItemType::Sample && !decoder_.isSupportedAudioFile(f))
                continue;

            const auto fullPath = f.getFullPathName();
            const bool alreadyIndexed = std::any_of(userItems_.begin(), userItems_.end(),
                [&](const LibraryItem& existing) { return existing.filePath == fullPath; });
            if (alreadyIndexed)
                continue;

            LibraryItem item;
            item.id            = "user::" + juce::Uuid().toString();
            item.name          = f.getFileNameWithoutExtension();
            item.filePath      = fullPath;
            item.virtualFolder = f.getParentDirectory().getRelativePathFrom(dir).replaceCharacter('\\', '/');
            if (item.virtualFolder == ".") item.virtualFolder = "";
            item.type          = type;
            item.isFactory     = false;
            item.bank          = "User";
            item.author        = "User";
            item.fileSizeBytes = f.getSize();
            item.category      = (type == LibraryItemType::Sample) ? SoundCategory::DrumsAndPercussion : SoundCategory::PlucksAndBells;

            if (type == LibraryItemType::Preset || type == LibraryItemType::Instrument)
            {
                const auto parsed = juce::JSON::parse(f);
                if (auto* obj = parsed.getDynamicObject())
                {
                    if (obj->hasProperty("name"))     item.name     = obj->getProperty("name").toString();
                    if (obj->hasProperty("author"))   item.author   = obj->getProperty("author").toString();
                    if (obj->hasProperty("category")) item.category = categoryFromString(obj->getProperty("category").toString());
                    if (obj->hasProperty("tags"))     item.tags     = obj->getProperty("tags").toString();
                }
            }

            userItems_.push_back(std::move(item));
        }
    }

    void LibraryManager::rescanUser()
    {
        loadUserIndex();
        scanDirectoryForUserItems(userRoot_.getChildFile("Presets"), LibraryItemType::Preset);
        scanDirectoryForUserItems(userRoot_.getChildFile("Instruments"), LibraryItemType::Instrument);
        scanDirectoryForUserItems(userRoot_.getChildFile("Samples"), LibraryItemType::Sample);
        saveUserIndex();
    }

    bool LibraryManager::isDuplicateContent(const juce::String& hash, juce::int64 sizeBytes) const
    {
        if (hash.isEmpty())
            return false;

        for (const auto& item : userItems_)
        {
            if (item.fileSizeBytes == sizeBytes && item.contentHash == hash && !item.isMissing)
                return true;
        }
        return false;
    }

    ImportSamplesResult LibraryManager::importAudioFiles(const juce::Array<juce::File>& files,
                                                         SoundCategory category,
                                                         const juce::String& targetFolder,
                                                         bool copyToUserFolder)
    {
        ImportSamplesResult res;
        if (targetFolder.isNotEmpty())
            userFolders_.addIfNotAlreadyThere(targetFolder);

        const auto destBaseDir = userRoot_.getChildFile("Samples").getChildFile(targetFolder);
        if (copyToUserFolder)
            destBaseDir.createDirectory();

        for (const auto& src : files)
        {
            if (!src.existsAsFile() || !decoder_.isSupportedAudioFile(src))
            {
                ++res.failedCount;
                res.messages.add("Unsupported or missing file: " + src.getFileName());
                continue;
            }

            const auto hash = SampleDecoder::computeFileHash(src);
            const auto sizeBytes = src.getSize();
            if (isDuplicateContent(hash, sizeBytes))
            {
                ++res.duplicateCount;
                res.messages.add("Duplicate skipped: " + src.getFileName());
                continue;
            }

            const auto decoded = decoder_.loadSample(src);
            if (!decoded.success || decoded.buffer == nullptr)
            {
                ++res.failedCount;
                res.messages.add(decoded.errorMessage);
                continue;
            }

            juce::File targetFile = src;
            if (copyToUserFolder)
            {
                targetFile = destBaseDir.getNonexistentChildFile(src.getFileNameWithoutExtension(),
                                                                 src.getFileExtension(),
                                                                 false);
                // Copy preserves the original untouched; never writes back to src
                if (!src.copyFileTo(targetFile))
                {
                    ++res.failedCount;
                    res.messages.add("Failed copying file to User Library: " + src.getFileName());
                    continue;
                }
            }

            LibraryItem item;
            item.id            = "user::" + juce::Uuid().toString();
            item.name          = src.getFileNameWithoutExtension();
            item.filePath      = targetFile.getFullPathName();
            item.virtualFolder = targetFolder;
            item.category      = category;
            item.bank          = "User";
            item.author        = "User";
            item.type          = LibraryItemType::Sample;
            item.isFactory     = false;
            item.contentHash   = hash;
            item.fileSizeBytes = sizeBytes;
            item.sampleRate    = decoded.buffer->sampleRate;
            item.channels      = decoded.buffer->numChannels;
            item.durationSec   = static_cast<double>(decoded.buffer->numSamples) / decoded.buffer->sampleRate;

            res.importedIds.add(item.id);
            userItems_.push_back(std::move(item));
            ++res.importedCount;
        }

        saveUserIndex();
        return res;
    }

    bool LibraryManager::createUserFolder(const juce::String& relativeFolderPath)
    {
        const auto clean = relativeFolderPath.trim();
        if (clean.isEmpty())
            return false;

        userFolders_.addIfNotAlreadyThere(clean);
        userRoot_.getChildFile("Samples").getChildFile(clean).createDirectory();
        saveUserIndex();
        return true;
    }

    bool LibraryManager::toggleFavorite(const juce::String& itemId)
    {
        for (auto& fItem : factoryItems_)
        {
            if (fItem.id == itemId)
            {
                fItem.isFavorite = !fItem.isFavorite;
                if (fItem.isFavorite)
                    factoryFavoriteIds_.addIfNotAlreadyThere(itemId);
                else
                    factoryFavoriteIds_.removeString(itemId);
                saveConfig();
                return true;
            }
        }

        for (auto& uItem : userItems_)
        {
            if (uItem.id == itemId)
            {
                uItem.isFavorite = !uItem.isFavorite;
                saveUserIndex();
                return true;
            }
        }
        return false;
    }

    bool LibraryManager::updateUserMetadata(const juce::String& itemId,
                                            const juce::String& newName,
                                            SoundCategory newCategory,
                                            const juce::String& newTags,
                                            const juce::String& newFolder)
    {
        // Factory items are strictly protected from modification
        for (auto& uItem : userItems_)
        {
            if (uItem.id == itemId && !uItem.isFactory)
            {
                if (newName.isNotEmpty()) uItem.name = newName;
                uItem.category = newCategory;
                uItem.tags = newTags;
                uItem.virtualFolder = newFolder;
                if (newFolder.isNotEmpty())
                    userFolders_.addIfNotAlreadyThere(newFolder);
                saveUserIndex();
                return true;
            }
        }
        return false;
    }

    bool LibraryManager::deleteUserItem(const juce::String& itemId)
    {
        // Factory items can never be deleted
        auto it = std::find_if(userItems_.begin(), userItems_.end(),
            [&](const LibraryItem& i) { return i.id == itemId && !i.isFactory; });
        if (it == userItems_.end())
            return false;

        userItems_.erase(it);
        saveUserIndex();
        return true;
    }

    void LibraryManager::registerOrUpdateUserItem(const LibraryItem& item)
    {
        if (item.isFactory)
            return;

        for (auto& existing : userItems_)
        {
            if (existing.id == item.id || existing.filePath == item.filePath)
            {
                existing = item;
                existing.isFactory = false;
                saveUserIndex();
                return;
            }
        }
        auto copy = item;
        copy.isFactory = false;
        userItems_.push_back(std::move(copy));
        saveUserIndex();
    }

    std::vector<LibraryItem> LibraryManager::query(const LibraryQuery& q) const
    {
        std::vector<LibraryItem> results;
        const auto searchLower = q.searchText.trim().toLowerCase();

        auto matches = [&](const LibraryItem& item) -> bool
        {
            if (q.filterByType && item.type != q.type)
                return false;
            if (q.category != SoundCategory::All && item.category != q.category)
                return false;
            if (q.favoritesOnly && !item.isFavorite)
                return false;
            if (q.folderFilter.isNotEmpty() && !item.virtualFolder.startsWithIgnoreCase(q.folderFilter))
                return false;

            if (searchLower.isNotEmpty())
            {
                const bool hit = item.name.toLowerCase().contains(searchLower)
                              || item.tags.toLowerCase().contains(searchLower)
                              || item.author.toLowerCase().contains(searchLower)
                              || categoryToString(item.category).toLowerCase().contains(searchLower);
                if (!hit)
                    return false;
            }
            return true;
        };

        if (q.scope == LibraryScope::All || q.scope == LibraryScope::Factory)
        {
            for (const auto& item : factoryItems_)
                if (matches(item))
                    results.push_back(item);
        }

        if (q.scope == LibraryScope::All || q.scope == LibraryScope::User)
        {
            for (const auto& item : userItems_)
                if (matches(item))
                    results.push_back(item);
        }

        return results;
    }

    const LibraryItem* LibraryManager::findById(const juce::String& id) const
    {
        for (const auto& item : factoryItems_)
            if (item.id == id)
                return &item;
        for (const auto& item : userItems_)
            if (item.id == id)
                return &item;
        return nullptr;
    }

    juce::StringArray LibraryManager::getUserFolders() const
    {
        return userFolders_;
    }
}
