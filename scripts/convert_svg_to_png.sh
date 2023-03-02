#!/usr/bin/env bash

set -exu

# needed because of macOS SIP (see https://stackoverflow.com/a/35570229)
export DYLD_LIBRARY_PATH="$MAGICK_HOME/lib"

currentDir="$(pwd)"
size=256

function convert() {
  #  magick convert \
  #    -background none \
  #    -density 144 \
  #    -resize 24x24 \
  #    "resources/icons/window-controls/$1.svg" \
  #    "resources/icons/window-controls/$1-magik.png"
  /Applications/Inkscape.app/Contents/MacOS/inkscape \
    -z \
    -w "$((size * 2))" -h "$((size * 2))" \
    "$currentDir/resources/icons/app/$1.svg" \
    -o "$currentDir/resources/icons/app/$1@2x.png"
}

convert "app"
