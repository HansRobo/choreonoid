#include "ItemFileDialog.h"
#include "FileDialog.h"
#include "RootItem.h"
#include "MainWindow.h"
#include "MessageView.h"
#include <cnoid/stdx/filesystem>
#include <QMessageBox>
#include <fmt/format.h>
#include "gettext.h"

using namespace std;
using namespace cnoid;
namespace filesystem = cnoid::stdx::filesystem;
using fmt::format;

namespace cnoid {

class ItemFileDialog::Impl
{
public:
    ItemFileDialog* self;
    enum Mode { Load, Save };
    Mode mode;
    const vector<ItemFileIO*>* pFileIoList;
    ItemFileIO* targetFileIO;
    ItemPtr currentItemToSave;
    bool isCurrentItemOptionsApplied;
    QWidget* optionPanel;
    bool isSingletonItem;
    bool isExportMode;
    
    Impl(ItemFileDialog* self);
    ItemList<Item>  loadItems(
        const std::vector<ItemFileIO*>& fileIoList, Item* parentItem, bool doAddition, Item* nextItem);
    bool saveItem(Item* item, const std::vector<ItemFileIO*>& fileIoList);
    std::string getSaveFilename();
    bool initializeFileIoFilters(const vector<ItemFileIO*>& fileIoList);
    void setTargetFileIO(ItemFileIO* fileIO);
    void onFilterSelected(const QString& filter);
    bool onFileDialogFinished(int result);
};

}

ItemFileDialog::ItemFileDialog()
    : FileDialog(MainWindow::instance())
{
    impl = new Impl(this);
}


ItemFileDialog::~ItemFileDialog()
{
    delete impl;
}


ItemFileDialog::Impl::Impl(ItemFileDialog* self)
    : self(self)
{
    pFileIoList = nullptr;
    targetFileIO = nullptr;
    
    self->setViewMode(QFileDialog::List);
    self->setLabelText(QFileDialog::Reject, _("Cancel"));
    self->setOption(QFileDialog::DontConfirmOverwrite);

    optionPanel = nullptr;
    
    QObject::connect(self->fileDialog(), &QFileDialog::filterSelected,
                     [this](const QString& filter){ onFilterSelected(filter); });

    self->sigAboutToFinished().connect(
        [this](int result){ return onFileDialogFinished(result); });

    isExportMode = false;
}


ItemList<Item> ItemFileDialog::loadItems
(const std::vector<ItemFileIO*>& fileIoList, Item* parentItem, bool doAddition, Item* nextItem)
{
    return impl->loadItems(fileIoList, parentItem, doAddition, nextItem);
}


ItemList<Item> ItemFileDialog::Impl::loadItems
(const vector<ItemFileIO*>& fileIoList, Item* parentItem, bool doAddition, Item* nextItem)
{
    mode = Load;
    
    ItemList<Item> loadedItems;

    if(!initializeFileIoFilters(fileIoList)){
        return loadedItems;
    }

    bool isImportMode = (fileIoList.front()->interfaceLevel() == ItemFileIO::Conversion);

    if(self->windowTitle().isEmpty()){
        string title;
        if(!isImportMode){
            title = _("Load {0}");
        } else {
            title = _("Import {0}");
        }
        self->setWindowTitle(format(title, fileIoList.front()->caption()).c_str());
    }
    self->setAcceptMode(QFileDialog::AcceptOpen);

    if(!isImportMode){
        self->setLabelText(QFileDialog::Accept, _("Load"));
    } else {
        self->setLabelText(QFileDialog::Accept, _("Import"));
    }

    self->updatePresetDirectories();

    if(self->exec() == QDialog::Accepted){

        QStringList filenames = self->selectedFiles();

        if(!parentItem){
            parentItem = RootItem::instance();
        }

        if(optionPanel){
            targetFileIO->fetchOptionPanelForLoading();
        }

        bool isSingleton = false;
        ItemPtr item = targetFileIO->findSingletonItemInstance();
        if(item){
            isSingleton = true;
            if(item->parentItem()){
                showWarningDialog(
                    format(_("The singleton instance of {} has already been loaded."),
                           targetFileIO->caption()));
                goto exit;
            }
        }

        for(int i=0; i < filenames.size(); ++i){
            if(!isSingleton){
                item = targetFileIO->createItem();
            }
            targetFileIO->setInvocationType(ItemFileIO::Dialog);
            bool loaded = targetFileIO->loadItem(
                item, filenames[i].toStdString(), parentItem, doAddition, nextItem, nullptr);
            if(loaded){
                loadedItems.push_back(item);
            }
        }
    }

exit:
    for(auto& fileIO : fileIoList){
        if(auto panel = fileIO->getOptionPanelForLoading()){
            panel->setParent(nullptr);
        }
    }

    return loadedItems;
}


void ItemFileDialog::setExportMode(bool on)
{
    impl->isExportMode = on;
}


bool ItemFileDialog::saveItem(Item* item, const std::vector<ItemFileIO*>& fileIoList)
{
    return impl->saveItem(item, fileIoList);
}


bool ItemFileDialog::Impl::saveItem(Item* item, const std::vector<ItemFileIO*>& fileIoList)
{
    mode = Save;
    currentItemToSave = item;
    isCurrentItemOptionsApplied = false;
    
    if(!initializeFileIoFilters(fileIoList)){
        string message;
        if(!isExportMode){
            message = _("Saving {0} to a file is not supported");
        } else {
            message = _("Exporting {0} to a file is not supported");
        }
        MessageView::instance()->putln(format(message, item->name()), MessageView::HIGHLIGHT);
        currentItemToSave.reset();
        return false;
    }


    string itemLabel = format("{0} \"{1}\"", fileIoList.front()->caption(), item->name());
    
    if(self->windowTitle().isEmpty()){
        string title;
        if(!isExportMode){
            title = _("Save {} as");
        } else {
            title = _("Export {} as");
        }
        self->setWindowTitle(format(title, itemLabel).c_str());
    }
    self->setAcceptMode(QFileDialog::AcceptSave);
    self->setLabelText(QFileDialog::Accept, _("Save"));
    self->setFileMode(QFileDialog::AnyFile);

    self->updatePresetDirectories();

    // default filename
    self->selectFile(item->name().c_str());

    bool saved = false;
    
    if(self->exec() == QDialog::Accepted){
        auto filename = getSaveFilename();
        if(!filename.empty()){
            if(optionPanel){
                targetFileIO->fetchOptionPanelForSaving();
            }
            targetFileIO->setInvocationType(ItemFileIO::Dialog);
            saved = targetFileIO->saveItem(item, filename, nullptr);
        }
    }

    for(auto& fileIO : fileIoList){
        if(auto panel = fileIO->getOptionPanelForLoading()){
            panel->setParent(nullptr);
        }
    }

    currentItemToSave.reset();

    return saved;
}


std::string ItemFileDialog::Impl::getSaveFilename()
{
    std::string filename;
    
    auto filenames = self->selectedFiles();
    if(!filenames.isEmpty()){
        filename = filenames.front().toStdString();

        // add a lacking extension automatically
        auto exts = targetFileIO->extensions();
        if(!exts.empty()){
            bool hasExtension = false;
            string dotextension = filesystem::path(filename).extension().string();
            if(!dotextension.empty()){
                string extension = dotextension.substr(1); // remove the first dot
                if(std::find(exts.begin(), exts.end(), extension) != exts.end()){
                    hasExtension = true;
                }
            }
            if(!hasExtension && !exts.empty()){
                filename += ".";
                filename += exts[0];
            }
        }
    }

    return filename;
}


bool ItemFileDialog::Impl::initializeFileIoFilters(const vector<ItemFileIO*>& fileIoList)
{
    if(fileIoList.empty()){
        return false;
    }
    pFileIoList = &fileIoList;

    QStringList filters;
    for(auto& fileIO : fileIoList){
        filters << makeNameFilter(fileIO->fileTypeCaption(), fileIO->extensions(), false);
    }
    if(filters.size() == 1){
        filters << _("Any files (*)");
    } else {
        // add "any file" filters for the file ios that supports it
    }
    self->setNameFilters(filters);

    setTargetFileIO(fileIoList.front());

    return true;
}


void ItemFileDialog::Impl::setTargetFileIO(ItemFileIO* fileIO)
{
    targetFileIO = fileIO;

    if(optionPanel){
        optionPanel->setParent(nullptr);
        optionPanel = nullptr;
    }
    int api = fileIO->api();
    if(api & ItemFileIO::Options){
        if(mode == Save){
            if(!isCurrentItemOptionsApplied &&
               fileIO->isFormat(currentItemToSave->fileFormat())){
                fileIO->resetOptions();
                fileIO->restoreOptions(currentItemToSave->fileOptions());
                isCurrentItemOptionsApplied = true;
            }
        }
        if(api & ItemFileIO::OptionPanelForLoading){
            optionPanel = fileIO->getOptionPanelForLoading();
            if(optionPanel){
                self->insertOptionPanel(optionPanel);
            }
        } else if(api & ItemFileIO::OptionPanelForSaving){
            optionPanel = fileIO->getOptionPanelForSaving(currentItemToSave);
            if(optionPanel){
                self->insertOptionPanel(optionPanel);
            }
        }
    }

    if(mode == Load){
        if(fileIO->isRegisteredForSingletonItem()){
            self->setFileMode(QFileDialog::ExistingFile);
        } else {
            self->setFileMode(QFileDialog::ExistingFiles);
        }
    }
}


void ItemFileDialog::Impl::onFilterSelected(const QString& filter)
{
    auto filters = self->nameFilters();
    int index = filters.indexOf(QRegExp(filter, Qt::CaseSensitive, QRegExp::FixedString));
    if(index >= pFileIoList->size()){
        index = 0;
    }
    setTargetFileIO((*pFileIoList)[index]);
}


bool ItemFileDialog::Impl::onFileDialogFinished(int result)
{
    bool finished = true;
    
    if(mode == Save && result == QDialog::Accepted){
        auto filename = getSaveFilename();
        if(filesystem::exists(filename)){
            self->fileDialog()->show();
            QString file(filesystem::path(filename).filename().native().c_str());
            QString message(QString(_("%1 already exists. Do you want to replace it? ")).arg(file));
            auto button =
                QMessageBox::warning(self, self->windowTitle(), message, QMessageBox::Ok | QMessageBox::Cancel);
            if(button == QMessageBox::Cancel){
                finished = false;
            }
        }
    }

    return finished;
}


QString ItemFileDialog::makeNameFilter
(const std::string& caption, const std::vector<std::string>& extensions, bool isAnyEnabled)
{
    QString filter(caption.c_str());

    if(!extensions.empty()){
        QString prefix = " (";
        for(auto& ext : extensions){
            filter += prefix;
            filter += "*.";
            filter += ext.c_str();
            prefix = " ";
        }
        filter += ")";
    } else if(isAnyEnabled){
        filter += " (*)";
    }

    return filter;
}
