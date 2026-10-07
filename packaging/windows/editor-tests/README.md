# Native Windows editor/model fixture

A separate console executable compiles the real Pad sources, without their
main.cpp or shipped-app test hooks. It uses a private SDL virtual joystick and
isolated temporary settings. The model lives on a worker thread as in Pad.
The normal application target is unchanged. Root CMake is not edited.

Configure this directory with the same Qt5/SDL2 variables as the Windows lane,
then build `pad-editor-tests`. Run `run.ps1` with build/Qt/SDL paths. Five separate
process cases cover Button properties, keyboard/mouse slot assignment and None,
Advanced settings/slots, Stick settings/presets and Mouse speed/mode/geometry.
Button, Advanced, Stick and Mouse dialogs are captured from the real 1024x768
Windows desktop before interaction. Geometry evidence accompanies each PNG.

These are GUI-to-controller-model assertions, not proof that physical controller
input reaches another application's keyboard/mouse. No physical controller,
SendInput delivery, anti-cheat, elevated app or high-DPI claim is made. Native
SendInput is initialized but these cases do not intentionally dispatch mapped
presses. Screenshots require human pixel inspection. A timeout or assertion fails
the CI job. The fixture binary is never included in the portable Pad artifact.
