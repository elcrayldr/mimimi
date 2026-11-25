#include "UI/Components/BrowserSidebar.h"
#include "UI/Theme/ColorPalette.h"
#include "Common/Categories.h"

namespace cray
{
    BrowserSidebar::BrowserSidebar(LibraryManager& library)
        : library_(library)
    {
        auto setupScopeBtn = [&](juce::TextButton& b, LibraryScope sc)
        {
            b.onClick = [this, sc]()
            {
                activeScope_ = sc;
                scopeAllBtn_.setToggleState(sc == LibraryScope::All, juce::dontSendNotification);
                scopeFactoryBtn_.setToggleState(sc == LibraryScope::Factory, juce::dontSendNotification);
                scopeUserBtn_.setToggleState(sc == LibraryScope::User, juce::dontSendNotification);
                refreshItems();
            };
            addAndMakeVisible(b);
        };
        setupScopeBtn(scopeAllBtn_, LibraryScope::All);
        setupScopeBtn(scopeFactoryBtn_, LibraryScope::Factory);
        setupScopeBtn(scopeUserBtn_, LibraryScope::User);
        scopeAllBtn_.setToggleState(true, juce::dontSendNotification);

        auto setupTypeBtn = [&](juce::TextButton& b, LibraryItemType tp)
        {
            b.onClick = [this, tp]()
            {
                activeType_ = tp;
                typePresetBtn_.setToggleState(tp == LibraryItemType::Preset, juce::dontSendNotification);
                typeInstBtn_.setToggleState(tp == LibraryItemType::Instrument, juce::dontSendNotification);
                typeSampleBtn_.setToggleState(tp == LibraryItemType::Sample, juce::dontSendNotification);
                refreshItems();
            };
            addAndMakeVisible(b);
        };
        setupTypeBtn(typePresetBtn_, LibraryItemType::Preset);
        setupTypeBtn(typeInstBtn_, LibraryItemType::Instrument);
        setupTypeBtn(typeSampleBtn_, LibraryItemType::Sample);
        typePresetBtn_.setToggleState(true, juce::dontSendNotification);

        searchBox_.setTextToShowWhenEmpty("Search presets, tags, samples...", Palette::textMuted);
        searchBox_.onTextChange = [this]() { refreshItems(); };
        addAndMakeVisible(searchBox_);

        categoryBox_.addItem("All Categories", 1);
        for (int i = 0; i < static_cast<int>(kCategoryNames.size()); ++i)
            categoryBox_.addItem(kCategoryNames[static_cast<size_t>(i)], i + 2);
        categoryBox_.setSelectedId(1, juce::dontSendNotification);
        categoryBox_.onChange = [this]() { refreshItems(); };
        addAndMakeVisible(categoryBox_);

        folderBox_.addItem("All Folders", 1);
        folderBox_.setSelectedId(1, juce::dontSendNotification);
        folderBox_.onChange = [this]() { refreshItems(); };
        addAndMakeVisible(folderBox_);

        favOnlyBtn_.onClick = [this]()
        {
            favOnly_ = !favOnly_;
            favOnlyBtn_.setToggleState(favOnly_, juce::dontSendNotification);
            refreshItems();
        };
        addAndMakeVisible(favOnlyBtn_);

        importBtn_.onClick = [this]()
        {
            if (onImportSamplesRequested) onImportSamplesRequested();
        };
        addAndMakeVisible(importBtn_);

        userDirBtn_.setTooltip("Select User Library root folder on any drive");
        userDirBtn_.onClick = [this]()
        {
            if (onChangeUserFolderRequested) onChangeUserFolderRequested();
        };
        addAndMakeVisible(userDirBtn_);

        newFolderBtn_.setTooltip("Create new folder in User Library");
        newFolderBtn_.onClick = [this]()
        {
            auto* alert = new juce::AlertWindow("New User Folder", "Enter folder name:", juce::MessageBoxIconType::QuestionIcon);
            alert->addTextEditor("folder", "My Urban Sounds");
            alert->getTextEditor("folder")->setTextToShowWhenEmpty("Folder or subfolder (e.g. Bass/808s)", Palette::textMuted);
            alert->addButton("Create", 1, juce::KeyPress(juce::KeyPress::returnKey));
            alert->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
            alert->enterModalState(true, juce::ModalCallbackFunction::create(
                [this, alert](int r)
                {
                    if (r == 1)
                    {
                        const auto requestedFolder = alert->getTextEditorContents("folder").trim();
                        if (library_.createUserFolder(requestedFolder))
                            selectUserFolder(requestedFolder);
                        else
                            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                "Could not create folder",
                                "Choose a valid folder name or relative path. Absolute paths and '..' are not allowed.");
                    }
                    delete alert;
                }), false);
        };
        addAndMakeVisible(newFolderBtn_);

        deleteUserBtn_.setTooltip("Remove selected User item (Factory items are protected)");
        deleteUserBtn_.onClick = [this]()
        {
            const auto sel = getSelectedItems();
            for (const auto& item : sel)
            {
                if (!item.isFactory)
                    library_.deleteUserItem(item.id);
            }
            refreshItems();
        };
        addAndMakeVisible(deleteUserBtn_);

        listBox_.setModel(this);
        listBox_.setMultipleSelectionEnabled(true);
        listBox_.setRowHeight(36);
        addAndMakeVisible(listBox_);

        refreshItems();
    }

    void BrowserSidebar::refreshItems()
    {
        // Rebuild folder dropdown cleanly
        const auto prevFolderText = folderBox_.getText();
        folderBox_.clear(juce::dontSendNotification);
        folderBox_.addItem("All Folders", 1);
        int idCounter = 2;
        for (const auto& fld : library_.getUserFolders())
        {
            if (fld.isNotEmpty())
                folderBox_.addItem(fld, idCounter++);
        }
        int restoredId = 1;
        for (int i = 0; i < folderBox_.getNumItems(); ++i)
        {
            if (folderBox_.getItemText(i) == prevFolderText)
            {
                restoredId = folderBox_.getItemId(i);
                break;
            }
        }
        folderBox_.setSelectedId(restoredId, juce::dontSendNotification);

        LibraryQuery q;
        q.scope         = activeScope_;
        q.type          = activeType_;
        q.filterByType  = true;
        q.favoritesOnly = favOnly_;
        q.searchText    = searchBox_.getText();

        const int catSel = categoryBox_.getSelectedId();
        q.category = (catSel <= 1) ? SoundCategory::All : static_cast<SoundCategory>(catSel - 1);

        if (folderBox_.getSelectedId() > 1)
            q.folderFilter = folderBox_.getText();

        visibleItems_ = library_.query(q);
        listBox_.updateContent();
        listBox_.repaint();
    }

    juce::String BrowserSidebar::getSelectedUserFolder() const
    {
        return folderBox_.getSelectedId() > 1 ? folderBox_.getText() : juce::String("Imported");
    }

    void BrowserSidebar::selectUserFolder(const juce::String& relativeFolderPath)
    {
        refreshItems();
        for (int i = 0; i < folderBox_.getNumItems(); ++i)
        {
            if (folderBox_.getItemText(i) == relativeFolderPath)
            {
                folderBox_.setSelectedId(folderBox_.getItemId(i), juce::dontSendNotification);
                refreshItems();
                return;
            }
        }
    }

    std::vector<LibraryItem> BrowserSidebar::getSelectedItems() const
    {
        std::vector<LibraryItem> out;
        const auto rows = listBox_.getSelectedRows();
        for (int i = 0; i < rows.size(); ++i)
        {
            const int r = rows[i];
            if (r >= 0 && r < static_cast<int>(visibleItems_.size()))
                out.push_back(visibleItems_[static_cast<size_t>(r)]);
        }
        return out;
    }

    int BrowserSidebar::getNumRows()
    {
        return static_cast<int>(visibleItems_.size());
    }

    void BrowserSidebar::paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected)
    {
        if (rowNumber < 0 || rowNumber >= static_cast<int>(visibleItems_.size()))
            return;

        const auto& item = visibleItems_[static_cast<size_t>(rowNumber)];
        const auto rowRect = juce::Rectangle<float>(2.0f, 1.0f, static_cast<float>(width - 4), static_cast<float>(height - 2));

        g.setColour(rowIsSelected ? Palette::goldPrimary.withAlpha(0.20f) : Palette::bgCard.withAlpha(0.65f));
        g.fillRoundedRectangle(rowRect, 4.0f);
        if (rowIsSelected)
        {
            g.setColour(Palette::goldPrimary);
            g.drawRoundedRectangle(rowRect, 4.0f, 1.0f);
        }

        // Favorite star on right
        const float starX = static_cast<float>(width - 22);
        g.setColour(item.isFavorite ? Palette::goldBright : Palette::textMuted);
        g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
        g.drawText(item.isFavorite ? "*" : "o", static_cast<int>(starX), 4, 18, height - 8, juce::Justification::centred);

        // Badge: FACTORY or USER
        g.setColour(item.isFactory ? Palette::goldDim : Palette::ledCyan.withAlpha(0.35f));
        g.fillRoundedRectangle(8.0f, 6.0f, 32.0f, 13.0f, 2.5f);
        g.setColour(Palette::textPrimary);
        g.setFont(juce::Font(juce::FontOptions(8.0f, juce::Font::bold)));
        g.drawText(item.isFactory ? "FAC" : "USR", 8, 6, 32, 13, juce::Justification::centred);

        // Name & Category
        g.setColour(item.isMissing ? Palette::ledRed : Palette::textPrimary);
        g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
        g.drawText(item.name, 45, 4, width - 72, 16, juce::Justification::centredLeft, true);

        g.setColour(Palette::textSecondary);
        g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::plain)));
        const auto sub = categoryToString(item.category)
                       + (item.virtualFolder.isNotEmpty() ? ("  /  " + item.virtualFolder) : "");
        g.drawText(sub, 45, 19, width - 72, 14, juce::Justification::centredLeft, true);
    }

    void BrowserSidebar::listBoxItemClicked(int row, const juce::MouseEvent& e)
    {
        if (row < 0 || row >= static_cast<int>(visibleItems_.size()))
            return;

        // Clicking favorite star zone on right edge toggles favorite
        if (e.x >= listBox_.getWidth() - 28)
        {
            library_.toggleFavorite(visibleItems_[static_cast<size_t>(row)].id);
            refreshItems();
        }
    }

    void BrowserSidebar::listBoxItemDoubleClicked(int row, const juce::MouseEvent&)
    {
        if (row >= 0 && row < static_cast<int>(visibleItems_.size()))
        {
            if (onItemActivated)
                onItemActivated(visibleItems_[static_cast<size_t>(row)]);
        }
    }

    bool BrowserSidebar::isInterestedInFileDrag(const juce::StringArray&)
    {
        isDragHover_ = true;
        repaint();
        return true;
    }

    void BrowserSidebar::filesDropped(const juce::StringArray& files, int, int)
    {
        isDragHover_ = false;
        repaint();

        juce::Array<juce::File> fileObjs;
        for (const auto& s : files)
            fileObjs.add(juce::File(s));

        if (onFilesDropped && !fileObjs.isEmpty())
            onFilesDropped(fileObjs);
    }

    void BrowserSidebar::paint(juce::Graphics& g)
    {
        g.setColour(Palette::bgPanel);
        g.fillRect(getLocalBounds());

        g.setColour(isDragHover_ ? Palette::goldPrimary : Palette::borderSubtle);
        g.drawRect(getLocalBounds(), isDragHover_ ? 2 : 1);
    }

    void BrowserSidebar::resized()
    {
        auto area = getLocalBounds().reduced(6);

        // Scope row
        auto scopeRow = area.removeFromTop(24);
        const int sw = scopeRow.getWidth() / 3;
        scopeAllBtn_.setBounds(scopeRow.removeFromLeft(sw).reduced(1));
        scopeFactoryBtn_.setBounds(scopeRow.removeFromLeft(sw).reduced(1));
        scopeUserBtn_.setBounds(scopeRow.reduced(1));

        area.removeFromTop(4);
        auto typeRow = area.removeFromTop(24);
        const int tw = typeRow.getWidth() / 3;
        typePresetBtn_.setBounds(typeRow.removeFromLeft(tw).reduced(1));
        typeInstBtn_.setBounds(typeRow.removeFromLeft(tw).reduced(1));
        typeSampleBtn_.setBounds(typeRow.reduced(1));

        area.removeFromTop(5);
        auto searchRow = area.removeFromTop(24);
        favOnlyBtn_.setBounds(searchRow.removeFromRight(38));
        searchRow.removeFromRight(4);
        searchBox_.setBounds(searchRow);

        area.removeFromTop(4);
        categoryBox_.setBounds(area.removeFromTop(24));

        area.removeFromTop(4);
        folderBox_.setBounds(area.removeFromTop(24));

        area.removeFromTop(5);
        auto actionRow = area.removeFromBottom(24);
        const int aw = actionRow.getWidth() / 4;
        importBtn_.setBounds(actionRow.removeFromLeft(aw + 14).reduced(1));
        newFolderBtn_.setBounds(actionRow.removeFromLeft(aw - 4).reduced(1));
        userDirBtn_.setBounds(actionRow.removeFromLeft(aw - 6).reduced(1));
        deleteUserBtn_.setBounds(actionRow.reduced(1));

        area.removeFromBottom(4);
        listBox_.setBounds(area);
    }
}
