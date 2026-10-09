// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PADSHELL_H
#define PADSHELL_H
class QMainWindow;
class QWidget;
class QStackedWidget;
class QTabWidget;
class QAction;
class QDialog;
namespace PadUi {
void initializeApplicationStyle();
void install(QMainWindow *window, QWidget *central, QStackedWidget *stack,
             QTabWidget *controllers, QAction *refresh, QAction *settings);
QDialog *createOnboardingDialog(QWidget *parent, QTabWidget *controllers, QAction *settings);
void showOnboardingIfFirstRun(QMainWindow *window, QTabWidget *controllers, QAction *settings);
}
#endif
