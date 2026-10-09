/* antimicrox Gamepad to KB+M event mapper
 * Copyright (C) 2015 Travis Nickles <nickles.travis@gmail.com>
 * Copyright (C) 2020 Jagoda Górska <juliagoda.pl@protonmail.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "gamecontrollerexample.h"

#include <QDebug>
#include <QPaintEvent>
#include <QPainter>
#include <QPixmap>
#include <QPen>
#include <QTransform>

struct ButtonImagePlacement
{
    int x;
    int y;
    GameControllerExample::ButtonType buttontype;
};

static ButtonImagePlacement buttonLocations[] = {
    {208, 98, GameControllerExample::Button},  // SDL_CONTROLLER_BUTTON_A
    {232, 74, GameControllerExample::Button},  // SDL_CONTROLLER_BUTTON_B
    {184, 74, GameControllerExample::Button},  // SDL_CONTROLLER_BUTTON_X
    {208, 50, GameControllerExample::Button},  // SDL_CONTROLLER_BUTTON_Y
    {112, 68, GameControllerExample::Button},  // SDL_CONTROLLER_BUTTON_BACK
    {160, 68, GameControllerExample::Button},  // SDL_CONTROLLER_BUTTON_START
    {136, 52, GameControllerExample::Button},  // SDL_CONTROLLER_BUTTON_GUIDE
    {66, 22, GameControllerExample::Button},   // SDL_CONTROLLER_BUTTON_LEFTSHOULDER
    {206, 22, GameControllerExample::Button},  // SDL_CONTROLLER_BUTTON_RIGHTSHOULDER
    {64, 74, GameControllerExample::Button},   // SDL_CONTROLLER_BUTTON_LEFTSTICK
    {174, 110, GameControllerExample::Button}, // SDL_CONTROLLER_BUTTON_RIGHTSTICK

    {64, 74, GameControllerExample::AxisX},   // SDL_CONTROLLER_AXIS_LEFTX
    {64, 74, GameControllerExample::AxisY},   // SDL_CONTROLLER_AXIS_LEFTY
    {174, 110, GameControllerExample::AxisX}, // SDL_CONTROLLER_AXIS_RIGHTX
    {174, 110, GameControllerExample::AxisY}, // SDL_CONTROLLER_AXIS_RIGHTY

    {66, 2, GameControllerExample::Button},  // SDL_CONTROLLER_AXIS_TRIGGERLEFT
    {206, 2, GameControllerExample::Button}, // SDL_CONTROLLER_AXIS_TRIGGERRIGHT

    {98, 98, GameControllerExample::Button},  // SDL_CONTROLLER_BUTTON_DPAD_UP
    {86, 110, GameControllerExample::Button},  // dialog index 18: D-pad left
    {98, 122, GameControllerExample::Button},  // dialog index 19: D-pad down
    {110, 110, GameControllerExample::Button}, // SDL_CONTROLLER_BUTTON_DPAD_RIGHT
};

GameControllerExample::GameControllerExample(QWidget *parent)
    : QWidget(parent)
{
    controllerimage = QImage(":/images/controllermap.svg");
    // Monochrome markers: keep the marker alpha, render it in the app's white.
    auto neutral = [](QImage image) {
        image = image.convertToFormat(QImage::Format_ARGB32);
        for (int y = 0; y < image.height(); ++y)
            for (int x = 0; x < image.width(); ++x)
            {
                const QColor pixel = image.pixelColor(x, y);
                if (pixel.alpha() > 0)
                    image.setPixelColor(x, y, QColor(238, 238, 239, pixel.alpha()));
            }
        return image;
    };
    buttonimage = neutral(QImage(":/images/button.png"));
    axisimage = neutral(QImage(":/images/axis.png"));

    QTransform myTransform;
    myTransform.rotate(90);
    rotatedaxisimage = axisimage.transformed(myTransform);
    currentIndex = 0;

    connect(this, &GameControllerExample::indexUpdated, this, [=]() { update(); });
}

// Marker positions on the realistic renders (hero-xbox.png 720x485, hero-ps4.png 720x429).
// Order follows the mapping table: A,B,X,Y,Back,Start,Guide,LB,RB,LS,RS,
// axes LX,LY,RX,RY, LT,RT, D-pad up,left,down,right.
static const QPointF xboxMarks[] = {
    {548, 188}, {598, 140}, {500, 140}, {550, 90}, {305, 138}, {413, 140}, {360, 78}, {175, 28}, {545, 28},
    {165, 130}, {460, 240}, {165, 130}, {165, 130}, {460, 240}, {460, 240}, {150, 8}, {570, 8},
    {262, 212}, {232, 243}, {262, 274}, {293, 243}};
static const QPointF psMarks[] = {
    {578, 175}, {630, 125}, {525, 125}, {578, 75}, {212, 60}, {508, 60}, {360, 215}, {190, 10}, {530, 10},
    {265, 215}, {455, 215}, {265, 215}, {265, 215}, {455, 215}, {455, 215}, {215, 2}, {505, 2},
    {130, 85}, {95, 125}, {130, 165}, {165, 125}};

void GameControllerExample::setDevice(const QString &name)
{
    const QString n = name.toLower();
    playstation = n.contains(QStringLiteral("ps4")) || n.contains(QStringLiteral("ps5")) || n.contains(QStringLiteral("dualshock"))
        || n.contains(QStringLiteral("dualsense")) || n.contains(QStringLiteral("playstation")) || n.contains(QStringLiteral("wireless controller"));
    art = QPixmap(playstation ? QStringLiteral(":/images/hero-ps4.png") : QStringLiteral(":/images/hero-xbox.png"));
    update();
}

void GameControllerExample::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    if (art.isNull()) setDevice(QString());

    QPainter paint(this);
    paint.setRenderHint(QPainter::Antialiasing);
    paint.setRenderHint(QPainter::SmoothPixmapTransform);
    const qreal dpr = devicePixelRatioF();
    const QPixmap scaled = art.scaled(size() * dpr, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    const QSizeF logical = QSizeF(scaled.size()) / dpr;
    const QPointF origin((width() - logical.width()) / 2, (height() - logical.height()) / 2);
    QPixmap out = scaled; out.setDevicePixelRatio(dpr);
    paint.drawPixmap(origin, out);

    const QPointF mark = (playstation ? psMarks : xboxMarks)[qBound(0, currentIndex, MAXBUTTONINDEX)];
    const qreal scale = logical.width() / 720.0;
    const QPointF centre = origin + mark * scale;
    const bool large = currentIndex == 9 || currentIndex == 10 || (currentIndex >= 11 && currentIndex <= 14);
    const qreal radius = (large ? 46 : 22) * scale * 1.6;
    paint.setBrush(QColor(244, 244, 245, 46));
    paint.setPen(QPen(QColor(244, 244, 245, 235), 1.5));
    paint.drawEllipse(centre, radius, radius);
    if (currentIndex >= 11 && currentIndex <= 14)
    {
        const bool horizontal = currentIndex == 11 || currentIndex == 13;
        paint.setPen(QPen(QColor(244, 244, 245, 235), 1.5));
        const qreal reach = radius * 0.7;
        if (horizontal) paint.drawLine(centre - QPointF(reach, 0), centre + QPointF(reach, 0));
        else paint.drawLine(centre - QPointF(0, reach), centre + QPointF(0, reach));
    }
}

void GameControllerExample::setActiveButton(int button)
{
    if (button <= MAXBUTTONINDEX)
    {
        currentIndex = button;
        emit indexUpdated(button);
    }
}
