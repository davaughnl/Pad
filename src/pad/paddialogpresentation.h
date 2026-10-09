// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PADDIALOGPRESENTATION_H
#define PADDIALOGPRESENTATION_H
#include <QApplication>
#include <QAbstractSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QEvent>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QStackedWidget>
#include <QPushButton>
#include <QSlider>
#include <QScrollArea>
#include <QScreen>
#include <QVBoxLayout>
namespace PadUi {
inline void compactDialog(QDialog *dialog)
{
    dialog->setProperty("padCompactDialog", true);
    // Remove native local sheets so monochrome application styling wins.
    dialog->setStyleSheet(dialog->styleSheet() + QStringLiteral(
        "QGroupBox { padding: 16px; margin-top: 16px; }"
        "QGroupBox::title { color: #b8b8bf; left: 16px; }"
        "QFrame[frameShape=\"4\"] { background: #303035; color: #303035; }"
        "QComboBox, QLineEdit, QSpinBox, QDoubleSpinBox, QPushButton { min-height: 24px; max-height: 24px; padding: 0 10px; }"
        "QSpinBox QLineEdit, QDoubleSpinBox QLineEdit { min-height: 0; max-height: 16777215px; padding: 0; border: none; background: transparent; }"
        "QFrame[frameShape=\"4\"] { color: #303035; background: #303035; max-height: 1px; border: none; }"));
    for (auto *layout : dialog->findChildren<QLayout *>()) {
        const bool root = layout == dialog->layout();
        layout->setContentsMargins(root ? 20 : 0, root ? 20 : 0, root ? 20 : 0, root ? 20 : 0);
        layout->setSpacing(8);
        if (auto *vertical = qobject_cast<QVBoxLayout *>(layout)) {
            vertical->setAlignment(Qt::AlignTop);
            for (int i = 0; i < vertical->count(); ++i) vertical->setStretch(i, 0);
        }
        for (int i = 0; i < layout->count(); ++i)
            if (auto *spacer = layout->itemAt(i)->spacerItem())
                if (!(spacer->expandingDirections() & Qt::Horizontal))
                    spacer->changeSize(0, 0, QSizePolicy::Minimum, QSizePolicy::Fixed);
    }
    for (auto *widget : dialog->findChildren<QWidget *>()) {
        QFont font = qApp->font(); font.setWeight(QFont::Normal); font.setItalic(false); widget->setFont(font);
        if (qobject_cast<QAbstractSpinBox *>(widget) || qobject_cast<QComboBox *>(widget) ||
            (qobject_cast<QLineEdit *>(widget) && !qobject_cast<QAbstractSpinBox *>(widget->parentWidget())) || qobject_cast<QPushButton *>(widget))
            widget->setFixedHeight(24);
        if (qobject_cast<QCheckBox *>(widget)) widget->setMinimumHeight(24);
        if (auto *frame = qobject_cast<QFrame *>(widget))
            if (frame->frameShape() == QFrame::HLine) frame->hide();
        if (auto *group = qobject_cast<QGroupBox *>(widget))
            group->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    }
}

// Expanding-content variant: same normalization, but native layout expansion is
// restored so browsers, tables and slot lists fill the dialog instead of
// clamping to their size hint with dead space below.
inline void compactDialogFill(QDialog *dialog)
{
    compactDialog(dialog);
    for (auto *layout : dialog->findChildren<QVBoxLayout *>()) layout->setAlignment(Qt::Alignment());
}
inline void polishAboutDialog(QDialog *dialog)
{
    compactDialogFill(dialog);
    // Redundant with the title row logo + name; renders as stray floating text.
    if (auto *brand = dialog->findChild<QLabel *>(QStringLiteral("copyrightLabel"))) brand->hide();
}
inline void polishAdvanceDialog(QDialog *dialog)
{
    compactDialogFill(dialog);
    // Section rail sizes to its four rows instead of a full-height empty card.
    if (auto *rail = dialog->findChild<QListWidget *>(QStringLiteral("listWidget"))) {
        const int row = rail->sizeHintForRow(0) > 0 ? rail->sizeHintForRow(0) : 40;
        rail->setFixedHeight(rail->count() * (row + 8) + 20);
        rail->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        rail->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    }
    // The assignment stack fills the remaining height.
    if (auto *stack = dialog->findChild<QStackedWidget *>(QStringLiteral("stackedWidget")))
        stack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    if (auto *root = qobject_cast<QVBoxLayout *>(dialog->layout())) root->setStretch(0, 1);
    if (auto *body = dialog->findChild<QHBoxLayout *>(QStringLiteral("horizontalLayout")))
        body->setAlignment(dialog->findChild<QListWidget *>(QStringLiteral("listWidget")), Qt::AlignTop);
    if (auto *slotList = dialog->findChild<QListWidget *>(QStringLiteral("slotListWidget"))) {
        slotList->setMaximumHeight(QWIDGETSIZE_MAX);
        slotList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }
    if (auto *controls = dialog->findChild<QStackedWidget *>(QStringLiteral("slotControlsStackedWidget"))) {
        // A stack otherwise reserves the tallest inactive page even for the
        // single Time row, leaving an empty band in Assignments.
        auto fitControls = [controls](int row) {
            auto *page = controls->widget(row);
            if (page && page->layout()) {
                page->layout()->activate();
                controls->setFixedHeight(page->layout()->sizeHint().height());
            }
        };
        QObject::connect(controls, &QStackedWidget::currentChanged, dialog, fitControls);
        fitControls(controls->currentIndex());
    }
}
inline void polishSensorDialog(QDialog *dialog)
{
    compactDialog(dialog);
    auto *status = dialog->findChild<QWidget *>("sensorStatusBoxWidget");
    status->setFixedSize(224, 224);
    for (auto *layout : dialog->findChildren<QHBoxLayout *>()) {
        if (layout->count() < 2) continue;
        if (auto *label = qobject_cast<QLabel *>(layout->itemAt(0)->widget())) label->setFixedWidth(112);
    }
    for (auto *spin : dialog->findChildren<QAbstractSpinBox *>()) spin->setFixedWidth(112);
    auto *mouse = dialog->findChild<QPushButton *>("mouseSettingsPushButton");
    mouse->setIcon(QIcon(QStringLiteral(":/pad/icons/mouse.svg"))); mouse->setIconSize(QSize(16,16));
    dialog->resize(800, 496);
}
inline void polishCaptureDialog(QDialog *dialog)
{
    compactDialog(dialog);
    for (const char *name : {"winClassLabel", "winTitleLabel", "winPathLabel"}) {
        auto *label = dialog->findChild<QLabel *>(name);
        label->setWordWrap(true); label->setTextInteractionFlags(Qt::TextSelectableByMouse);
        label->setMinimumWidth(0); label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    }
    for (auto *form : dialog->findChildren<QFormLayout *>()) {
        form->setLabelAlignment(Qt::AlignLeft | Qt::AlignTop);
        form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        for (int row=0; row<form->rowCount(); ++row)
            if (auto *item=form->itemAt(row,QFormLayout::LabelRole))
                if (item->widget()) item->widget()->setFixedWidth(72);
    }
    dialog->resize(640, 432);
}
// Derived mouse dialogs decide which native groups are visible in their ctor.
// Compact on first show, after those decisions, without altering any signals.
class MousePresentation : public QObject {
    bool ready = false;
public:
    explicit MousePresentation(QDialog *dialog) : QObject(dialog) { dialog->installEventFilter(this); }
protected:
    bool eventFilter(QObject *object, QEvent *event) override {
        if (event->type() == QEvent::Show && !ready) {
            ready = true;
            auto *dialog = static_cast<QDialog *>(object);
            compactDialog(dialog);
            auto *scroll = dialog->findChild<QScrollArea *>("mouseSettingsScroll");
            scroll->widget()->layout()->activate();
            scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            dialog->layout()->setAlignment(Qt::Alignment());
            if (auto *root = qobject_cast<QVBoxLayout *>(dialog->layout())) root->setStretch(0, 1);
            const int bodyHeight = scroll->widget()->layout()->sizeHint().height();
            const int availableHeight = QGuiApplication::primaryScreen()->availableGeometry().height() - 96;
            dialog->resize(736, qMin(bodyHeight + 96, availableHeight));
        }
        return false;
    }
};
inline void polishMouseDialog(QDialog *dialog) { new MousePresentation(dialog); }
}
#endif
