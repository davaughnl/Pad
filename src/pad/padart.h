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
    {570, 183}, {620, 133}, {519, 133}, {570, 83}, {278, 128}, {445, 128}, {362, 128}, {140, 16}, {580, 16},
    {155, 125}, {458, 243}, {155, 125}, {155, 125}, {458, 243}, {458, 243}, {150, 4}, {570, 4},
    {252, 213}, {222, 243}, {252, 273}, {282, 243}};
static const QPointF switchpro[] = {
    {565, 185}, {615, 136}, {513, 136}, {565, 86}, {275, 83}, {446, 83}, {410, 140}, {150, 15}, {560, 15},
    {158, 118}, {452, 223}, {158, 118}, {158, 118}, {452, 223}, {452, 223}, {150, 4}, {555, 4},
    {245, 188}, {205, 236}, {245, 258}, {285, 236}};
static const QPointF stadia[] = {
    {563, 173}, {610, 127}, {516, 127}, {563, 80}, {284, 98}, {437, 98}, {360, 150}, {140, 16}, {580, 16},
    {248, 220}, {469, 220}, {248, 220}, {248, 220}, {469, 220}, {469, 220}, {140, 4}, {580, 4},
    {158, 90}, {120, 127}, {158, 163}, {196, 127}};
static const QPointF bitdo[] = {
    {565, 181}, {617, 136}, {513, 136}, {565, 87}, {327, 130}, {396, 130}, {362, 180}, {160, 16}, {580, 16},
    {265, 233}, {459, 233}, {265, 233}, {265, 233}, {459, 233}, {459, 233}, {150, 4}, {580, 4},
    {147, 98}, {107, 133}, {147, 170}, {187, 133}};
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
