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
#include <QTransform>

struct ButtonImagePlacement
{
    int x;
    int y;
    GameControllerExample::ButtonType buttontype;
};

static ButtonImagePlacement buttonLocations[] = {
    {221, 98, GameControllerExample::Button},  // SDL_CONTROLLER_BUTTON_A
    {248, 77, GameControllerExample::Button},  // SDL_CONTROLLER_BUTTON_B
    {196, 77, GameControllerExample::Button},  // SDL_CONTROLLER_BUTTON_X
    {223, 59, GameControllerExample::Button},  // SDL_CONTROLLER_BUTTON_Y
    {102, 77, GameControllerExample::Button},  // SDL_CONTROLLER_BUTTON_BACK
    {169, 77, GameControllerExample::Button},  // SDL_CONTROLLER_BUTTON_START
    {137, 77, GameControllerExample::Button},  // SDL_CONTROLLER_BUTTON_GUIDE
    {45, 23, GameControllerExample::Button},   // SDL_CONTROLLER_BUTTON_LEFTSHOULDER
    {232, 21, GameControllerExample::Button},  // SDL_CONTROLLER_BUTTON_RIGHTSHOULDER
    {44, 90, GameControllerExample::Button},   // SDL_CONTROLLER_BUTTON_LEFTSTICK
    {179, 123, GameControllerExample::Button}, // SDL_CONTROLLER_BUTTON_RIGHTSTICK

    {44, 90, GameControllerExample::AxisX},   // SDL_CONTROLLER_AXIS_LEFTX
    {44, 90, GameControllerExample::AxisY},   // SDL_CONTROLLER_AXIS_LEFTY
    {179, 123, GameControllerExample::AxisX}, // SDL_CONTROLLER_AXIS_RIGHTX
    {179, 123, GameControllerExample::AxisY}, // SDL_CONTROLLER_AXIS_RIGHTY

    {53, 0, GameControllerExample::Button},  // SDL_CONTROLLER_AXIS_TRIGGERLEFT
    {220, 0, GameControllerExample::Button}, // SDL_CONTROLLER_AXIS_TRIGGERRIGHT

    {94, 109, GameControllerExample::Button},  // SDL_CONTROLLER_BUTTON_DPAD_UP
    {94, 135, GameControllerExample::Button},  // SDL_CONTROLLER_BUTTON_DPAD_DOWN
    {80, 122, GameControllerExample::Button},  // SDL_CONTROLLER_BUTTON_DPAD_LEFT
    {108, 122, GameControllerExample::Button}, // SDL_CONTROLLER_BUTTON_DPAD_RIGHT
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

void GameControllerExample::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter paint(this);
    // 300x186 is hardcoded size of original image
    paint.drawImage(QRect(0, 0, 300, 186), controllerimage);
    ButtonImagePlacement current = buttonLocations[currentIndex];

    paint.setOpacity(0.85);

    switch (current.buttontype)
    {
    case Button:
        paint.drawImage(QRect(current.x, current.y, buttonimage.width(), buttonimage.height()), buttonimage);
        break;

    case AxisX:
        paint.drawImage(QRect(current.x, current.y, axisimage.width(), axisimage.height()), axisimage);
        break;

    case AxisY:
        paint.drawImage(QRect(current.x, current.y, rotatedaxisimage.width(), rotatedaxisimage.height()), rotatedaxisimage);
        break;
    }

    paint.setOpacity(1.0);
}

void GameControllerExample::setActiveButton(int button)
{
    if (button <= MAXBUTTONINDEX)
    {
        currentIndex = button;
        emit indexUpdated(button);
    }
}
