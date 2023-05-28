#!/bin/bash

set -eu -o pipefail
trap 'echo "ERROR: l.$LINENO, exit status = $?" >&2; exit 1' ERR

source tests/common.sh

IMAGES=("fat12.img" "fat16.img" "fat32.img" "exfat.img")

function test_shell () {
	expect -c "
	set timeout 5
	spawn ./debugfatfs -iq $1
	expect \"/> \"
	send \"entry 0\n\"
	expect \"/> \"
	send \"entry 1\n\"
	expect \"/> \"
	send \"entry 2\n\"
	expect \"/> \"
	send \"entry 3\n\"
	expect \"/> \"
	send \"entry 4\n\"
	expect \"/> \"
	send \"entry 5\n\"
	expect \"/> \"
	send \"exit\n\"
	expect eof
	exit
	"
	echo ""
}

function test_options () {
	./debugfatfs -e 0 $1
	./debugfatfs -e 1 $1
	./debugfatfs -e 2 $1
	./debugfatfs -e 3 $1
	./debugfatfs -e 4 $1
	./debugfatfs -e 5 $1
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
