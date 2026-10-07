// Regression coverage for unavailable accelerometer orientation and safe consumers.
#include "antimicrosettings.h"
#include "antkeymapper.h"
#include "eventhandlerfactory.h"
#include "eventhandlers/baseeventhandler.h"
#include "logger.h"
#include "joystick.h"
#include "sensors/joyaccelerometersensor.h"
#include "sensors/joysensorstatusbox.h"
#include "pad/padshell.h"
#include <QApplication>
#include <QTemporaryDir>
#include <QImage>
#include <cmath>
#include <limits>
#include <random>
#include <cstdio>
#include <stdexcept>
#include <cstring>
#include <new>

static void check(bool ok, const char *message) { if (!ok) throw std::runtime_error(message); }
class Probe : public JoyAccelerometerSensor {
public:
    explicit Probe(SetJoystick *set) : JoyAccelerometerSensor(200, 0, set, nullptr) {}
    void sample(float x,float y,float z) { m_current_value[0]=x; m_current_value[1]=y; m_current_value[2]=z; }
    JoySensorDirection direction() { return calculateSensorDirection(); }
};
static double originalPitch(double x,double y,double z) {
    double rad=std::sqrt(x*x+y*y+z*z); double p=-std::atan2(z/rad,y/rad)-M_PI/2;
    if(p < -M_PI) p+=2*M_PI;
    return p;
}
static double originalRoll(double x,double y,double z) {
    double rad=std::sqrt(x*x+y*y+z*z); double xp=x/rad,yp=y/rad,zp=z/rad;
    double r=std::atan2(std::sqrt(yp*yp+zp*zp),-xp)-M_PI/2;
    if(r < -M_PI) r+=2*M_PI;
    return r;
}
int main(int argc,char **argv) {
    QApplication app(argc,argv); PadUi::initializeApplicationStyle(); Logger::createInstance(nullptr,Logger::LOG_NONE);
    try {
        QTemporaryDir tmp; check(SDL_Init(SDL_INIT_JOYSTICK)==0,"SDL init failed");
        int index=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_UNKNOWN,2,4,1); check(index>=0,"virtual attach failed");
        AntKeyMapper::getInstance("xtest"); check(EventHandlerFactory::getInstance("xtest")->handler()->init(),"XTest init failed");
        auto *settings=new AntiMicroSettings(tmp.filePath("settings.ini"),QSettings::IniFormat);
        auto *joystick=new Joystick(SDL_JoystickOpen(index),index,settings,nullptr);
        // Poison raw storage before construction: first-state values must not depend on heap bytes.
        alignas(Probe) unsigned char storage[sizeof(Probe)];
        std::memset(storage, 0x3f, sizeof(storage));
        auto *fresh = new (storage) Probe(joystick->getSetJoystick(0));
        bool initialized = fresh->getXCoordinate() == 0 && fresh->getYCoordinate() == 0 && fresh->getZCoordinate() == 0;
        bool unavailable = std::isnan(fresh->calculatePitch()) && std::isnan(fresh->calculateRoll());
        fresh->~Probe();
        check(initialized && unavailable, "fresh sensor exposes uninitialized current sample before first event");
        Probe sensor(joystick->getSetJoystick(0)), control(joystick->getSetJoystick(0));
        const double inf=std::numeric_limits<double>::infinity(), nan=std::numeric_limits<double>::quiet_NaN();
        const double big=std::numeric_limits<double>::max(), tiny=std::numeric_limits<double>::denorm_min();
        const double bad[][3]={{0,0,0},{-0.,0.,-0.},{inf,1,1},{1,-inf,1},{1,1,inf},
                             {nan,1,1},{1,nan,1},{1,1,nan},{big,big,big},{tiny,0,0}};
        for(auto &v:bad) {
            check(std::isnan(sensor.calculatePitch(v[0],v[1],v[2])),"invalid pitch is not unavailable");
            check(std::isnan(sensor.calculateRoll(v[0],v[1],v[2])),"invalid roll is not unavailable");
        }
        std::mt19937 random(837); std::uniform_real_distribution<double> dist(-1000,1000);
        for(int i=0;i<10000;++i) {
            double x=dist(random),y=dist(random),z=dist(random);
            check(sensor.calculatePitch(x,y,z)==originalPitch(x,y,z),"valid pitch behavior changed");
            check(sensor.calculateRoll(x,y,z)==originalRoll(x,y,z),"valid roll behavior changed");
        }
        const float invalid[][3]={{0,0,0},{INFINITY,0,0},{0,-INFINITY,0},{0,0,INFINITY},
                                {NAN,1,1},{1,NAN,1},{1,1,NAN}};
        JoySensorStatusBox box; box.resize(320,320); box.setSensor(&sensor); box.show(); app.processEvents();
        for(auto &v:invalid) {
            sensor.sample(v[0],v[1],v[2]);
            check(!std::isfinite(sensor.calculatePitch()) && !std::isfinite(sensor.calculateRoll()),"current invalid sample has angles");
            check(sensor.direction()==SENSOR_CENTERED,"invalid sample activated a direction/shock");
            QImage image(box.size(),QImage::Format_ARGB32); image.fill(Qt::transparent); box.render(&image);
        }
        if(argc>1) { QImage image(box.size(),QImage::Format_ARGB32); image.fill(Qt::transparent); box.render(&image); image.save(argv[1]); }
        // Invalid samples never enter shock history: following valid samples match a clean sensor.
        for(int i=0;i<50;++i) {
            sensor.sample(30,-30,30); control.sample(30,-30,30);
            check(sensor.direction()==control.direction(),"invalid sample poisoned shock-filter state");
        }
        sensor.sample(0,-9.81f,0); QImage valid(box.size(),QImage::Format_ARGB32); valid.fill(Qt::transparent); box.render(&valid);
        if(argc>2) valid.save(argv[2]);
        std::puts("PASS: invalidity, 10000 exact valid-vector comparisons, activation/filter guard, and paint rendering");
        return 0;
    } catch(const std::exception &e) { std::fprintf(stderr,"FAIL: %s\n",e.what()); return 1; }
}
