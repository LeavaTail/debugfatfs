#!/bin/bash

set -eu -o pipefail
trap 'echo "ERROR: l.$LINENO, exit status = $?" >&2; exit 1' ERR

source tests/common.sh

IMAGE="exfat.img"
OUTPUT=data.dat

function test_exfat_command () {
	expect -c "
	set timeout 10
	spawn ./debugfatfs -iq ${IMAGE}
	expect \"/> \"
	send \"cd 00\n\"
	expect \"/00> \"
	send \"cluster 5\n\"
	expect \"/00> \"
	send \"alloc 100\n\"
	expect \"Alloc: cluster 100.\"
	expect \"/00> \"
	send \"release 100\n\"
	expect \"Release: cluster 100.\"
	expect \"/00> \"
	send \"fat 101 0\n\"
	expect \"Set: Cluster 101 is FAT entry 00000000\"
	expect \"/00> \"
	send \"fat 101\n\"
	expect \"Get: Cluster 101 is FAT entry 00000000\"
	expect \"/00> \"
	send \"tail FILE1.TXT\n\"
	expect \"/00> \"
	send \"stat FILE1.TXT\n\"
	expect \"File Name:   FILE1.TXT\"
	expect \"/00> \"
	send \"create SAMPLE00.TXT\n\"
	expect \"/00> \"
	send \"mkdir DIR00001.TXT\n\"
	expect \"/00> \"
	send \"remove FILE2.TXT\n\"
	expect \"/00> \"
	send \"rmdir DIR00001.TXT\n\"
	expect \"/00> \"
	send \"trim\n\"
	expect \"/00> \"
	send \"exit\n\"
	expect eof
	exit
	"
	echo ""
}

function main() {
	require_command expect
	init_image "${IMAGE}"

	./debugfatfs -r "${IMAGE}"
	./debugfatfs -ao "${OUTPUT}" "${IMAGE}"
	grep -F "Allocation Bitmap:" "${OUTPUT}"

	./debugfatfs -r "${IMAGE}" /00/FILE1.TXT

	OUTPUT=$(./debugfatfs -rqu a "${IMAGE}")
	echo "${OUTPUT}"
	echo "${OUTPUT}" | grep -F "Convert: a -> A"

	test_exfat_command
}

### main function ###
main "$@"

