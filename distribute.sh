vst3_path="build/rel/Automate_artefacts/Release/VST3/Automate.vst3"
codesign_id="DJ662GG36K"
name="Automate"
dmg_path="dist/Automate.dmg"

mkdir dist
codesign -f --deep -o runtime --timestamp -s $codesign_id $vst3_path
hdiutil create -volname $name -srcfolder $vst3_path -ov -format UDBZ $dmg_path
codesign -s $codesign_id --timestamp $dmg_path
xcrun notarytool submit $dmg_path --keychain-profile "notarytool-password" --wait
xcrun stapler staple $dmg_path 
xcrun stapler validate $dmg_path
