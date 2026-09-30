; =============================================================================
; ProyecThor - Instalador Moderno y Ligero para Windows (NSIS)
; Reemplaza instalaciones anteriores de forma limpia sin duplicar programas
; Compatible para compilación nativa en Linux (makensis) y Windows
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
  !define VERSION "1.0.2"
!endif

!ifndef BUILD_DIR
  !define BUILD_DIR "../../build-win"
!endif

!ifndef OUT_DIR
  !define OUT_DIR "dist"
!endif

Name "${PRODUCT_NAME} ${VERSION}"
OutFile "${OUT_DIR}/ProyecThor_Setup.exe"

; Instalación per-user limpia en AppData local (sin necesidad de permisos de Administrador)
InstallDir "$LOCALAPPDATA\Programs\ProyecThor"
InstallDirRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ProyecThor" "InstallLocation"
RequestExecutionLevel user

; Iconos
!define MUI_ICON "../../proyecthor.ico"
!define MUI_UNICON "../../proyecthor.ico"

; Configuración de la interfaz moderna
!define MUI_ABORTWARNING

; Páginas del instalador
!insertmacro MUI_PAGE_LICENSE "LICENSE.rtf"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES

; Página final con opción de ejecutar la aplicación
!define MUI_FINISHPAGE_RUN "$INSTDIR\ProyecThor.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Ejecutar ProyecThor ahora"
!insertmacro MUI_PAGE_FINISH

; Páginas del desinstalador
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

; Idiomas
!insertmacro MUI_LANGUAGE "Spanish"
!insertmacro MUI_LANGUAGE "English"

; -----------------------------------------------------------------------------
; Inicialización: Detección y limpieza de versiones anteriores (Inno Setup o NSIS)
; para garantizar que la nueva actualización reemplace a la anterior sin duplicados
; -----------------------------------------------------------------------------
Function .onInit
    ; 1. Desinstalar versión previa de NSIS si ya existe
    ReadRegStr $0 HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ProyecThor" "UninstallString"
    ${If} $0 != ""
        ReadRegStr $1 HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ProyecThor" "InstallLocation"
        ${If} $1 == ""
            StrCpy $1 "$INSTDIR"
        ${EndIf}
        ; Ejecutar desinstalador silencioso y esperar a que termine la limpieza
        ExecWait '$0 /S _?=$1'
    ${EndIf}

    ; 2. Desinstalar versión previa de Inno Setup si existía ({66A0344F-F850-49CB-9F63-488AB7B3DBCD}_is1)
    ReadRegStr $2 HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\{66A0344F-F850-49CB-9F63-488AB7B3DBCD}_is1" "UninstallString"
    ${If} $2 == ""
        ReadRegStr $2 HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\{66A0344F-F850-49CB-9F63-488AB7B3DBCD}_is1" "UninstallString"
    ${EndIf}
    ${If} $2 != ""
        ExecWait '$2 /VERYSILENT /SUPPRESSMSGBOXES /NORESTART'
    ${EndIf}
FunctionEnd

; -----------------------------------------------------------------------------
; Sección Principal: Copia de archivos y registro
; -----------------------------------------------------------------------------
Section "ProyecThor" SEC01
    SetOutPath "$INSTDIR"

    ; Cerrar instancias activas de ProyecThor para no bloquear archivos DLL ni el ejecutable
    DetailPrint "Cerrando instancias previas si las hubiera..."
    nsExec::Exec 'cmd.exe /c taskkill /f /im ProyecThor.exe >nul 2>&1'

    ; Copiar ejecutables y librerías runtime principales
    SetOverwrite on
    File "${BUILD_DIR}/ProyecThor.exe"
    File "${BUILD_DIR}/*.dll"
    File /nonfatal "${BUILD_DIR}/ffmpeg.exe"
    File /nonfatal "${BUILD_DIR}/yt-dlp.exe"
    File /nonfatal "${BUILD_DIR}/proyecthor.ico"
    File /nonfatal "${BUILD_DIR}/proyecthor.png"
    File /nonfatal "${BUILD_DIR}/proyecthor_ui.ini"
    File /nonfatal "${BUILD_DIR}/splash_bg*.jpg"
    File /nonfatal "${BUILD_DIR}/splash_bg*.png"
    File /nonfatal "${BUILD_DIR}/bg_splash3.*"

    ; Copiar carpetas de recursos completas
    SetOutPath "$INSTDIR\bin"
    File /r /x CMakeLists.txt /x *.ninja /x .ninja* "${BUILD_DIR}/bin/*.*"

    SetOutPath "$INSTDIR\lua"
    File /r /x CMakeLists.txt /x *.ninja /x .ninja* "${BUILD_DIR}/lua/*.*"

    SetOutPath "$INSTDIR\plugins"
    File /r /x CMakeLists.txt /x *.ninja /x .ninja* "${BUILD_DIR}/plugins/*.*"

    SetOutPath "$INSTDIR\shaders"
    File /r /x CMakeLists.txt /x *.ninja /x .ninja* "${BUILD_DIR}/shaders/*.*"

    ; Crear desinstalador propio
    SetOutPath "$INSTDIR"
    WriteUninstaller "$INSTDIR\Uninstall.exe"

    ; Crear accesos directos
    CreateDirectory "$SMPROGRAMS\ProyecThor"
    CreateShortcut "$SMPROGRAMS\ProyecThor\ProyecThor.lnk" "$INSTDIR\ProyecThor.exe" "" "$INSTDIR\proyecthor.ico" 0
    CreateShortcut "$SMPROGRAMS\ProyecThor\Desinstalar ProyecThor.lnk" "$INSTDIR\Uninstall.exe" "" "$INSTDIR\Uninstall.exe" 0
    CreateShortcut "$DESKTOP\ProyecThor.lnk" "$INSTDIR\ProyecThor.exe" "" "$INSTDIR\proyecthor.ico" 0

    ; Registro en Agregar o Quitar Programas (Windows Apps & Features) bajo HKCU
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ProyecThor" "DisplayName" "ProyecThor"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ProyecThor" "DisplayVersion" "${VERSION}"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ProyecThor" "Publisher" "${PRODUCT_PUBLISHER}"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ProyecThor" "DisplayIcon" "$INSTDIR\proyecthor.ico"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ProyecThor" "UninstallString" '"$INSTDIR\Uninstall.exe"'
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ProyecThor" "QuietUninstallString" '"$INSTDIR\Uninstall.exe" /S'
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ProyecThor" "InstallLocation" "$INSTDIR"
    WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ProyecThor" "NoModify" 1
    WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ProyecThor" "NoRepair" 1

    ; Registro de tipos soportados en "Abrir con..." sin forzar cambio de programa predeterminado
    WriteRegStr HKCU "Software\Classes\Applications\ProyecThor.exe" "FriendlyAppName" "ProyecThor"
    WriteRegStr HKCU "Software\Classes\Applications\ProyecThor.exe\shell\open\command" "" '"$INSTDIR\ProyecThor.exe" "%1"'
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
SectionEnd

; -----------------------------------------------------------------------------
; Sección de Desinstalación Limpia
; -----------------------------------------------------------------------------
Section "Uninstall"
    ; Cerrar aplicación si estuviera corriendo
    nsExec::Exec 'cmd.exe /c taskkill /f /im ProyecThor.exe >nul 2>&1'

    ; Eliminar accesos directos
    Delete "$DESKTOP\ProyecThor.lnk"
    Delete "$SMPROGRAMS\ProyecThor\ProyecThor.lnk"
    Delete "$SMPROGRAMS\ProyecThor\Desinstalar ProyecThor.lnk"
    RMDir "$SMPROGRAMS\ProyecThor"

    ; Eliminar claves de registro
    DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ProyecThor"
    DeleteRegKey HKCU "Software\Classes\Applications\ProyecThor.exe"

    ; Eliminar subdirectorios
    RMDir /r "$INSTDIR\bin"
    RMDir /r "$INSTDIR\lua"
    RMDir /r "$INSTDIR\plugins"
    RMDir /r "$INSTDIR\shaders"

    ; Eliminar archivos de la carpeta raíz
    Delete "$INSTDIR\ProyecThor.exe"
    Delete "$INSTDIR\Uninstall.exe"
    Delete "$INSTDIR\*.dll"
    Delete "$INSTDIR\ffmpeg.exe"
    Delete "$INSTDIR\yt-dlp.exe"
    Delete "$INSTDIR\proyecthor.ico"
    Delete "$INSTDIR\proyecthor.png"
    Delete "$INSTDIR\proyecthor_ui.ini"
    Delete "$INSTDIR\splash_bg*.jpg"
    Delete "$INSTDIR\splash_bg*.png"
    Delete "$INSTDIR\bg_splash3.*"

    ; Eliminar carpeta de instalación si queda vacía
    RMDir "$INSTDIR"
SectionEnd
