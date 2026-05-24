#!/bin/bash

set -eu -o pipefail
trap 'echo "ERROR: l.$LINENO, exit status = $?" >&2; exit 1' ERR

source tests/common.sh

IMAGES=("fat12.img" "fat16.img" "fat32.img")

function test_mkdir_allocates_real_cluster() {
	local output

	output=$(printf "cd 00\nmkdir NEWDIR\nstat NEWDIR\nexit\n" | ./debugfatfs -iq "$1")
	echo "${output}"

	if echo "${output}" | grep -F "First Clu:   0"; then
		echo "ERROR: mkdir allocated cluster 0 in $1." >&2
		exit 1
	fi

	echo "${output}" | grep -F "File Attr:   ---D-"
}

function main() {
	init_image "${IMAGES[@]}"

	for fs in ${IMAGES[@]}; do
		test_mkdir_allocates_real_cluster ${fs}
	done
}

### main function ###
main "$@"

