// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PADSETTINGS_H
#define PADSETTINGS_H
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QRegularExpression>
#include <QSpinBox>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace PadUi {
inline void polishSettings(QDialog *dialog)
{
    dialog->setObjectName(QStringLiteral("padSettings"));
    dialog->setWindowTitle(QObject::tr("Settings"));
    dialog->setMinimumSize(840, 640); dialog->resize(920, 720);
    auto *root = qobject_cast<QVBoxLayout *>(dialog->layout());
    if (root) { root->setContentsMargins(24, 24, 24, 24); root->setSpacing(16); }
    auto *categories = dialog->findChild<QListWidget *>(QStringLiteral("categoriesListWidget"));
    if (categories) {
        categories->setFixedWidth(224); categories->setIconSize(QSize(16, 16));
        categories->setSpacing(0); categories->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        categories->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        const QStringList icons = {"settings", "gamepad", "text", "folder", "mouse", "sliders"};
        for (int i = 0; i < categories->count(); ++i) {
            categories->item(i)->setIcon(QIcon(QStringLiteral(":/pad/icons/%1.svg").arg(icons.value(i, "settings"))));
            categories->item(i)->setSizeHint(QSize(224, 48));
        }
        categories->setCurrentRow(0);
    }
    auto *stack = dialog->findChild<QStackedWidget *>(QStringLiteral("stackedWidget"));
    if (stack) {
        stack->setCurrentIndex(0);
        for (int i = 0; i < stack->count(); ++i) {
            QWidget *page = stack->widget(i);
            if (page->layout()) { page->layout()->setContentsMargins(24, 0, 0, 0); page->layout()->setSpacing(16); }
            for (auto *layout : page->findChildren<QLayout *>()) {
                layout->setSpacing(8);
                if (layout != page->layout()) layout->setContentsMargins(0, 0, 0, 0);
                // Remove arbitrary 5/10/40px gaps, keep only the bottom stretch.
                for (int j = 0; j < layout->count(); ++j) {
                    if (auto *spacer = layout->itemAt(j)->spacerItem()) {
                        if (spacer->expandingDirections() & Qt::Vertical)
                            spacer->changeSize(0, 0, QSizePolicy::Minimum, QSizePolicy::Fixed);
                        else if (!(spacer->expandingDirections() & Qt::Horizontal))
                            spacer->changeSize(0, 8, QSizePolicy::Minimum, QSizePolicy::Fixed);
                    }
                }
            }
            if (i == 0 || page->objectName() == "mouseSettingsPage" || i == stack->count() - 1) {
                if (auto *vertical = qobject_cast<QVBoxLayout *>(page->layout())) {
                    for (int j = 0; j < vertical->count(); ++j) vertical->setStretch(j, 0);
                    vertical->addStretch(1);
                }
            }
            for (auto *group : page->findChildren<QGroupBox *>()) {
                if (auto *vertical = qobject_cast<QVBoxLayout *>(group->layout())) {
                    for (int j = 0; j < vertical->count(); ++j) vertical->setStretch(j, 0);
                    vertical->setAlignment(Qt::AlignTop);
                }
                group->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
            }
            for (auto *form : page->findChildren<QFormLayout *>()) {
                form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
                for (int row = 0; row < form->rowCount(); ++row)
                    if (auto *item = form->itemAt(row, QFormLayout::LabelRole))
                        if (item->widget()) item->widget()->setFixedWidth(176);
            }
            // Label/control rows share one label column, including folder-picker rows.
            for (auto *layout : page->findChildren<QHBoxLayout *>()) {
                if (!layout->count()) continue;
                if (auto *label = qobject_cast<QLabel *>(layout->itemAt(0)->widget())) {
                    if (layout->count() > 1 && !label->wordWrap()) label->setFixedWidth(176);
                }
            }
        }
    }
    for (auto *widget : dialog->findChildren<QWidget *>()) {
        QFont font = qApp->font(); font.setWeight(QFont::Normal); font.setItalic(false); widget->setFont(font);
        if (qobject_cast<QComboBox *>(widget) || qobject_cast<QLineEdit *>(widget) ||
            qobject_cast<QAbstractSpinBox *>(widget) || qobject_cast<QPushButton *>(widget)) {
            widget->setFixedHeight(40); widget->setSizePolicy(widget->sizePolicy().horizontalPolicy(), QSizePolicy::Fixed);
        }
        if (auto *check = qobject_cast<QCheckBox *>(widget)) check->setMinimumHeight(32);
        if (auto *label = qobject_cast<QLabel *>(widget)) {
            QString value = label->text();
            value.replace(QRegularExpression("font-style\\s*:\\s*italic;?"), "");
            label->setText(value);
            if (label->wordWrap()) { label->setMinimumWidth(0); label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred); }
        }
    }
    for (auto *group : dialog->findChildren<QGroupBox *>()) {
        if (group->layout()) group->layout()->invalidate();
        group->setMinimumHeight(group->sizeHint().height());
    }
}
}
#endif
