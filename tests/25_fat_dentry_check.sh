#!/bin/bash

set -eu -o pipefail
trap 'echo "ERROR: l.$LINENO, exit status = $?" >&2; exit 1' ERR

source tests/common.sh

function test_fat_dentry_short () {
	expect -c "
	set timeout 5
	spawn ./debugfatfs --no-update-checksum -iq fat16.img
	expect \"/> \"
	send \"dentry /00/FILE1.TXT\n\"
	expect \"DIR_NTRes:       0x00\"
	expect \"/> \"
	send \"dentry-set /00/FILE1.TXT fat.short.DIR_NTRes 0x12\n\"
	expect \"Set: FILE1.TXT fat.short.DIR_NTRes = 0x12\"
	expect \"/> \"
	send \"dentry /00/FILE1.TXT\n\"
	expect \"DIR_NTRes:       0x12\"
	expect \"/> \"
	send \"exit\n\"
	expect eof
	exit
	"
	echo ""
	sync
}

function test_fat_dentry_raw () {
	expect -c "
	set timeout 5
	spawn ./debugfatfs --no-update-checksum -iq fat32.img
	expect \"/> \"
	send \"dentry-raw /00/FILE1.TXT short 0x0c 1 0x34\n\"
	expect \"Set: FILE1.TXT short\"
	expect \"/> \"
	send \"dentry /00/FILE1.TXT\n\"
	expect \"DIR_NTRes:       0x34\"
	expect \"/> \"
	send \"exit\n\"
	expect eof
	exit
	"
	echo ""
	sync
}

function test_fat_dentry_lfn () {
	expect -c "
	set timeout 5
	spawn ./debugfatfs --no-update-checksum -iq fat12.img
	expect \"/> \"
	send \"dentry /01/ABCDEFGHIJKLMNOPQRSTUVWXYZ!\n\"
	expect \"lfn count:   2\"
	expect \"/> \"
	send \"dentry-set /01/ABCDEFGHIJKLMNOPQRSTUVWXYZ! fat.lfn0.LDIR_Chksum 0x00\n\"
	expect \"Set: ABCDEFGHIJKLMNOPQRSTUVWXYZ! fat.lfn0.LDIR_Chksum = 0x0\"
	expect \"/> \"
	send \"dentry /01/ABCDEFGHIJKLMNOPQRSTUVWXYZ!\n\"
	expect \"LDIR_Chksum:     0x00\"
	expect \"/> \"
	send \"exit\n\"
	expect eof
	exit
	"
	echo ""
	sync
}

function test_fat_dentry_update_checksum () {
	expect -c "
	set timeout 5
	spawn ./debugfatfs --no-update-checksum -iq fat12.img
	expect \"/> \"
	send \"dentry-set /01/ABCDEFGHIJKLMNOPQRSTUVWXYZ! fat.lfn0.LDIR_Chksum 0x00\n\"
	expect \"Set: ABCDEFGHIJKLMNOPQRSTUVWXYZ! fat.lfn0.LDIR_Chksum = 0x0\"
	expect \"/> \"
	send \"exit\n\"
	expect eof
	exit
	"
	echo ""
	sync

	expect -c "
	set timeout 5
	spawn ./debugfatfs --update-checksum -iq fat12.img
	expect \"/> \"
	send \"dentry-set /01/ABCDEFGHIJKLMNOPQRSTUVWXYZ! fat.short.DIR_NTRes 0x01\n\"
	expect \"Set: ABCDEFGHIJKLMNOPQRSTUVWXYZ! fat.short.DIR_NTRes = 0x1\"
	expect \"/> \"
	send \"dentry /01/ABCDEFGHIJKLMNOPQRSTUVWXYZ!\n\"
	expect \"LDIR_Chksum:     0xca\"
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
	init_image fat12.img fat16.img fat32.img

	test_fat_dentry_short
	test_fat_dentry_raw
	test_fat_dentry_lfn
	test_fat_dentry_update_checksum
}

### main function ###
main "$@"
