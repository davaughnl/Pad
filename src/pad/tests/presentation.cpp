#include "antimicrosettings.h"
#include "antkeymapper.h"
#include "eventhandlerfactory.h"
#include "joystick.h"
#include "logger.h"
#include "joyaxis.h"
#include "joybuttontypes/joyaxisbutton.h"
#include "joycontrolstick.h"
#include "gui/joytabwidget.h"
#include "gui/mainsettingsdialog.h"
#include "pad/padshell.h"
#include "xmlconfigreader.h"
#include <QApplication>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <QStackedWidget>
#include <QListWidget>
#include <QPushButton>
#include <QTimer>
#include <QEventLoop>
#include <QMenu>
#include <QLabel>
#include <QComboBox>
#include <QAbstractSpinBox>
#include <QFontInfo>
#include <QGroupBox>
#include <QCheckBox>
#include <QScrollArea>
#include <QScrollBar>
#include <cstdio>
#include <stdexcept>
static void check(bool ok,const char *msg) { if(!ok) throw std::runtime_error(msg); }
static void spin() { QEventLoop loop; QTimer::singleShot(120,&loop,&QEventLoop::quit); loop.exec(); }
static void dismiss(JoyTabWidget &tab) { for(auto *d:tab.findChildren<QDialog *>()) d->close(); spin(); }
int main(int argc,char **argv) {
 QApplication app(argc,argv); PadUi::initializeApplicationStyle();
 Logger::createInstance(nullptr,Logger::LOG_NONE); QTemporaryDir tmp;
 try {
  check(SDL_Init(SDL_INIT_JOYSTICK)==0,"SDL init");
  int index=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_UNKNOWN,4,4,1);
  AntKeyMapper::getInstance("xtest"); EventHandlerFactory::getInstance("xtest");
  AntiMicroSettings settings(tmp.filePath("settings.ini"),QSettings::IniFormat);
  Joystick joystick(SDL_JoystickOpen(index),index,&settings,nullptr);
  for(int i=0;i<8;i++) {auto *s=joystick.getSetJoystick(i);auto *stick=new JoyControlStick(s->getJoyAxis(0),s->getJoyAxis(1),0,i,s);stick->setDefaultStickName("Left stick");s->addControlStick(0,stick);}
  joystick.getSetJoystick(0)->getJoyButton(0)->setAssignedSlot(0x41,JoyButtonSlot::JoyKeyboard);
  JoyTabWidget tab(&joystick,&settings);tab.resize(960,640);tab.fillButtons();tab.show();spin();
  auto *stack=tab.findChild<QStackedWidget *>("stackedWidget_2");
  auto table=[&](){return stack->currentWidget()->findChild<QTreeWidget *>("padMappingTable");};
  check(table()!=nullptr,"table exists");
  auto *t=table(); check(t->topLevelItemCount()>8,"directions/axis/buttons included");
  for(int i=0;i<t->topLevelItemCount();i++) { auto *item=t->topLevelItem(i);auto *cell=t->itemWidget(item,1);auto *action=cell?cell->findChild<QPushButton *>():nullptr;check(action!=nullptr,"click target");
   if(item->isHidden()) { continue; } action->click();spin();check(!tab.findChildren<QDialog *>().isEmpty(),"original editor opens");dismiss(tab);
  }
  auto *first=t->itemWidget(t->topLevelItem(0),1)->findChild<QPushButton *>();
  QMetaObject::invokeMethod(first,"customContextMenuRequested",Q_ARG(QPoint,QPoint(12,12)));spin();
  bool menu=false;for(auto *m:tab.findChildren<QMenu *>()) if(m->isVisible()){menu=true;m->close();}check(menu,"original context menu opens");
  tab.changeCurrentSet(1);spin();check(stack->currentIndex()==1,"set switch");tab.changeCurrentSet(0);spin();
  tab.refreshButtons();spin();check(table()!=nullptr,"refresh rebuild");check(table()->columnWidth(0)>300,"refresh column sizing");
  tab.changeNameDisplay(true);spin();tab.changeNameDisplay(false);spin();
  joystick.getSetJoystick(0)->getJoyStick(0)->setJoyMode(JoyControlStick::EightWayMode);spin();
  joystick.getSetJoystick(0)->getJoyButton(0)->setToggle(true);
  joystick.getSetJoystick(0)->getJoyButton(0)->setUseTurbo(true);
  spin();
  { auto *current=table(); bool found=false;
    for(int i=0;i<current->topLevelItemCount();i++) if(current->topLevelItem(i)->text(0)=="Button 1") {
        auto *cell=current->itemWidget(current->topLevelItem(i),1);
        auto *action=cell->findChild<QPushButton *>();
        check(action->text().contains("A",Qt::CaseInsensitive),"live assigned action");
        auto *behavior=qobject_cast<QLabel *>(current->itemWidget(current->topLevelItem(i),2));
        check(behavior->text().contains("Toggle")&&behavior->text().contains("Turbo"),"live behavior"); found=true;
    } check(found,"ordinary button row"); }
  settings.setValue("HideEmptyButtons",true);tab.checkHideEmptyOption();tab.refreshButtons();spin();
  check(table()->topLevelItemCount()==1,"hide empty retained only assigned input");
  settings.setValue("HideEmptyButtons",false);tab.checkHideEmptyOption();tab.refreshButtons();spin();
  auto *devices=new QList<InputDevice *>{&joystick};auto *dialog=new MainSettingsDialog(&settings,devices);dialog->show();spin();
  auto *cats=dialog->findChild<QListWidget *>("categoriesListWidget");auto *pages=dialog->findChild<QStackedWidget *>("stackedWidget");
  check(cats->currentRow()==0&&pages->currentIndex()==0,"initial settings page matches");
  for(int i=0;i<cats->count();i++){cats->setCurrentRow(i);spin();check(pages->currentIndex()==i,"settings category route");for(auto *w:pages->currentWidget()->findChildren<QWidget *>()) {
      if((qobject_cast<QAbstractSpinBox *>(w)||qobject_cast<QComboBox *>(w)||qobject_cast<QPushButton *>(w)) && w->isVisible())
        check(w->height()==32,"uniform settings control height");
    } dialog->grab().save(QString(tmp.filePath("settings-page-%1.png")).arg(i));}
  for(auto *w:dialog->findChildren<QWidget *>()){check(w->font().weight()==QFont::Normal,"regular weight");check(QFontInfo(w->font()).family()=="Geist","Geist loaded");}
  // Exercise the longest Windows page even in Linux fixture builds.
  cats->setCurrentRow(0); dialog->resize(840,640);
  auto *repeat=dialog->findChild<QGroupBox *>("keyRepeatGroupBox");
  auto *startup=dialog->findChild<QCheckBox *>("launchAtWinStartupCheckBox");
  check(repeat&&startup,"Windows controls exist");repeat->show();startup->show();spin();
  auto *scroll=qobject_cast<QScrollArea *>(pages->currentWidget());
  check(scroll&&scroll->verticalScrollBar()->maximum()>0,"long settings page scrolls");
  scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());spin();
  check(scroll->viewport()->rect().contains(repeat->mapTo(scroll->viewport(),repeat->rect().bottomRight())),"complete Key Repeat group reachable");

  dialog->close();spin();tab.grab().save(tmp.filePath("table.png"));
  puts("PASS real Qt table: original editors/context menu, sets, refresh columns, names, direction mode, live action/behavior, hide-empty, all settings pages, Geist Regular");
 }catch(const std::exception &e){fprintf(stderr,"FAIL %s\n",e.what());return 1;}
}
