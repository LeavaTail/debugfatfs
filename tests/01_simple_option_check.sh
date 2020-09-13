#!/bin/bash

set -eu -o pipefail
trap 'echo "ERROR: l.$LINENO, exit status = $?" >&2; exit 1' ERR

OUTPUT=data.dat

function test_options () {
	./debugfatfs $1
	./debugfatfs -a $1
	./debugfatfs -b 512 $1
	./debugfatfs -c 4 $1
	./debugfatfs -o $OUTPUT $1
	cat $OUTPUT
	rm $OUTPUT
	./debugfatfs -v $1
}

### main function ###
test_options fat12.img
test_options fat16.img
test_options fat32.img
test_options exfat.img
