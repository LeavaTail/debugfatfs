#!/bin/bash

set -eu -o pipefail
trap 'echo "ERROR: l.$LINENO, exit status = $?" >&2; exit 1' ERR

source tests/common.sh

IMAGES=("fat12.img" "fat16.img" "fat32.img")

function test_options () {
	./debugfatfs $1 /00
	./debugfatfs $1 /00/FILE1.TXT
}

function main() {
	init_image "${IMAGES[@]}"

	for fs in ${IMAGES[@]}; do
		test_options ${fs}
	done
}

### main function ###
main "$@"
