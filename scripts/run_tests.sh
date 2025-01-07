#!/usr/bin/env bash

set -ex

xcodebuild -version
xcodebuild -workspace fat-drive-sorter.xcworkspace -scheme Release -showBuildSettings | grep SWIFT_VERSION

set -o pipefail && xcodebuild test -workspace fat-drive-sorter.xcworkspace -scheme Test | scripts/xcbeautify
