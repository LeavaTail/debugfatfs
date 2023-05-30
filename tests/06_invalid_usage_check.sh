#!/bin/bash

set -u

source tests/common.sh

IMAGES=("fat12.img" "fat16.img" "fat32.img" "exfat.img")
OUTPUT=data.dat

function test_options () {
	./debugfatfs -c $1
	./debugfatfs -c 0 $1
}

function test_shell () {
	expect -c "
	set timeout 5
	spawn ./debugfatfs -iq $1
	expect \"/> \"
	send \"create /00/DIR/SUBFILE\n\"
	expect \"/> \"
	send \"create 00\n\"
	expect \"/> \"
	send \"mkdir /00/DIR\n\"
	expect \"/> \"
	send \"mkdir 00\n\"
	expect \"/> \"
	send \"remove /00/FILE3.TXT\n\"
	expect \"/> \"
	send \"remove 00\n\"
	expect \"/> \"
	send \"rmdir /00/SUBDIR\n\"
	expect \"/> \"
	send \"rmdir /00\n\"
	expect \"/> \"
	send \"exit\n\"
	expect eof
	exit
	"
}

function main() {
	init_image

	for fs in ${IMAGES[@]}; do
		test_options ${fs}
		test_shell ${fs}
	done
}

### main function ###
main "$@"
