#!/bin/bash

set -eu -o pipefail
trap 'echo "ERROR: l.$LINENO, exit status = $?" >&2; exit 1' ERR

function test_shell () {
	expect -c "
	set timeout 5
	spawn ./debugfatfs -iq $1
	expect \"/> \"
	send \"cd\n\"
	expect \"/> \"
	send \"cd 00_SIMPLE\n\"
	expect \"00_SIMPLE> \"
	send \"ls\n\"
	expect \"00_SIMPLE> \"
	send \"cd /01_LONGNAME\n\"
	expect \"/01_LONGNAME> \"
	send \"ls\n\"
	expect \"/01_LONGNAME> \"
	send \"cd /02_UNICODE\n\"
	expect \"/02_UNICODE> \"
	send \"ls\n\"
	expect \"/02_UNICODE> \"
	send \"cd /03_DELETE\n\"
	expect \"/03_DELETE> \"
	send \"ls\n\"
	expect \"/03_DELETE> \"
	send \"cluster 5\n\"
	expect \"/03_DELETE> \"
	send \"alloc 100\n\"
	expect \"/03_DELETE> \"
	send \"release 100\n\"
	expect \"/03_DELETE> \"
	send \"fat 101 0\n\"
	expect \"/03_DELETE> \"
	send \"fat 101\n\"
	expect \"/03_DELETE> \"
	send \"cd /00_SIMPLE\n\"
	expect \"/00_SIMPLE> \"
	send \"create SAMPLE00.TXT\n\"
	expect \"/00_SIMPLE> \"
	send \"remove FILE.TXT\n\"
	expect \"/00_SIMPLE> \"
	send \"fill\n\"
	expect \"/00_SIMPLE> \"
	send \"entry 1\n\"
	expect \"/00_SIMPLE> \"
	send \"help\n\"
	expect \"/00_SIMPLE> \"
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
