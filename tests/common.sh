#!/bin/bash

function require_command() {
	if ! command -v "$1" >/dev/null 2>&1; then
		echo "Required command '$1' is not installed." >&2
		exit 127
	fi
}

function require_test_tools() {
	require_command tar
	require_command md5sum

	if ! command -v bzip2 >/dev/null 2>&1 &&
			! command -v lbzip2 >/dev/null 2>&1; then
		echo "Required command 'bzip2' or 'lbzip2' is not installed." >&2
		exit 127
	fi
}

function image_name() {
	local image="$1"

	image="${image##*/}"
	image="${image%.img}"
	image="${image%.tar.bz2}"
	printf "%s" "${image}"
}

# md5sum
declare -A images=(
	["fat12"]="4fae2d03cd312b9f69296a6ed53345ad"
	["fat16"]="1d63e0009104645628b91e90f46cb250"
	["fat32"]="27f1ad30b2a35c63883eefb42de63f46"
	["exfat"]="927ca233c0a75166ea3a11e4c2d2dc48"
)

function init_image() {
	local targets=("$@")
	local fsname
	local hash
	local img

	require_test_tools

	if [ ${#targets[@]} -eq 0 ]; then
		targets=(tests/sample/*.tar.bz2)
	fi

	for image in "${targets[@]}"; do
		fsname=$(image_name "${image}")
		hash="${images[${fsname}]}"
		img=${fsname}.img
		if [ -e ${img} ]; then
			A=`md5sum ${img} | cut -d" " -f 1`
			if [ "$A" = "$hash" ]; then
				continue
			fi
		fi
		echo "Initialize ${fsname} filesystem image"
		tar -xf tests/sample/${fsname}.tar.bz2
	done
}
