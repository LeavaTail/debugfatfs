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
	send \"cd 00\n\"
	expect \"/00> \"
	send \"cluster 5\n\"
	expect \"/00> \"
	send \"alloc 100\n\"
	expect \"/00> \"
	send \"release 100\n\"
	expect \"/00> \"
	send \"fat 101 0\n\"
	expect \"/00> \"
	send \"fat 101\n\"
	expect \"/00> \"
	send \"trim\n\"
	expect \"/00> \"
	send \"tail FILE1.TXT\n\"
	expect \"/00> \"
	send \"tail FILE2.TXT\n\"
	expect \"/00> \"
	send \"stat FILE1.TXT\n\"
	expect \"/00> \"
	send \"create SAMPLE00.TXT\n\"
	expect \"/00> \"
	send \"mkdir DIR00001.TXT\n\"
	expect \"/00> \"
	send \"remove FILE1.TXT\n\"
	expect \"/00> \"
	send \"rmdir DIR00001.TXT\n\"
	expect \"/00> \"
	send \"fill 5\n\"
	expect \"/00> \"
	send \"cd /\n\"
	expect \"/> \"
	send \"help\n\"
	expect \"/> \"
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
		test_shell ${fs}
	done
}

### main function ###
main "$@"
