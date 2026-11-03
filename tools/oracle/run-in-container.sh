#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# Runs the rivals' code, and Chromium, only in a throwaway container.
#
#   tools/oracle/run-in-container.sh fetch
#   tools/oracle/run-in-container.sh rivals [--check]
#   tools/oracle/run-in-container.sh chromium PAGE.html
#
# fetch runs on the host: tools/oracle/rivals.mjs --fetch downloads each
# package by npm pack at its pinned version, which runs none of its
# code, and CSS Color 4's sample files, into $MTWN_RIVALS_DIR (by
# default ${TMPDIR:-/tmp}/mtwn-rivals), and refuses any whose hash is
# not the pinned one.
#
# rivals runs the oracle on what fetch left, and chromium loads one page
# (tools/oracle/chromium-page.mjs) in the Chromium build Playwright
# 1.63.0 caches: revision 1243, Chrome for Testing 153.0.8010.12, in
# ~/.cache/ms-playwright/chromium-1243 or $MTWN_CHROMIUM_DIR. Each runs
# in a container of an image pinned by digest, with no network, a
# read-only root, every capability dropped, no new privileges, an
# unprivileged user, and bounded memory, CPUs and processes. The script,
# the packages, the committed headers, the page's folder and the
# Chromium build are mounted read-only; the only writable mounts are an
# empty output folder and a /tmp in memory. Bind mounts refuse a host
# path that does not exist, where -v would make it. The output folder is printed
# at the end: copy what it holds into the tree by hand, and review the
# diff.
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
rivals=${MTWN_RIVALS_DIR:-${TMPDIR:-/tmp}/mtwn-rivals}
chromium=${MTWN_CHROMIUM_DIR:-$HOME/.cache/ms-playwright/chromium-1243}
# node:24-trixie-slim and mcr.microsoft.com/playwright:v1.63.0-noble,
# which has the libraries Chromium 1243 needs.
node_image=node@sha256:54f84b4cf8432b21aaffd5d3ce8d41e12cea0d10d33c09d2f3c9879d49d6b446
chromium_image=mcr.microsoft.com/playwright@sha256:eff16c30e6f3f4af0a03fa4b706120d5e9b0891c344a27d64559aff5900a4a27

usage() {
    echo "usage: tools/oracle/run-in-container.sh fetch | rivals [--check] | chromium PAGE.html" >&2
    exit 2
}

[ $# -ge 1 ] || usage
command=$1
shift
case $command in
fetch | rivals | chromium) ;;
*) usage ;;
esac

if [ "$command" = fetch ]; then
    [ $# -eq 0 ] || usage
    MTWN_RIVALS_DIR=$rivals exec node "$root/tools/oracle/rivals.mjs" --fetch
fi

# The output folder, fresh and empty. Run as root, the container's user
# is 1000 and owns it; otherwise it is the caller, unprivileged already.
# Left behind only when something was written to it.
out=$(mktemp -d "${TMPDIR:-/tmp}/mtwn-oracle.XXXXXX")
trap 'rmdir "$out" 2>/dev/null || true' EXIT
if [ "$(id -u)" -eq 0 ]; then
    user=1000:1000
    chown "$user" "$out"
else
    user="$(id -u):$(id -g)"
fi
contain=(
    docker run --rm --network none --read-only --cap-drop ALL
    --security-opt no-new-privileges --user "$user"
    --memory 4g --cpus 4 --pids-limit 512
    --tmpfs "/tmp:rw,nosuid,nodev,size=512m,mode=1777"
    -e HOME=/tmp -e MTWN_RIVALS_CONTAINER=1 -e MTWN_RIVALS_OUT=/out
    --mount "type=bind,src=$out,dst=/out" -w /tmp
)

case $command in
rivals)
    [ $# -eq 0 ] || { [ $# -eq 1 ] && [ "$1" = --check ]; } || usage
    [ -d "$rivals" ] || {
        echo "run-in-container: nothing fetched in $rivals; run it with fetch first" >&2
        exit 1
    }
    "${contain[@]}" \
        --mount "type=bind,src=$root/tools/oracle/rivals.mjs,dst=/src/tools/oracle/rivals.mjs,readonly" \
        --mount "type=bind,src=$root/test/generated,dst=/src/test/generated,readonly" \
        --mount "type=bind,src=$rivals,dst=/rivals,readonly" \
        -e MTWN_RIVALS_DIR=/rivals -e MTWN_RIVALS_WORK=/tmp/rivals-work \
        "$node_image" node /src/tools/oracle/rivals.mjs "$@"
    if [ $# -eq 0 ]; then
        echo "run-in-container: the headers are in $out; copy them into test/generated and review the diff"
    fi
    ;;
chromium)
    if [ $# -ne 1 ] || [ ! -f "$1" ]; then
        usage
    fi
    [ -x "$chromium/chrome-linux64/chrome" ] || {
        echo "run-in-container: no Chromium 1243 in $chromium" >&2
        exit 1
    }
    page=$(realpath "$1")
    "${contain[@]}" \
        --mount "type=bind,src=$root/tools/oracle/chromium-page.mjs,dst=/driver/chromium-page.mjs,readonly" \
        --mount "type=bind,src=$(dirname "$page"),dst=/page,readonly" \
        --mount "type=bind,src=$chromium,dst=/chromium,readonly" \
        "$chromium_image" node /driver/chromium-page.mjs "/page/$(basename "$page")"
    echo "run-in-container: the page's text is also in $out"
    ;;
esac
