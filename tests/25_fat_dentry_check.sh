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

function test_exfat_dentry () {
	expect -c "
	set timeout 5
	spawn ./debugfatfs --no-update-checksum -iq exfat.img
	expect \"/> \"
	send \"dentry /00/FILE1.TXT\n\"
	expect \"SetChecksum:               0x8fa4\"
	expect \"/> \"
	send \"dentry-set /00/FILE1.TXT exfat.file.FileAttributes 0x21\n\"
	expect \"Set: FILE1.TXT exfat.file.FileAttributes = 0x21\"
	expect \"/> \"
	send \"dentry /00/FILE1.TXT\n\"
	expect \"FileAttributes:            0x0021\"
	expect \"/> \"
	send \"exit\n\"
	expect eof
	exit
	"
	echo ""
	sync
}

function test_exfat_dentry_raw () {
	expect -c "
	set timeout 5
	spawn ./debugfatfs --no-update-checksum -iq exfat.img
	expect \"/> \"
	send \"dentry-raw /00/FILE1.TXT stream 0x01 1 0x03\n\"
	expect \"Set: FILE1.TXT stream\"
	expect \"/> \"
	send \"dentry /00/FILE1.TXT\n\"
	expect \"GeneralSecondaryFlags:     0x03\"
	expect \"/> \"
	send \"exit\n\"
	expect eof
	exit
	"
	echo ""
	sync
}

function test_exfat_dentry_name_and_invalid () {
	expect -c "
	set timeout 5
	spawn ./debugfatfs --no-update-checksum -iq exfat.img
	expect \"/> \"
	send \"dentry-set /00/FILE1.TXT exfat.name\\[0\\].GeneralSecondaryFlags 0x01\n\"
	expect \"Set: FILE1.TXT exfat.name\\[0\\].GeneralSecondaryFlags = 0x1\"
	expect \"/> \"
	send \"dentry-raw /00/FILE1.TXT name0 0x01 1 0x02\n\"
	expect \"Set: FILE1.TXT name0\"
	expect \"/> \"
	send \"dentry /00/NOFILE.TXT\n\"
	expect \"File is not found.\"
	expect \"/> \"
	send \"dentry-set /00/FILE1.TXT invalid 0x1\n\"
	expect \"invalid dentry field: invalid\"
	expect \"/> \"
	send \"dentry-set /00/FILE1.TXT exfat.file.INVALID 0x1\n\"
	expect \"unsupported dentry field: exfat.file.INVALID\"
	expect \"/> \"
	send \"dentry-set /00/FILE1.TXT exfat.file.FileAttributes invalid\n\"
	expect \"invalid value: invalid\"
	expect \"/> \"
	send \"dentry-set /00/NOFILE.TXT exfat.file.FileAttributes 0x20\n\"
	expect \"File is not found.\"
	expect \"/> \"
	send \"dentry-set /00/FILE1.TXT exfat.name\\[99\\].GeneralSecondaryFlags 0x1\n\"
	expect \"invalid name index: 99\"
	expect \"/> \"
	send \"dentry-raw /00/FILE1.TXT stream invalid 1 0x1\n\"
	expect \"invalid raw dentry argument.\"
	expect \"/> \"
	send \"dentry-raw /00/FILE1.TXT stream 0 3 0x1\n\"
	expect \"invalid raw write size: 3\"
	expect \"/> \"
	send \"dentry-raw /00/FILE1.TXT stream 0x20 1 0x1\n\"
	expect \"raw write exceeds dentry size.\"
	expect \"/> \"
	send \"dentry-raw /00/NOFILE.TXT stream 0 1 0x1\n\"
	expect \"File is not found.\"
	expect \"/> \"
	send \"dentry-raw /00/FILE1.TXT invalid 0 1 0x1\n\"
	expect \"invalid dentry selector: invalid\"
	expect \"/> \"
	send \"dentry-raw /00/FILE1.TXT name0 0x01 1 0x00\n\"
	expect \"Set: FILE1.TXT name0\"
	expect \"/> \"
	send \"exit\n\"
	expect eof
	exit
	"
	echo ""
	sync
}

function test_exfat_dentry_update_checksum () {
	expect -c "
	set timeout 5
	spawn ./debugfatfs --no-update-checksum -iq exfat.img
	expect \"/> \"
	send \"dentry-set /00/FILE1.TXT exfat.file.SetChecksum 0x0000\n\"
	expect \"Set: FILE1.TXT exfat.file.SetChecksum = 0x0\"
	expect \"/> \"
	send \"exit\n\"
	expect eof
	exit
	"
	echo ""
	sync

	expect -c "
	set timeout 5
	spawn ./debugfatfs --update-checksum -iq exfat.img
	expect \"/> \"
	send \"dentry-set /00/FILE1.TXT exfat.stream.GeneralSecondaryFlags 0x01\n\"
	expect \"Set: FILE1.TXT exfat.stream.GeneralSecondaryFlags = 0x1\"
	expect \"/> \"
	send \"dentry-set /00/FILE1.TXT exfat.file.FileAttributes 0x20\n\"
	expect \"Set: FILE1.TXT exfat.file.FileAttributes = 0x20\"
	expect \"/> \"
	send \"dentry /00/FILE1.TXT\n\"
	expect \"SetChecksum:               0x8fa4\"
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
	init_image fat12.img fat16.img fat32.img exfat.img

	test_fat_dentry_short
	test_fat_dentry_raw
	test_fat_dentry_lfn
	test_fat_dentry_update_checksum
	test_exfat_dentry
	test_exfat_dentry_raw
	test_exfat_dentry_name_and_invalid
	test_exfat_dentry_update_checksum
}

### main function ###
main "$@"
