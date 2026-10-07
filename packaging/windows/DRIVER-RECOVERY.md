# If your keyboard or mouse stops working

The experimental input driver affects Windows, not just Pad. Turning driver mode off does not remove it. If Pad offers to remove the driver during app uninstall, removal still needs administrator approval and a restart.

1. Start Windows in Safe Mode. If you can still use the sign-in screen, hold **Shift** while choosing **Power > Restart**. If neither keyboard nor mouse works, turn the PC on and hold its power button to shut it down when the Windows or maker's logo appears. Do this twice; start it a third time and choose **Advanced options**. Forced shutdowns can lose unsaved work, so use this only when normal restart is not possible.
2. Choose **Troubleshoot > Advanced options > Startup Settings > Restart**, then press **4** or **F4** for Safe Mode. You may need your BitLocker recovery key.
3. In Safe Mode, open **Command Prompt as administrator**. Open the folder where Pad is installed, then run:

   ```cmd
   driver\install-interception.exe /uninstall
   ```

   For Pad's default install location, the full command is:

   ```cmd
   "%LOCALAPPDATA%\Programs\Pad\driver\install-interception.exe" /uninstall
   ```

   If you extracted the portable zip or chose another install location, use its `driver` folder instead. If you sign in or elevate with another account, use the original account's full Pad folder path rather than `%LOCALAPPDATA%`.
4. Restart normally. Leave driver mode off and use Pad's standard input mode.

If the helper is missing, use the `install-interception.exe` you saved before installation, or extract it from the official [Interception v1.0.1 download](https://github.com/oblitum/Interception/releases/tag/v1.0.1). Run it with `/uninstall` as administrator, then restart.

If input still does not work in Safe Mode, or removal fails, return to **Troubleshoot > Advanced options > System Restore** and use a restore point from before the driver install, if available. Otherwise get Windows recovery help. Do not delete `keyboard.sys` or `mouse.sys` by hand or change registry filters.

Before installing, save this note and a copy of `driver\install-interception.exe` outside Pad's folder. Keep your BitLocker recovery key available on another device. Driver removal can affect other apps that use Interception.
