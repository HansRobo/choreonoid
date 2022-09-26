/*! @file
  @author Shin'ichiro Nakaoka
*/

#include "PoseSeqViewBase.h"
#include "BodyMotionGenerationBar.h"
#include "PronunSymbol.h"
#include "PoseFilters.h"
#include "BodyMotionGenerationBar.h"
#include <cnoid/RootItem>
#include <cnoid/LeggedBodyHelper>
#include <cnoid/BodySelectionManager>
#include <cnoid/MessageView>
#include <cnoid/InfoBar>
#include <cnoid/Dialog>
#include <fmt/format.h>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QMouseEvent>
#include <QScrollBar>
#include <QLineEdit>
#include <iostream>
#include "gettext.h"

using namespace std;
using namespace cnoid;
using fmt::format;

namespace {

inline double degree(double rad) { return (180.0 * rad / 3.14159265358979); }
inline double radian(double deg) { return (3.14159265358979 * deg / 180.0); }

}

class ColumnCheckBox : public CheckBox
{
public:
    ColumnCheckBox(std::function<void(Qt::CheckState)> slotOnClicked)
        : slotOnClicked(slotOnClicked) {
    }
    virtual void nextCheckState(){
        slotOnClicked(checkState());
    };
    std::function<void(Qt::CheckState)> slotOnClicked;        
};

class LinkTreeWidgetEx : public LinkDeviceTreeWidget
{
public:
    LinkTreeWidgetEx(QWidget* parent) : LinkDeviceTreeWidget(parent) {
        header()->setSectionResizeMode(nameColumn(), QHeaderView::ResizeToContents);
    }
    virtual QSize sizeHint() const {
        QSize size = QTreeWidget::sizeHint();
        int width = header()->length();
        size.setWidth(width);
        return size;
    }
};

namespace cnoid {

class LinkPositionAdjustmentDialog : public Dialog
{
public:
    RadioButton absoluteRadio;
    RadioButton relativeRadio;
    CheckBox targetAxisCheck[3];
    DoubleSpinBox positionSpin[3];

    LinkPositionAdjustmentDialog(View* parentView)
        : Dialog(parentView) {

        setWindowTitle(_("Link Position Adjustment"));

        auto vbox = new QVBoxLayout;
        auto hbox = new QHBoxLayout;

        vbox->addLayout(hbox);

        absoluteRadio.setText(_("Absolute"));
        hbox->addWidget(&absoluteRadio);
        relativeRadio.setText(_("Relative"));
        relativeRadio.setChecked(true);
        hbox->addWidget(&relativeRadio);

        hbox = new QHBoxLayout;
        vbox->addLayout(hbox);

        const char* axisLabel[] = { "X", "Y", "Z" };
            
        for(int i=0; i < 3; ++i){
            targetAxisCheck[i].setText(axisLabel[i]);
            hbox->addWidget(&targetAxisCheck[i]);
            positionSpin[i].setDecimals(3);
            positionSpin[i].setRange(-99.999, 99.999);
            positionSpin[i].setSingleStep(0.001);
            positionSpin[i].setValue(0.0);
            hbox->addWidget(&positionSpin[i]);
        }

        auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok);
        connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
        vbox->addWidget(buttonBox);

        setLayout(vbox);
    }

    bool storeState(Archive& archive) {
        return true;
    }

    bool restoreState(const Archive& archive) {
        return true;
    }
};


class YawOrientationRotationDialog : public Dialog
{
public:
    DoubleSpinBox angleSpin;
    DoubleSpinBox centerPosSpins[2];

    YawOrientationRotationDialog(View* parentView)
        : Dialog(parentView) {

        setWindowTitle(_("Yaw Orientation Rotation"));

        auto vbox = new QVBoxLayout;
        auto hbox = new QHBoxLayout;
        vbox->addLayout(hbox);

        hbox->addWidget(new QLabel(_("Center:")));
        hbox->addSpacing(8);
        const char* axisLabel[] = { "X", "Y" };
        for(int i=0; i < 2; ++i){
            hbox->addWidget(new QLabel(axisLabel[i]));
            centerPosSpins[i].setDecimals(3);
            centerPosSpins[i].setRange(-99.999, 99.999);
            centerPosSpins[i].setSingleStep(0.001);
            hbox->addWidget(&centerPosSpins[i]);
        }

        hbox = new QHBoxLayout;
        vbox->addLayout(hbox);

        hbox->addWidget(new QLabel(_("Angle")));
        angleSpin.setDecimals(1);
        angleSpin.setRange(0.1, 90.0);
        angleSpin.setSingleStep(0.1);
        hbox->addWidget(&angleSpin);
        hbox->addWidget(new QLabel(_("[deg]")));

        auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok);
        connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
        vbox->addWidget(buttonBox);
            
        setLayout(vbox);
    }

    bool storeState(Archive& archive) {
        return true;
    }

    bool restoreState(const Archive& archive) {
        return true;
    }
};
    

class PoseSelectionDialog : public Dialog
{
public:
    DoubleSpinBox startTimeSpin;
    DoubleSpinBox endTimeSpin;
    RadioButton allPartRadio;
    RadioButton selectedPartRadio;
    RadioButton justSelectedPartRadio;

    PoseSelectionDialog(View* parentView)
        : Dialog(parentView) {

        setWindowTitle(_("Select Specified Key Poses"));

        auto vbox = new QVBoxLayout;

        auto hbox = new QHBoxLayout;
        vbox->addLayout(hbox);

        hbox->addWidget(new QLabel(_("Start")));
        startTimeSpin.setDecimals(2);
        startTimeSpin.setRange(0.00, 999.99);
        startTimeSpin.setSingleStep(0.01);
        hbox->addWidget(&startTimeSpin);
        hbox->addWidget(new QLabel(_("[s]")));

        hbox->addWidget(new QLabel(_("End")));
        endTimeSpin.setDecimals(2);
        endTimeSpin.setRange(0.00, 999.99);
        endTimeSpin.setSingleStep(0.01);
        hbox->addWidget(&endTimeSpin);
        hbox->addWidget(new QLabel(_("[s]")));
            
        hbox = new QHBoxLayout;
        vbox->addLayout(hbox);

        allPartRadio.setText(_("all parts"));
        hbox->addWidget(&allPartRadio);
        selectedPartRadio.setText(_("having selected parts"));
        selectedPartRadio.setChecked(true);
        hbox->addWidget(&selectedPartRadio);
        justSelectedPartRadio.setText(_("just selected parts"));
        hbox->addWidget(&justSelectedPartRadio);

        auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok);
        connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
        vbox->addWidget(buttonBox);
            
        setLayout(vbox);
    }

    bool storeState(Archive& archive) {
        return true;
    }

    bool restoreState(const Archive& archive) {
        return true;
    }
};
}


PoseSeqViewBase::PoseSeqViewBase(View* view)
    : view(view),
      os(MessageView::mainInstance()->cout()),
      textForEmptyName("----------"),
      menuManager(&popupMenu)
{
    view->sigActivated().connect([this](){ onViewActivated(); });
    view->sigDeactivated().connect([this](){ onViewDeactivated(); });

    currentTime = 0.0;

    timeBar = TimeBar::instance();

    auto generationBar = BodyMotionGenerationBar::instance();
    timeScale = generationBar->timeScaleRatio();
    staticConnections.add(
        generationBar->sigInterpolationParametersChanged().connect(
            [this](){ onInterpolationParametersChanged(); }));

    setupOperationParts();
    setupLinkTreeWidget();

    staticConnections.add(
        RootItem::instance()->sigSelectedItemsChanged().connect(
            [this](const ItemList<>& selectedItems){ onSelectedItemsChanged(selectedItems); }));

    isSelectedPoseMoving = false;

    copiedPoses = new PoseSeq;
    
    poseSelectionDialog = new PoseSelectionDialog(view);
    poseSelectionDialog->sigAccepted().connect(
        [this](){ onPoseSelectionDialogAccepted(); });

    linkPositionAdjustmentDialog = new LinkPositionAdjustmentDialog(view);
    linkPositionAdjustmentDialog->sigAccepted().connect(
        [this](){ onLinkPositionAdjustmentDialogAccepted(); });

    yawOrientationRotationDialog = new YawOrientationRotationDialog(view);
    yawOrientationRotationDialog->sigAccepted().connect(
        [this](){ onYawOrientationRotationDialogAccepted(); });

    menuManager.addItem(_("Select all poses after current position"))->sigTriggered().connect(
        [this](){ selectAllPosesAfterCurrentPosition(); });
    menuManager.addItem(_("Select all poses before current position"))->sigTriggered().connect(
        [this](){ selectAllPosesBeforeCurrentPosition(); });
    menuManager.addItem(_("Adjust step positions"))->sigTriggered().connect(
        [this](){ onAdjustStepPositionsActivated(); });
    menuManager.addItem(_("Count selected key poses"))->sigTriggered().connect(
        [this](){ countSelectedKeyPoses(); });
}
        


PoseSeqViewBase::~PoseSeqViewBase()
{
    staticConnections.disconnect();
    poseSeqConnections.disconnect();
    connectionOfBodyKinematicStateUpdated.disconnect();
}


void PoseSeqViewBase::onViewActivated()
{
    if(timeSyncCheck.isChecked()){
        if(!connectionOfTimeChanged.connected()){
            connectionOfTimeChanged = timeBar->sigTimeChanged().connect(
                [this](double time){ return onTimeChanged(time); });
        }
        onTimeChanged(timeBar->time());
    }
}


void PoseSeqViewBase::onViewDeactivated()
{
    connectionOfTimeChanged.disconnect();
}


void PoseSeqViewBase::onTimeSyncCheckToggled()
{
    if(timeSyncCheck.isChecked()){
        if(!connectionOfTimeChanged.connected()){
            connectionOfTimeChanged = timeBar->sigTimeChanged().connect(
                [this](double time){ return onTimeChanged(time); });
        }
    } else {
        connectionOfTimeChanged.disconnect();
    }
}


void PoseSeqViewBase::setupOperationParts()
{
    currentItemLabel.setText(textForEmptyName);
    currentItemLabel.setAlignment(Qt::AlignCenter);

    insertPoseButton.setText(_(" Insert "));
    insertPoseButton.setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    insertPoseButton.setToolTip(_("Insert a new pose at the current time position"));
    insertPoseButton.sigClicked().connect(
        [this](){ onInsertPoseButtonClicked(); });

    transitionTimeSpin.setToolTip(_("Transition time of a newly inserted pose"));
    transitionTimeSpin.setAlignment(Qt::AlignCenter);
    transitionTimeSpin.setDecimals(3);
    transitionTimeSpin.setRange(0.0, 9.999);
    transitionTimeSpin.setSingleStep(0.005);
    transitionTimeSpin.sigEditingFinished().connect(
        [this](){ onInsertPoseButtonClicked(); });

    updateButton.setText(_("Update"));
    updateButton.setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    updateButton.setToolTip(_("Update the selected pose with the current robot state"));
    updateButton.sigClicked().connect(
        [this](){ onUpdateButtonClicked(); });

    updateAllToggle.setText(_("All"));
    updateAllToggle.setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    updateAllToggle.setToolTip(_("The update button updates all the element of the selected pose."));
    updateAllToggle.setChecked(true);
    
    autoUpdateModeCheck.setText(_("Auto"));
    autoUpdateModeCheck.setToolTip(_("The selected pose is automatically updated when the robot state changes."));
    autoUpdateModeCheck.setChecked(false);

    deleteButton.setText(_("Delete"));
    deleteButton.setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    deleteButton.sigClicked().connect(
        [this](){ onDeleteButtonClicked(); });
    
    timeSyncCheck.setText(_("Time sync"));
    timeSyncCheck.setChecked(true);
    timeSyncCheck.sigToggled().connect(
        [this](bool){ onTimeSyncCheckToggled(); });
}

        
void PoseSeqViewBase::setupLinkTreeWidget()
{
    linkTreeWidget = new LinkTreeWidgetEx(view);

    //linkTreeWidget->setStyleSheet("QTreeView::item { border-right: 1px solid black }");
    
    auto header = linkTreeWidget->header();
    header->hideSection(linkTreeWidget->numberColumn());

    poseForDefaultStateSetting = new Pose;
    
    baseLinkColumn = linkTreeWidget->addColumn("BL");
    linkTreeWidget->moveVisualColumnIndex(baseLinkColumn, 0);
    baseLinkRadioGroup = nullptr;

    validPartColumn = linkTreeWidget->addColumn("ON");
    stationaryPointColumn = linkTreeWidget->addColumn("SP");
    ikPartColumn = linkTreeWidget->addColumn("IK");
    
    zmpRow = new LinkDeviceTreeItem("ZMP");
    linkTreeWidget->addCustomRow(zmpRow);

    linkTreeWidget->sigUpdateRequest().connect(
        [this](bool isInitialCreation){ onLinkTreeUpdateRequest(isInitialCreation); });

    linkTreeWidget->setFrameShape(QFrame::NoFrame);
    linkTreeWidget->setCacheEnabled(true);
    linkTreeWidget->setListingMode(LinkDeviceTreeWidget::GroupedTree);

    MenuManager& mm = linkTreeWidget->popupMenuManager();
    mm.addItem(_("Select key poses having the selected links"))->sigTriggered().connect(
        [this](){ selectPosesHavingSelectedLinks(); });
    mm.addItem(_("Select key poses just having the selected links"))->sigTriggered().connect(
        [this](){ selectPosesJustHavingSelectedLinks(); });
    mm.addItem(_("Remove the selected parts from the selected poses"))->sigTriggered().connect(
        [this](){ removeSelectedPartsFromKeyPoses(); });
}


bool PoseSeqViewBase::isChecked(LinkDeviceTreeItem* item, int column)
{
    auto check = dynamic_cast<QAbstractButton*>(linkTreeWidget->alignedItemWidget(item, column));
    return check ? check->isChecked() : Qt::Unchecked;
}


void PoseSeqViewBase::setChecked(LinkDeviceTreeItem* item, int column, bool checked)
{
    auto check = dynamic_cast<QAbstractButton*>(linkTreeWidget->alignedItemWidget(item, column));
    if(check){
        check->setChecked(checked);
    }
}


void PoseSeqViewBase::setCheckState(LinkDeviceTreeItem* item, int column, Qt::CheckState state)
{
    auto check = dynamic_cast<QCheckBox*>(linkTreeWidget->alignedItemWidget(item, column));
    if(check){
        check->setCheckState(state);
    }
}


void PoseSeqViewBase::onLinkTreeUpdateRequest(bool isInitialCreation)
{
    if(!linkTreeWidget->bodyItem()){
        setCurrentPoseSeqItem(nullptr);
    } else {
        if(isInitialCreation){
            initializeLinkTree();
        }
        updateLinkTreeModel();
    }
}


void PoseSeqViewBase::initializeLinkTree()
{
    poseForDefaultStateSetting->clear();

    if(baseLinkRadioGroup){
        delete baseLinkRadioGroup;
    }
    baseLinkRadioGroup = new ButtonGroup(linkTreeWidget);
    baseLinkRadioGroup->sigButtonClicked().connect(
        [this](int){ onBaseLinkRadioClicked(); });

    initializeLinkTreeIkLinkColumn();

    auto rootLink = body->rootLink();
    poseForDefaultStateSetting->setBaseLink(rootLink->index(), rootLink->p(), rootLink->R());

    initializeLinkTreeTraverse(linkTreeWidget->invisibleRootItem());
}


void PoseSeqViewBase::initializeLinkTreeIkLinkColumn()
{
    const Mapping& info = *body->info();

    possibleIkLinkFlag.clear();
    possibleIkLinkFlag.resize(body->numLinks(), false);

    if(body->numLinks() > 0){
        const Listing& possibleIkLinks = *info.findListing("possibleIkInterpolationLinks");
        if(possibleIkLinks.isValid()){
            for(int i=0; i < possibleIkLinks.size(); ++i){
                if(auto link = body->link(possibleIkLinks[i])){
                    possibleIkLinkFlag[link->index()] = true;
                    if(auto item = linkTreeWidget->itemOfLink(link->index())){
                        auto checkBox = new ColumnCheckBox(
                            [this, item](Qt::CheckState state){ onIkPartCheckClicked(item, state); });
                        linkTreeWidget->setAlignedItemWidget(item, ikPartColumn, checkBox);
                    }
                }
            }
        }
        if(!body->isFixedRootModel()){
            possibleIkLinkFlag[0] = true;
        }
    }
    
    const Listing& defaultIkLinks = *info.findListing("defaultIkInterpolationLinks");
    if(defaultIkLinks.isValid()){
        for(int i=0; i < defaultIkLinks.size(); ++i){
            if(auto link = body->link(defaultIkLinks[i])){
                poseForDefaultStateSetting->addIkLink(link->index());
            }
        }
    }
}


void PoseSeqViewBase::initializeLinkTreeTraverse(QTreeWidgetItem* parentItem)
{
    int n = parentItem->childCount();
    for(int i=0; i < n; ++i){
        auto item = dynamic_cast<LinkDeviceTreeItem*>(parentItem->child(i));
        if(!item){
            continue;
        }
        auto link = item->link();

        if(!link || link->parent()){

            auto checkBox = new ColumnCheckBox(
                [this, item](Qt::CheckState state){ onValidPartCheckClicked(item, state); });
            
            if(link && link->jointId() >= 0){
                poseForDefaultStateSetting->setJointPosition(link->jointId(), 0.0);
            } else if(item->isLinkGroup()){
                checkBox->setTristate(true);
            }
            linkTreeWidget->setAlignedItemWidget(item, validPartColumn, checkBox);
        }
        if(link){
            auto radioButton = new RadioButton;
            baseLinkRadioGroup->addButton(radioButton, link->index());
            linkTreeWidget->setAlignedItemWidget(item, baseLinkColumn, radioButton);
        }

        auto spCheck = new ColumnCheckBox(
            [this, item](Qt::CheckState state){ onStationaryPointCheckClicked(item, state); });
        linkTreeWidget->setAlignedItemWidget(item, stationaryPointColumn, spCheck);
        
        initializeLinkTreeTraverse(item);
    }
}


void PoseSeqViewBase::togglePoseAttribute(std::function<bool(Pose* pose)> toggleFunction)
{
    if(selectedPoseIters.empty()){
        if(toggleFunction(poseForDefaultStateSetting)){
            updateLinkTreeModel();
        }

    } else {
        currentPoseSeqItem->beginEditing();
        bool modified = false;
        for(auto p = selectedPoseIters.begin(); p != selectedPoseIters.end(); ++p){
            PosePtr pose = (*p)->get<Pose>();
            if(pose){
                seq->beginPoseModification(*p);
                modified = toggleFunction(pose);
                if(modified){
                    seq->endPoseModification(*p);
                }
            }
        }
        currentPoseSeqItem->endEditing(modified);

        //! \todo This should be processed by PoseSeq slots
        if(modified){
            doAutomaticInterpolationUpdate();
        }
    }
}


void PoseSeqViewBase::onBaseLinkRadioClicked()
{
    int linkIndex = baseLinkRadioGroup->checkedId();
    Link* link = (linkIndex >= 0) ? body->link(linkIndex) : nullptr;
    togglePoseAttribute([this, link](Pose* pose){ return setBaseLink(pose, link); });
}


bool PoseSeqViewBase::setBaseLink(Pose* pose, Link* link)
{
    bool modified = false;
    if(link){
        if(pose->baseLinkIndex() != link->index()){
            pose->setBaseLink(link->index(), link->p(), link->R());
            modified = true;
        }
    } else {
        if(pose->baseLinkInfo()){
            pose->invalidateBaseLink();
            modified = true;
        }
    }
    return modified;
}


void PoseSeqViewBase::onValidPartCheckClicked(LinkDeviceTreeItem* item, Qt::CheckState checkState)
{
    bool on = ((checkState == Qt::Unchecked) || (checkState == Qt::PartiallyChecked));
    
    if(item == zmpRow){
        togglePoseAttribute([this, on](Pose* pose){ return toggleZmp(pose, on); });
    } else {
        if(auto link = item->link()){
            bool isIkPartChecked = isChecked(item, ikPartColumn);
            togglePoseAttribute(
                [this, item, link, on, isIkPartChecked](Pose* pose){
                    return toggleLink(pose, item, link, on, isIkPartChecked); });
        } else {
            togglePoseAttribute(
                [this, item, on](Pose* pose){
                    return togglePart(pose, item, on);
                });
        }
    }
}


bool PoseSeqViewBase::toggleZmp(Pose* pose, bool on)
{
    bool modified = false;
    if(on){
        const Vector3& zmp = currentBodyItem->zmp();
        if(!pose->isZmpValid() || zmp != pose->zmp()){
            pose->setZmp(currentBodyItem->zmp());
            modified = true;
        }
    } else {
        if(pose->isZmpValid()){
            pose->invalidateZmp();
            modified = true;
        }
    }
    return modified;
}



bool PoseSeqViewBase::toggleLink(Pose* pose, LinkDeviceTreeItem* item, Link* link, bool partOn, bool ikOn)
{
    bool modified = false;
    int jId = link->jointId();
    if(partOn){
        if(jId >= 0){
            bool isSpChecked = isChecked(item, stationaryPointColumn);
            if(!pose->isJointValid(jId) || pose->jointPosition(jId) != link->q() ||
               pose->isJointStationaryPoint(jId) != isSpChecked){
                pose->setJointPosition(jId, link->q());
                pose->setJointStationaryPoint(jId, isSpChecked);
                modified = true;
            }
        }
        if(possibleIkLinkFlag[link->index()]){
            Pose::LinkInfo* info = pose->ikLinkInfo(link->index());
            if(!info){
                info = pose->addIkLink(link->index());
                modified = true;
            }
            if(setCurrentLinkStateToIkLink(link, info)){
                modified = true;
            }
            bool isSlave = !ikOn;
            if(info->isSlave() != isSlave){
                info->setSlave(isSlave);
                modified = true;
            }
        }
    } else {
        if(pose->isJointValid(jId)){
            pose->invalidateJoint(jId);
            modified = true;
        }
        if(pose->removeIkLink(link->index())){
            modified = true;
        }
    }
    return modified;
}


bool PoseSeqViewBase::togglePart(Pose* pose, LinkDeviceTreeItem* item, bool on)
{
    bool modified = false;
    
    if(auto link = item->link()){
        bool ikOn = false;
        if(possibleIkLinkFlag[link->index()]){
            if(isChecked(item, validPartColumn)){
                ikOn = isChecked(item, ikPartColumn);
            } else {
                ikOn = on;
            }
        }
        modified = toggleLink(pose, item, link, on, ikOn);
    }

    int n = item->childCount();
    for(int i=0; i < n; ++i){
        if(auto childItem = dynamic_cast<LinkDeviceTreeItem*>(item->child(i))){
            modified |= togglePart(pose, childItem, on);
        }
    }

    return modified;
}


void PoseSeqViewBase::onStationaryPointCheckClicked(LinkDeviceTreeItem* item, Qt::CheckState checkState)
{
    bool on = (checkState == Qt::Unchecked);
    if(item == zmpRow){
        togglePoseAttribute(
            [this, on](Pose* pose){
                return toggleZmpStationaryPoint(pose, on);
            });
    } else {
        if(auto link = item->link()){
            togglePoseAttribute(
                [this, link, on](Pose* pose){
                    return toggleStationaryPoint(pose, link, on);
                });
        } else {
            if(checkState == Qt::PartiallyChecked){
                on = true;
            }
            togglePoseAttribute(
                [this, item, on](Pose* pose){
                    return togglePartStationaryPoints(pose, item, on);
                });
        }
    }
}


bool PoseSeqViewBase::toggleZmpStationaryPoint(Pose* pose, bool on)
{
    bool modified = false;
    if(on){
        if(!pose->isZmpStationaryPoint()){
            pose->setZmpStationaryPoint(on);
            modified = true;
        }
    } else {
        if(pose->isZmpStationaryPoint()){
            pose->setZmpStationaryPoint(on);
            modified = true;
        }
    }
    return modified;
}


bool PoseSeqViewBase::toggleStationaryPoint(Pose* pose, Link* link, bool on)
{
    bool modified = false;

    int id = link->jointId();
    if(pose->isJointValid(id)){
        pose->setJointStationaryPoint(id, on);
        modified = true;
    }
    Pose::LinkInfo* info = pose->ikLinkInfo(link->index());
    if(info){
        info->setStationaryPoint(on);
        modified = true;
    }
    return modified;
}


bool PoseSeqViewBase::togglePartStationaryPoints(Pose* pose, LinkDeviceTreeItem* item, bool on)
{
    bool modified = false;
    
    if(auto link = item->link()){
        modified = toggleStationaryPoint(pose, link, on);
    }
    int n = item->childCount();
    for(int i=0; i < n; ++i){
        if(auto childItem = dynamic_cast<LinkDeviceTreeItem*>(item->child(i))){
            modified |= togglePartStationaryPoints(pose, childItem, on);
        }
    }
    return modified;
}


void PoseSeqViewBase::onIkPartCheckClicked(LinkDeviceTreeItem* item, Qt::CheckState checkState)
{
    if(auto link = item->link()){
        bool ikOn = (checkState == Qt::Unchecked);
        bool partOn = ikOn | isChecked(item, validPartColumn);
        togglePoseAttribute(
            [this, item, link, partOn, ikOn](Pose* pose){
                return toggleLink(pose, item, link, partOn, ikOn);
            });
    }
}


void PoseSeqViewBase::onInterpolationParametersChanged()
{
    double newTimeScale = BodyMotionGenerationBar::instance()->timeScaleRatio();
    if(newTimeScale != timeScale){
        timeScale = newTimeScale;
        onTimeScaleChanged();
    }
}


void PoseSeqViewBase::onTimeScaleChanged()
{
    onSelectedPosesModified();
}


void PoseSeqViewBase::onSelectedItemsChanged(ItemList<PoseSeqItem> selectedItems)
{
    if(auto item = selectedItems.toSingle()){
        setCurrentPoseSeqItem(item);
    }
}


void PoseSeqViewBase::setCurrentPoseSeqItem(PoseSeqItem* poseSeqItem)
{
    if(poseSeqItem == currentPoseSeqItem){
        return;
    }
    
    poseSeqConnections.disconnect();

    currentPoseSeqItem = poseSeqItem;

    setCurrentItemName(poseSeqItem);

    connectionOfBodyKinematicStateUpdated.disconnect();

    seq.reset();
    currentBodyItem.reset();
    body.reset();
    selectedPoseIters.clear();

    if(!poseSeqItem){
        if(linkTreeWidget->bodyItem()){
            linkTreeWidget->setBodyItem(nullptr);
        }

    } else {
        poseSeqConnections.add(
            poseSeqItem->sigNameChanged().connect(
                [this, poseSeqItem](const std::string& /* oldName */){
                    setCurrentItemName(poseSeqItem);
                }));

        seq = currentPoseSeqItem->poseSeq();
        currentPoseIter = seq->end();

        currentBodyItem = poseSeqItem->findOwnerItem<BodyItem>();
        if(currentBodyItem){
            body = currentBodyItem->body();
        }
        linkTreeWidget->setBodyItem(currentBodyItem);
        if(currentBodyItem){
            connectionOfBodyKinematicStateUpdated =
                currentBodyItem->sigKinematicStateUpdated().connect(
                    [this](){ onBodyKinematicStateUpdated(); });
        }
            
        poseSeqConnections.add(
            seq->connectSignalSet(
                [this](PoseSeq::iterator it, bool isMoving){ onPoseInserted(it, isMoving); },
                [this](PoseSeq::iterator it, bool isMoving){ onPoseRemoving(it, isMoving); },
                [this](PoseSeq::iterator it){ onPoseModified(it); }));

        poseSeqConnections.add(
            poseSeqItem->sigDisconnectedFromRoot().connect(
                [this](){ setCurrentPoseSeqItem(nullptr);}));
    }
}


PoseSeqViewBase::PoseIterSet::iterator PoseSeqViewBase::findPoseIterInSelected(PoseSeq::iterator poseIter)
{
    auto range = selectedPoseIters.equal_range(poseIter);
    for(auto p = range.first; p != range.second; ++p){
        if((*p) == poseIter){
            return p;
        }
    }
    return selectedPoseIters.end();
}


bool PoseSeqViewBase::toggleSelection(PoseSeq::iterator poseIter, bool adding, bool changeTime)
{
    if(!(selectedPoseIters.size() == 1 && *selectedPoseIters.begin() == poseIter)){ // Skip same single selection
    
        if(poseIter == seq->end()){
            if(selectedPoseIters.empty()){
                return false;
            }
            selectedPoseIters.clear();

        } else {
            auto p = findPoseIterInSelected(poseIter);
            if(p == selectedPoseIters.end()){
                if(!adding){
                    selectedPoseIters.clear();
                }
                selectedPoseIters.insert(poseIter);
            } else {
                if(adding){
                    selectedPoseIters.erase(p);
                }
            }
        }
        
        updateLinkTreeModel();
        onSelectedPosesModified();
    }
    
    if(changeTime && (poseIter != seq->end())){
        double time = timeScale * poseIter->time();
        if(timeSyncCheck.isChecked()){
            timeBar->setTime(time);
        } else {
            onTimeChanged(time);
        }
    }

    return true;
}


void PoseSeqViewBase::selectAllPoses()
{
    selectedPoseIters.clear();
    for(auto it = seq->begin(); it != seq->end(); ++it){
        selectedPoseIters.insert(it);
    }
    updateLinkTreeModel();
    onSelectedPosesModified();
}


void PoseSeqViewBase::selectAllPosesAfterCurrentPosition()
{
    selectedPoseIters.clear();
    auto it = seq->seek(seq->begin(), currentTime);
    while(it != seq->end()){
        selectedPoseIters.insert(it++);
    }
    updateLinkTreeModel();
    onSelectedPosesModified();
}


void PoseSeqViewBase::selectAllPosesBeforeCurrentPosition()
{
    selectedPoseIters.clear();
    if(!seq->empty()){
        auto it = seq->seek(seq->begin(), currentTime);
        if(it != seq->end() && (it->time() == currentTime)){
            ++it;
        }
        do {
            selectedPoseIters.insert(--it);
        } while(it != seq->begin());
    }
    updateLinkTreeModel();
    onSelectedPosesModified();
}


void PoseSeqViewBase::selectPosesHavingSelectedLinks()
{
    if(!body || !seq){
        return;
    }
    
    const vector<int> selectedLinkIndices = linkTreeWidget->selectedLinkIndices();

    selectedPoseIters.clear();
    for(auto it = seq->begin(); it != seq->end(); ++it){
        if(auto pose = it->get<Pose>()){
            bool match = true;
            for(size_t i=0; i < selectedLinkIndices.size(); ++i){
                int linkIndex = selectedLinkIndices[i];
                if(!pose->isJointValid(body->link(linkIndex)->jointId())){
                    if(!pose->ikLinkInfo(linkIndex)){
                        match = false;
                        break;
                    }
                }
            }
            if(match){
                selectedPoseIters.insert(it);
            }
        }
    }
    updateLinkTreeModel();
    onSelectedPosesModified();
}


void PoseSeqViewBase::selectPosesJustHavingSelectedLinks()
{
    if(!body || !seq){
        return;
    }
    
    const auto& linkSelection = linkTreeWidget->linkSelection();

    selectedPoseIters.clear();
    for(auto it = seq->begin(); it != seq->end(); ++it){
        if(auto pose = it->get<Pose>()){
            bool match = true;
            for(size_t linkIndex=0; linkIndex < linkSelection.size(); ++linkIndex){
                bool hasLink = pose->isJointValid(body->link(linkIndex)->jointId()) || pose->ikLinkInfo(linkIndex);
                if((linkSelection[linkIndex] && !hasLink) || (!linkSelection[linkIndex] && hasLink)){
                    match = false;
                    break;
                }
            }
            if(match){
                selectedPoseIters.insert(it);
            }
        }
    }
    updateLinkTreeModel();
    onSelectedPosesModified();
}


void PoseSeqViewBase::removeSelectedPartsFromKeyPoses()
{
    if(!body || !seq || selectedPoseIters.empty()){
        return;
    }
    
    auto& selected = linkTreeWidget->selectedLinkIndices();
    bool doRemoveZmp = zmpRow->isSelected();

    if(selected.empty() && !doRemoveZmp){
        return;
    }

    PoseIterSet orgSelected(selectedPoseIters);
    currentPoseSeqItem->beginEditing();
    bool removed = false;

    for(auto it = orgSelected.begin(); it != orgSelected.end(); ++it){
        if(auto pose = (*it)->get<Pose>()){
            seq->beginPoseModification(*it);
            bool modified = false;
            for(size_t i=0; i < selected.size(); ++i){
                int linkIndex = selected[i];
                int jointId = body->link(linkIndex)->jointId();
                if(jointId >= 0){
                    modified |= pose->invalidateJoint(jointId);
                }
                modified |= pose->removeIkLink(linkIndex);
            }
            if(doRemoveZmp){
                modified |= pose->invalidateZmp();
            }
            if(pose->empty()){
                seq->erase(*it);
            } else if(modified){
                seq->endPoseModification(*it);
            }
            removed |= modified;
        }
    }
    
    if(currentPoseSeqItem->endEditing(removed)){
        doAutomaticInterpolationUpdate();
    }
}


bool PoseSeqViewBase::deleteSelectedPoses()
{
    if(!selectedPoseIters.empty()){
        PoseIterSet orgSelected(selectedPoseIters);
        currentPoseSeqItem->beginEditing();
        for(auto it = orgSelected.begin(); it != orgSelected.end(); ++it){
            seq->erase(*it);
        }
        currentPoseSeqItem->endEditing();
        doAutomaticInterpolationUpdate();
        return true;
    }
    return false;
}


bool PoseSeqViewBase::cutSelectedPoses()
{
    if(copySelectedPoses()){
        return deleteSelectedPoses();
    }
    return false;
}


bool PoseSeqViewBase::copySelectedPoses()
{
    if(!selectedPoseIters.empty()){
        copiedPoses = new PoseSeq;
        auto destIter = copiedPoses->begin();
        double offset = - (*selectedPoseIters.begin())->time();
        for(auto it = selectedPoseIters.begin(); it != selectedPoseIters.end(); ++it){
            PoseSeq::iterator srcIter = *it;
            destIter = copiedPoses->copyElement(destIter, srcIter, offset);
        }    
        return true;
    }
    return false;
}


bool PoseSeqViewBase::pasteCopiedPoses(double timeToPaste)
{
    if(!copiedPoses->empty()){
        currentPoseSeqItem->beginEditing();
        auto destIter = seq->seek(currentPoseIter, timeToPaste, true);
        for(auto it = copiedPoses->begin(); it != copiedPoses->end(); ++it){
            destIter = seq->copyElement(destIter, it, timeToPaste);
        }
        currentPoseIter = destIter;
        currentPoseSeqItem->endEditing();
        doAutomaticInterpolationUpdate();        
        return true;
    }
    return false;
}


/**
   @ret true if actually modified
*/
bool PoseSeqViewBase::moveSelectedPoses(double time0)
{
    bool modified = false;

    if(!selectedPoseIters.empty()){
        time0 = std::max(0.0, time0);
        double diff = time0 - (*selectedPoseIters.begin())->time();
        if(diff != 0.0){
            // Copy is needed because selectedPoseIters may change during the following loops
            PoseIterSet tmpSelectedPoseIters(selectedPoseIters);
            if(diff > 0.0){
                for(auto p = tmpSelectedPoseIters.rbegin(); p != tmpSelectedPoseIters.rend(); ++p){
                    seq->changeTime(*p, (*p)->time() + diff);
                }
            } else {
                for(auto p = tmpSelectedPoseIters.begin(); p != tmpSelectedPoseIters.end(); ++p){
                    seq->changeTime(*p, (*p)->time() + diff);
                }
            }
            modified = true;
        }
    }

    return modified;
}


bool PoseSeqViewBase::modifyTransitionTimeOfSelectedPoses(double ttime)
{
    bool modified = false;

    if(!selectedPoseIters.empty()){
        for(auto it = selectedPoseIters.begin(); it != selectedPoseIters.end(); ++it){
            seq->beginPoseModification(*it);
            (*it)->setMaxTransitionTime(ttime);
            seq->endPoseModification(*it);
        }
        modified = true;
    }

    return modified;
}


void PoseSeqViewBase::popupContextMenu(QMouseEvent* event)
{
    popupMenu.popup(event->globalPos());
}


void PoseSeqViewBase::onSelectSpecifiedKeyPosesActivated()
{
    poseSelectionDialog->show();
}


void PoseSeqViewBase::onPoseSelectionDialogAccepted()
{
    if(!body || !seq){
        return;
    }

    selectedPoseIters.clear();
    const vector<int> selectedLinkIndices = linkTreeWidget->selectedLinkIndices();
    const double t0 = poseSelectionDialog->startTimeSpin.value();
    const double t1 = poseSelectionDialog->endTimeSpin.value();

    auto p = seq->seek(seq->begin(), t0);

    while(p != seq->end()){

        if(p->time() > t1){
            break;
        }

        if(poseSelectionDialog->selectedPartRadio.isChecked()){
            if(auto pose = p->get<Pose>()){
                bool match = false;
                for(size_t i=0; i < selectedLinkIndices.size(); ++i){
                    int linkIndex = selectedLinkIndices[i];
                    if(pose->isJointValid(body->link(linkIndex)->jointId()) || pose->ikLinkInfo(linkIndex)){
                        match = true;
                        break;
                    }
                }
                if(match){
                    selectedPoseIters.insert(p);
                }
            }
        } else {
            selectedPoseIters.insert(p);
        }
        
        ++p;
    }

    updateLinkTreeModel();
    onSelectedPosesModified();
}


void PoseSeqViewBase::onAdjustStepPositionsActivated()
{
    if(currentPoseSeqItem && currentBodyItem){
        PoseSeq::iterator origin;
        if(selectedPoseIters.size() == 1){
            origin = *selectedPoseIters.begin();
        } else {
            origin = seq->begin();
        }

        LeggedBodyHelperPtr legged = getLeggedBodyHelper(body);
        if(legged->isValid()){
            int n = legged->numFeet();
            vector<int> footLinkIndices(n);
            for(int i=0; i < n; ++i){
                footLinkIndices[i] = legged->footLink(i)->index();
            }
            adjustStepPositions(seq, footLinkIndices, origin);
            doAutomaticInterpolationUpdate();
        }
    }
}


void PoseSeqViewBase::onRotateYawOrientationsActivated()
{
    yawOrientationRotationDialog->show();
}


void PoseSeqViewBase::onYawOrientationRotationDialogAccepted()
{
    if(currentPoseSeqItem && selectedPoseIters.size() == 1){

        PoseSeq::iterator poseIter = *selectedPoseIters.begin();
        Vector3 center(yawOrientationRotationDialog->centerPosSpins[0].value(),
                       yawOrientationRotationDialog->centerPosSpins[1].value(),
                       0.0);
        double angle = radian(yawOrientationRotationDialog->angleSpin.value());
        rotateYawOrientations(seq, poseIter, center, angle);

        /*
          PosePtr pose = poseIter->get<Pose>();
          if(pose){
          const std::vector<int>& selectedLinkIndices =
          LinkSelectionView::instance()->getSelectedLinkIndices(currentBodyItem);

          if(selectedLinkIndices.size() == 1){
          Pose::LinkInfo* linkInfo = pose->ikLinkInfo(selectedLinkIndices.front());
          if(linkInfo){
          double angle = radian(yawOrientationRotationDialog->angleSpin.value());
          rotateYawOrientations(seq, ++poseIter, linkInfo->p, angle);
          }
          }
          }
        */
    }
}



void PoseSeqViewBase::onAdjustWaistPositionActivated()
{
    linkPositionAdjustmentDialog->show();
}


void PoseSeqViewBase::onLinkPositionAdjustmentDialogAccepted()
{
    if(currentPoseSeqItem && currentBodyItem && !selectedPoseIters.empty()){

        LeggedBodyHelperPtr legged = getLeggedBodyHelper(body);
        if(legged->isValid()){

            int waistLinkIndex = currentBodyItem->body()->rootLink()->index();
        
            int n = legged->numFeet();
            vector<int> footLinkIndices(n);
            for(int i=0; i < n; ++i){
                footLinkIndices[i] = legged->footLink(i)->index();
            }
        
            currentPoseSeqItem->beginEditing();
        
            for(auto it = selectedPoseIters.begin(); it != selectedPoseIters.end(); ++it){
                if(auto pose = (*it)->get<Pose>()){
                    seq->beginPoseModification(*it);
                    
                    /// \todo arbitrar selected link should be processed here
                    Pose::LinkInfo* waistInfo = pose->ikLinkInfo(waistLinkIndex);
                    if(waistInfo){
                        for(int i=0; i < 3; ++i){
                            if(linkPositionAdjustmentDialog->targetAxisCheck[i].isChecked()){
                                double p = linkPositionAdjustmentDialog->positionSpin[i].value();
                                if(linkPositionAdjustmentDialog->absoluteRadio.isChecked()){
                                    waistInfo->p[i] = p;
                                } else {
                                    waistInfo->p[i] += p;
                                }
                            }
                        }
                    }
                    seq->endPoseModification(*it);
                }
            }
            
            currentPoseSeqItem->endEditing();
            doAutomaticInterpolationUpdate();
        }
    }
}


void PoseSeqViewBase::onUpdateKeyposesWithBalancedTrajectoriesActivated()
{
    if(currentPoseSeqItem){
        ostringstream mout;
        if(currentPoseSeqItem->updateKeyPosesWithBalancedTrajectories(mout)){
            MessageView::mainInstance()->notify(
                _("Original key poses have been updated to be balanced ones."));
        } else {
            MessageView::mainInstance()->notify(
                _("Operation failed ! Key poses cannot be updated."));
        }
        if(!mout.str().empty()){
            os << mout.str() << endl;
        }
    }
}


void PoseSeqViewBase::onFlipPosesActivated()
{
    if(currentPoseSeqItem && currentBodyItem){
        MessageView::mainInstance()->notify(_("flipping all the poses against x-z plane ..."));
        flipPoses(seq, body);
        doAutomaticInterpolationUpdate();
    }
}


void PoseSeqViewBase::countSelectedKeyPoses()
{
    MessageView::mainInstance()->notify(
        format(_("The number of selected key poses is {}"), selectedPoseIters.size()));
}


void PoseSeqViewBase::onSelectedPosesModified()
{
    if(selectedPoseIters.empty()){
        updateButton.setEnabled(false);
        deleteButton.setEnabled(false);
    } else {
        updateButton.setEnabled(true);
        deleteButton.setEnabled(true);
    }
}


void PoseSeqViewBase::setCurrentItemName(Item* item)
{
    string name;
    if(item){
        name = item->displayName();
    }
    if(name.empty()){
        currentItemLabel.setText(textForEmptyName);
    } else {
        currentItemLabel.setText(name.c_str());
    }
}


/**
   \todo Show visual effect to emphasize the key poses beging updated
*/
void PoseSeqViewBase::onBodyKinematicStateUpdated()
{
    if(autoUpdateModeCheck.isChecked()){
        if(!selectedPoseIters.empty()){
            bool timeMatches = true;
            for(auto it = selectedPoseIters.begin(); it != selectedPoseIters.end(); ++it){
                double qtime = quantizedTime((*it)->time());
                if(qtime != timeBar->time()){
                    timeMatches = false;
                    break;
                }
            }
            if(timeMatches){
                setCurrentBodyStateToSelectedPoses(!updateAllToggle.isChecked());
                InfoBar::instance()->notify(_("Selected key poses have been updated."));
            }
        }
    }
}


PoseSeq::iterator PoseSeqViewBase::insertPose()
{
    PoseSeq::iterator poseIter = seq->end();
        
    PosePtr pose = new Pose(body->numJoints());

    bool hasValidPart = false;

    for(int i=0; i < body->numLinks(); ++i){
        auto link = body->link(i);
        if(auto item = linkTreeWidget->itemOfLink(link->index())){
            if(link->jointId() >= 0 && isChecked(item, validPartColumn)){
                pose->setJointPosition(link->jointId(), link->q());
                hasValidPart = true;
                if(isChecked(item, stationaryPointColumn)){
                    pose->setJointStationaryPoint(link->jointId());
                }
            }
            if(possibleIkLinkFlag[link->index()]){
                if(isChecked(item, validPartColumn) || isChecked(item, ikPartColumn)){
                    Pose::LinkInfo* info = pose->addIkLink(link->index());
                    info->setStationaryPoint(isChecked(item, stationaryPointColumn));
                    setCurrentLinkStateToIkLink(link, info);
                    if(isChecked(item, baseLinkColumn)){
                        pose->setBaseLink(link->index(), link->p(), link->R());
                    }
                    if(!isChecked(item, ikPartColumn)){
                        info->setSlave(true);
                    }
                    hasValidPart = true;
                }
            }
        }
    }

    if(isChecked(zmpRow, validPartColumn)){
        pose->setZmp(currentBodyItem->zmp());
        hasValidPart = true;
        if(isChecked(zmpRow, stationaryPointColumn)){
            pose->setZmpStationaryPoint();
        }
    }

    if(hasValidPart){
        poseIter = insertPoseUnit(pose);
    } else {
        showWarningDialog(_("Please check parts needed for making a desired pose."));
    }

    return poseIter;
}


PoseSeq::iterator PoseSeqViewBase::insertPronunSymbol()
{
    PronunSymbolPtr pronun(new PronunSymbol);
    return insertPoseUnit(pronun);
}


PoseSeq::iterator PoseSeqViewBase::insertPoseUnit(PoseUnit* poseUnit)
{
    auto it = seq->insert(currentPoseIter, currentTime / timeScale, poseUnit);
    it->setMaxTransitionTime(transitionTimeSpin.value() / timeScale);
    doAutomaticInterpolationUpdate();
    toggleSelection(it, false, false);

    currentPoseIter = it;

    return it;
}


double PoseSeqViewBase::quantizedTime(double time)
{
    double r = timeBar->frameRate();
    double frame = nearbyint(r * time);
    return frame / r;
}


void PoseSeqViewBase::onUpdateButtonClicked()
{
    setCurrentBodyStateToSelectedPoses(!updateAllToggle.isChecked());
}


void PoseSeqViewBase::setCurrentBodyStateToSelectedPoses(bool onlySelected)
{
    if(body){
        if(!selectedPoseIters.empty()){

            // quantize selected pose times
            PoseIterSet prevSelected(selectedPoseIters);
            selectedPoseIters.clear();
            for(auto it = prevSelected.begin(); it != prevSelected.end(); ++it){
                double qtime = quantizedTime((*it)->time());
                selectedPoseIters.insert(seq->changeTime(*it, qtime));
            }
            
            bool updated = false;
            currentPoseSeqItem->beginEditing();
            for(auto it = selectedPoseIters.begin(); it != selectedPoseIters.end(); ++it){
                if(auto pose = (*it)->get<Pose>()){
                    seq->beginPoseModification(*it);
                    if(setCurrentBodyStateToPose(pose, onlySelected)){
                        updated = true;
                        seq->endPoseModification(*it);
                    }
                }
            }
            currentPoseSeqItem->endEditing(updated);

            //! \todo This should be processed by PoseSeq slots
            if(updated){
                doAutomaticInterpolationUpdate();
            }
        }
    }
}


bool PoseSeqViewBase::setCurrentBodyStateToPose(Pose* pose, bool onlySelected)
{
    auto& linkSelection = BodySelectionManager::instance()->linkSelection(currentBodyItem);
            
    bool updated = false;
            
    int n = pose->numJoints();
    for(int i=0; i < n; ++i){
        if(pose->isJointValid(i)){
            auto joint = body->joint(i);
            if(!onlySelected || linkSelection[joint->index()]){
                const double q = body->joint(i)->q();
                if(q != pose->jointPosition(i)){
                    pose->setJointPosition(i, q);
                    updated = true;
                }
            }
        }
    }

    for(auto it = pose->ikLinkBegin(); it != pose->ikLinkEnd(); ++it){
        const int linkIndex = it->first;
        auto link = body->link(linkIndex);
        if(link && (!onlySelected || linkSelection[link->index()])){
            updated |= setCurrentLinkStateToIkLink(link, &it->second);
        }
    }

    if(pose->isZmpValid()){
        const Vector3& zmp = currentBodyItem->zmp();
        if(zmp != pose->zmp()){
            pose->setZmp(zmp);
            updated = true;
        }
    }

    return updated;
}


/**
   @return true if state is modified
*/
bool PoseSeqViewBase::setCurrentLinkStateToIkLink(Link* link, Pose::LinkInfo* linkInfo)
{
    bool updated = false;
    
    if(linkInfo->p != link->p()){
        linkInfo->p = link->p();
        updated = true;
    }
    if(linkInfo->R != link->R()){
        linkInfo->R = link->R();
        updated = true;
    }
    
    std::vector<Vector3> contactPoints;
    const std::vector<CollisionLinkPairPtr>& collisions = currentBodyItem->collisionsOfLink(link->index());
    for(size_t i=0; i < collisions.size(); ++i){
        if(!collisions[i]->isSelfCollision()){
            CollisionLinkPair& collisionPair = *collisions[i];
            for(auto it = collisionPair.collisions.begin(); it != collisionPair.collisions.end(); ++it){
                contactPoints.push_back(link->R().transpose()*(it->point - link->p()));
            }
        }
    }

    if(contactPoints.size() > 0){
        /**
           \todo set a parting direction correctly
           (now it is assumed that the touching only happens for the flat and level floor).
        */
        Vector3 partingDirection(0.0, 0.0, 1.0);
        if(!linkInfo->isTouching() || linkInfo->partingDirection() != partingDirection || linkInfo->contactPoints() != contactPoints){
            linkInfo->setTouching(partingDirection, contactPoints);
            updated = true;
        }
    } else {
        if(linkInfo->isTouching()){
            linkInfo->clearTouching();
            updated = true;
        }
    }

    return updated;
}


void PoseSeqViewBase::onDeleteButtonClicked()
{
    cutSelectedPoses();
}


void PoseSeqViewBase::onPoseInserted(PoseSeq::iterator it, bool isMoving)
{
    if(isSelectedPoseMoving && isMoving){
        selectedPoseIters.insert(it);
        isSelectedPoseMoving = false;
        onSelectedPosesModified();
    }
}

    
void PoseSeqViewBase::onPoseRemoving(PoseSeq::iterator it, bool isMoving)
{
    if(it == currentPoseIter){
        if(currentPoseIter != seq->begin()){
            --currentPoseIter;
        } else if(currentPoseIter != seq->end()){
            ++currentPoseIter;
        }
    }

    auto poseIter = findPoseIterInSelected(it);
    if(poseIter != selectedPoseIters.end()){
        selectedPoseIters.erase(poseIter);
        if(isMoving){
            isSelectedPoseMoving = true;
        } else {
            onSelectedPosesModified();
        }
    }
}


/**
   @todo update currentBodyItem and call notifyUpdate in this function ?
*/
void PoseSeqViewBase::onPoseModified(PoseSeq::iterator it)
{
    if(!selectedPoseIters.empty() && it == *selectedPoseIters.begin()){
        updateLinkTreeModel();
        onSelectedPosesModified();
    }
}


void PoseSeqViewBase::updateLinkTreeModel()
{
    PosePtr pose;

    for(auto it = selectedPoseIters.begin(); it != selectedPoseIters.end(); ++it){
        pose = (*it)->get<Pose>();
        if(pose){
            break;
        }
    } 
    if(!pose){
        pose = poseForDefaultStateSetting;
    }

    linkTreeAttributeChangeConnections.block(); // Probably this set of block / unblock is not needed

    int n = linkTreeWidget->topLevelItemCount();
    for(int i=0; i < n; ++i){
        if(auto item = dynamic_cast<LinkDeviceTreeItem*>(linkTreeWidget->topLevelItem(i))){
            updateLinkTreeModelSub(item, linkTreeWidget->bodyItem()->body(), pose);
        }
    }

    linkTreeAttributeChangeConnections.unblock();
}


PoseSeqViewBase::ChildrenState PoseSeqViewBase::updateLinkTreeModelSub
(LinkDeviceTreeItem* item, Body* body, Pose* pose)
{
    ChildrenState state;

    int n = item->childCount();
    for(int i=0; i < n; ++i){
        if(auto childItem = dynamic_cast<LinkDeviceTreeItem*>(item->child(i))){
            ChildrenState childrenState = updateLinkTreeModelSub(childItem, body, pose);
            state.validChildExists |= childrenState.validChildExists;
            state.allChildrenAreValid &= childrenState.allChildrenAreValid;
            state.childWithStationaryPointExists |= childrenState.childWithStationaryPointExists;
            state.allChildrenAreStationaryPoints &= childrenState.allChildrenAreStationaryPoints;
        }
    }

    if(item == zmpRow){
        if(pose->isZmpValid()){
            setChecked(item, validPartColumn, true);
            setChecked(item, stationaryPointColumn, pose->isZmpStationaryPoint());
        } else {
            setChecked(item, validPartColumn, false);
            setChecked(item, stationaryPointColumn, false);
        }
    } else {
        if(auto link = item->link()){
            bool isBaseLink = false;
            bool isValidPart = false;
            bool isStationaryPoint = false;
            bool isIkPart = false;
            
            const Pose::LinkInfo* linkInfo = pose->ikLinkInfo(link->index());
            if(linkInfo){
                isValidPart = true;
                if(!possibleIkLinkFlag[link->index()]){
                    /// \todo put warning here or do the following call ?
                    //pose->removeIkLink(link->index);
                } else if(!linkInfo->isSlave()){
                    isIkPart = true;
                    isBaseLink = linkInfo->isBaseLink();
                    if(linkInfo->isStationaryPoint()){
                        isStationaryPoint = true;
                    }
                }
            }
            
            int jointId = link->jointId();
            if(jointId >= 0){
                if(pose->isJointValid(jointId)){
                    isValidPart = true;
                    if(pose->isJointStationaryPoint(jointId)){
                        isStationaryPoint = true;
                    }
                }
            }

            if(isValidPart && !isStationaryPoint){
                state.allChildrenAreStationaryPoints = false;
            }
            if(!isValidPart){
                state.allChildrenAreValid = false;
            }

            setChecked(item, baseLinkColumn, isBaseLink);
            setChecked(item, validPartColumn, isValidPart);
            setChecked(item, stationaryPointColumn, isStationaryPoint);
            setChecked(item, ikPartColumn, isIkPart);
            state.validChildExists = isValidPart;
            state.childWithStationaryPointExists |= isStationaryPoint;

        } else {
            
            if(state.allChildrenAreValid){
                setChecked(item, validPartColumn, true);
            } else if(state.validChildExists){
                setCheckState(item, validPartColumn, Qt::PartiallyChecked);
            } else {
                setChecked(item, validPartColumn, false);
            }

            if(state.allChildrenAreStationaryPoints && state.childWithStationaryPointExists){
                setChecked(item, stationaryPointColumn, true);
            } else if(state.childWithStationaryPointExists){
                setCheckState(item, stationaryPointColumn, Qt::PartiallyChecked);
            } else {
                setChecked(item, stationaryPointColumn, false);
            }
        }
    }

    return state;
}


void PoseSeqViewBase::doAutomaticInterpolationUpdate()
{
    BodyMotionGenerationBar* generationBar = BodyMotionGenerationBar::instance();
    if(generationBar->isAutoInterpolationUpdateMode()){
        currentPoseSeqItem->updateInterpolation(); // not needed ?
        if(generationBar->isAutoGenerationMode()){
            currentPoseSeqItem->updateTrajectory();
        }
    }
}


bool PoseSeqViewBase::storeState(Archive& archive)
{
    archive.writeItemId("currentPoseSeqItem", currentPoseSeqItem);
    archive.write("defaultTransitionTime", transitionTimeSpin.value());
    archive.write("updateAll", updateAllToggle.isChecked());
    archive.write("autoUpdate", autoUpdateModeCheck.isChecked());
    archive.write("timeSync", timeSyncCheck.isChecked());

    linkPositionAdjustmentDialog->storeState(archive);
    
    return linkTreeWidget->storeState(archive);
}


bool PoseSeqViewBase::restoreState(const Archive& archive)
{
    transitionTimeSpin.setValue(archive.get("defaultTransitionTime", transitionTimeSpin.value()));
    updateAllToggle.setChecked(archive.get("updateAll", updateAllToggle.isChecked()));
    autoUpdateModeCheck.setChecked(archive.get("autoUpdate", autoUpdateModeCheck.isChecked()));
    timeSyncCheck.setChecked(archive.get("timeSync", timeSyncCheck.isChecked()));
    
    linkPositionAdjustmentDialog->restoreState(archive);
    
    archive.addPostProcess(
        [this, &archive](){ restoreCurrentPoseSeqItem(archive); });

    return true;
}


void PoseSeqViewBase::restoreCurrentPoseSeqItem(const Archive& archive)
{
    linkTreeWidget->restoreState(archive);
        
    if(auto item = archive.findItem<PoseSeqItem>("currentPoseSeqItem")){
        setCurrentPoseSeqItem(item);
    }
}
