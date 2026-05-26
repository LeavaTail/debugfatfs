#!/bin/bash

set -eu -o pipefail
trap 'echo "ERROR: l.$LINENO, exit status = $?" >&2; exit 1' ERR

source tests/common.sh

IMAGES=("fat12.img" "fat16.img" "fat32.img")
SCRIPT=data.dat

function test_shell () {
	expect -c "
	set timeout 5
	spawn ./debugfatfs -iq $1
	expect \"/> \"
	send \"\n\"
	expect \"/> \"
	send \"cd\n\"
	expect \"/> \"
	send \"cd 00\n\"
	expect \"/00> \"
	send \"ls\n\"
	expect \"/00> \"
	send \"cd /01\n\"
	expect \"/01> \"
	send \"ls\n\"
	expect \"/01> \"
	send \"cd /02\n\"
	expect \"/02> \"
	send \"ls\n\"
	expect \"/02> \"
	send \"cd /00/DIR\n\"
	expect \"/00/DIR> \"
	send \"ls\n\"
	expect \"/00/DIR> \"
	send \"cd /\n\"
	expect \"/> \"
	send \"ls\n\"
	expect \"/> \"
	send \"exit\n\"
	expect eof
	exit
	"
	echo ""
	sync
}

function test_script () {
	printf "%s\n" \
		"# run shell commands from a script file" \
		"cd 00" \
		"stat FILE1.TXT" \
		"dentry-set FILE1.TXT fat.short.DIR_NTRes 0x12" \
		"dentry FILE1.TXT" > ${SCRIPT}

	OUTPUT=$(./debugfatfs -q --no-update-checksum --script ${SCRIPT} $1)
	echo "${OUTPUT}"
	echo "${OUTPUT}" | grep -F "Short Name:  FILE1.TXT"
	echo "${OUTPUT}" | grep -F "Dentry: FILE1.TXT"
	echo "${OUTPUT}" | grep -F "DIR_NTRes:       0x12"
}

function main() {
	require_command expect
	init_image "${IMAGES[@]}"

	for fs in ${IMAGES[@]}; do
		test_shell ${fs}
	done
	test_script fat12.img
}

### main function ###
main "$@"
