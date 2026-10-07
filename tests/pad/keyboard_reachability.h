// Shared real-viewport regression, used by Linux and native Windows fixtures.
#include <QScrollArea>
#include <QScrollBar>
#include <QCheckBox>
static void verifyKeyboardReachability(ButtonEditDialog *dialog, JoyButton *button, const QString &output = QString())
{
    dialog->resize(800,520); QTest::qWait(200);
    for (int variant=0;variant<4;++variant) {
        const bool keypad=variant%2;
        // Larger native/style metrics must expand content, not clip the last column.
        if(variant==2)dialog->setStyleSheet(dialog->styleSheet()+QStringLiteral(" VirtualKeyPushButton { min-width: 64px; }"));
        auto *toggle = dialog->findChild<QCheckBox*>("attachNumKeypadCheckbox");
        check(toggle, "Keypad toggle absent");
        if(toggle->isChecked()!=keypad) {toggle->click();QTest::qWait(200);}
        auto *keyboard=dialog->findChild<VirtualKeyboardMouseWidget*>("padVirtualInputs");
        auto *scroll=dialog->findChild<QScrollArea*>("padInputScroll");
        check(keyboard&&scroll,"Keyboard scroll container absent");
        keyboard->setCurrentIndex(0);QTest::qWait(100);
        VirtualKeyPushButton *rightmost=nullptr;int right=-1;int count=0;
        for(auto *key:keyboard->findChildren<VirtualKeyPushButton*>()) {
            if(!key->isVisible())continue;
            ++count;
            int edge=key->mapTo(keyboard,QPoint(key->width(),0)).x();
            if(edge>right){right=edge;rightmost=key;}
            scroll->ensureWidgetVisible(key,8,8);QTest::qWait(10);
            QRect rect(key->mapTo(scroll->viewport(),QPoint()),key->size());
            check(scroll->viewport()->rect().contains(rect),"A virtual key cannot be fully reached through scrolling");
            check(scroll->viewport()->childAt(rect.center())==key,"Viewport hit target is not the visible key");
        }
        check(count>50&&rightmost,"Keyboard key population incomplete");
        scroll->ensureWidgetVisible(rightmost,8,8);QTest::qWait(100);
        QPoint target=rightmost->mapTo(scroll->viewport(),rightmost->rect().center());
        check(scroll->viewport()->rect().contains(target),"Rightmost key outside viewport");
        // Physical-style mouse event, never QPushButton::click() on a clipped key.
        QTest::mouseClick(rightmost,Qt::LeftButton,Qt::NoModifier,rightmost->rect().center());QTest::qWait(100);
        check(button->getAssignedSlots()->size()==1&&button->getAssignedSlots()->first()->getSlotCodeAlias()==rightmost->getQkeyalias(),"Rightmost visible key did not assign its alias");
        if(!output.isEmpty()&&variant<2)check(dialog->grab().save(output+(keypad?"/keyboard-keypad-rightmost.png":"/keyboard-rightmost.png")),"Keyboard screenshot save failed");
        keyboard->getNoneButton()->click();QTest::qWait(100);
        check(button->getAssignedSlots()->isEmpty(),"Keyboard clear failed after rightmost click");
    }
}
