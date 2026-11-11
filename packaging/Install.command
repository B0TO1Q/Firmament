#!/bin/bash
# Double-click to install Firmament for the current user.
cd "$(dirname "$0")" || exit 1
VST3="$HOME/Library/Audio/Plug-Ins/VST3"
AU="$HOME/Library/Audio/Plug-Ins/Components"
mkdir -p "$VST3" "$AU"
rm -rf "$VST3/Firmament.vst3" "$AU/Firmament.component"
cp -R Firmament.vst3 "$VST3/" && cp -R Firmament.component "$AU/" || { echo "Install failed."; exit 1; }
# These builds are not notarized by Apple yet; clear the download quarantine flag.
xattr -cr "$VST3/Firmament.vst3" "$AU/Firmament.component" Firmament.app 2>/dev/null
killall -9 AudioComponentRegistrar 2>/dev/null
echo "Firmament installed. Restart your DAW and rescan plugins."
