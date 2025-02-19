#!/bin/bash

SDK_PATH=$1
GITHUB_TOKEN=$2
SEL4CP_REPO="Ivan-Velickovic/sel4cp"
# zip is the only available option
ARCHIVE_FORMAT="zip"

ARTIFACT_ID=`curl \
  -H "Accept: application/vnd.github+json" \
  -H "Authorization: Bearer ${GITHUB_TOKEN}"\
  -H "X-GitHub-Api-Version: 2022-11-28" \
  https://api.github.com/repos/$SEL4CP_REPO/actions/artifacts | jq '.artifacts[0].id'`

echo "Downloading SDK with artifact ID: ${ARTIFACT_ID}"
curl \
  -L \
  -u "Ivan-Velickovic:${GITHUB_TOKEN}" \
  -o $SDK_PATH \
  -H "Accept: application/vnd.github+json" \
  -H "X-GitHub-Api-Version: 2022-11-28" \
  https://api.github.com/repos/$SEL4CP_REPO/actions/artifacts/$ARTIFACT_ID/$ARCHIVE_FORMAT

