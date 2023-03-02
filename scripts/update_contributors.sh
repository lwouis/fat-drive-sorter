#!/usr/bin/env bash

set -exu

readOnlyToken="8170d6b4f0531ffd7f52edea374a3689"
projectId="600511"

(
  echo -e "# Contributors\n"

  echo -e "They helped [develop the app](https://github.com/lwouis/fat-drive-sorter/graphs/contributors):\n"

  # this script is ran on shared CI machines which go over github's quota
  # thus we use a token here to guaranty that the call will succeed
  curl https://api.github.com/repos/lwouis/fat-drive-sorter/contributors \
    -H "Authorization: token $GITHUB_TOKEN" |
    jq -r '.[]|("[" + .login + "](" + .html_url + ")")' |
    sed -e '/semantic-release-bot/d' |
    LC_ALL=en_US.UTF-8 sort -f |
    sed -e 's/^/* /'

  echo -e "\nThey helped [localize the app](https://poeditor.com/join/project/TTqkcdPO9V):\n"

  (
    echo -e "lwouis"
    curl -s -X POST https://api.poeditor.com/v2/contributors/list \
      -d api_token="$readOnlyToken" \
      -d id="$projectId" |
      jq -r '.result.contributors[].name'
  ) |
    LC_ALL=en_US.UTF-8 sort -f |
    sed -e 's/^/* /'
) >docs/Contributors.md
