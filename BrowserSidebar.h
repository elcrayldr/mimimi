#pragma once

#include "Library/LibraryManager.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace cray
{
    /**
     * Left Browser Sidebar:
     * - Factory vs User Library tabs
     * - Presets vs Instruments vs Samples tabs
     * - Category filter, search bar, favorites filter, folder filter
     * - Drag-and-Drop import, multi-selection, favorite toggle, User Library folder config
     */
    class BrowserSidebar : public juce::Component,
                           public juce::ListBoxModel,
                           public juce::FileDragAndDropTarget
    {
    public:
        explicit BrowserSidebar(LibraryManager& library);

        void refreshItems();
        std::vector<LibraryItem> getSelectedItems() const;
        /** Folder receiving newly imported samples; defaults to Imported when All Folders is selected. */
        juce::String getSelectedUserFolder() const;
        const std::vector<LibraryItem>& getFilteredItems() const noexcept { return visibleItems_; }

        std::function<void(const LibraryItem&)>             onItemActivated;
        std::function<void(const juce::Array<juce::File>&)> onFilesDropped;
        std::function<void()>                               onImportSamplesRequested;
        std::function<void()>                               onChangeUserFolderRequested;

        // ListBoxModel
        int getNumRows() override;
        void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override;
        void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override;
        void listBoxItemClicked(int row, const juce::MouseEvent& e) override;

        // FileDragAndDropTarget
        bool isInterestedInFileDrag(const juce::StringArray& files) override;
        void filesDropped(const juce::StringArray& files, int x, int y) override;

        void paint(juce::Graphics& g) override;
        void resized() override;

    private:
        LibraryManager&          library_;
        std::vector<LibraryItem> visibleItems_;

        juce::TextButton scopeAllBtn_     { "ALL" };
        juce::TextButton scopeFactoryBtn_ { "FACTORY" };
        juce::TextButton scopeUserBtn_    { "USER" };

        juce::TextButton typePresetBtn_   { "PRESETS" };
        juce::TextButton typeInstBtn_     { "INSTR" };
        juce::TextButton typeSampleBtn_   { "SAMPLES" };

        juce::TextEditor searchBox_;
        juce::ComboBox   categoryBox_;
        juce::ComboBox   folderBox_;
        juce::TextButton favOnlyBtn_      { "FAV" };
        juce::TextButton importBtn_       { "+ IMPORT" };
        juce::TextButton userDirBtn_      { "DIR" };
        juce::TextButton newFolderBtn_    { "+ FLD" };
        juce::TextButton deleteUserBtn_   { "DEL" };

        juce::ListBox    listBox_;

        LibraryScope     activeScope_    = LibraryScope::All;
        LibraryItemType  activeType_     = LibraryItemType::Preset;
        bool             favOnly_        = false;
        bool             isDragHover_    = false;

        std::unique_ptr<juce::FileChooser> activeChooser_;
    };
}
