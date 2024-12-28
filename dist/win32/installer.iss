#ifndef AppVersion
#define AppVersion "1.0" ; Default version if not provided via command line
#endif

#ifndef OutputBaseFilename
#define OutputBaseFilename "VST3Installer" ; Default name if not provided via command line
#endif

[Setup]
AppName=Automate
AppVersion={#AppVersion}
DefaultDirName={commoncf}\VST3
OutputBaseFilename={#OutputBaseFilename}
Compression=none
SolidCompression=no

[Files]
Source: "Output\build-fx\Automate-FX_artefacts\Release\VST3\Automate-FX.vst3\*"; DestDir: "{commoncf}\VST3\Automate-FX.vst3"; Flags: ignoreversion recursesubdirs
Source: "Output\build-inst\Automate-Instrument_artefacts\Release\VST3\Automate-Instrument.vst3\*"; DestDir: "{commoncf}\VST3\Automate-Instrument.vst3"; Flags: ignoreversion recursesubdirs

