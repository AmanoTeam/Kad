#!/usr/bin/env bash

set -eu

declare -r APP_DIRECTORY="$(realpath "$(( [ -n "${BASH_SOURCE}" ] && dirname "$(realpath "${BASH_SOURCE[0]}")" ) || dirname "$(realpath "${0}")")")/.."

declare -ra targets=(
	'brotli'
	'c-ares'
	'cabundle'
	'jansson'
	'libevent'
	'nghttp2'
	'picohttpparser'
	'zlib'
	'zstd'
)

for target in "${targets[@]}"; do
	git -C "${APP_DIRECTORY}" submodule update --init --remote "${APP_DIRECTORY}/submodules/${target}"
done
