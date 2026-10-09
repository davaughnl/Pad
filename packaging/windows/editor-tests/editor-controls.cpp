// SPDX-License-Identifier: GPL-3.0-or-later
// Native fixture only. No entry points or hooks are added to shipped Pad.
#include "antimicrosettings.h"
#include "antkeymapper.h"
#include "eventhandlerfactory.h"
#include "eventhandlers/baseeventhandler.h"
#include "logger.h"
#include "joystick.h"
#include "joyaxis.h"
#include "joycontrolstick.h"
#include "joybuttontypes/joybutton.h"
#include "joybuttontypes/joycontrolstickbutton.h"
#include "gui/joystickstatuswindow.h"
#include <QProgressBar>
#include <QPointer>
#include <QScrollArea>
#include <QScrollBar>
#include "gui/aboutdialog.h"
#include "gui/mainwindow.h"
#include "gui/gamecontrollermappingdialog.h"
#include "commandlineutility.h"
#include "gamecontrollerexample.h"
#include <QTableWidget>
#include <QTextBrowser>
#include "gui/joysensoreditdialog.h"
#include "sensors/joysensor.h"
#include "sensors/joysensorpreset.h"
#include <QLabel>
#include <cmath>
#include "gui/buttoneditdialog.h"
#include "gui/advancebuttondialog.h"
#include "gui/joycontrolstickeditdialog.h"
#include "mousedialog/mousebuttonsettingsdialog.h"
#include "keyboard/virtualkeyboardmousewidget.h"
#include "keyboard/virtualkeypushbutton.h"
#include "keyboard/virtualmousepushbutton.h"
#include "pad/padshell.h"
#include "gui/joybuttonslot.h"
#include "gui/quicksetdialog.h"
#include "gui/setnamesdialog.h"
#include "gui/mainsettingsdialog.h"
#include "gui/calibration.h"
#include "gui/extraprofilesettingsdialog.h"
#include "gui/addeditautoprofiledialog.h"
#include "gui/editalldefaultautoprofiledialog.h"
#include "gui/dpadeditdialog.h"
#include "gui/axiseditdialog.h"
#include "gui/setaxisthrottledialog.h"
#include "gui/qkeydisplaydialog.h"
#include "gui/advancestickassignmentdialog.h"
#include "gui/winappprofiletimerdialog.h"
#include "autoprofileinfo.h"
#include <QListWidget>
#include <QMessageBox>
#include "pad/padupdatestrip.h"
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QPixmap>
#include <QApplication>
#include <QScreen>
#include <QDir>
#include <QFile>
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QSlider>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QTemporaryDir>
#include <QThread>
#include <QtTest/QTest>
#include <cstdio>
#include <stdexcept>
static void check(bool ok, const char *message) { if (!ok) throw std::runtime_error(message); }
template<class T> static T *control(QObject *dialog, const char *name) {
    auto *w=dialog->findChild<T*>(name); if (!w) throw std::runtime_error(std::string("Missing control: ")+name); return w;
}
static void settle() { QTest::qWait(100); }
static void click(QAbstractButton *button) { check(button->isEnabled(), "Control disabled"); button->click(); settle(); }
static void spin(QDialog *dialog,const char *name,int value) { control<QSpinBox>(dialog,name)->setValue(value); settle(); }
static void toggle(QDialog *dialog,const char *name) { click(control<QCheckBox>(dialog,name)); }
static void capture(QDialog *dialog,const QString &output,const QString &name) {
    dialog->setAttribute(Qt::WA_DeleteOnClose,false); dialog->show(); dialog->raise(); dialog->activateWindow();
    QTest::qWait(750);
    const auto screen=QApplication::primaryScreen();
    check(screen && screen->geometry().size()==QSize(1024,768), "Runner desktop must be 1024x768");
    check(screen->grabWindow(0).save(output+"/"+name+".png"), "Screenshot save failed");
    QFile geometry(output+"/"+name+"-geometry.txt"); check(geometry.open(QFile::WriteOnly), "Geometry evidence failed");
    geometry.write(QString("desktop=1024x768; dialog=%1x%2; position=%3,%4\n").arg(dialog->width()).arg(dialog->height()).arg(dialog->x()).arg(dialog->y()).toUtf8());
}
static void closeDialog(QDialog *dialog) {
    click(control<QDialogButtonBox>(dialog,"buttonBox")->button(QDialogButtonBox::Close));
    check(!dialog->isVisible(), "Close failed"); delete dialog;
}
// Test-only capability adapter. No claim of physical sensor hardware.
class SensorFixtureJoystick : public Joystick {
public:
    using Joystick::Joystick;
    bool hasRawSensor(JoySensorType) override { return true; }
    double getRawSensorRate(JoySensorType) override { return 200.0; }
};
#include "../../../tests/pad/keyboard_reachability.h"
int main(int argc,char **argv) {
    QApplication app(argc,argv); PadUi::initializeApplicationStyle(); Logger::createInstance(nullptr,Logger::LOG_NONE);
    QThread worker;
    try {
        check(argc==3,"Usage: pad-editor-tests CASE OUTPUT_DIRECTORY"); const QString test=argv[1], output=argv[2];
        check(QDir().mkpath(output),"Evidence directory failed"); QTemporaryDir temp;
        check(SDL_Init(SDL_INIT_JOYSTICK)==0,"SDL init failed");
        const int index=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_UNKNOWN,2,4,1); check(index>=0,"SDL virtual attach failed");
        AntKeyMapper::getInstance("sendinput"); check(EventHandlerFactory::getInstance("sendinput")->handler()->init(),"SendInput init failed");
        auto *settings=new AntiMicroSettings(temp.filePath("settings.ini"),QSettings::IniFormat);
        auto *joystick=new SensorFixtureJoystick(SDL_JoystickOpen(index),index,settings,nullptr);
        if(test.startsWith("sensor-")) for(auto *current:joystick->getJoystick_sets()) current->refreshSensors();
        auto *set=joystick->getSetJoystick(0); auto *button=set->getJoyButton(0);
        for(auto *current:joystick->getJoystick_sets())
            current->addControlStick(0,new JoyControlStick(current->getJoyAxis(0),current->getJoyAxis(1),0,current->getIndex(),current));
        auto *stick=set->getJoyStick(0);
        joystick->moveToThread(&worker); worker.start();
        if(test=="controller-art") {
            QMap<SDL_JoystickID,InputDevice*> devices;
            devices.insert(SDL_JoystickInstanceID(joystick->getJoyHandle()),joystick);
            CommandLineUtility command;
            {
                QMap<SDL_JoystickID,InputDevice*> none; CommandLineUtility emptyCommand;
                auto *empty=new MainWindow(&none,&emptyCommand,settings);
                empty->makeJoystickTabs();empty->fillButtons();
                empty->resize(1000,700);empty->show();empty->raise();empty->activateWindow();QTest::qWait(1000);
                check(empty->grab().save(output+"/main-empty-full.png"),"Empty state capture failed");
                empty->hide();
            }
            auto *window=new MainWindow(&devices,&command,settings);
            window->makeJoystickTabs();window->fillButtons();
            window->resize(1000,700);window->show();window->raise();window->activateWindow();QTest::qWait(1000);
            auto *overview=control<QWidget>(window,"padOverview");check(overview->isVisible(),"Connected controller overview hidden");
            check(window->grab().save(output+"/controller-overview-full.png"),"Overview full capture failed");
            auto *dialog=new GameControllerMappingDialog(joystick,settings,window);
            capture(dialog,output,"controller-mapping");
            auto *table=control<QTableWidget>(dialog,"buttonMappingTableWidget");
            auto *art=control<GameControllerExample>(dialog,"gameControllerDisplayWidget");
            art->setActiveButton(-1);settle();
            check(dialog->grab().save(output+"/controller-mapping-full.png"),"Mapping full capture failed");
            table->setCurrentCell(1,0);table->setCurrentCell(0,0);settle();
            check(table->currentRow()==0,"Mapping A row selection failed");
            check(dialog->grab().save(output+"/controller-mapping-highlight-a-full.png"),"Mapping highlighted A capture failed");
            table->setCurrentCell(3,0);settle();
            check(dialog->grab().save(output+"/controller-mapping-highlight-y-full.png"),"Mapping highlighted Y capture failed");
            table->setCurrentCell(18,0);settle();
            check(dialog->grab().save(output+"/controller-mapping-highlight-dpad-left-full.png"),"Mapping highlighted DPad Left capture failed");
            table->setCurrentCell(19,0);settle();
            check(dialog->grab().save(output+"/controller-mapping-highlight-dpad-down-full.png"),"Mapping highlighted DPad Down capture failed");
            for(int row:{17,20}){table->setCurrentCell(row,0);settle();check(dialog->grab().save(output+"/controller-mapping-xbox-row"+QString::number(row)+"-full.png"),"Xbox dpad capture failed");}
            art->setDevice("Wireless Controller");
            for(int row:{0,3,17,18,19,20,6}){table->setCurrentCell(row,0);settle();check(dialog->grab().save(output+"/controller-mapping-ps4-row"+QString::number(row)+"-full.png"),"PS4 capture failed");}
            closeDialog(dialog);delete window;
        } else if(test=="status") {
            // Richer virtual device matching the row-clipping regression shape.
            const int sindex=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_UNKNOWN,6,12,1);check(sindex>=0,"Status virtual attach failed");
            auto *statusjoy=new SensorFixtureJoystick(SDL_JoystickOpen(sindex),sindex,settings,nullptr);
            check(statusjoy->getNumberAxes()==6&&statusjoy->getNumberButtons()==12,"Status virtual device shape wrong");
            auto *dialog=new JoystickStatusWindow(statusjoy);capture(dialog,output,test);
            for(const char *name:{"axesScrollArea","buttonsScrollArea"}) {
                auto *scroll=control<QScrollArea>(dialog,name);
                check(scroll->verticalScrollBar()->maximum()==0,"Status rows still need scrolling");
                for(auto *child:scroll->widget()->findChildren<QWidget*>()) {
                    if(!child->isVisible()||child->height()<=0)continue;
                    QRect rect(child->mapTo(scroll->viewport(),QPoint()),child->size());
                    const QRect viewport=scroll->viewport()->rect();
                    std::fprintf(stderr,"Status geometry: area=%s class=%s object=%s child=%d,%d %dx%d viewport=%d,%d %dx%d contained=%d\n",name,child->metaObject()->className(),child->objectName().toUtf8().constData(),rect.x(),rect.y(),rect.width(),rect.height(),viewport.x(),viewport.y(),viewport.width(),viewport.height(),int(viewport.contains(rect)));
                    check(viewport.contains(rect),"Status row clipped");
                }
            }
            check(control<QScrollArea>(dialog,"axesScrollArea")->widget()->findChildren<QProgressBar*>().count()>=6,"Axis rows missing");
            int buttonRows=0;for(auto *w:control<QScrollArea>(dialog,"buttonsScrollArea")->widget()->findChildren<QWidget*>())if(w->isVisible()&&w->height()>=20)++buttonRows;check(buttonRows>=12,"Button rows missing");
            // Status reject() explicitly schedules deletion, independent of
            // WA_DeleteOnClose. Do not use closeDialog's post-click raw pointer.
            QPointer<JoystickStatusWindow> statusGuard(dialog);
            click(control<QDialogButtonBox>(dialog,"buttonBox")->button(QDialogButtonBox::Close));
            check(statusGuard.isNull(),"Status Close did not delete dialog");
            delete statusjoy;
        } else if(test=="about") {
            auto *dialog=new AboutDialog;capture(dialog,output,test);
            check(control<QLabel>(dialog,"versionLabel")->text()=="Development build","About display version is not Development build");
            auto info=control<QTextBrowser>(dialog,"infoTextBrowser")->toPlainText();
            check(info.contains("About Pad")&&info.contains("Open Source Licenses")&&!info.contains("3.6.1"),"About info copy missing or exposes inherited version");
            auto *tabs=control<QTabWidget>(dialog,"tabWidget");
            check(tabs->count()==2&&!dialog->findChild<QWidget*>("credits")&&!dialog->findChild<QWidget*>("aboutDev"),"Retired About tabs remain");
            for(auto *browser:dialog->findChildren<QTextBrowser*>()){if(browser->objectName()=="textBrowser_2"||browser->objectName()=="infoTextBrowser")continue; /* License tab and owner-written Info copy: owner-approved attribution */ check(!browser->toPlainText().contains("antimicro",Qt::CaseInsensitive),"About contains old product branding");}
            closeDialog(dialog);
        } else if(test=="sensor-accel" || test=="sensor-gyro") {
            auto *sensor=set->getSensor(test=="sensor-accel"?ACCELEROMETER:GYROSCOPE);check(sensor,"Sensor model absent");
            auto *dialog=new JoySensorEditDialog(sensor);capture(dialog,output,test);
            auto *presets=control<QComboBox>(dialog,"presetsComboBox");
            check(presets->currentData().toInt()==int(JoySensorPreset::PRESET_NONE),"Fresh sensor preset is not None");
            if(test=="sensor-accel") {
                check(std::isnan(sensor->calculatePitch(0,0,0))&&std::isnan(sensor->calculateRoll(0,0,0)),"Zero vector math not invalid");
                check(control<QLabel>(dialog,"pitchValue")->text()=="Unavailable"&&control<QLabel>(dialog,"rollValue")->text()=="Unavailable","Zero vector UI not Unavailable");
            }
            control<QDoubleSpinBox>(dialog,"deadZoneSpinBox")->setValue(12);settle();check(std::abs(sensor->getDeadZone()-12)<.01,"Sensor dead zone failed");
            for(int n=0;n<presets->count();++n) {
                presets->setCurrentIndex(n);settle();JoySensorPreset actual(sensor);
                check(int(actual.currentPreset())==presets->itemData(n).toInt(),"Sensor preset assignments mismatch");
                auto *reopened=new JoySensorEditDialog(sensor);reopened->setAttribute(Qt::WA_DeleteOnClose,false);reopened->show();settle();
                check(control<QComboBox>(reopened,"presetsComboBox")->currentData()==presets->itemData(n),"Sensor reopen preset mismatch");
                closeDialog(reopened);
            }
            presets->setCurrentIndex(presets->findData(int(JoySensorPreset::PRESET_NONE)));settle();check(!sensor->hasSlotsAssigned(),"Sensor None did not clear");
            closeDialog(dialog);
        } else if(test=="button" || test=="keyboard-mouse") {
            auto *dialog=new ButtonEditDialog(button,joystick,false); capture(dialog,output,"button");
            if(test=="button") {
                toggle(dialog,"toggleCheckBox"); check(button->getToggleState(),"Button toggle did not reach model");
                toggle(dialog,"toggleCheckBox"); check(!button->getToggleState(),"Button toggle did not clear");
                toggle(dialog,"turboCheckBox"); check(button->isUsingTurbo(),"Button turbo did not reach model");
                toggle(dialog,"turboCheckBox"); check(!button->isUsingTurbo(),"Button turbo did not clear");
                auto *name=control<QLineEdit>(dialog,"buttonNameLineEdit");name->setFocus();QTest::keyClicks(name,"Windows QA Button");settle();
                check(button->getButtonName()=="Windows QA Button","Button name did not reach model");
                closeDialog(dialog);
            } else {
                auto *keyboard=dialog->findChild<VirtualKeyboardMouseWidget*>();check(keyboard,"Keyboard absent");
                verifyKeyboardReachability(dialog,button,output);keyboard=dialog->findChild<VirtualKeyboardMouseWidget*>("padVirtualInputs");
                VirtualKeyPushButton *key=nullptr;
                for(auto *candidate:keyboard->findChildren<VirtualKeyPushButton*>()) if(candidate->getQkeyalias()==Qt::Key_A) {key=candidate;break;}
                check(key,"A key absent");click(key);
                check(button->getAssignedSlots()->size()==1,"Keyboard slot count wrong");
                check(button->getAssignedSlots()->first()->getSlotCodeAlias()==Qt::Key_A,"Keyboard alias wrong");
                check(button->getAssignedSlots()->first()->getSlotMode()==JoyButtonSlot::JoyKeyboard,"Keyboard mode wrong");
                click(keyboard->getNoneButton());check(button->getAssignedSlots()->isEmpty(),"None did not clear keyboard mapping");
                VirtualMousePushButton *mouse=nullptr;
                for(auto *candidate:keyboard->getMouseTab()->findChildren<VirtualMousePushButton*>())
                    if(candidate->getMouseCode()==1&&candidate->getMouseMode()==JoyButtonSlot::JoyMouseButton) {mouse=candidate;break;}
                check(mouse,"Mouse control absent");click(mouse);
                check(button->getAssignedSlots()->size()==1,"Mouse slot count wrong");
                check(button->getAssignedSlots()->first()->getSlotMode()==JoyButtonSlot::JoyMouseButton&&button->getAssignedSlots()->first()->getSlotCode()==1,"Mouse slot wrong");
                click(keyboard->getNoneButton());check(button->getAssignedSlots()->isEmpty(),"None did not clear mouse mapping");
                dialog->close();delete dialog;
            }
        } else if(test=="advanced") {
            QMetaObject::invokeMethod(button,[button]{button->setAssignedSlot(0x41,Qt::Key_A,JoyButtonSlot::JoyKeyboard);},Qt::BlockingQueuedConnection);
            auto *dialog=new AdvanceButtonDialog(button);capture(dialog,output,"advanced");
            toggle(dialog,"toggleCheckbox");check(button->getToggleState(),"Advanced toggle failed");
            toggle(dialog,"turboCheckbox");check(button->isUsingTurbo(),"Advanced turbo failed");
            control<QSlider>(dialog,"turboSlider")->setValue(35);settle();check(button->getTurboInterval()==350,"Turbo interval failed");
            toggle(dialog,"autoResetCycleCheckBox");check(button->isCycleResetActive(),"Cycle reset failed");
            control<QDoubleSpinBox>(dialog,"resetCycleDoubleSpinBox")->setValue(2.5);settle();check(button->getCycleResetTime()==2500,"Cycle timing failed");
            click(control<QPushButton>(dialog,"clearAllPushButton"));check(button->getAssignedSlots()->isEmpty(),"Advanced clear failed");
            dialog->placeNewSlot(new JoyButtonSlot(0x42,Qt::Key_B,JoyButtonSlot::JoyKeyboard));settle();
            check(button->getAssignedSlots()->size()==1&&button->getAssignedSlots()->first()->getSlotCodeAlias()==Qt::Key_B,"Advanced keyboard add failed");
            closeDialog(dialog);
        } else if(test=="stick") {
            auto *dialog=new JoyControlStickEditDialog(stick,false);capture(dialog,output,"stick");
            spin(dialog,"deadZoneSpinBox",9500);check(stick->getDeadZone()==9500,"Stick dead zone failed");
            spin(dialog,"diagonalRangeSpinBox",55);check(stick->getDiagonalRange()==55,"Stick diagonal range failed");
            spin(dialog,"maxZoneSpinBox",30500);check(stick->getMaxZone()==30500,"Stick max zone failed");
            spin(dialog,"modifierZoneSpinBox",19000);check(stick->getModifierZone()==19000,"Stick modifier zone failed");
            auto *presets=control<QComboBox>(dialog,"presetsComboBox");presets->setCurrentIndex(1);settle();
            auto *up=stick->getButtons()->value(JoyControlStick::StickUp);
            check(up->getAssignedSlots()->size()==1,"Stick preset failed to assign model slot");
            presets->setCurrentIndex(presets->count()-1);settle();check(up->getAssignedSlots()->isEmpty(),"Stick None did not clear model");
            closeDialog(dialog);
        } else if(test=="mouse") {
            auto *dialog=new MouseButtonSettingsDialog(button);capture(dialog,output,"mouse");
            spin(dialog,"horizontalSpinBox",47);spin(dialog,"verticalSpinBox",53);
            check(button->getMouseSpeedX()==47&&button->getMouseSpeedY()==53,"Mouse speeds did not reach model");
            control<QComboBox>(dialog,"mouseModeComboBox")->setCurrentIndex(2);settle();check(button->getMouseMode()==JoyButton::MouseSpring,"Spring mode failed");
            spin(dialog,"wheelHoriSpeedSpinBox",22);spin(dialog,"wheelVertSpeedSpinBox",24);
            check(button->getWheelSpeedX()==22&&button->getWheelSpeedY()==24,"Wheel speeds did not reach model");
            spin(dialog,"springWidthSpinBox",321);spin(dialog,"springHeightSpinBox",245);
            check(button->getSpringWidth()==321&&button->getSpringHeight()==245,"Spring geometry failed");
            control<QComboBox>(dialog,"mouseModeComboBox")->setCurrentIndex(1);settle();check(button->getMouseMode()==JoyButton::MouseCursor,"Cursor mode restore failed");
            closeDialog(dialog);
        } else if(test=="screens") {
            QMap<SDL_JoystickID,InputDevice*> devices;
            devices.insert(SDL_JoystickInstanceID(joystick->getJoyHandle()),joystick);
            QList<InputDevice*> list; list.append(joystick);
            auto shot=[&](QWidget *w,const char *name){std::fprintf(stderr,"SHOT %s\n",name);std::fflush(stderr);
                w->show();w->raise();w->activateWindow();QTest::qWait(600);
                check(w->grab().save(output+"/screen-"+name+".png"),"Screen capture failed");
            };
            auto *quick=new QuickSetDialog(joystick);shot(quick,"quickset");quick->hide();
            auto *names=new SetNamesDialog(joystick);shot(names,"setnames");names->hide();
            auto *settingsDlg=new MainSettingsDialog(settings,&list);shot(settingsDlg,"settings");
            if(auto *cats=settingsDlg->findChild<QListWidget*>("categoriesListWidget")) for(int i=1;i<cats->count();++i){cats->setCurrentRow(i);QTest::qWait(400);check(settingsDlg->grab().save(output+"/screen-settings-"+QString::number(i)+".png"),"tab capture");}
            settingsDlg->hide();
            std::fprintf(stderr,"MAKE cal\n");std::fflush(stderr);auto *cal=new Calibration(joystick);shot(cal,"calibration");cal->hide();
            auto *extra=new ExtraProfileSettingsDialog(joystick);shot(extra,"extraprofile");extra->hide();
            auto *axis=new AxisEditDialog(set->getJoyAxis(0),false);shot(axis,"axis");axis->hide();
            auto *thr=new SetAxisThrottleDialog(set->getJoyAxis(0));shot(thr,"axisthrottle");thr->hide();
            auto *kd=new QKeyDisplayDialog;shot(kd,"keydisplay");kd->hide();
            auto *adv=new AdvanceStickAssignmentDialog(joystick);shot(adv,"stickassign");adv->hide();
            auto *tmr=new WinAppProfileTimerDialog;shot(tmr,"apptimer");tmr->hide();
            AutoProfileInfo info("default","",true,false,nullptr);
            auto *eall=new EditAllDefaultAutoProfileDialog(&info,settings);shot(eall,"autoprofile-default");eall->hide();
            QList<QString> reserved;
            auto *ap=new AddEditAutoProfileDialog(&info,settings,&list,reserved,false);shot(ap,"autoprofile-add");ap->hide();
            auto *window=new MainWindow(&devices,new CommandLineUtility,settings);
            window->makeJoystickTabs();window->fillButtons();window->resize(1000,700);window->show();QTest::qWait(800);
            std::fprintf(stderr,"STEP menus\n");std::fflush(stderr);
            for(auto *menu:window->findChildren<QMenu*>()){
                if(menu->actions().isEmpty())continue;
                menu->popup(window->mapToGlobal(QPoint(300,120)));QTest::qWait(400);
                check(menu->grab().save(output+"/screen-menu-"+(menu->objectName().isEmpty()?QString::number(qintptr(menu)%9973):menu->objectName())+".png"),"menu capture");
                menu->hide();
            }
            window->show();
            window->hide();std::fprintf(stderr,"STEP msgbox\n");std::fflush(stderr);
            {
                QMessageBox box(QMessageBox::Warning,"Profile could not be loaded","The profile file is missing or damaged.",QMessageBox::Ok|QMessageBox::Cancel);
                box.setInformativeText("Choose another profile or create a new one.");
                shot(&box,"messagebox");
            }
            std::fprintf(stderr,"STEP multi\n");std::fflush(stderr);
            {
                const int i2=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_UNKNOWN,4,10,1);check(i2>=0,"second virtual attach failed");
                auto *joy2=new SensorFixtureJoystick(SDL_JoystickOpen(i2),i2,settings,nullptr);
                QMap<SDL_JoystickID,InputDevice*> two;two.insert(SDL_JoystickInstanceID(joystick->getJoyHandle()),joystick);two.insert(SDL_JoystickInstanceID(joy2->getJoyHandle()),joy2);
                CommandLineUtility c2;auto *multi=new MainWindow(&two,&c2,settings);
                multi->makeJoystickTabs();multi->fillButtons();multi->resize(1000,700);multi->show();QTest::qWait(1000);
                check(multi->grab().save(output+"/screen-main-two-controllers-full.png"),"multi capture");
                multi->hide();
            }
            window->show();
            std::fprintf(stderr,"STEP update\n");std::fflush(stderr);window->show();
            if(auto *strip=window->findChild<PadUpdateStrip*>()){
                using S=PadUpdateStrip::State;
                struct{S st;const char *n;const char *d;int pct;} states[]={{S::Checking,"checking","",0},{S::UpToDate,"uptodate","",0},{S::Available,"available","1.2.0",0},{S::Downloading,"downloading","",42},{S::Ready,"ready","1.2.0",0},{S::Error,"error","Could not reach the update server",0}};
                for(auto &st:states){std::fprintf(stderr,"STATE %s\n",st.n);std::fflush(stderr);strip->setState(st.st,st.d,st.pct);QTest::qWait(400);check(window->grab().save(output+QString("/screen-update-")+st.n+"-full.png"),"update capture");}
                strip->setState(S::Hidden);
            }
        } else throw std::runtime_error("Unknown case");
        QMetaObject::invokeMethod(joystick,[joystick]{delete joystick;},Qt::BlockingQueuedConnection);
        worker.quit();worker.wait();delete settings;SDL_JoystickDetachVirtual(index);SDL_Quit();
        std::printf("PASS native Windows editor/model %s; SDL virtual joystick; no physical input assertion\n",argv[1]);std::fflush(stdout);
        return 0;
    } catch(const std::exception &error) {
        std::fprintf(stderr,"FAIL native Windows editor/model: %s\n",error.what());std::fflush(stderr);
        worker.quit();worker.wait();return 1;
    }
}
