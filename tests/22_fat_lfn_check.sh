#!/bin/bash

set -eu -o pipefail
trap 'echo "ERROR: l.$LINENO, exit status = $?" >&2; exit 1' ERR

source tests/common.sh

IMAGES=("fat12.img" "fat16.img" "fat32.img")

function test_lower () {
	expect -c "
	set timeout 5
	spawn ./debugfatfs -iq $1
	expect \"/> \"
	send \"cd 00\n\"
	expect \"/00> \"
	send \"create sample00.txt\n\"
	expect \"/00> \"
	send \"exit\n\"
	expect eof
	exit
	"
	echo ""
	sync
}

function test_mixed () {
	expect -c "
	set timeout 5
	spawn ./debugfatfs -iq $1
	expect \"/> \"
	send \"cd 00\n\"
	expect \"/00> \"
	send \"create SAMPLE00.txt\n\"
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
		test_lower ${fs}
		test_mixed ${fs}
	done
}

### main function ###
main "$@"

