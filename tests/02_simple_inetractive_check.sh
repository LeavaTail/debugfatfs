#!/bin/bash

set -eu -o pipefail
trap 'echo "ERROR: l.$LINENO, exit status = $?" >&2; exit 1' ERR

function test_shell () {
	expect -c "
	set timeout 5
	spawn ./debugfatfs -iq $1
	expect \"/> \"
	send \"cd 00_SIMPLE\n\"
	expect \"/> \"
	send \"ls\n\"
	expect \"/> \"
	send \"cd /01_LONGNAME\n\"
	expect \"/> \"
	send \"ls\n\"
	expect \"/> \"
	send \"cd /02_UNICODE\n\"
	expect \"/> \"
	send \"ls\n\"
	expect \"/> \"
	send \"cd /03_DELETE\n\"
	expect \"/> \"
	send \"ls\n\"
	expect \"/> \"
	send \"cluster 5\n\"
	expect \"/> \"
	send \"alloc 30\n\"
	expect \"/> \"
	send \"release 30\n\"
	expect \"/> \"
	send \"fat 2\n\"
	expect \"/> \"
	send \"cd /00_SIMPLE\n\"
	expect \"/> \"
	send \"create SAMPLE00.TXT\n\"
	expect \"/> \"
	send \"remove FILE.TXT\n\"
	expect \"/> \"
	send \"help\n\"
	expect \"/> \"
	send \"exit\n\"
	"
	echo ""
	sync
	sleep 5
}

function check_mount () {
	mkdir -p mnt
	sleep 5
	sudo mount $1 mnt
	mount
	ls -l mnt/00_SIMPLE

	if [ ! -e mnt/00_SIMPLE/SAMPLE00.TXT ]; then
		echo "SAMPLE00.TXT should be exist."
		ls mnt/00_SIMPLE
		sudo umount mnt
		exit 1
	fi

	if [ -e mnt/00_SIMPLE/FILE.TXT ]; then
		echo "FILE.TXT shouldn't be exist."
		ls mnt/00_SIMPLE
		sudo umount mnt
		exit 1
	fi

	echo "create/remove command is fine."
	sleep 5

	sudo umount mnt
	rmdir mnt
}

### main function ###
test_shell fat12.img
check_mount fat12.img
test_shell fat16.img
check_mount fat16.img
test_shell fat32.img
check_mount fat32.img
test_shell exfat.img
check_mount exfat.img
