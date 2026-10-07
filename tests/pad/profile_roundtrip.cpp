#include "antimicrosettings.h"
#include "antkeymapper.h"
#include "eventhandlerfactory.h"
#include "joystick.h"
#include "logger.h"
#include "joyaxis.h"
#include "joybuttontypes/joyaxisbutton.h"
#include "xml/inputdevicexml.h"
#include "xmlconfigreader.h"
#include "xmlconfigwriter.h"
#include <QApplication>
#include <QFile>
#include <QTemporaryDir>
#include <QXmlStreamReader>
#include <cstdio>
#include <stdexcept>
static void check(bool ok, const char *msg) { if (!ok) throw std::runtime_error(msg); }
static void load(Joystick &j, const QString &path) {
    XMLConfigReader reader; reader.setJoystick(&j); reader.setFileName(path);
    check(!reader.read(), "reader reported error"); check(!reader.hasError(), "reader XML error");
}
static void save(Joystick &j, const QString &path) {
    InputDeviceXml xml(&j); XMLConfigWriter writer; writer.setFileName(path); writer.write(&xml);
    check(!writer.hasError(), "writer failed");
}
static void slot(JoyButton *b, int code, JoyButtonSlot::JoySlotInputAction mode) {
    check(b->getAssignedSlots()->size()==1, "slot count changed");
    auto *s=b->getAssignedSlots()->first();
    check(s->getSlotMode()==mode, "slot mode changed");
    // Key slots serialize aliases; assert the persisted key alias, not the platform keycode.
    check((mode==JoyButtonSlot::JoyKeyboard ? s->getSlotCodeAlias() : s->getSlotCode())==code, "slot code changed");
}
static void verify(Joystick &j) {
    check(j.getProfileName()=="Pad deterministic input regression", "profile name changed");
    auto *s=j.getSetJoystick(0); check(s->getName()=="Regression", "set name changed");
    slot(s->getJoyButton(0),0x41,JoyButtonSlot::JoyKeyboard);
    slot(s->getJoyButton(1),1,JoyButtonSlot::JoyMouseButton);
    auto *axis=s->getJoyAxis(0); check(axis->getDeadZone()==8000,"dead zone changed");
    slot(axis->getNAxisButton(),0x42,JoyButtonSlot::JoyKeyboard);
    slot(axis->getPAxisButton(),0x43,JoyButtonSlot::JoyKeyboard);
}
int main(int argc,char **argv) {
    QApplication app(argc,argv);
    Logger::createInstance(nullptr, Logger::LOG_NONE);
    try {
        check(argc==2,"pass fixture path"); QTemporaryDir tmp; check(tmp.isValid(),"temp dir failed");
        check(SDL_Init(SDL_INIT_JOYSTICK)==0,"SDL init failed");
        int index=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_UNKNOWN,2,4,1); check(index>=0,"attach failed");
        AntKeyMapper::getInstance("xtest");
        AntiMicroSettings settings(tmp.filePath("settings.ini"),QSettings::IniFormat);
        Joystick first(SDL_JoystickOpen(index),index,&settings,nullptr);
        load(first,argv[1]); verify(first); save(first,tmp.filePath("first.amgp"));
        Joystick second(SDL_JoystickOpen(index),index,&settings,nullptr);
        load(second,tmp.filePath("first.amgp")); verify(second); save(second,tmp.filePath("second.amgp"));
        QFile a(tmp.filePath("first.amgp")),b(tmp.filePath("second.amgp")); check(a.open(QFile::ReadOnly)&&b.open(QFile::ReadOnly),"open failed");
        check(a.readAll()==b.readAll(),"save-load-save XML is not stable");
        // Verify a fresh load clears stale state in another set, rather than merely adding slots.
        second.getSetJoystick(1)->getJoyButton(2)->setAssignedSlot(0x64,JoyButtonSlot::JoyKeyboard);
        load(second,tmp.filePath("first.amgp")); verify(second);
        check(second.getSetJoystick(1)->getJoyButton(2)->getAssignedSlots()->isEmpty(),"stale mapping survived load");
        puts("PASS profile native read/write/read, stable XML, stale-state reset");
    } catch (const std::exception &e) { fprintf(stderr,"FAIL profile: %s\n",e.what()); return 1; }
}
