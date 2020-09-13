#!/bin/bash

set -eu -o pipefail
trap 'echo "ERROR: l.$LINENO, exit status = $?" >&2; exit 1' ERR

function test_shell () {
	expect -c "
	set timeout 5
	spawn ./debugfatfs -iq $1
	expect \"/> \"
	send \"ls\n\"
	expect \"/> \"
	send \"exit\n\"
	"
}

### main function ###
test_shell fat12.img
test_shell fat16.img
test_shell fat32.img
test_shell exfat.img
