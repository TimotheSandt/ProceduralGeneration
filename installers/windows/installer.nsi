; NSIS script for ProceduralGeneration installer
; This bundles the release build and creates an uninstaller

Name "${PRODUCT_NAME}"
OutFile "${OUTPUT_FILE}"
InstallDir "$PROGRAMFILES\${PRODUCT_NAME}"
InstallDirRegKey HKCU "Software\${PRODUCT_NAME}" ""
RequestExecutionLevel admin

Page directory
Page instfiles

Section "Install"
  SetOutPath "$INSTDIR"
  File "..\..\bin\release\ProceduralGeneration.exe"
  File "..\..\bin\release\*.dll"
  File "..\..\bin\release\*.lib"
  File /r "..\..\bin\release\res\*"
  
  WriteUninstaller "$INSTDIR\Uninstall${PRODUCT_NAME}.exe"
  
  CreateDirectory "$SMPROGRAMS\${PRODUCT_NAME}"
  CreateShortcut "$SMPROGRAMS\${PRODUCT_NAME}\${PRODUCT_NAME}.lnk" "$INSTDIR\${PRODUCT_NAME}.exe"
  CreateShortcut "$SMPROGRAMS\${PRODUCT_NAME}\Uninstall${PRODUCT_NAME}.lnk" "$INSTDIR\Uninstall${PRODUCT_NAME}.exe"
  
  ; Registry for uninstall
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" "DisplayName" "${PRODUCT_NAME}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" "UninstallString" "$INSTDIR\Uninstall${PRODUCT_NAME}.exe"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" "Publisher" "${PUBLISHER}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" "Version" "${VERSION}"
SectionEnd

Section "Uninstall"
  ExecWait 'taskkill /f /im ProceduralGeneration.exe' $0
  
  Delete "$INSTDIR\uninstaller.exe"
  RMDir /r "$INSTDIR"
  
  Delete "$SMPROGRAMS\${PRODUCT_NAME}\${PRODUCT_NAME}.lnk"
  Delete "$SMPROGRAMS\${PRODUCT_NAME}\Uninstall${PRODUCT_NAME}.lnk"
  RMDir "$SMPROGRAMS\${PRODUCT_NAME}"
  
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
  DeleteRegKey /ifempty HKCU "Software\${PRODUCT_NAME}"
SectionEnd
