; GP5 VST Editor - Inno Setup Installer Script
; Version: 1.2.0
; No code signing (unsigned)

#define MyAppName "GP5 VST Editor"
#define MyAppVersion "1.2.0"
#define MyAppPublisher "AR-Sounds"
#define MyAppURL "https://github.com/andreasknopke/GP5-Plugin"

; Build output paths (relative to this .iss file)
#define BuildDir "..\build\build_release\release_build\release_build"
#define VST3Bundle BuildDir + "\GP5_VST_Editor_artefacts\Release\VST3\GP5_VST_Editor.vst3"
#define OnnxDll BuildDir + "\GP5_VST_Editor_artefacts\Release\onnxruntime.dll"

[Setup]
AppId={{A7F3B2C1-5D4E-4F6A-8B9C-0E1D2F3A4B5C}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}/issues
DefaultDirName={commonpf}\Common Files\VST3\GP5_VST_Editor.vst3
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
DisableDirPage=yes
LicenseFile=..\LICENSE
OutputDir=..\releases
OutputBaseFilename=GP5_VST_Editor_{#MyAppVersion}_Setup
Compression=lzma2/ultra64
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
WizardStyle=modern
UninstallDisplayIcon={app}\Contents\x86_64-win\GP5_VST_Editor.vst3
UninstallDisplayName={#MyAppName}
VersionInfoVersion=1.2.0.0
VersionInfoDescription={#MyAppName} Installer
VersionInfoCopyright=Copyright (C) 2026 {#MyAppPublisher}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "german"; MessagesFile: "compiler:Languages\German.isl"

[Messages]
english.WelcomeLabel2=This will install [name/ver] on your computer.%n%nThe VST3 plugin will be installed to:%n  C:\Program Files\Common Files\VST3\%n%nPlease close your DAW before continuing.
german.WelcomeLabel2=Dieses Programm installiert [name/ver] auf Ihrem Computer.%n%nDas VST3-Plugin wird installiert nach:%n  C:\Program Files\Common Files\VST3\%n%nBitte schließen Sie Ihre DAW vor der Installation.

[Files]
; VST3 Bundle structure
Source: "{#VST3Bundle}\Contents\x86_64-win\GP5_VST_Editor.vst3"; DestDir: "{app}\Contents\x86_64-win"; Flags: ignoreversion
Source: "{#VST3Bundle}\Contents\Resources\moduleinfo.json"; DestDir: "{app}\Contents\Resources"; Flags: ignoreversion

; ONNX Runtime DLL - must be next to the VST3 DLL for the plugin to find it
Source: "{#OnnxDll}"; DestDir: "{app}\Contents\x86_64-win"; Flags: ignoreversion

[UninstallDelete]
Type: filesandordirs; Name: "{app}"
