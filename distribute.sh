vst3_path="build/rel/Automate_artefacts/Release/VST3/Automate.vst3"
au_path="build/rel/Automate_artefacts/Release/AU/Automate.component"

vst3_dist_path="dist/Automate/Automate.vst3"
au_dist_path="dist/Automate/Automate.component"

vst3_bin_path="dist/Automate/Automate.vst3/Contents/MacOS/Automate"
au_bin_path="dist/Automate/Automate.component/Contents/MacOS/Automate"

codesign_id="DJ662GG36K"
name="Automate"
dmg_path="dist/Automate.dmg"

rm -rf dist
mkdir dist
mkdir dist/Automate
ln -s /Library/Audio/Plug-ins/VST3 dist/Automate
ln -s /Library/Audio/Plug-ins/Components dist/Automate
mkdir build
rm -rf build/rel
mkdir build/rel

cmake -B build/rel -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" && cmake --build build/rel &&
cp -r $vst3_path $vst3_dist_path && cp -r $au_path $au_dist_path &&
llvm-strip $vst3_bin_path && llvm-strip $au_bin_path &&
codesign -f --deep -o runtime --timestamp -s $codesign_id $vst3_dist_path &&
codesign -f --deep -o runtime --timestamp -s $codesign_id $au_dist_path &&
hdiutil create -volname $name -srcfolder "dist/Automate" -ov -format UDBZ $dmg_path &&
codesign -s $codesign_id --timestamp $dmg_path &&
xcrun notarytool submit $dmg_path --keychain-profile "notarytool-password" --wait &&
xcrun stapler staple $dmg_path &&
xcrun stapler validate $dmg_path
