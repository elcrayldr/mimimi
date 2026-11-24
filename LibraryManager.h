#pragma once

#include "Library/LibraryItem.h"
#include "Library/SampleDecoder.h"
#include <juce_core/juce_core.h>
#include <vector>

namespace cray
{
    struct ImportSamplesResult
    {
        int               importedCount  = 0;
        int               duplicateCount = 0;
        int               failedCount    = 0;
        juce::StringArray importedIds;
        juce::StringArray messages;
    };

    /**
     * Manages separate Factory Library (read-only, protected) and User Library
     * (configurable on any Windows drive, supports import, drag-and-drop, folders,
     * favorites, search, metadata, duplicate detection, and original file preservation).
     */
    class LibraryManager
    {
    public:
        explicit LibraryManager(SampleDecoder& decoder);

        /** Initialize paths from settings or explicit directories (useful for tests). */
        void initialize(const juce::File& factoryRootDir, const juce::File& userConfigFile);

        void setUserLibraryRoot(const juce::File& newRoot);
        juce::File getUserLibraryRoot() const { return userRoot_; }
        juce::File getFactoryLibraryRoot() const { return factoryRoot_; }

        void rescanAll();
        void rescanFactory();
        void rescanUser();

        /**
         * Import multiple audio files into User Library without modifying original files.
         * @param copyToUserFolder If true, copies intact original files into User Library/Samples/<folder>;
         *                         if false, indexes external references without moving or modifying originals.
         */
        ImportSamplesResult importAudioFiles(const juce::Array<juce::File>& files,
                                             SoundCategory category = SoundCategory::DrumsAndPercussion,
                                             const juce::String& targetFolder = "Imported",
                                             bool copyToUserFolder = true);

        bool createUserFolder(const juce::String& relativeFolderPath);
        bool toggleFavorite(const juce::String& itemId);
        bool updateUserMetadata(const juce::String& itemId,
                                const juce::String& newName,
                                SoundCategory newCategory,
                                const juce::String& newTags,
                                const juce::String& newFolder);
        bool deleteUserItem(const juce::String& itemId);

        void registerOrUpdateUserItem(const LibraryItem& item);

        std::vector<LibraryItem> query(const LibraryQuery& q) const;
        const LibraryItem*       findById(const juce::String& id) const;
        juce::StringArray        getUserFolders() const;

        static juce::File resolveDefaultFactoryRoot();
        static juce::File resolveDefaultConfigFile();

    private:
        void loadConfig();
        void saveConfig() const;
        void loadUserIndex();
        void saveUserIndex() const;
        void scanDirectoryForFactoryItems(const juce::File& dir, LibraryItemType type, SoundCategory fallbackCat);
        void scanDirectoryForUserItems(const juce::File& dir, LibraryItemType type);
        bool isDuplicateContent(const juce::String& hash, juce::int64 sizeBytes) const;

        SampleDecoder&           decoder_;
        juce::File               factoryRoot_;
        juce::File               userRoot_;
        juce::File               configFile_;
        std::vector<LibraryItem> factoryItems_;
        std::vector<LibraryItem> userItems_;
        juce::StringArray        userFolders_;
        juce::StringArray        factoryFavoriteIds_;
    };
}
