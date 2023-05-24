#!/bin/bash

set -eu -o pipefail
trap 'echo "ERROR: l.$LINENO, exit status = $?" >&2; exit 1' ERR

source tests/common.sh

IMAGES=("fat12.img" "fat16.img" "fat32.img" "exfat.img")

function test_allocate () {
	expect -c "
	set timeout 5
	spawn ./debugfatfs -iq $1
	expect \"/> \"
	send \"cd 00\n\"
	expect \"/00> \"
	send \"fill\n\"
	expect \"/00> \"
	send \"cd /00\n\"
	expect \"/00> \"
	send \"create SAMPLE00.TXT\n\"
	expect \"/00> \"
	send \"exit\n\"
	expect eof
	exit
	"
	echo ""
	sync
}

function test_release () {
	expect -c "
	set timeout 5
	spawn ./debugfatfs -iq $1
	expect \"/> \"
	send \"cd /00\n\"
	expect \"/00> \"
	send \"remove SAMPLE00.TXT\n\"
	expect \"/00> \"
	send \"trim\n\"
	expect \"/00> \"
	send \"exit\n\"
	expect eof
	exit
	"
	echo ""
	sync
}

function main() {
	init_image

	for fs in ${IMAGES[@]}; do
		test_allocate ${fs}
		test_release ${fs}
	done
}

### main function ###
main "$@"

