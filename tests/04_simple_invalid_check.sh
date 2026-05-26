#!/bin/bash

set -u

source tests/common.sh

IMAGES=("fat12.img")
OUTPUT=data.dat

function test_options () {
	./debugfatfs --invalid $1 || return

	echo "ERROR: Mishandling the invalid options." >&2; exit 1
}

function test_shell () {
	expect -c "
	set timeout 5
	spawn ./debugfatfs -iq $1
	expect \"/> \"
	send \"cluster\n\"
	expect \"/> \"
	send \"alloc\n\"
	expect \"/> \"
	send \"release\n\"
	expect \"/> \"
	send \"fat\n\"
	expect \"/> \"
	send \"stat\n\"
	expect \"/> \"
	send \"create\n\"
	expect \"/> \"
	send \"mkdir\n\"
	expect \"/> \"
	send \"remove\n\"
	expect \"/> \"
	send \"unlink\n\"
	expect \"/> \"
	send \"tail\n\"
	expect \"/> \"
	send \"dentry\n\"
	expect \"/> \"
	send \"dentry-set\n\"
	expect \"/> \"
	send \"dentry-raw\n\"
	expect \"/> \"
	send \"exit\n\"
	expect eof
	exit
	"
	echo ""
	expect -c "
	set timeout 5
	spawn ./debugfatfs -iq $1
	expect \"/> \"
	send \"cd A B\n\"
	expect \"/> \"
	send \"cluster A B\n\"
	expect \"/> \"
	send \"alloc A B\n\"
	expect \"/> \"
	send \"release A B\n\"
	expect \"/> \"
	send \"fat A B C\n\"
	expect \"/> \"
	send \"stat A B\n\"
	expect \"/> \"
	send \"create A B\n\"
	expect \"/> \"
	send \"mkdir A B\n\"
	expect \"/> \"
	send \"remove A B\n\"
	expect \"/> \"
	send \"rmdir A B\n\"
	expect \"/> \"
	send \"trim A\n\"
	expect \"/> \"
	send \"fill A B\n\"
	expect \"/> \"
	send \"tail A B\n\"
	expect \"/> \"
	send \"dentry A B\n\"
	expect \"/> \"
	send \"dentry-set A B C D\n\"
	expect \"/> \"
	send \"dentry-raw A B C D E F\n\"
	expect \"/> \"
	send \"nothing\n\"
	expect \"/> \"
	send \"exit\n\"
	expect eof
	exit
	"
	echo ""
}

function test_dentry_invalid () {
	expect -c "
	set timeout 5
	spawn ./debugfatfs -iq $1
	expect \"/> \"
	send \"dentry-set /00/FILE1.TXT invalid 0x1\n\"
	expect \"invalid dentry field: invalid\"
	expect \"/> \"
	send \"dentry-set /00/FILE1.TXT fat.short.INVALID 0x1\n\"
	expect \"unsupported dentry field: fat.short.INVALID\"
	expect \"/> \"
	send \"dentry-set /00/FILE1.TXT fat.short.DIR_NTRes invalid\n\"
	expect \"invalid value: invalid\"
	expect \"/> \"
	send \"dentry-set /00/FILE1.TXT fat.short.DIR_NTRes 0x100\n\"
	expect \"value exceeds dentry field size: 0x100\"
	expect \"/> \"
	send \"dentry-set /00/NOFILE.TXT fat.short.DIR_NTRes 0x1\n\"
	expect \"File is not found.\"
	expect \"/> \"
	send \"dentry-set /01/ABCDEFGHIJKLMNOPQRSTUVWXYZ! fat.lfn[99].LDIR_Chksum 0x1\n\"
	expect \"invalid lfn index: 99\"
	expect \"/> \"
	send \"dentry-raw /00/FILE1.TXT short invalid 1 0x1\n\"
	expect \"invalid raw dentry argument.\"
	expect \"/> \"
	send \"dentry-raw /00/FILE1.TXT short 0 3 0x1\n\"
	expect \"invalid raw write size: 3\"
	expect \"/> \"
	send \"dentry-raw /00/FILE1.TXT short 0x20 1 0x1\n\"
	expect \"raw write exceeds dentry size.\"
	expect \"/> \"
	send \"dentry-raw /00/NOFILE.TXT short 0 1 0x1\n\"
	expect \"File is not found.\"
	expect \"/> \"
	send \"dentry-raw /00/FILE1.TXT invalid 0 1 0x1\n\"
	expect \"invalid dentry selector: invalid\"
	expect \"/> \"
	send \"exit\n\"
	expect eof
	exit
	"
	echo ""
}

function main() {
	require_command expect
	init_image "${IMAGES[@]}"

	for fs in ${IMAGES[@]}; do
		test_options ${fs}
		test_shell ${fs}
		test_dentry_invalid ${fs}
	done
}

### main function ###
main "$@"
