#!/bin/zsh
# shellcheck disable=SC1071

# Exit immediately if a command exits with a non-zero status
set -e

# Remember the directory the script was invoked from and restore it before exiting
START_DIR="$(pwd)"

cd /opt/vcpkg || exit 1

git fetch --tags
LATEST_VCPKG_TAG="$(git tag --sort=-creatordate | head -n 1)"
git checkout "$LATEST_VCPKG_TAG"

./bootstrap-vcpkg.sh

vcpkg integrate install

# Update VCPKG baseline in vcpkg.json

if [ -f "$START_DIR/vcpkg.json" ]; then
	BASELINE="$(git -C /opt/vcpkg rev-parse "${LATEST_VCPKG_TAG}^{commit}")"
	tmpfile="$(mktemp)"
	jq --arg baseline "$BASELINE" '. + {"builtin-baseline": $baseline}' "$START_DIR/vcpkg.json" > "$tmpfile"
	mv "$tmpfile" "$START_DIR/vcpkg.json"

	UPDATED_BASELINE="$(jq -r '.["builtin-baseline"] // empty' "$START_DIR/vcpkg.json")"
	if [ -z "$UPDATED_BASELINE" ]; then
		echo "Error: builtin-baseline missing in $START_DIR/vcpkg.json after update." >&2
		exit 1
	fi

	if [ "$UPDATED_BASELINE" != "$BASELINE" ]; then
		echo "Error: builtin-baseline ($UPDATED_BASELINE) does not match latest tag commit ($BASELINE)." >&2
		exit 1
	fi

	echo "vcpkg.json baseline was updated to latest tag $LATEST_VCPKG_TAG commit: $BASELINE"
fi

# Return to the original working directory (project root when invoked from there)
cd "$START_DIR" || exit 1
