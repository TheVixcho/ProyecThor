; ProyecThor — instalador de Windows (Inno Setup), reemplaza el .msi de WiX
; Toolset (ver ProyecThor.wxs, que a su vez habia reemplazado a un .iss
; manual anterior — volvemos a Inno pero con desinstalacion "limpia" de
; CUALQUIER version anterior, sea Inno o MSI).
;
; AppId: NUNCA cambiar entre versiones. Es lo que le permite a este mismo
; instalador reconocer "es la misma app, version nueva" (registro bajo
; HKCU\...\Uninstall\<AppId>_is1). Reusamos el UpgradeCode que ya tenia el
; .wxs para mantener una sola identidad de producto documentada:
;     66A0344F-F850-49CB-9F63-488AB7B3DBCD
;
; Variables de preprocesador (podés pasarlas con /D al compilar, ej.
; ISCC ProyecThor.iss /DBuildDir=C:\ruta\a\build-win /DProductVersion=0.6.0):
;   BuildDir       -> carpeta con el build de Windows ya compilado
;                      (build-win/, con ProyecThor.exe + todas las DLLs,
;                      ffmpeg.exe, yt-dlp.exe, lua/, plugins/, shaders/,
;                      bin/assets/... , splash_bg*.png, proyecthor.ico, etc.)
;   ProductVersion -> version del instalador, mantenida a mano en sync con
;                      PROYECTHOR_VERSION_STRING (project(VERSION ...) +
;                      PROYECTHOR_VERSION_SUFFIX de CMakeLists.txt -- incluir
;                      el sufijo "-beta.N"/"-rc.N" aca tambien si el build es
;                      una pre-release, para que AppVersion coincida con lo
;                      que la app reporta como version instalada).
#ifndef BuildDir
  #define BuildDir "..\..\build-win"
#endif
#ifndef ProductVersion
  #define ProductVersion "1.0.0"
#endif

[Setup]
; Escapeo de Inno para "{" literal: "{{" -> "{". El resultado final que
; queda registrado es {66A0344F-F850-49CB-9F63-488AB7B3DBCD} (con llaves).
; Mismo GUID que aparece mas abajo en [Code] (ahi va sin el escapeo "{{"
; porque adentro de un string de Pascal las llaves no tienen significado
; especial, son texto literal comun).
AppId={{66A0344F-F850-49CB-9F63-488AB7B3DBCD}
AppName=ProyecThor
AppVersion={#ProductVersion}
AppPublisher=vixcho
DefaultDirName={localappdata}\Programs\ProyecThor
DefaultGroupName=ProyecThor
DisableProgramGroupPage=yes
; Instala sin pedir admin (equivalente a Scope="perUser" del .wxs / a
; PrivilegesRequired=lowest de Inno): todo vive bajo LocalAppData del
; usuario actual, nada toca Program Files ni HKLM.
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=dist
OutputBaseFilename=ProyecThor_Setup
SetupIconFile=..\..\proyecthor.ico
UninstallDisplayIcon={app}\proyecthor.ico
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
; Mismo criterio minimalista que WixUI_Minimal en el .wxs viejo: nada de
; pagina de bienvenida, seleccion de carpeta ni "listo para instalar" —
; licencia y listo.
DisableWelcomePage=yes
DisableDirPage=yes
DisableReadyPage=yes
LicenseFile=LICENSE.rtf

[Languages]
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
; TODO el contenido de build-win/, recursivo — mismo criterio que el glob
; harvesting del .wxs (<Files Include="$(var.BuildDir)\**">): asi ningun
; archivo nuevo (una DLL de plugin de VLC, un shader, un icono) se puede
; "olvidar" a mano. Los Excludes son SOLO artefactos de CMake/Ninja/dev que
; nunca deberian llegar a la PC del usuario final — el icono (proyecthor.ico
; /.png) y las imagenes de splash (splash_bg1.png, splash_bg2.png,
; bg_splash3.png) NO estan excluidos a proposito: son assets reales que usa
; la app en tiempo de ejecucion, no basura de build.
Source: "{#BuildDir}\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion; \
    Excludes: "\CMakeFiles\*,\_deps\*,\generated\*,\FoudreVue\*,\CMakeCache.txt,\cmake_install.cmake,\compile_commands.json,\build.ninja,\.ninja_log,\.ninja_deps,\pasteinbuild.txt"

[Icons]
Name: "{group}\ProyecThor"; Filename: "{app}\ProyecThor.exe"; WorkingDir: "{app}"; IconFilename: "{app}\proyecthor.ico"
Name: "{userdesktop}\ProyecThor"; Filename: "{app}\ProyecThor.exe"; WorkingDir: "{app}"; IconFilename: "{app}\proyecthor.ico"

[Registry]
; Registra ProyecThor bajo HKCU\Software\Classes\Applications\ProyecThor.exe --
; el mecanismo estandar de Windows para "aparecer en Abrir con" SIN pisar la
; app predeterminada actual de esas extensiones (a diferencia de registrar
; la extension misma con un ProgID propio, que si cambiaria el default).
; SupportedTypes lista las extensiones que este .exe declara poder abrir;
; main.cpp (GetPendingOpenFilePath/ImportAndPreviewExternalFile) importa el
; archivo a la biblioteca correspondiente y lo carga en Preview.
; Bajo HKCU y sin admin, coherente con PrivilegesRequired=lowest de arriba.
Root: HKCU; Subkey: "Software\Classes\Applications\ProyecThor.exe"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\Applications\ProyecThor.exe"; ValueType: string; ValueName: "FriendlyAppName"; ValueData: "ProyecThor"
Root: HKCU; Subkey: "Software\Classes\Applications\ProyecThor.exe\shell\open\command"; ValueType: string; ValueData: """{app}\ProyecThor.exe"" ""%1"""
; Audio
Root: HKCU; Subkey: "Software\Classes\Applications\ProyecThor.exe\SupportedTypes"; ValueType: string; ValueName: ".mp3"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\ProyecThor.exe\SupportedTypes"; ValueType: string; ValueName: ".flac"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\ProyecThor.exe\SupportedTypes"; ValueType: string; ValueName: ".wav"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\ProyecThor.exe\SupportedTypes"; ValueType: string; ValueName: ".ogg"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\ProyecThor.exe\SupportedTypes"; ValueType: string; ValueName: ".aac"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\ProyecThor.exe\SupportedTypes"; ValueType: string; ValueName: ".m4a"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\ProyecThor.exe\SupportedTypes"; ValueType: string; ValueName: ".wma"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\ProyecThor.exe\SupportedTypes"; ValueType: string; ValueName: ".opus"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\ProyecThor.exe\SupportedTypes"; ValueType: string; ValueName: ".aiff"; ValueData: ""
; Video
Root: HKCU; Subkey: "Software\Classes\Applications\ProyecThor.exe\SupportedTypes"; ValueType: string; ValueName: ".mp4"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\ProyecThor.exe\SupportedTypes"; ValueType: string; ValueName: ".mkv"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\ProyecThor.exe\SupportedTypes"; ValueType: string; ValueName: ".avi"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\ProyecThor.exe\SupportedTypes"; ValueType: string; ValueName: ".mov"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\ProyecThor.exe\SupportedTypes"; ValueType: string; ValueName: ".webm"; ValueData: ""

[Run]
Filename: "{app}\ProyecThor.exe"; Description: "{cm:LaunchProgram,ProyecThor}"; Flags: nowait postinstall skipifsilent

[Code]
// =============================================================================
//  Desinstalacion "limpia" de versiones anteriores — ANTES de copiar un solo
//  archivo nuevo, para que Inno nunca deje archivos huerfanos de una version
//  vieja que la nueva ya no incluye (a diferencia de MSI, Inno no hace ese
//  diff automaticamente entre versiones).
// =============================================================================

// Desinstalador silencioso de una version anterior instalada con ESTE MISMO
// instalador Inno (mismo AppId, registrado bajo HKCU\...\Uninstall\<AppId>_is1).
function GetSelfUninstallString(): String;
var
  UninstPath, UninstString: String;
begin
  // Llaves literales: dentro de un string de Pascal no tienen significado
  // especial (a diferencia de un valor de directiva [Setup], donde "{"
  // arranca una constante y hay que escaparla con "{{" — ver AppId arriba).
  UninstPath := 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{66A0344F-F850-49CB-9F63-488AB7B3DBCD}_is1';
  UninstString := '';
  if not RegQueryStringValue(HKCU, UninstPath, 'UninstallString', UninstString) then
    RegQueryStringValue(HKLM, UninstPath, 'UninstallString', UninstString);
  Result := UninstString;
end;

// Instalacion vieja hecha con el .msi de WiX Toolset (ver ProyecThor.wxs,
// retirado en favor de este .iss). WiX con Scope="perUser" registra el
// desinstalador bajo HKCU usando el ProductCode (no el UpgradeCode) como
// nombre de subclave, asi que hay que recorrer TODAS las subclaves de
// Uninstall buscando la que tenga DisplayName=ProyecThor y una
// UninstallString que use MsiExec.exe.
procedure UninstallOldMsiIfFound();
var
  Names: TArrayOfString;
  I: Integer;
  KeyPath, DisplayName, UninstallString: String;
  ResultCode: Integer;
begin
  if not RegGetSubkeyNames(HKCU, 'Software\Microsoft\Windows\CurrentVersion\Uninstall', Names) then
    exit;

  for I := 0 to GetArrayLength(Names) - 1 do
  begin
    KeyPath := 'Software\Microsoft\Windows\CurrentVersion\Uninstall\' + Names[I];

    if RegQueryStringValue(HKCU, KeyPath, 'DisplayName', DisplayName) and (DisplayName = 'ProyecThor') then
    begin
      if RegQueryStringValue(HKCU, KeyPath, 'UninstallString', UninstallString) and
         (Pos('MsiExec.exe', UninstallString) > 0) then
      begin
        // Names[I] es el ProductCode del MSI (formato {GUID}) cuando el
        // paquete se instalo per-user.
        Exec('msiexec.exe', '/x ' + Names[I] + ' /qn /norestart', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
      end;
    end;
  end;
end;

function InitializeSetup(): Boolean;
var
  SelfUninstallString: String;
  ResultCode: Integer;
begin
  Result := True;

  SelfUninstallString := GetSelfUninstallString();
  if SelfUninstallString <> '' then
  begin
    SelfUninstallString := RemoveQuotes(SelfUninstallString);
    Exec(SelfUninstallString, '/VERYSILENT /SUPPRESSMSGBOXES /NORESTART', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
  end;

  UninstallOldMsiIfFound();
end;
