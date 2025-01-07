#!/usr/bin/env bash

set -ex

set -o pipefail && xcodebuild -workspace fat-drive-sorter.xcworkspace -scheme Release -derivedDataPath DerivedData | scripts/xcbeautify
file "$BUILD_DIR/$XCODE_BUILD_PATH/$APP_NAME.app/Contents/MacOS/$APP_NAME"
