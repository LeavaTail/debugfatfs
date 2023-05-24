#!/bin/bash

set -eu -o pipefail
trap 'echo "ERROR: l.$LINENO, exit status = $?" >&2; exit 1' ERR

source tests/common.sh

IMAGES=("fat12.img" "fat16.img" "fat32.img" "exfat.img")
OUTPUT=data.dat

function test_options () {
	./debugfatfs $1
	./debugfatfs -a $1
	./debugfatfs -b 512 $1
	./debugfatfs -c 4 $1
	./debugfatfs -d /00_SIMPLE $1
	./debugfatfs -e 4 $1
	./debugfatfs -f $1
	./debugfatfs -o $OUTPUT $1
	./debugfatfs -q $1
	./debugfatfs -r $1
	./debugfatfs -u a $1
	./debugfatfs -v $1
	./debugfatfs --version
}

function main() {
	init_image

	for fs in ${IMAGES[@]}; do
		test_options ${fs}
	done
}

### main function ###
main "$@"
