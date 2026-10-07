// Test-only sensor capability: real sensor model classes, no physical device claim.
class SensorJoystick : public Joystick {
public:
    using Joystick::Joystick;
    bool hasRawSensor(JoySensorType) override { return true; }
    double getRawSensorRate(JoySensorType) override { return 200.0; }
};
static void near(double got,double want,double tolerance,const char *message) {
    check(std::isfinite(got)&&std::abs(got-want)<=tolerance,message);
}
static void persistCalibration(Joystick *j,const QString &path,const QByteArray &element) {
    QMetaObject::invokeMethod(j,[j,path] {InputDeviceXml xml(j); XMLConfigWriter writer; writer.setFileName(path);writer.write(&xml);check(!writer.hasError(),"calibration profile write failed");},Qt::BlockingQueuedConnection);
    QFile file(path);check(file.open(QFile::ReadOnly),"calibration profile missing");check(file.readAll().contains(element),"saved calibration absent in XML");
}
static bool extended(const QString &test,Joystick *j,AntiMicroSettings *settings,const QTemporaryDir &tmp) {
    auto *set=j->getActiveSetJoystick();auto *button=set->getJoyButton(0);
    if(test=="sensor-zero-vector") {
        auto *sensor=set->getSensor(ACCELEROMETER);check(sensor,"sensor fixture missing");auto *d=new JoySensorEditDialog(sensor);show(d);
        check(widget<QLabel>(d,"pitchValue")->text()=="Unavailable"&&widget<QLabel>(d,"rollValue")->text()=="Unavailable","initial zero-vector stats must be Unavailable");
        QMetaObject::invokeMethod(sensor,[sensor]{float v[3]={0,0,0};sensor->joyEvent(v);emit sensor->moved(0,0,0);},Qt::BlockingQueuedConnection);settle();
        check(std::isnan(sensor->calculatePitch(0,0,0))&&std::isnan(sensor->calculateRoll(0,0,0)),"zero-vector math must report invalidity with NaN");
        check(widget<QLabel>(d,"pitchValue")->text()=="Unavailable"&&widget<QLabel>(d,"rollValue")->text()=="Unavailable","zero-vector stats must render Unavailable");
        for(const char *name:{"xCoordinateValue","yCoordinateValue","zCoordinateValue","accelerationValue"}) {auto *label=widget<QLabel>(d,name);bool ok=false;double v=label->text().toDouble(&ok);check(ok&&v==0&&label->isVisible(),"zero-vector raw values/magnitude must remain visible as zero");}
        check(sensor->getCurrentDirection()==SENSOR_CENTERED,"zero-vector sensor activated a phantom direction");
        double bad=std::numeric_limits<double>::infinity();check(std::isnan(sensor->calculatePitch(bad,1,1))&&std::isnan(sensor->calculateRoll(bad,1,1)),"non-finite vector math must report invalidity");
        QMetaObject::invokeMethod(sensor,[sensor]{float v[3]={std::numeric_limits<float>::infinity(),0,0};sensor->joyEvent(v);},Qt::BlockingQueuedConnection);settle();check(sensor->getCurrentDirection()==SENSOR_CENTERED,"non-finite sensor activated a phantom direction");
        closeButton(d,QDialogButtonBox::Close);delete d;return true;
    }
    if(test=="sensor-default-preset") {
        for(auto type:{ACCELEROMETER,GYROSCOPE}) {auto *sensor=set->getSensor(type);auto *d=new JoySensorEditDialog(sensor);show(d);auto *presets=widget<QComboBox>(d,"presetsComboBox");fprintf(stderr,"fresh sensor type=%d selected=%s actual=None\n",int(type),presets->currentText().toUtf8().constData());check(presets->currentData().toInt()==int(JoySensorPreset::PRESET_NONE),"fresh unmapped sensor shows a non-None preset");closeButton(d,QDialogButtonBox::Close);delete d;}return true;
    }
    if(test=="sensor-accel"||test=="sensor-gyro") {
        auto *sensor=set->getSensor(test=="sensor-accel"?ACCELEROMETER:GYROSCOPE);check(sensor,"sensor fixture missing");
        auto *d=new JoySensorEditDialog(sensor);show(d);
        widget<QDoubleSpinBox>(d,"deadZoneSpinBox")->setValue(12);settle();near(sensor->getDeadZone(),12,0.01,"sensor dead zone failed");
        widget<QSlider>(d,"deadZoneSlider")->setValue(15);settle();near(sensor->getDeadZone(),15,0.01,"sensor slider failed");
        widget<QDoubleSpinBox>(d,"maxZoneSpinBox")->setValue(65);settle();near(sensor->getMaxZone(),65,0.01,"sensor max zone failed");
        spin(d,"diagonalRangeSpinBox",60);near(sensor->getDiagonalRange(),60,0.01,"sensor diagonal failed");
        widget<QDoubleSpinBox>(d,"sensorDelayDoubleSpinBox")->setValue(.12);settle();check(sensor->getSensorDelay()==120,"sensor delay failed");
        text(d,"sensorNameLineEdit","QA Sensor");check(sensor->getSensorName()=="QA Sensor","sensor rename failed");
        auto *presets=widget<QComboBox>(d,"presetsComboBox");
        presets->setCurrentIndex(presets->findData(int(JoySensorPreset::PRESET_NONE)));settle();
        for(int n=0;n<presets->count();++n) {
            presets->setCurrentIndex(n);settle();JoySensorPreset p(sensor);check(int(p.currentPreset())==presets->itemData(n).toInt(),"sensor preset actual assignments mismatch");
            auto snapshot=[sensor] {
                QMap<int,QStringList> result;
                QMetaObject::invokeMethod(sensor,[sensor,&result] {
                    for(auto it=sensor->getButtons()->cbegin();it!=sensor->getButtons()->cend();++it) {
                        QStringList values;
                        for(auto *slot:*it.value()->getAssignedSlots()) values.append(QString("%1:%2:%3:%4:%5").arg(int(slot->getSlotMode())).arg(slot->getSlotCode()).arg(slot->getSlotCodeAlias()).arg(slot->getTextData()).arg(slot->getExtraData().toString()));
                        result.insert(int(it.key()),values);
                    }
                },Qt::BlockingQueuedConnection);
                return result;
            };
            auto before=snapshot();auto *reopened=new JoySensorEditDialog(sensor);show(reopened);
            check(widget<QComboBox>(reopened,"presetsComboBox")->currentData()==presets->itemData(n),"reopened sensor preset selection mismatch");
            check(before==snapshot(),"sensor dialog reopen mutated slot contents/order");closeButton(reopened,QDialogButtonBox::Close);delete reopened;
        }
        presets->setCurrentIndex(presets->findData(int(JoySensorPreset::PRESET_NONE)));settle();check(!sensor->hasSlotsAssigned(),"sensor None didn't clear");
        click(widget<QPushButton>(d,"mouseSettingsPushButton"));
        auto *mouse=d->findChild<MouseSensorSettingsDialog*>();check(mouse&&mouse->isVisible(),"sensor mouse route didn't open");mouse->setAttribute(Qt::WA_DeleteOnClose,false);
        spin(mouse,"horizontalSpinBox",44);spin(mouse,"verticalSpinBox",55);spin(mouse,"wheelHoriSpeedSpinBox",23);spin(mouse,"wheelVertSpeedSpinBox",25);
        for(auto *b:*sensor->getButtons())check(b->getMouseSpeedX()==44&&b->getMouseSpeedY()==55&&b->getWheelSpeedX()==23&&b->getWheelSpeedY()==25,"sensor mouse properties failed");
        closeButton(mouse,QDialogButtonBox::Close);delete mouse;check(widget<QPushButton>(d,"mouseSettingsPushButton")->isEnabled(),"sensor mouse route stayed disabled");closeButton(d,QDialogButtonBox::Close);delete d;
        return true;
    }
    if(test=="calibration-gyro"||test=="calibration-accel") {
        auto type=test=="calibration-gyro"?GYROSCOPE:ACCELEROMETER;auto *sensor=set->getSensor(type);check(sensor,"calibration sensor missing");
        auto *d=new Calibration(j);show(d);auto *devices=widget<QComboBox>(d,"deviceComboBox");int index=devices->findText(type==GYROSCOPE?"Gyroscope":"Accelerometer");check(index>=0,"calibration type missing");devices->setCurrentIndex(index);settle();
        click(widget<QPushButton>(d,"startBtn"));click(widget<QPushButton>(d,"startBtn"));
        for(int n=0;n<20&&!widget<QPushButton>(d,"saveBtn")->isEnabled();++n){QMetaObject::invokeMethod(sensor,[sensor,type]{emit sensor->moved(type==GYROSCOPE?.1f:1.f,type==GYROSCOPE?.2f:2.f,type==GYROSCOPE?.3f:9.f);},Qt::BlockingQueuedConnection);QTest::qWait(3);}
        check(widget<QPushButton>(d,"saveBtn")->isEnabled(),"sensor samples didn't complete calibration");check(!sensor->isCalibrated(),"sensor calibrated before Save");click(widget<QPushButton>(d,"saveBtn"));
        for(auto *s:j->getJoystick_sets()){auto *saved=s->getSensor(type);check(saved->isCalibrated(),"sensor Save not applied across sets");double x,y,z;saved->getCalibration(&x,&y,&z);near(x,type==GYROSCOPE?.1:1,1e-5,"sensor calibration x wrong");near(y,type==GYROSCOPE?.2:2,1e-5,"sensor calibration y wrong");near(z,type==GYROSCOPE?.3:9,1e-5,"sensor calibration z wrong");}
        persistCalibration(j,tmp.filePath("calibration.amgp"),type==GYROSCOPE?"gyroscope":"accelerometer");check(!widget<QPushButton>(d,"saveBtn")->isEnabled(),"Save didn't reset dirty state");click(widget<QPushButton>(d,"cancelBtn"));delete d;return true;
    }
    if(test=="calibration-stick-full") {
        auto *stick=set->getJoyStick(0);auto *d=new Calibration(j);show(d);click(widget<QPushButton>(d,"startBtn"));
        auto sample=[stick](int x,int y){QMetaObject::invokeMethod(stick,[stick,x,y]{emit stick->moved(x,y);},Qt::BlockingQueuedConnection);QTest::qWait(2);};
        for(int n=0;n<102;++n)sample(1000,1500);
        for(int n=0;n<2000&&!widget<QPushButton>(d,"startBtn")->isEnabled();++n)sample(n%2?1000:1001,n%2?1500:1501);
        check(widget<QPushButton>(d,"startBtn")->isEnabled(),"stick offset samples didn't converge");click(widget<QPushButton>(d,"startBtn"));
        for(int n=0;n<14000&&!widget<QPushButton>(d,"saveBtn")->isEnabled();++n){int sign=n%300<150?1:-1;sample(1000+29000*sign,1500+28500*sign);}
        check(widget<QPushButton>(d,"saveBtn")->isEnabled(),"stick gain samples didn't converge");check(!stick->isCalibrated(),"stick calibrated before Save");click(widget<QPushButton>(d,"saveBtn"));
        for(auto *s:j->getJoystick_sets()){auto *saved=s->getJoyStick(0);check(saved->isCalibrated(),"stick Save not applied to all sets");double ox,gx,oy,gy;saved->getCalibration(&ox,&gx,&oy,&gy);check(std::isfinite(ox)&&std::isfinite(oy)&&std::abs(ox)<3000&&std::abs(oy)<3000&&gx>1&&gx<1.5&&gy>1&&gy<1.5,"stick calibration coefficients invalid");}
        persistCalibration(j,tmp.filePath("calibration.amgp"),"stick");click(widget<QPushButton>(d,"cancelBtn"));delete d;return true;
    }
    if(test=="advanced-slots") {
        auto *d=new AdvanceButtonDialog(button);show(d);auto *list=widget<QListWidget>(d,"slotListWidget");auto *type=widget<QComboBox>(d,"slotTypeComboBox");
        struct Case{int index;JoyButtonSlot::JoySlotInputAction mode;};
        for(auto c:{Case{1,JoyButtonSlot::JoyCycle},Case{2,JoyButtonSlot::JoyDelay},Case{5,JoyButtonSlot::JoyHold},Case{8,JoyButtonSlot::JoyPause},Case{9,JoyButtonSlot::JoyKeyPress},Case{10,JoyButtonSlot::JoyRelease},Case{3,JoyButtonSlot::JoyDistance},Case{7,JoyButtonSlot::JoyMouseSpeedMod},Case{11,JoyButtonSlot::JoySetChange}}){
            click(widget<QPushButton>(d,"clearAllPushButton"));list->setCurrentRow(0);list->item(0)->setSelected(true);type->setCurrentIndex(c.index);settle();
            widget<QComboBox>(d,"actionSecondsComboBox")->setCurrentIndex(2);widget<QComboBox>(d,"actionTenthsComboBox")->setCurrentIndex(3);widget<QComboBox>(d,"actionHundredthsComboBox")->setCurrentIndex(4);spin(d,"distanceSpinBox",27);spin(d,"mouseSpeedModSpinBox",73);widget<QComboBox>(d,"slotSetChangeComboBox")->setCurrentIndex(1);settle();
            click(widget<QPushButton>(d,"insertSlotButton"));check(button->getAssignedSlots()->size()==1,"advanced special slot wasn't inserted");auto *slot=button->getAssignedSlots()->first();check(slot->getSlotMode()==c.mode,"advanced special slot wrong mode");
            if(c.index==2||c.index==5||c.index==8||c.index==9||c.index==10)check(slot->getSlotCode()==2340,"advanced time conversion wrong");
            if(c.index==2) {
                list->setCurrentRow(0);auto *item=list->item(0);QMetaObject::invokeMethod(d,"performStatsWidgetRefresh",Q_ARG(QListWidgetItem*,item));settle();
                widget<QComboBox>(d,"actionSecondsComboBox")->setCurrentIndex(4);settle();check(button->getAssignedSlots()->first()->getSlotCode()==4340,"selected timing slot edit failed");
            }
            if(c.index==3)check(slot->getSlotCode()==27,"advanced distance wrong");
            if(c.index==7)check(slot->getSlotCode()==73,"advanced mouse mod wrong");
            if(c.index==11)check(slot->getSlotCode()==widget<QComboBox>(d,"slotSetChangeComboBox")->itemData(1).toInt(),"advanced set change target wrong");
        }
        closeButton(d,QDialogButtonBox::Close);delete d;return true;
    }
    if(test=="advanced-text-files") {
        auto *d=new AdvanceButtonDialog(button);show(d);auto *list=widget<QListWidget>(d,"slotListWidget");auto *type=widget<QComboBox>(d,"slotTypeComboBox");
        auto prepare=[&](int index){click(widget<QPushButton>(d,"clearAllPushButton"));list->setCurrentRow(0);list->item(0)->setSelected(true);type->setCurrentIndex(index);settle();};
        prepare(12);text(d,"textEntryLineEdit","literal text 123");click(widget<QPushButton>(d,"insertSlotButton"));check(button->getAssignedSlots()->size()==1&&button->getAssignedSlots()->first()->getSlotMode()==JoyButtonSlot::JoyTextEntry&&button->getAssignedSlots()->first()->getTextData()=="literal text 123","text slot contents wrong");
        QString exec=tmp.filePath("test-script.sh");QFile script(exec);check(script.open(QFile::WriteOnly),"script fixture write failed");script.write("#!/bin/sh\nexit 0\n");script.close();
        prepare(4);text(d,"execLineEdit",exec);text(d,"execArgumentsLineEdit","--test arg");click(widget<QPushButton>(d,"insertSlotButton"));auto *slot=button->getAssignedSlots()->first();check(slot->getSlotMode()==JoyButtonSlot::JoyExecute&&slot->getTextData()==exec&&slot->getExtraData().toString()=="--test arg","execute slot contents wrong");
        QString profile=tmp.filePath("target.amgp");QFile file(profile);check(file.open(QFile::WriteOnly),"target profile write failed");file.write("<joystick/>");file.close();
        prepare(6);fileChoice(profile);click(widget<QPushButton>(d,"insertSlotButton"));slot=button->getAssignedSlots()->first();check(slot->getSlotMode()==JoyButtonSlot::JoyLoadProfile&&slot->getTextData()==profile,"load profile slot contents wrong");
        persistCalibration(j,tmp.filePath("slot-save.amgp"),"loadprofile");closeButton(d,QDialogButtonBox::Close);delete d;return true;
    }
    if(test=="main-menu-routes") {
        QMap<SDL_JoystickID,InputDevice*> devices;devices.insert(j->getSDLJoystickID(),j);CommandLineUtility cmd;
        auto *window=new MainWindow(&devices,&cmd,settings,true);window->fillButtons();window->show();settle();
        widget<QAction>(window,"actionKeyValue")->trigger();settle();auto *keys=window->findChild<QKeyDisplayDialog*>();check(keys&&keys->isVisible(),"main key checker route failed");keys->setAttribute(Qt::WA_DeleteOnClose,false);closeButton(keys,QDialogButtonBox::Close);delete keys;
        widget<QAction>(window,"actionOptions")->trigger();settle();auto *options=window->findChild<MainSettingsDialog*>();check(options&&options->isVisible(),"main settings route failed");options->setAttribute(Qt::WA_DeleteOnClose,false);closeButton(options,QDialogButtonBox::Cancel);delete options;
        widget<QAction>(window,"actionCalibration")->trigger();settle();Calibration *cal=nullptr;for(auto *w:QApplication::topLevelWidgets())if(auto *c=qobject_cast<Calibration*>(w))if(c->isVisible())cal=c;check(cal,"main calibration route failed");cal->setAttribute(Qt::WA_DeleteOnClose,false);click(widget<QPushButton>(cal,"cancelBtn"));delete cal;
        delete window;return true;
    }
    if(test=="menu-routes") {
        auto *tab=new JoyTabWidget(j,settings);tab->show();settle();QMenu *sets=nullptr,*copy=nullptr;
        for(auto *m:tab->findChildren<QMenu*>()){if(m->title()=="Copy from Set")copy=m;else for(auto *a:m->actions())if(a->text()=="Set 8")sets=m;}
        check(sets&&copy,"set menus missing");
        for(int n=0;n<8;++n){QAction *action=nullptr;for(auto *a:sets->actions())if(a->text()==QString("Set %1").arg(n+1))action=a;check(action,"set menu action missing");action->trigger();settle();check(j->getActiveSetNumber()==n,"set menu route wrong active set");}
        QMetaObject::invokeMethod(j,[j]{auto *b=j->getSetJoystick(0)->getJoyButton(0);b->setAssignedSlot(97,Qt::Key_A,JoyButtonSlot::JoyKeyboard);},Qt::BlockingQueuedConnection);settle();
        QAction *source=nullptr;for(auto *a:copy->actions())if(a->data().toInt()==0)source=a;check(source,"copy source action missing");answer(QMessageBox::No);source->trigger();settle();check(j->getActiveSetJoystick()->getJoyButton(0)->getAssignedSlots()->isEmpty(),"Copy No committed");answer(QMessageBox::Yes);source->trigger();settle();check(j->getActiveSetJoystick()->getJoyButton(0)->getAssignedSlots()->size()==1,"Copy Yes didn't copy mappings");
        for(auto *a:sets->actions())if(a->text()=="Settings"){a->trigger();settle();auto *d=tab->findChild<SetNamesDialog*>();check(d&&d->isVisible(),"set menu Settings route failed");d->setAttribute(Qt::WA_DeleteOnClose,false);closeButton(d,QDialogButtonBox::Cancel);delete d;}
        bool before=tab->isDisplayingNames();click(widget<QPushButton>(tab,"namesPushButton"));check(tab->isDisplayingNames()!=before,"names route didn't toggle");click(widget<QPushButton>(tab,"namesPushButton"));check(tab->isDisplayingNames()==before,"names route didn't restore");delete tab;return true;
    }
    return false;
}
