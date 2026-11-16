#!/bin/bash
# Double-click to install the plugin in this folder for the current user.
cd "$(dirname "$0")" || exit 1
VST3="$HOME/Library/Audio/Plug-Ins/VST3"
AU="$HOME/Library/Audio/Plug-Ins/Components"
mkdir -p "$VST3" "$AU"
for B in *.vst3; do
  [ -e "$B" ] || continue
  rm -rf "$VST3/$B"; cp -R "$B" "$VST3/" || { echo "Install failed."; exit 1; }
  xattr -cr "$VST3/$B" 2>/dev/null
done
for B in *.component; do
  [ -e "$B" ] || continue
  rm -rf "$AU/$B"; cp -R "$B" "$AU/" || { echo "Install failed."; exit 1; }
  xattr -cr "$AU/$B" 2>/dev/null
done
# These builds are not notarized by Apple yet; clear the download quarantine flag.
xattr -cr ./*.app 2>/dev/null
killall -9 AudioComponentRegistrar 2>/dev/null
echo "Installed. Restart your DAW and rescan plugins."
