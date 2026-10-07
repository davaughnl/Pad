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
#include "gui/aboutdialog.h"
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
        if(test=="about") {
            auto *dialog=new AboutDialog;capture(dialog,output,test);
            check(control<QLabel>(dialog,"versionLabel")->text()=="Development build","About display version is not Development build");
            auto info=control<QTextBrowser>(dialog,"infoTextBrowser")->toPlainText();
            check(info.contains("Program Version Development build")&&!info.contains("3.6.1"),"About info exposes inherited version");
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
