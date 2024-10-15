#! /bin/bash

if [ -z "$1" ]; then
  exit 1
fi

CODESIGN_ID=$1
VERSION=`cat VERSION`

echo "Build CMake Project"

rm -rf build/rel && mkdir -p build/rel/fx && mkdir -p build/rel/inst
cmake -B build/rel/fx -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" -DPLUGIN_TYPE=Effect && cmake --build build/rel/fx
cmake -B build/rel/inst -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" -DPLUGIN_TYPE=Instrument && cmake --build build/rel/inst

echo "Package"

mkdir -p dist/macOS/VST3 && mkdir -p dist/macOS/AU
cp -r build/rel/fx/Automate-FX_artefacts/Release/VST3/Automate-FX.vst3 dist/macOS/VST3 &&
cp -r build/rel/fx/Automate-FX_artefacts/Release/AU/Automate-FX.component dist/macOS/AU &&
cp -r build/rel/inst/Automate-Instrument_artefacts/Release/VST3/Automate-Instrument.vst3 dist/macOS/VST3 &&
cp -r build/rel/inst/Automate-Instrument_artefacts/Release/AU/Automate-Instrument.component dist/macOS/AU &&

# NOTE(luca): keep strip disabled while in beta so crash reports will be more informative
#llvm-strip $vst3_bin_path && llvm-strip $au_bin_path &&

cd dist/macOS 

codesign -f --deep -o runtime --timestamp --entitlements "entitlements.plist" -s $CODESIGN_ID "VST3/Automate-FX.vst3" &&
codesign -f --deep -o runtime --timestamp --entitlements "entitlements.plist" -s $CODESIGN_ID "AU/Automate-FX.component" &&
codesign -f --deep -o runtime --timestamp --entitlements "entitlements.plist" -s $CODESIGN_ID "VST3/Automate-Instrument.vst3" &&
codesign -f --deep -o runtime --timestamp --entitlements "entitlements.plist" -s $CODESIGN_ID "AU/Automate-Instrument.component" &&

PKG="Automate-$VERSION-macOS-setup.pkg"

pkgbuild --root VST3 --install-location "/Library/Audio/Plug-Ins/VST3" --identifier com.chromaaudio.automate-vst3 --version $VERSION "Automate-VST3.pkg" &&
pkgbuild --root AU --install-location "/Library/Audio/Plug-Ins/Components" --identifier com.chromaaudio.automate-au --version $VERSION "Automate-AU.pkg" &&

productbuild --sign $CODESIGN_ID --distribution distribution.xml $PKG &&

rm -rf VST3 && rm -rf AU && rm Automate-VST3.pkg && rm Automate-AU.pkg

xcrun notarytool submit $PKG --keychain-profile "notarytool-password" --wait &&
xcrun stapler staple $PKG &&
xcrun stapler validate $PKG
