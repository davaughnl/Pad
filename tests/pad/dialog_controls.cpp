#include "antimicrosettings.h"
#include "antkeymapper.h"
#include "eventhandlerfactory.h"
#include "eventhandlers/baseeventhandler.h"
#include "logger.h"
#include "joystick.h"
#include "joyaxis.h"
#include "joycontrolstick.h"
#include "joydpad.h"
#include "joybuttontypes/joyaxisbutton.h"
#include "joybuttontypes/joycontrolstickbutton.h"
#include "joybuttontypes/joydpadbutton.h"
#include "gui/buttoneditdialog.h"
#include "gui/advancebuttondialog.h"
#include "gui/axiseditdialog.h"
#include "gui/dpadeditdialog.h"
#include "gui/joycontrolstickeditdialog.h"
#include "gui/mainsettingsdialog.h"
#include "gui/aboutdialog.h"
#include "gui/calibration.h"
#include "gui/extraprofilesettingsdialog.h"
#include "gui/setnamesdialog.h"
#include "gui/qkeydisplaydialog.h"
#include "gui/addeditautoprofiledialog.h"
#include "gui/editalldefaultautoprofiledialog.h"
#include "gui/advancestickassignmentdialog.h"
#include "gui/setaxisthrottledialog.h"
#include "gui/quicksetdialog.h"
#include "gui/gamecontrollermappingdialog.h"
#include "gui/joytabwidget.h"
#include "uihelpers/buttoneditdialoghelper.h"
#include <QPlainTextEdit>
#include "autoprofileinfo.h"
#include "pad/padshell.h"
#include <QSignalSpy>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include "mousedialog/mousebuttonsettingsdialog.h"
#include "mousedialog/mouseaxissettingsdialog.h"
#include "mousedialog/mousedpadsettingsdialog.h"
#include "mousedialog/mousecontrolsticksettingsdialog.h"
#include "keyboard/virtualkeyboardmousewidget.h"
#include "keyboard/virtualkeypushbutton.h"
#include "keyboard/virtualmousepushbutton.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QSlider>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QTableWidget>
#include <QListWidget>
#include <QTabWidget>
#include <QMessageBox>
#include <QLabel>
#include <QTextBrowser>
#include <QTimer>
#include <QTemporaryDir>
#include <QThread>
#include <QtTest/QTest>
#include <cstdio>
#include <cmath>
#include <limits>
#include <cstring>
#include <new>
#include "sensors/joyaccelerometersensor.h"
#include <QMenu>
#include <QAction>
#include "gui/joysensoreditdialog.h"
#include "gui/mainwindow.h"
#include "mousedialog/mousesensorsettingsdialog.h"
#include "sensors/joysensor.h"
#include "joybuttontypes/joysensorbutton.h"
#include "xml/inputdevicexml.h"
#include "xmlconfigwriter.h"
#include <functional>
#include <stdexcept>
static void check(bool ok, const char *msg) { if (!ok) throw std::runtime_error(msg); }
template<class T> T *widget(QObject *d, const char *name) { auto *w=d->findChild<T*>(name); if (!w) throw std::runtime_error(std::string("Missing control: ")+name); return w; }
static void settle() { QTest::qWait(35); }
static void show(QDialog *d) { d->setAttribute(Qt::WA_DeleteOnClose,false); d->show(); settle(); }
static void click(QAbstractButton *b) { check(b->isEnabled(),"control unexpectedly disabled"); b->click(); settle(); }
static void closeButton(QDialog *d, QDialogButtonBox::StandardButton id) { auto *b=widget<QDialogButtonBox>(d,"buttonBox")->button(id); check(b,"close button missing"); click(b); check(!d->isVisible(),"close button did not close dialog"); }
static void text(QObject *d,const char *name,const QString &s) { auto *w=widget<QLineEdit>(d,name); w->setFocus(); w->selectAll(); QTest::keyClicks(w,s); settle(); }
static void spin(QObject *d,const char *name,int value) { widget<QSpinBox>(d,name)->setValue(value); settle(); }
static void toggle(QObject *d,const char *name) { click(widget<QCheckBox>(d,name)); }
static void answer(QMessageBox::StandardButton button) { QTimer::singleShot(20,[button] { auto *box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget()); if (box) box->done(button); }); }
static void fileChoice(const QString &path,bool accept=true) {
    auto *timer=new QTimer(qApp); timer->setInterval(30);
    QObject::connect(timer,&QTimer::timeout,[timer,path,accept] {
        QWidget *active=QApplication::activeModalWidget();
        if(!active) for(auto *w:QApplication::topLevelWidgets()) if(w->isVisible()&&(qobject_cast<QMessageBox*>(w)||qobject_cast<QFileDialog*>(w))) {active=w;break;}
        if(auto *box=qobject_cast<QMessageBox*>(active)) {
            if(box->standardButtons().testFlag(QMessageBox::Discard)) box->done(QMessageBox::Discard);
        } else if(auto *d=qobject_cast<QFileDialog*>(active)) {
            timer->stop(); timer->deleteLater();
            if(accept) {d->setDirectory(QFileInfo(path).absolutePath()); d->selectFile(QFileInfo(path).fileName()); QTimer::singleShot(150,d,[d,path]{if(auto *edit=d->findChild<QLineEdit*>("fileNameEdit")) edit->setText(path); QMetaObject::invokeMethod(d,"accept");});} else d->reject();
        }
    }); timer->start();
}
static void mouseTest(QDialog *d,const QList<JoyButton*> &buttons) {
    show(d); spin(d,"horizontalSpinBox",47); spin(d,"verticalSpinBox",53);
    for(auto *b:buttons) check(b->getMouseSpeedX()==47&&b->getMouseSpeedY()==53,"mouse speed did not reach model");
    widget<QComboBox>(d,"mouseModeComboBox")->setCurrentIndex(2); settle();
    for(auto *b:buttons) check(b->getMouseMode()==JoyButton::MouseSpring,"spring mode did not reach model");
    spin(d,"wheelHoriSpeedSpinBox",22); spin(d,"wheelVertSpeedSpinBox",24);
    for(auto *b:buttons) check(b->getWheelSpeedX()==22&&b->getWheelSpeedY()==24,"wheel speed failed");
    spin(d,"springWidthSpinBox",321); spin(d,"springHeightSpinBox",245);
    for(auto *b:buttons) check(b->getSpringWidth()==321&&b->getSpringHeight()==245,"spring dimensions did not reach model");
    widget<QComboBox>(d,"mouseModeComboBox")->setCurrentIndex(1); settle();
    for(auto *b:buttons) check(b->getMouseMode()==JoyButton::MouseCursor,"cursor mode did not restore");
    closeButton(d,QDialogButtonBox::Close); delete d;
}
#include "extended_controls.inc"
#include "keyboard_reachability.h"
int main(int argc,char **argv) {
    QApplication app(argc,argv); PadUi::initializeApplicationStyle(); Logger::createInstance(nullptr,Logger::LOG_NONE);
    QThread worker;
    try {
        check(argc==2,"pass test case name"); QString test=argv[1]; QTemporaryDir tmp;
        check(SDL_Init(SDL_INIT_JOYSTICK)==0,"SDL init failed");
        int index=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_UNKNOWN,2,4,1); check(index>=0,"virtual attach failed");
        AntKeyMapper::getInstance("xtest"); check(EventHandlerFactory::getInstance("xtest")->handler()->init(),"XTest init failed");
        auto *settings=new AntiMicroSettings(tmp.filePath("settings.ini"),QSettings::IniFormat);
        auto *j=new SensorJoystick(SDL_JoystickOpen(index),index,settings,nullptr);
        if(test.startsWith("sensor-")||test=="calibration-gyro"||test=="calibration-accel") for(auto *current:j->getJoystick_sets())current->refreshSensors();
        auto *set=j->getSetJoystick(0); auto *button=set->getJoyButton(0); auto *axis=set->getJoyAxis(0); auto *dpad=set->getJoyDPad(0);
        // Production stick associations exist in every set, not only the active set.
        for(auto *current:j->getJoystick_sets()) current->addControlStick(0,new JoyControlStick(current->getJoyAxis(0),current->getJoyAxis(1),0,current->getIndex(),current));
        auto *stick=set->getJoyStick(0);
        // Match production: models/helper objects on input thread, UI on GUI thread.
        j->moveToThread(&worker); worker.start();
        if(extended(test,j,settings,tmp)) {}
        else if(test=="slot-owner-thread") {
            // Queue a mutation behind a delayed owner-thread operation. GUI
            // reads must wait for it, never traverse slots being replaced.
            for(int i=0;i<100;++i) {
                const int alias=i%2?Qt::Key_B:Qt::Key_A;
                const int code=AntKeyMapper::getInstance()->returnVirtualKey(alias);
                QMetaObject::invokeMethod(button,[button,code,alias]{
                    QThread::msleep(5);
                    button->clearSlotsEventReset(false);
                    button->setAssignedSlot(code,alias,JoyButtonSlot::JoyKeyboard);
                },Qt::QueuedConnection);
                QString slotText=button->getSlotsString();
                check(button->getSlotsSummary().contains(i%2?"B":"A",Qt::CaseInsensitive),"Slots summary read skipped owner mutation");
                check(slotText.contains(i%2?"B":"A",Qt::CaseInsensitive),"Slot text read skipped queued owner mutation");
                check(button->getActiveZoneSummary().contains(i%2?"B":"A",Qt::CaseInsensitive),"Active summary read skipped owner mutation");
            }
            auto *direction=stick->getDirectionButton(JoyControlStick::StickUp);
            QMetaObject::invokeMethod(direction,[direction]{QThread::msleep(10);direction->setAssignedSlot(AntKeyMapper::getInstance()->returnVirtualKey(Qt::Key_C),Qt::Key_C,JoyButtonSlot::JoyKeyboard);},Qt::QueuedConnection);
            check(direction->getActiveZoneSummary().contains("C",Qt::CaseInsensitive),"Stick override skipped owner mutation");
            QMetaObject::invokeMethod(button,[button]{button->clearSlotsEventReset(false);},Qt::BlockingQueuedConnection);
            QThread stopped;
            JoyButton dormant(0,0,set,nullptr);
            dormant.moveToThread(&stopped);
            check(dormant.getSlotsString()=="[NO KEY]"&&dormant.getSlotsSummary()=="[NO KEY]"&&dormant.getActiveZoneSummary()=="[NO KEY]","Stopped owner must not block or traverse slots");
        } else if(test=="button") {
            auto *d=new ButtonEditDialog(button,j,false); show(d);
            toggle(d,"toggleCheckBox"); check(button->getToggleState(),"toggle on not applied");
            toggle(d,"toggleCheckBox"); check(!button->getToggleState(),"toggle off not applied");
            toggle(d,"turboCheckBox"); check(button->isUsingTurbo(),"turbo on not applied");
            toggle(d,"turboCheckBox"); check(!button->isUsingTurbo(),"turbo off not applied");
            text(d,"buttonNameLineEdit","QA Button"); check(button->getButtonName()=="QA Button","button name not applied");
            text(d,"actionNameLineEdit","QA Action"); check(button->getActionName()=="QA Action","action name not applied");
            toggle(d,"attachNumKeypadCheckbox"); check(settings->value("AttachNumKeypad").toString()=="1","keypad preference not applied");
            toggle(d,"attachNumKeypadCheckbox"); check(settings->value("AttachNumKeypad").toString()=="0","keypad preference not reverted");
            closeButton(d,QDialogButtonBox::Close); delete d;
        } else if(test=="keyboard") {
            auto *d=new ButtonEditDialog(button,j,false); show(d);
            auto *kb=d->findChild<VirtualKeyboardMouseWidget*>(); check(kb,"keyboard widget missing");
            verifyKeyboardReachability(d,button);kb=d->findChild<VirtualKeyboardMouseWidget*>("padVirtualInputs");
            VirtualKeyPushButton *key=nullptr;
            for(auto *k:kb->findChildren<VirtualKeyPushButton*>()) if(k->getQkeyalias()==Qt::Key_A) {key=k;break;}
            check(key,"A virtual key missing"); click(key);
            check(button->getAssignedSlots()->size()==1,"virtual key did not assign");
            check(button->getAssignedSlots()->first()->getSlotCodeAlias()==Qt::Key_A,"wrong virtual key assigned");
            click(kb->getNoneButton()); check(button->getAssignedSlots()->isEmpty(),"None did not clear mapping");
            VirtualMousePushButton *mouse=nullptr;
            for(auto *m:kb->getMouseTab()->findChildren<VirtualMousePushButton*>()) if(m->getMouseCode()==1&&m->getMouseMode()==JoyButtonSlot::JoyMouseButton) {mouse=m;break;}
            check(mouse,"virtual left mouse missing"); click(mouse);
            check(button->getAssignedSlots()->size()==1&&button->getAssignedSlots()->first()->getSlotMode()==JoyButtonSlot::JoyMouseButton,"virtual mouse assignment failed");
            click(kb->getNoneButton()); check(button->getAssignedSlots()->isEmpty(),"None mouse clear failed");
            d->close(); delete d;
        } else if(test=="advanced") {
            QMetaObject::invokeMethod(button,[button]{button->setAssignedSlot(97,Qt::Key_A,JoyButtonSlot::JoyKeyboard);},Qt::BlockingQueuedConnection);
            auto *d=new AdvanceButtonDialog(button); show(d);
            toggle(d,"toggleCheckbox"); check(button->getToggleState(),"advanced toggle failed");
            toggle(d,"turboCheckbox"); check(button->isUsingTurbo(),"advanced turbo failed");
            widget<QSlider>(d,"turboSlider")->setValue(35); settle(); check(button->getTurboInterval()==350,"turbo interval failed");
            toggle(d,"autoResetCycleCheckBox"); check(button->isCycleResetActive(),"cycle reset toggle failed");
            widget<QDoubleSpinBox>(d,"resetCycleDoubleSpinBox")->setValue(2.5); settle(); check(button->getCycleResetTime()==2500,"cycle interval failed");
            click(widget<QPushButton>(d,"clearAllPushButton")); check(button->getAssignedSlots()->isEmpty(),"advanced clear failed");
            d->placeNewSlot(new JoyButtonSlot(98,Qt::Key_B,JoyButtonSlot::JoyKeyboard)); settle(); check(button->getAssignedSlots()->size()==1,"advanced add failed");
            auto *list=widget<QListWidget>(d,"slotListWidget"); list->setCurrentRow(0); list->item(0)->setSelected(true);
            answer(QMessageBox::Yes); click(widget<QPushButton>(d,"deleteSlotButton")); check(button->getAssignedSlots()->isEmpty(),"advanced delete failed");
            closeButton(d,QDialogButtonBox::Close); delete d;
        } else if(test=="join-split") {
            auto *d=new AdvanceButtonDialog(button); show(d);
            d->placeNewSlot(new JoyButtonSlot(97,Qt::Key_A,JoyButtonSlot::JoyKeyboard)); settle();
            d->placeNewSlot(new JoyButtonSlot(98,Qt::Key_B,JoyButtonSlot::JoyKeyboard)); settle();
            auto *list=widget<QListWidget>(d,"slotListWidget"); list->setCurrentRow(0); list->item(0)->setSelected(true); list->item(1)->setSelected(true);
            click(widget<QPushButton>(d,"joinSlotButton")); check(button->getAssignedSlots()->size()==1&&button->getAssignedSlots()->first()->getSlotMode()==JoyButtonSlot::JoyMix,"join did not create mix");
            list->clearSelection(); list->setCurrentRow(0); list->item(0)->setSelected(true);
            click(widget<QPushButton>(d,"splitSlotButton")); check(button->getAssignedSlots()->size()==2,"split did not restore two keys");
            check(button->getAssignedSlots()->at(0)->getSlotCodeAlias()==Qt::Key_A&&button->getAssignedSlots()->at(1)->getSlotCodeAlias()==Qt::Key_B,"split changed key order");
            closeButton(d,QDialogButtonBox::Close); delete d;
        } else if(test=="axis") {
            auto *d=new AxisEditDialog(axis,false); show(d); spin(d,"deadZoneSpinBox",9000);
            check(axis->getDeadZone()==9000&&widget<QSlider>(d,"deadZoneSlider")->value()==9000,"axis dead-zone sync failed");
            widget<QSlider>(d,"deadZoneSlider")->setValue(10000); settle(); check(axis->getDeadZone()==10000&&widget<QSpinBox>(d,"deadZoneSpinBox")->value()==10000,"axis reverse dead-zone sync failed");
            spin(d,"maxZoneSpinBox",30000); check(axis->getMaxZoneValue()==30000,"axis max zone failed");
            auto *combo=widget<QComboBox>(d,"presetsComboBox"); combo->setCurrentIndex(1); settle(); check(!axis->getPAxisButton()->getAssignedSlots()->isEmpty(),"axis preset didn't assign");
            combo->setCurrentIndex(combo->count()-1); settle(); check(axis->getPAxisButton()->getAssignedSlots()->isEmpty(),"axis None didn't clear");
            closeButton(d,QDialogButtonBox::Close); delete d;
        } else if(test=="dpad") {
            auto *d=new DPadEditDialog(dpad); show(d); auto *combo=widget<QComboBox>(d,"presetsComboBox"); combo->setCurrentIndex(1); settle(); check(dpad->getJoyButton(1)->getAssignedSlots()->size()==1,"dpad preset failed");
            combo->setCurrentIndex(combo->count()-1); settle(); check(dpad->getJoyButton(1)->getAssignedSlots()->isEmpty(),"dpad None failed");
            widget<QSlider>(d,"dpadDelaySlider")->setValue(12); settle(); check(dpad->getDPadDelay()==120,"dpad delay failed");
            widget<QComboBox>(d,"joyModeComboBox")->setCurrentIndex(1); settle(); check(int(dpad->getJoyMode())==1,"dpad mode failed");
            closeButton(d,QDialogButtonBox::Close); delete d;
        } else if(test=="stick") {
            auto *d=new JoyControlStickEditDialog(stick,false); show(d); spin(d,"deadZoneSpinBox",9500); check(stick->getDeadZone()==9500,"stick dead zone failed");
            spin(d,"diagonalRangeSpinBox",55); check(stick->getDiagonalRange()==55,"stick diagonal failed");
            spin(d,"maxZoneSpinBox",30500); check(stick->getMaxZone()==30500,"stick max zone failed");
            spin(d,"modifierZoneSpinBox",19000); check(stick->getModifierZone()==19000,"stick modifier zone failed");
            toggle(d,"modifierZoneInvertedCheckBox"); check(stick->getModifierZoneInverted(),"stick invert failed");
            auto *combo=widget<QComboBox>(d,"presetsComboBox"); combo->setCurrentIndex(1); settle(); check(stick->getButtons()->value(JoyControlStick::StickUp)->getAssignedSlots()->size()==1,"stick preset failed");
            combo->setCurrentIndex(combo->count()-1); settle(); check(stick->getButtons()->value(JoyControlStick::StickUp)->getAssignedSlots()->isEmpty(),"stick None failed");
            closeButton(d,QDialogButtonBox::Close); delete d;
        } else if(test=="mouse-button") mouseTest(new MouseButtonSettingsDialog(button),{button});
        else if(test=="mouse-axis") mouseTest(new MouseAxisSettingsDialog(axis),{axis->getNAxisButton(),axis->getPAxisButton()});
        else if(test=="mouse-dpad") { QList<JoyButton*> bs; for(auto *b:*dpad->getButtons()) bs.append(b); mouseTest(new MouseDPadSettingsDialog(dpad),bs); }
        else if(test=="mouse-stick") { QList<JoyButton*> bs; for(auto *b:*stick->getButtons()) bs.append(b); mouseTest(new MouseControlStickSettingsDialog(stick),bs); }
        else if(test=="profile") {
            auto *d=new ExtraProfileSettingsDialog(j); show(d); text(d,"profileNameLineEdit","QA Profile"); check(j->getProfileName()=="QA Profile","profile name failed");
            widget<QSlider>(d,"keyPressHorizontalSlider")->setValue(17); settle(); check(j->getDeviceKeyPressTime()==170,"profile keypress duration failed"); closeButton(d,QDialogButtonBox::Close); delete d;
            auto *names=new SetNamesDialog(j); show(names); widget<QTableWidget>(names,"setNamesTableWidget")->item(0,0)->setText("QA Set"); closeButton(names,QDialogButtonBox::Cancel); check(set->getName()!="QA Set","set cancel committed"); delete names;
            names=new SetNamesDialog(j); show(names); widget<QTableWidget>(names,"setNamesTableWidget")->item(0,0)->setText("QA Set"); closeButton(names,QDialogButtonBox::Ok); check(set->getName()=="QA Set","set save failed"); delete names;
        } else if(test=="settings") {
            settings->setValue("NumberRecentProfiles",5);
            auto *d=new MainSettingsDialog(settings,new QList<InputDevice*>{j}); show(d); check(!d->findChild<QLabel*>("label_8"),"Language narrative remains"); check(!d->findChild<QLabel*>("label_translation_help"),"Language help paragraph remains"); spin(d,"numberRecentProfileSpinBox",8); closeButton(d,QDialogButtonBox::Cancel); check(settings->value("NumberRecentProfiles").toInt()==5,"settings cancel committed"); delete d;
            d=new MainSettingsDialog(settings,new QList<InputDevice*>{j}); show(d);
            auto *table=widget<QTableWidget>(d,"controllerMappingsTableWidget"); int rows=table->rowCount();
            click(widget<QPushButton>(d,"mappngInsertPushButton")); check(table->rowCount()==rows+1,"mapping row add failed");
            table->setCurrentCell(rows,0); click(widget<QPushButton>(d,"mappingDeletePushButton")); check(table->rowCount()==rows,"mapping row remove failed");
            toggle(d,"keyRepeatEnableCheckBox"); check(widget<QSpinBox>(d,"keyDelaySpinBox")->isEnabled(),"key repeat dependent controls not enabled");
            toggle(d,"keyRepeatEnableCheckBox"); check(!widget<QSpinBox>(d,"keyDelaySpinBox")->isEnabled(),"key repeat dependent controls not disabled");
            check(!d->findChild<QLabel*>("label_8"),"Language narrative remains"); check(!d->findChild<QLabel*>("label_translation_help"),"Language help paragraph remains"); spin(d,"numberRecentProfileSpinBox",8); toggle(d,"attachNumKeypadCheckbox"); closeButton(d,QDialogButtonBox::Ok); check(settings->value("NumberRecentProfiles").toInt()==8,"settings save failed"); check(settings->value("AttachNumKeypad").toString()=="1","settings keypad save failed"); delete d;
        } else if(test=="calibration") {
            auto *d=new Calibration(j); show(d); check(widget<QComboBox>(d,"deviceComboBox")->count()==1,"calibration stick missing");
            check(!widget<QPushButton>(d,"saveBtn")->isEnabled(),"unsampled calibration Save enabled");
            click(widget<QPushButton>(d,"startBtn")); check(!widget<QPushButton>(d,"startBtn")->isEnabled()&&!widget<QComboBox>(d,"deviceComboBox")->isEnabled(),"calibration didn't start sampling");
            click(widget<QPushButton>(d,"cancelBtn")); check(!d->isVisible(),"calibration cancel failed"); delete d;
        } else if(test=="autoprofile") {
            QString path=tmp.filePath("fixture.amgp"); QFile file(path); check(file.open(QFile::WriteOnly),"temp file open failed"); file.write("<joystick/>"); file.close();
            AutoProfileInfo info(nullptr); QList<InputDevice*> devices{j}; QList<QString> reserved;
            auto *d=new AddEditAutoProfileDialog(&info,settings,&devices,reserved); show(d);
            text(d,"profileLineEdit",path); text(d,"winClassLineEdit","QA Class"); text(d,"winNameLineEdit","QA Window"); toggle(d,"setPartialCheckBox");
            closeButton(d,QDialogButtonBox::Cancel); check(info.getProfileLocation().isEmpty(),"auto profile Cancel committed"); delete d;
            d=new AddEditAutoProfileDialog(&info,settings,&devices,reserved); show(d);
            text(d,"profileLineEdit",path); text(d,"winClassLineEdit","QA Class"); text(d,"winNameLineEdit","QA Window"); toggle(d,"setPartialCheckBox");
            closeButton(d,QDialogButtonBox::Ok); check(info.getProfileLocation()==path&&info.getWindowClass()=="QA Class"&&info.getWindowName()=="QA Window"&&info.isPartialState(),"auto profile Save failed"); delete d;
            auto *all=new EditAllDefaultAutoProfileDialog(&info,settings); show(all); text(all,"profileLineEdit",tmp.filePath("missing.amgp")); answer(QMessageBox::Close); click(widget<QDialogButtonBox>(all,"buttonBox")->button(QDialogButtonBox::Ok)); check(all->isVisible(),"invalid default profile accepted");
            text(all,"profileLineEdit",path); closeButton(all,QDialogButtonBox::Ok); check(info.getUniqueID()=="all"&&info.getProfileLocation()==path&&info.isActive(),"default profile Save failed"); delete all;
        } else if(test=="throttle") {
            auto *d=new SetAxisThrottleDialog(axis); show(d); QSignalSpy spy(d,&SetAxisThrottleDialog::initiateSetAxisThrottleChange);
            closeButton(d,QDialogButtonBox::No); check(spy.count()==0,"throttle Cancel emitted change"); delete d;
            d=new SetAxisThrottleDialog(axis); show(d); QSignalSpy saved(d,&SetAxisThrottleDialog::initiateSetAxisThrottleChange);
            closeButton(d,QDialogButtonBox::Yes); check(saved.count()==1,"throttle Save didn't emit"); delete d;
        } else if(test=="key-display") {
            auto *d=new QKeyDisplayDialog; show(d); QKeyEvent e(QEvent::KeyRelease,Qt::Key_A,Qt::NoModifier,38,97,0,"a"); QApplication::sendEvent(d,&e); settle();
            check(widget<QLabel>(d,"qtKeyLabel")->text()=="0x41"&&widget<QLabel>(d,"nativeKeyLabel")->text()=="0x61","key display values wrong"); closeButton(d,QDialogButtonBox::Close); delete d;
        } else if(test=="stick-assignment") {
            auto *d=new AdvanceStickAssignmentDialog(j); show(d); toggle(d,"enableOneCheckBox"); check(!set->getJoyStick(0),"disable stick failed");
            toggle(d,"enableOneCheckBox"); widget<QComboBox>(d,"xAxisOneComboBox")->setCurrentIndex(1); widget<QComboBox>(d,"yAxisOneComboBox")->setCurrentIndex(2); settle();
            check(j->getActiveSetJoystick()->getJoyStick(0),"enable/configure stick failed");
            for(auto *current:j->getJoystick_sets()) {auto *configured=current->getJoyStick(0); check(configured&&configured->getAxisX()==current->getJoyAxis(0)&&configured->getAxisY()==current->getJoyAxis(1),"stick axes/all sets mismatch");} closeButton(d,QDialogButtonBox::Close); delete d;
        } else if(test=="quick-set") {
            ButtonEditDialogHelper helper(button); auto *d=new QuickSetDialog(j,&helper,"setAssignedSlot",97,Qt::Key_A,-1,JoyButtonSlot::JoyKeyboard,true,false); show(d);
            check(button->getIgnoreEventState(),"quick set didn't suppress mapped output");
            QMetaObject::invokeMethod(button,[button]{emit button->clicked(0);},Qt::BlockingQueuedConnection); settle();
            check(!d->isVisible(),"quick set didn't finish after controller button");
            check(button->getAssignedSlots()->size()==1&&button->getAssignedSlots()->first()->getSlotCodeAlias()==Qt::Key_A,"quick set didn't map chosen controller button");
            check(!button->getIgnoreEventState(),"quick set didn't restore mapped output"); delete d;
        } else if(test=="controller-mapping") {
            auto *d=new GameControllerMappingDialog(j,settings); show(d);
            auto *table=widget<QTableWidget>(d,"buttonMappingTableWidget"); table->setCurrentCell(0,0);
            QMetaObject::invokeMethod(j,[j]{emit j->rawButtonClick(0);},Qt::BlockingQueuedConnection); settle();
            check(table->item(0,0)&&table->item(0,0)->text()=="Button 1","controller button binding not shown");
            auto *mapping=widget<QPlainTextEdit>(d,"mappingStringPlainTextEdit"); check(mapping->toPlainText().contains("a:b0"),"generated SDL mapping missing a:b0");
            widget<QComboBox>(d,"axisDeadZoneComboBox")->setCurrentIndex(0); settle(); check(axis->getDeadZone()==5000,"mapping dead zone not applied");
            click(widget<QDialogButtonBox>(d,"buttonBox")->button(QDialogButtonBox::Save));
            check(settings->value("Mappings/"+j->getUniqueIDString()).toString().contains("a:b0"),"controller mapping Save not persisted"); delete d;
            d=new GameControllerMappingDialog(j,settings); show(d); answer(QMessageBox::Yes); click(widget<QDialogButtonBox>(d,"buttonBox")->button(QDialogButtonBox::Discard));
            check(!settings->contains("Mappings/"+j->getUniqueIDString()),"controller mapping Discard not removed"); delete d;
        } else if(test=="profile-operations") {
            settings->setValue("DefaultProfileDir",tmp.path());
            auto *tab=new JoyTabWidget(j,settings); tab->show(); settle();
            auto *combo=widget<QComboBox>(tab,"configBox");
            QString path=tmp.filePath("saved.joystick.amgp");
            QMetaObject::invokeMethod(button,[button]{button->setAssignedSlot(97,Qt::Key_A,JoyButtonSlot::JoyKeyboard);},Qt::BlockingQueuedConnection);
            fileChoice(path); click(widget<QPushButton>(tab,"saveAsButton"));
            check(QFile::exists(path)&&combo->currentIndex()>0,"Save As didn't create/select profile");
            QFile saved(path); check(saved.open(QFile::ReadOnly)&&saved.readAll().contains("0x41"),"Save As missing actual mapping"); saved.close();
            button=j->getActiveSetJoystick()->getJoyButton(0);
            QMetaObject::invokeMethod(button,[button]{button->clearSlotsEventReset();button->setAssignedSlot(98,Qt::Key_B,JoyButtonSlot::JoyKeyboard);},Qt::BlockingQueuedConnection);
            click(widget<QPushButton>(tab,"saveButton")); saved.open(QFile::ReadOnly); check(saved.readAll().contains("0x42"),"Save didn't update actual mapping"); saved.close();
            QString cancelPath=tmp.filePath("must-not-exist.amgp"); fileChoice(cancelPath,false); click(widget<QPushButton>(tab,"saveAsButton")); check(!QFile::exists(cancelPath),"Save As Cancel created file");
            click(widget<QPushButton>(tab,"removeButton")); check(combo->currentIndex()==0&&QFile::exists(path),"Remove did not remove recent entry or deleted file");
            fileChoice(path); click(widget<QPushButton>(tab,"loadButton")); check(combo->currentIndex()>0,"Load didn't select profile");
            auto *loaded=j->getActiveSetJoystick()->getJoyButton(0); check(loaded->getAssignedSlots()->size()==1&&loaded->getAssignedSlots()->first()->getSlotCodeAlias()==Qt::Key_B,"Load didn't restore saved mapping");
            delete tab;
        } else if(test=="about") {
            auto *d=new AboutDialog; show(d); check(widget<QLabel>(d,"versionLabel")->text()=="Development build","About exposes an unchosen release version"); check(!widget<QTextBrowser>(d,"infoTextBrowser")->toPlainText().contains("3.6.1"),"About exposes inherited upstream version"); auto *tabs=widget<QTabWidget>(d,"tabWidget"); check(widget<QTextBrowser>(d,"textBrowser_2")->toPlainText().contains("TERMS AND CONDITIONS"),"GPL resource absent from License tab"); check(tabs->count()==2,"About must contain only Info and License"); check(!d->findChild<QWidget*>("credits")&&!d->findChild<QWidget*>("aboutDev"),"Retired About narrative remains"); for(auto *browser:d->findChildren<QTextBrowser*>()){if(browser->objectName()=="textBrowser_2")continue; /* License tab: the owner-approved legal exception */ check(!browser->toPlainText().contains("antimicro",Qt::CaseInsensitive),"About contains old product branding");} for(int n=0;n<tabs->count();n++){tabs->setCurrentIndex(n);settle();check(tabs->currentIndex()==n,"about tab switch failed");} check(!widget<QTextBrowser>(d,"infoTextBrowser")->toPlainText().isEmpty(),"about info empty"); closeButton(d,QDialogButtonBox::Close); delete d;
        } else throw std::runtime_error("unknown test");
        fprintf(stdout,"PASS dialog %s\n",argv[1]); fflush(stdout);
        QMetaObject::invokeMethod(j,[j]{delete j;},Qt::BlockingQueuedConnection); worker.quit(); worker.wait(); delete settings;
    } catch(const std::exception &e) { fprintf(stderr,"FAIL dialog %s: %s\n",argc>1?argv[1]:"?",e.what()); worker.quit(); worker.wait(); return 1; }
}
