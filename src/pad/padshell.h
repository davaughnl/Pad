// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PADSHELL_H
#define PADSHELL_H
class QMainWindow;
class QWidget;
class QStackedWidget;
class QTabWidget;
class QAction;
namespace PadUi {
void initializeApplicationStyle();
void install(QMainWindow *window, QWidget *central, QStackedWidget *stack,
             QTabWidget *controllers, QAction *refresh, QAction *settings);
}
#endif
