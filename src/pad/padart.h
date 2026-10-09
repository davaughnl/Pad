#ifndef PADART_H
#define PADART_H

#include <QPointF>
#include <QString>

// Controller renders and the marker position of every mapping row on each render (px, render is 720 wide).
// Marker order: A,B,X,Y,Back,Start,Guide,LB,RB,LS,RS,LX,LY,RX,RY,LT,RT,DpadUp,DpadLeft,DpadDown,DpadRight.
struct PadArt
{
    const char *path;
    const QPointF *marks;
};

namespace PadArtData {
static const QPointF xbox[] = {
    {548, 188}, {598, 140}, {500, 140}, {550, 90}, {305, 138}, {413, 140}, {360, 78}, {175, 28}, {545, 28},
    {165, 130}, {460, 240}, {165, 130}, {165, 130}, {460, 240}, {460, 240}, {150, 8}, {570, 8},
    {262, 212}, {232, 243}, {262, 274}, {293, 243}};
static const QPointF ps4[] = {
    {578, 175}, {630, 125}, {525, 125}, {578, 75}, {212, 60}, {508, 60}, {360, 215}, {190, 10}, {530, 10},
    {265, 215}, {455, 215}, {265, 215}, {265, 215}, {455, 215}, {455, 215}, {215, 2}, {505, 2},
    {130, 85}, {104, 125}, {142, 156}, {165, 125}};
static const QPointF ps5[] = {
    {592, 170}, {634, 128}, {548, 128}, {592, 85}, {187, 50}, {533, 50}, {361, 219}, {139, 14}, {581, 14},
    {241, 219}, {479, 219}, {241, 219}, {241, 219}, {479, 219}, {479, 219}, {128, 4}, {592, 4},
    {130, 92}, {96, 126}, {130, 160}, {162, 126}};
static const QPointF x360[] = {
    {563, 184}, {615, 130}, {511, 130}, {563, 78}, {280, 128}, {438, 128}, {359, 128}, {140, 18}, {585, 18}, {157, 125}, {456, 235}, {157, 125}, {157, 125}, {456, 235}, {456, 235}, {150, 4}, {580, 4}, {252, 203}, {215, 238}, {252, 272}, {290, 238}};
static const QPointF switchpro[] = {
    {561, 188}, {612, 138}, {509, 138}, {561, 88}, {265, 80}, {456, 80}, {414, 147}, {170, 14}, {545, 14}, {155, 126}, {458, 244}, {155, 126}, {155, 126}, {458, 244}, {458, 244}, {170, 4}, {545, 4}, {243, 205}, {208, 240}, {243, 277}, {279, 240}};
static const QPointF stadia[] = {
    {559, 164}, {604, 118}, {513, 118}, {559, 73}, {295, 90}, {427, 90}, {361, 138}, {150, 14}, {545, 12}, {245, 224}, {474, 224}, {245, 224}, {245, 224}, {474, 224}, {474, 224}, {160, 4}, {545, 4}, {161, 78}, {125, 113}, {161, 150}, {197, 113}};
static const QPointF bitdo[] = {
    {553, 184}, {603, 137}, {505, 137}, {555, 89}, {324, 143}, {401, 143}, {362, 143}, {170, 14}, {545, 14}, {270, 240}, {453, 240}, {270, 240}, {270, 240}, {453, 240}, {453, 240}, {170, 4}, {545, 4}, {172, 102}, {135, 138}, {172, 176}, {210, 138}};
} // namespace PadArtData

// SDL_GameControllerType values (stable since SDL 2.0.14) appended to the device name as a tag.
inline QString padArtTag(int sdlType)
{
    switch (sdlType)
    {
    case 4: return QStringLiteral(" padtype-ps4");
    case 5: return QStringLiteral(" padtype-switch");
    case 7: return QStringLiteral(" padtype-ps5");
    case 9: return QStringLiteral(" padtype-stadia");
    default: return QString();
    }
}

inline PadArt padArtFor(const QString &deviceName)
{
    const QString n = deviceName.toLower();
    auto has = [&n](const char *s) { return n.contains(QLatin1String(s)); };
    if (has("dualsense") || has("ps5") || has("padtype-ps5"))
        return {":/images/hero-ps5.png", PadArtData::ps5};
    if (has("dualshock") || has("ps4") || has("playstation") || (has("wireless controller") && !has("xbox")) || has("padtype-ps4"))
        return {":/images/hero-ps4.png", PadArtData::ps4};
    if (has("switch") || has("nintendo") || has("pro controller"))
        return {":/images/hero-switch.png", PadArtData::switchpro};
    if (has("stadia"))
        return {":/images/hero-stadia.png", PadArtData::stadia};
    if (has("8bitdo") || has("8bit"))
        return {":/images/hero-8bitdo.png", PadArtData::bitdo};
    if (has("360"))
        return {":/images/hero-x360.png", PadArtData::x360};
    return {":/images/hero-xbox.png", PadArtData::xbox};
}

#endif // PADART_H
