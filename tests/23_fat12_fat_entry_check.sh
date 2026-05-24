#!/bin/bash

set -eu -o pipefail
trap 'echo "ERROR: l.$LINENO, exit status = $?" >&2; exit 1' ERR

source tests/common.sh

function test_fat12_entry_beyond_first_sector () {
	expect -c "
	set timeout 5
	spawn ./debugfatfs -iq fat12.img
	expect \"/> \"
	send \"fat 400 fff\n\"
	expect \"Set: Cluster 400 is FAT entry 00000fff\"
	expect \"/> \"
	send \"fat 400\n\"
	expect \"Get: Cluster 400 is FAT entry 00000fff\"
	expect \"/> \"
	send \"fat 400 0\n\"
	expect \"Set: Cluster 400 is FAT entry 00000000\"
	expect \"/> \"
	send \"fat 400\n\"
	expect \"Get: Cluster 400 is FAT entry 00000000\"
	expect \"/> \"
	send \"exit\n\"
	expect eof
	exit
	"
	echo ""
	sync
}

function main() {
	require_command expect
	init_image fat12.img
	test_fat12_entry_beyond_first_sector
}

### main function ###
main "$@"
