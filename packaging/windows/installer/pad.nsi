; Pad per-user installer. Built by packaging/windows/make-installer.ps1.
; Defines passed on the command line: VERSION, STAGE, OUTFILE, IMAGES.
Unicode true
!include "MUI2.nsh"
!include "FileFunc.nsh"

!ifndef VERSION
  !error "VERSION is required"
!endif
!ifndef STAGE
  !error "STAGE is required"
!endif
!ifndef OUTFILE
  !error "OUTFILE is required"
!endif
!ifndef IMAGES
  !error "IMAGES is required"
!endif

!define UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\Pad"

Name "Pad"
OutFile "${OUTFILE}"
RequestExecutionLevel user
InstallDir "$LOCALAPPDATA\Programs\Pad"
InstallDirRegKey HKCU "Software\Pad" "InstallDir"
SetCompressor /SOLID lzma
BrandingText "Pad ${VERSION}"

VIProductVersion "${VERSION}.0"
VIAddVersionKey "ProductName" "Pad"
VIAddVersionKey "FileDescription" "Pad installer"
VIAddVersionKey "FileVersion" "${VERSION}"
VIAddVersionKey "ProductVersion" "${VERSION}"

!define MUI_ICON "${IMAGES}\pad.ico"
!define MUI_UNICON "${IMAGES}\pad.ico"
!define MUI_ABORTWARNING
!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_BITMAP "${IMAGES}\pad-installer-header-150x57.bmp"
!define MUI_HEADERIMAGE_RIGHT
!define MUI_WELCOMEFINISHPAGE_BITMAP "${IMAGES}\pad-installer-welcome-164x314.bmp"
!define MUI_UNWELCOMEFINISHPAGE_BITMAP "${IMAGES}\pad-installer-welcome-164x314.bmp"
!define MUI_FINISHPAGE_RUN "$INSTDIR\bin\pad.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Launch Pad"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

Function .onInit
  ; Silent upgrades are started by the running app, which quits right after.
  ; Give it a moment, then make sure no Pad process holds the files open.
  IfSilent 0 +4
    Sleep 2000
    nsExec::Exec 'taskkill /F /IM pad.exe'
    Pop $0
FunctionEnd

Section "Pad" SecApp
  SectionIn RO
  SetOutPath "$INSTDIR"
  File /r "${STAGE}\*.*"
  WriteUninstaller "$INSTDIR\uninstall.exe"
  CreateDirectory "$SMPROGRAMS\Pad"
  CreateShortcut "$SMPROGRAMS\Pad\Pad.lnk" "$INSTDIR\bin\pad.exe" "" "$INSTDIR\bin\pad.exe" 0
  CreateShortcut "$SMPROGRAMS\Pad\Uninstall Pad.lnk" "$INSTDIR\uninstall.exe"
  WriteRegStr HKCU "Software\Pad" "InstallDir" "$INSTDIR"
  WriteRegStr HKCU "${UNINST_KEY}" "DisplayName" "Pad"
  WriteRegStr HKCU "${UNINST_KEY}" "DisplayVersion" "${VERSION}"
  WriteRegStr HKCU "${UNINST_KEY}" "Publisher" "Pad"
  WriteRegStr HKCU "${UNINST_KEY}" "DisplayIcon" "$INSTDIR\bin\pad.exe"
  WriteRegStr HKCU "${UNINST_KEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKCU "${UNINST_KEY}" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegStr HKCU "${UNINST_KEY}" "QuietUninstallString" '"$INSTDIR\uninstall.exe" /S'
  WriteRegDWORD HKCU "${UNINST_KEY}" "NoModify" 1
  WriteRegDWORD HKCU "${UNINST_KEY}" "NoRepair" 1
  ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
  IntFmt $0 "0x%08X" $0
  WriteRegDWORD HKCU "${UNINST_KEY}" "EstimatedSize" "$0"
SectionEnd

Section /o "Desktop shortcut" SecDesktop
  CreateShortcut "$DESKTOP\Pad.lnk" "$INSTDIR\bin\pad.exe" "" "$INSTDIR\bin\pad.exe" 0
SectionEnd

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SecApp} "Pad and its required runtime files."
  !insertmacro MUI_DESCRIPTION_TEXT ${SecDesktop} "Add a Pad shortcut to the desktop."
!insertmacro MUI_FUNCTION_DESCRIPTION_END

Function .onInstSuccess
  ; Silent (in-app) updates relaunch Pad; the interactive finish page has its own checkbox.
  IfSilent 0 +2
    Exec '"$INSTDIR\bin\pad.exe"'
FunctionEnd

Section "Uninstall"
  nsExec::Exec 'taskkill /F /IM pad.exe'
  Pop $0
  ; Remove only what the installer put down. Profiles and settings stay.
  Delete "$DESKTOP\Pad.lnk"
  Delete "$SMPROGRAMS\Pad\Pad.lnk"
  Delete "$SMPROGRAMS\Pad\Uninstall Pad.lnk"
  RMDir "$SMPROGRAMS\Pad"
  RMDir /r "$INSTDIR\licenses"
  RMDir /r "$INSTDIR\driver"
  RMDir /r "$INSTDIR\share"
  Delete "$INSTDIR\README.txt"
  Delete "$INSTDIR\build-info.json"
  Delete "$INSTDIR\bin\*.dll"
  Delete "$INSTDIR\bin\pad.exe"
  Delete "$INSTDIR\bin\qt.conf"
  RMDir /r "$INSTDIR\bin\platforms"
  RMDir /r "$INSTDIR\bin\imageformats"
  RMDir /r "$INSTDIR\bin\styles"
  RMDir /r "$INSTDIR\bin\iconengines"
  RMDir /r "$INSTDIR\bin\translations"
  RMDir /r "$INSTDIR\bin\share"
  RMDir "$INSTDIR\bin"
  Delete "$INSTDIR\uninstall.exe"
  RMDir "$INSTDIR"
  DeleteRegKey HKCU "${UNINST_KEY}"
  DeleteRegKey HKCU "Software\Pad"
SectionEnd
