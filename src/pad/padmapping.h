// SPDX-License-Identifier: GPL-3.0-or-later
// Presentation adapter: the native mapping widgets remain alive and own every
// editor, context menu and controller signal. No mapping state is duplicated.
#ifndef PADMAPPING_H
#define PADMAPPING_H
#include <QLineEdit>
#include "joybuttonwidget.h"
#include "joyaxiswidget.h"
#include "joycontrolstickbuttonpushbutton.h"
#include "joycontrolstickpushbutton.h"
#include "dpadpushbutton.h"
#include "joysensorbuttonpushbutton.h"
#include "joysensorpushbutton.h"
#include "joybuttontypes/joybutton.h"
#include "joybuttontypes/joycontrolstickbutton.h"
#include "joybuttontypes/joysensorbutton.h"
#include "joycontrolstick.h"
#include "joyaxis.h"
#include "joydpad.h"
#include "joysensor.h"
#include <algorithm>
#include <QStyle>
#include <QStyleOptionButton>
#include <QStylePainter>
#include <QGridLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPointer>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace PadUi {
class MappingSurface : public QWidget
{
    class ActionButton : public QPushButton
    {
    protected:
        void paintEvent(QPaintEvent *) override
        {
            QStyleOptionButton option;
            initStyleOption(&option);
            const int available = qMax(0, width() - 48);
            option.text = fontMetrics().elidedText(text(), Qt::ElideRight, available);
            QStylePainter painter(this);
            painter.drawControl(QStyle::CE_PushButton, option);
        }
    public:
        explicit ActionButton(QWidget *parent) : QPushButton(parent) {}
    };
    struct Row { QTreeWidgetItem *item; QPointer<FlashButtonWidget> source; QPushButton *action; QLabel *behavior; JoyButton *button; };
    QTreeWidget *table;
    QList<Row> rows;
    QTimer *refreshTimer = nullptr;
    bool retired = false;
    static JoyButton *model(FlashButtonWidget *source)
    {
        if (auto *b = qobject_cast<JoyButtonWidget *>(source)) return b->getJoyButton();
        if (auto *b = qobject_cast<JoyControlStickButtonPushButton *>(source)) return b->getButton();
        if (auto *b = qobject_cast<JoySensorButtonPushButton *>(source)) return b->getButton();
        return nullptr;
    }
    static QString stickLabel(JoyControlStick *stick, bool names)
    {
        if (names && !stick->getStickName().isEmpty()) return stick->getStickName();
        if (!stick->getDefaultStickName().isEmpty()) return stick->getDefaultStickName();
        return tr("Stick %1").arg(stick->getRealJoyIndex());
    }
    void fitColumns()
    {
        const int width = qMax(0, table->viewport()->width());
        table->setColumnWidth(0, width * 38 / 100);
        table->setColumnWidth(1, width * 42 / 100);
    }
    void sync()
    {
        if (retired) return; // Queued show callbacks can run after model retirement.
        fitColumns();
        for (const Row &row : rows) {
            if (!row.source) continue;
            auto *source = row.source.data();
            row.item->setHidden(source->isHidden()); // Direction-mode visibility, not ancestor visibility.
            QString input, action, behavior;
            bool flashing = source->isButtonFlashing();
            if (row.button) {
                if (auto *direction = qobject_cast<JoyControlStickButton *>(row.button)) {
                    input = stickLabel(direction->getStick(), source->isDisplayingNames()) + QStringLiteral(" / ") + direction->getDirectionName();
                } else input = row.button->getPartialName(false, source->isDisplayingNames());
                action = source->isDisplayingNames() && !row.button->getActionName().isEmpty()
                    ? row.button->getActionName() : row.button->getCalculatedActiveZoneSummary();
                if (row.button->getAssignedSlots()->isEmpty()) action = tr("Unassigned");
                QStringList modes;
                if (row.button->getToggleState()) modes << tr("Toggle");
                if (row.button->isUsingTurbo()) modes << tr("Turbo");
                if (row.button->getChangeSetCondition() != JoyButton::SetChangeDisabled)
                    modes << tr("Set %1").arg(row.button->getSetSelection() + 1);
                behavior = modes.isEmpty() ? tr("Standard") : modes.join(QStringLiteral(" + "));
            } else if (auto *axis = qobject_cast<JoyAxisWidget *>(source)) {
                input = axis->getAxis()->getPartialName(false, source->isDisplayingNames());
                action = axis->getAxis()->getName(false, source->isDisplayingNames()).mid(input.length()).trimmed();
                if (action.startsWith(':')) action.remove(0, 1);
                action = action.trimmed();
                if (axis->getAxis()->getNAxisButton()->getAssignedSlots()->isEmpty() &&
                    axis->getAxis()->getPAxisButton()->getAssignedSlots()->isEmpty()) action = tr("Unassigned");
                behavior = tr("Analog");
            } else {
                if (auto *stick = qobject_cast<JoyControlStickPushButton *>(source))
                    input = stickLabel(stick->getStick(), source->isDisplayingNames());
                else if (auto *dpad = qobject_cast<DPadPushButton *>(source)) input = dpad->getDPad()->getName();
                else if (auto *sensor = qobject_cast<JoySensorPushButton *>(source))
                    input = sensor->getSensor()->getPartialName(false, source->isDisplayingNames());
                else input = source->text();
                action = tr("Configure directions"); behavior = tr("Directional");
            }
            row.item->setText(0, input);
            row.item->setToolTip(0, input);
            row.action->setText(action.replace('&', QStringLiteral("&&")));
            row.action->setToolTip(action);
            row.action->setEnabled(source->isEnabled());
            row.behavior->setText(behavior);
            if (row.action->property("padActive").toBool() != flashing) {
                row.action->setProperty("padActive", flashing);
                row.action->style()->unpolish(row.action); row.action->style()->polish(row.action);
            }
        }
    }
protected:
    void resizeEvent(QResizeEvent *event) override
    {
        QWidget::resizeEvent(event);
        fitColumns();
    }
    void showEvent(QShowEvent *event) override
    {
        QWidget::showEvent(event);
        QTimer::singleShot(0, this, [this]() { sync(); });
    }
public:
    void retire()
    {
        if (retired) return;
        retired = true;
        refreshTimer->stop();
        hide();
        setEnabled(false);
        for (const Row &row : rows) {
            QObject::disconnect(row.action, nullptr, nullptr, nullptr);
        }
        rows.clear();
    }
    explicit MappingSurface(QGridLayout *grid) : QWidget(grid->parentWidget())
    {
        setObjectName(QStringLiteral("padMappingSurface"));
        auto *layout = new QVBoxLayout(this); layout->setContentsMargins(0, 0, 0, 0); layout->setSpacing(0);
        auto *storage = new QWidget(this); auto *native = new QGridLayout(storage);
        QList<QWidget *> roots;
        while (auto *item = grid->takeAt(0)) {
            if (item->widget()) { roots << item->widget(); native->addWidget(item->widget()); }
            delete item;
        }
        storage->hide();
        table = new QTreeWidget(this); table->setObjectName(QStringLiteral("padMappingTable"));
        table->setHeaderLabels({tr("Controller input"), tr("Assigned action"), tr("Behavior")});
        table->setRootIsDecorated(false); table->setIndentation(0); table->setUniformRowHeights(true);
        table->setSelectionMode(QAbstractItemView::SingleSelection); table->setAlternatingRowColors(false);
        table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        table->header()->setSectionResizeMode(QHeaderView::Fixed);
        table->header()->setStretchLastSection(true); table->header()->setMinimumSectionSize(64);
        auto *search = new QLineEdit(this); search->setObjectName(QStringLiteral("padSearch"));
        search->setPlaceholderText(tr("Search inputs and actions")); search->setClearButtonEnabled(true);
        search->addAction(QIcon(QStringLiteral(":/pad/icons/search.svg")), QLineEdit::LeadingPosition);
        layout->addWidget(search); layout->addSpacing(8);
        QObject::connect(search, &QLineEdit::textChanged, this, [this](const QString &needle) {
            for (const Row &row : rows)
                row.item->setHidden(!needle.isEmpty() && !row.item->text(0).contains(needle, Qt::CaseInsensitive)
                                    && !row.action->text().contains(needle, Qt::CaseInsensitive));
        });
        layout->addWidget(table);
        for (QWidget *root : roots) {
            QList<FlashButtonWidget *> sources = root->findChildren<FlashButtonWidget *>();
            if (auto *button = qobject_cast<FlashButtonWidget *>(root)) sources.prepend(button);
            // Put the group editor before its direction rows, regardless of native grid position.
            std::stable_sort(sources.begin(), sources.end(), [](FlashButtonWidget *a, FlashButtonWidget *b) {
                return model(a) == nullptr && model(b) != nullptr;
            });
            for (auto *source : sources) {
                auto *item = new QTreeWidgetItem(table); item->setSizeHint(0, QSize(0, 40));
                const bool group = model(source) == nullptr;
                item->setIcon(0, QIcon(group ? ":/pad/icons/move.svg" : ":/pad/icons/gamepad.svg"));
                auto *cell = new QWidget(table); auto *cellLayout = new QVBoxLayout(cell);
                cellLayout->setContentsMargins(0, 6, 14, 6);
                auto *action = new ActionButton(cell); cellLayout->addWidget(action); action->setFixedHeight(26); action->setProperty("padMappingAction", true);
                action->setMinimumHeight(0); action->setIcon(QIcon(group ? ":/pad/icons/sliders.svg" : ":/pad/icons/keyboard.svg")); action->setIconSize(QSize(14, 14));
                action->setContextMenuPolicy(Qt::CustomContextMenu);
                QObject::connect(action, &QPushButton::clicked, source, &QPushButton::click);
                QObject::connect(action, &QWidget::customContextMenuRequested, source, [source, action](const QPoint &point) {
                    const QPoint nativePoint = source->mapFromGlobal(action->mapToGlobal(point));
                    QMetaObject::invokeMethod(source, "customContextMenuRequested", Q_ARG(QPoint, nativePoint));
                });
                auto *behavior = new QLabel(table); behavior->setObjectName(QStringLiteral("padBehavior"));
                table->setItemWidget(item, 1, cell); table->setItemWidget(item, 2, behavior);
                rows.append({item, source, action, behavior, model(source)});
            }
        }
        QObject::connect(table, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem *item, int) {
            for (const Row &row : rows) if (row.item == item) { row.action->click(); break; }
        });
        if (rows.isEmpty()) {
            auto *item = new QTreeWidgetItem(table); item->setText(0, tr("No assigned inputs"));
            item->setToolTip(0, tr("Use Quick Set or disable Hide empty buttons to add mappings."));
        }
        refreshTimer = new QTimer(this); refreshTimer->setInterval(100);
        QObject::connect(refreshTimer, &QTimer::timeout, this, [this]() { if (isVisible()) sync(); });
        sync(); refreshTimer->start();
        grid->setContentsMargins(0, 0, 0, 0); grid->addWidget(this, 0, 0);
    }
};
inline void retireMappingTable(QWidget *widget)
{
    if (auto *surface = dynamic_cast<MappingSurface *>(widget)) surface->retire();
}
inline void makeMappingTable(QGridLayout *grid) { new MappingSurface(grid); }
}
#endif
