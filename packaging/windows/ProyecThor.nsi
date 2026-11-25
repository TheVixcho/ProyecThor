; =============================================================================
; ProyecThor - Instalador Moderno y Libre para Windows (NSIS)
; Licencia 100% gratuita para software libre y comercial de paga (zlib/libpng)
; Desinstalación limpia garantizada: detecta y elimina versiones antiguas
; (NSIS, Inno Setup o MSI) para evitar duplicados en 'Agregar o Quitar Programas'.
; =============================================================================

Unicode True
SetCompressor /SOLID lzma

!include "MUI2.nsh"
!include "FileFunc.nsh"
!include "LogicLib.nsh"

; Definiciones del producto
!define PRODUCT_NAME "ProyecThor"
!define PRODUCT_PUBLISHER "vixcho"
!define PRODUCT_WEB_SITE "https://github.com/TheVixcho/ProyecThor"

!ifndef VERSION
  !define VERSION "1.2.0"
!endif

!ifndef BUILD_DIR
  !define BUILD_DIR "../../build-win"
!endif

!ifndef OUT_DIR
  !define OUT_DIR "dist"
!endif

!ifndef OUT_FILE
  !define OUT_FILE "ProyecThor_Setup.exe"
!endif

Name "${PRODUCT_NAME} ${VERSION}"
OutFile "${OUT_DIR}/${OUT_FILE}"

; Metadatos del ejecutable del instalador
VIProductVersion "${VERSION}.0"
VIAddVersionKey "ProductName" "${PRODUCT_NAME}"
VIAddVersionKey "CompanyName" "${PRODUCT_PUBLISHER}"
VIAddVersionKey "LegalCopyright" "Copyright (c) 2026 ProyecThor Project / TheVixcho"
VIAddVersionKey "FileDescription" "ProyecThor Setup"
VIAddVersionKey "FileVersion" "${VERSION}.0"
VIAddVersionKey "ProductVersion" "${VERSION}"

; Instalación per-user limpia en AppData local (sin necesidad de privilegios de Administrador)
InstallDir "$LOCALAPPDATA\Programs\ProyecThor"
InstallDirRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ProyecThor" "InstallLocation"
RequestExecutionLevel user

; Iconos del instalador y desinstalador
!define MUI_ICON "..\..\proyecthor.ico"
!define MUI_UNICON "..\..\proyecthor.ico"

; Configuración de la interfaz moderna
!define MUI_ABORTWARNING

; Páginas del instalador
!insertmacro MUI_PAGE_LICENSE "LICENSE.rtf"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES

; Página final con opción de ejecutar la aplicación
!define MUI_FINISHPAGE_RUN "$INSTDIR\ProyecThorSlint.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Ejecutar ProyecThor ahora"
!insertmacro MUI_PAGE_FINISH

; Páginas del desinstalador
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

; Idiomas soportados
!insertmacro MUI_LANGUAGE "Spanish"
!insertmacro MUI_LANGUAGE "English"

; -----------------------------------------------------------------------------
; Función de limpieza profunda previa a la instalación (.onInit)
; Desinstala cualquier versión anterior de forma desatendida y purga claves
; residuales para que Windows NO duplique 'ProyecThor' en la lista de aplicaciones.
; -----------------------------------------------------------------------------
Function .onInit
    ; 0. Cerrar instancias en ejecución de ProyecThor antes de cualquier acción
    nsExec::Exec 'cmd.exe /c taskkill /f /im ProyecThorSlint.exe >nul 2>&1'
    nsExec::Exec 'cmd.exe /c taskkill /f /im ProyecThor.exe >nul 2>&1'

    ; 1. Detectar y desinstalar versión previa de NSIS si existía
    ReadRegStr $0 HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ProyecThor" "UninstallString"
    ${If} $0 == ""
        ReadRegStr $0 HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\ProyecThor" "UninstallString"
    ${EndIf}
    ${If} $0 != ""
        ReadRegStr $1 HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ProyecThor" "InstallLocation"
        ${If} $1 == ""
            ReadRegStr $1 HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\ProyecThor" "InstallLocation"
        ${EndIf}
        ${If} $1 == ""
            StrCpy $1 "$INSTDIR"
        ${EndIf}
        ; Ejecutar desinstalador silencioso propio de NSIS en modo sincrónico (_?=)
        ExecWait '$0 /S _?=$1'
        ; Eliminar registro anterior por seguridad
        DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ProyecThor"
        DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\ProyecThor"
    ${EndIf}

    ; 2. Detectar y desinstalar versión previa generada por Inno Setup ({66A0344F-F850-49CB-9F63-488AB7B3DBCD}_is1)
    ReadRegStr $2 HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\{66A0344F-F850-49CB-9F63-488AB7B3DBCD}_is1" "UninstallString"
    ${If} $2 == ""
        ReadRegStr $2 HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\{66A0344F-F850-49CB-9F63-488AB7B3DBCD}_is1" "UninstallString"
    ${EndIf}
    ${If} $2 != ""
        ; Ejecutar desinstalador silencioso de Inno Setup
        ExecWait '$2 /VERYSILENT /SUPPRESSMSGBOXES /NORESTART'
    ${EndIf}
    ; Purgar claves de Inno Setup para evitar cualquier duplicado en 'Configuración > Aplicaciones instaladas'
    DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\{66A0344F-F850-49CB-9F63-488AB7B3DBCD}_is1"
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\{66A0344F-F850-49CB-9F63-488AB7B3DBCD}_is1"
    DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ProyecThor_is1"
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\ProyecThor_is1"

    ; 3. Detectar instalaciones MSI antiguas de WiX ({66A0344F-F850-49CB-9F63-488AB7B3DBCD})
    ReadRegStr $3 HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\{66A0344F-F850-49CB-9F63-488AB7B3DBCD}" "UninstallString"
    ${If} $3 == ""
        ReadRegStr $3 HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\{66A0344F-F850-49CB-9F63-488AB7B3DBCD}" "UninstallString"
    ${EndIf}
    ${If} $3 != ""
        ExecWait 'msiexec.exe /x {66A0344F-F850-49CB-9F63-488AB7B3DBCD} /qn /norestart'
        DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\{66A0344F-F850-49CB-9F63-488AB7B3DBCD}"
        DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\{66A0344F-F850-49CB-9F63-488AB7B3DBCD}"
    ${EndIf}
FunctionEnd

; -----------------------------------------------------------------------------
; Sección Principal de Instalación
; -----------------------------------------------------------------------------
Section "ProyecThor" SEC01
    SetOutPath "$INSTDIR"

    ; Asegurar que no hay procesos corriendo
    DetailPrint "Verificando procesos..."
    nsExec::Exec 'cmd.exe /c taskkill /f /im ProyecThorSlint.exe >nul 2>&1'
    nsExec::Exec 'cmd.exe /c taskkill /f /im ProyecThor.exe >nul 2>&1'
    Sleep 500

    ; Limpiar residuos de versiones anteriores antes de copiar archivos nuevos
    Delete "$INSTDIR\unins000.*"
    Delete "$INSTDIR\ProyecThor.old.*"
    Delete "$INSTDIR\ProyecThorSlint.exe"
    Delete "$INSTDIR\ProyecThor.exe"
    Delete "$INSTDIR\Uninstall.exe"
    Delete "$INSTDIR\*.dll"

    ; Copiar ejecutables y dependencias binarias runtime
    SetOverwrite on
    File "${BUILD_DIR}\ProyecThorSlint.exe"
    File /nonfatal "${BUILD_DIR}\ProyecThor.exe"
    File "${BUILD_DIR}\*.dll"
    File /nonfatal "${BUILD_DIR}\ffmpeg.exe"
    File /nonfatal "${BUILD_DIR}\yt-dlp.exe"
    File /nonfatal "${BUILD_DIR}\catalog.json"
    File /nonfatal "${BUILD_DIR}\munin_catalog.json"
    File /nonfatal "${BUILD_DIR}\proyecthor.ico"
    File /nonfatal "${BUILD_DIR}\proyecthor.png"
    File /nonfatal "${BUILD_DIR}\proyecthor_ui.ini"
    File /nonfatal "${BUILD_DIR}\splash_bg*.jpg"
    File /nonfatal "${BUILD_DIR}\splash_bg*.png"
    File /nonfatal "${BUILD_DIR}\bg_splash3.*"

    ; Copiar subdirectorios de recursos
    SetOutPath "$INSTDIR\bin"
    File /nonfatal /r /x CMakeLists.txt /x *.ninja /x .ninja* /x *.o "${BUILD_DIR}\bin\*.*"

    SetOutPath "$INSTDIR\lua"
    File /nonfatal /r /x CMakeLists.txt /x *.ninja /x .ninja* /x *.o "${BUILD_DIR}\lua\*.*"

    SetOutPath "$INSTDIR\plugins"
    File /nonfatal /r /x CMakeLists.txt /x *.ninja /x .ninja* /x *.o "${BUILD_DIR}\plugins\*.*"

    SetOutPath "$INSTDIR\shaders"
    File /nonfatal /r /x CMakeLists.txt /x *.ninja /x .ninja* /x *.o "${BUILD_DIR}\shaders\*.*"

    ; Crear desinstalador propio y limpio de NSIS
    SetOutPath "$INSTDIR"
    WriteUninstaller "$INSTDIR\Uninstall.exe"

    ; Crear accesos directos (ProyecThorSlint como app principal + modo clásico opcional)
    CreateDirectory "$SMPROGRAMS\ProyecThor"
    CreateShortcut "$SMPROGRAMS\ProyecThor\ProyecThor.lnk" "$INSTDIR\ProyecThorSlint.exe" "" "$INSTDIR\proyecthor.ico" 0
    IfFileExists "$INSTDIR\ProyecThor.exe" 0 +2
        CreateShortcut "$SMPROGRAMS\ProyecThor\ProyecThor (Modo Clásico).lnk" "$INSTDIR\ProyecThor.exe" "" "$INSTDIR\proyecthor.ico" 0
    CreateShortcut "$SMPROGRAMS\ProyecThor\Desinstalar ProyecThor.lnk" "$INSTDIR\Uninstall.exe" "" "$INSTDIR\Uninstall.exe" 0
    CreateShortcut "$DESKTOP\ProyecThor.lnk" "$INSTDIR\ProyecThorSlint.exe" "" "$INSTDIR\proyecthor.ico" 0

    ; Registro único y exclusivo en Agregar o Quitar Programas (HKCU)
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ProyecThor" "DisplayName" "ProyecThor"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ProyecThor" "DisplayVersion" "${VERSION}"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ProyecThor" "Publisher" "${PRODUCT_PUBLISHER}"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ProyecThor" "URLInfoAbout" "${PRODUCT_WEB_SITE}"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ProyecThor" "DisplayIcon" "$INSTDIR\proyecthor.ico"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ProyecThor" "UninstallString" '"$INSTDIR\Uninstall.exe"'
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ProyecThor" "QuietUninstallString" '"$INSTDIR\Uninstall.exe" /S'
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ProyecThor" "InstallLocation" "$INSTDIR"
    WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ProyecThor" "NoModify" 1
    WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ProyecThor" "NoRepair" 1

    ; Registro de tipos de archivos soportados en "Abrir con..."
    WriteRegStr HKCU "Software\Classes\Applications\ProyecThor.exe" "FriendlyAppName" "ProyecThor"
    WriteRegStr HKCU "Software\Classes\Applications\ProyecThor.exe\shell\open\command" "" '"$INSTDIR\ProyecThorSlint.exe" "%1"'
    WriteRegStr HKCU "Software\Classes\Applications\ProyecThor.exe\SupportedTypes" ".mp3" ""
    WriteRegStr HKCU "Software\Classes\Applications\ProyecThor.exe\SupportedTypes" ".flac" ""
    WriteRegStr HKCU "Software\Classes\Applications\ProyecThor.exe\SupportedTypes" ".wav" ""
    WriteRegStr HKCU "Software\Classes\Applications\ProyecThor.exe\SupportedTypes" ".ogg" ""
    WriteRegStr HKCU "Software\Classes\Applications\ProyecThor.exe\SupportedTypes" ".aac" ""
    WriteRegStr HKCU "Software\Classes\Applications\ProyecThor.exe\SupportedTypes" ".m4a" ""
    WriteRegStr HKCU "Software\Classes\Applications\ProyecThor.exe\SupportedTypes" ".wma" ""
    WriteRegStr HKCU "Software\Classes\Applications\ProyecThor.exe\SupportedTypes" ".opus" ""
    WriteRegStr HKCU "Software\Classes\Applications\ProyecThor.exe\SupportedTypes" ".aiff" ""
    WriteRegStr HKCU "Software\Classes\Applications\ProyecThor.exe\SupportedTypes" ".mp4" ""
    WriteRegStr HKCU "Software\Classes\Applications\ProyecThor.exe\SupportedTypes" ".mkv" ""
    WriteRegStr HKCU "Software\Classes\Applications\ProyecThor.exe\SupportedTypes" ".avi" ""
    WriteRegStr HKCU "Software\Classes\Applications\ProyecThor.exe\SupportedTypes" ".mov" ""
    WriteRegStr HKCU "Software\Classes\Applications\ProyecThor.exe\SupportedTypes" ".webm" ""
    WriteRegStr HKCU "Software\Classes\Applications\ProyecThor.exe\SupportedTypes" ".jpg" ""
    WriteRegStr HKCU "Software\Classes\Applications\ProyecThor.exe\SupportedTypes" ".jpeg" ""
    WriteRegStr HKCU "Software\Classes\Applications\ProyecThor.exe\SupportedTypes" ".png" ""
    WriteRegStr HKCU "Software\Classes\Applications\ProyecThor.exe\SupportedTypes" ".bmp" ""
    WriteRegStr HKCU "Software\Classes\Applications\ProyecThor.exe\SupportedTypes" ".webp" ""
    WriteRegStr HKCU "Software\Classes\Applications\ProyecThor.exe\SupportedTypes" ".txt" ""
    WriteRegStr HKCU "Software\Classes\Applications\ProyecThor.exe\SupportedTypes" ".prosong" ""

    ; Menú contextual clic derecho directo en Windows Explorer: "Abrir con ProyecThor"
    WriteRegStr HKCU "Software\Classes\*\shell\ProyecThor" "" "Abrir con ProyecThor"
    WriteRegStr HKCU "Software\Classes\*\shell\ProyecThor" "Icon" '"$INSTDIR\proyecthor.ico"'
    WriteRegStr HKCU "Software\Classes\*\shell\ProyecThor\command" "" '"$INSTDIR\ProyecThorSlint.exe" "%1"'
SectionEnd

; -----------------------------------------------------------------------------
; Sección de Desinstalación Limpia de NSIS
; -----------------------------------------------------------------------------
Section "Uninstall"
    ; Cerrar aplicación si estuviera en ejecución
    nsExec::Exec 'cmd.exe /c taskkill /f /im ProyecThorSlint.exe >nul 2>&1'
    nsExec::Exec 'cmd.exe /c taskkill /f /im ProyecThor.exe >nul 2>&1'
    Sleep 500

    ; Eliminar accesos directos
    Delete "$DESKTOP\ProyecThor.lnk"
    Delete "$SMPROGRAMS\ProyecThor\ProyecThor.lnk"
    Delete "$SMPROGRAMS\ProyecThor\ProyecThor (Modo Clásico).lnk"
    Delete "$SMPROGRAMS\ProyecThor\Desinstalar ProyecThor.lnk"
    RMDir "$SMPROGRAMS\ProyecThor"

    ; Eliminar claves de registro del programa y tipos asociados
    DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ProyecThor"
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\ProyecThor"
    DeleteRegKey HKCU "Software\Classes\Applications\ProyecThor.exe"
    DeleteRegKey HKCU "Software\Classes\*\shell\ProyecThor"

    ; Purgar posibles claves residuales de Inno Setup
    DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\{66A0344F-F850-49CB-9F63-488AB7B3DBCD}_is1"
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\{66A0344F-F850-49CB-9F63-488AB7B3DBCD}_is1"

    ; Eliminar subdirectorios
    RMDir /r "$INSTDIR\bin"
    RMDir /r "$INSTDIR\lua"
    RMDir /r "$INSTDIR\plugins"
    RMDir /r "$INSTDIR\shaders"

    ; Eliminar archivos de la carpeta raíz
    Delete "$INSTDIR\ProyecThorSlint.exe"
    Delete "$INSTDIR\ProyecThor.exe"
    Delete "$INSTDIR\Uninstall.exe"
    Delete "$INSTDIR\unins000.*"
    Delete "$INSTDIR\*.dll"
    Delete "$INSTDIR\ffmpeg.exe"
    Delete "$INSTDIR\yt-dlp.exe"
    Delete "$INSTDIR\catalog.json"
    Delete "$INSTDIR\munin_catalog.json"
    Delete "$INSTDIR\proyecthor.ico"
    Delete "$INSTDIR\proyecthor.png"
    Delete "$INSTDIR\proyecthor_ui.ini"
    Delete "$INSTDIR\splash_bg*.jpg"
    Delete "$INSTDIR\splash_bg*.png"
    Delete "$INSTDIR\bg_splash3.*"

    ; Eliminar carpeta de instalación si queda vacía
    RMDir "$INSTDIR"
SectionEnd
