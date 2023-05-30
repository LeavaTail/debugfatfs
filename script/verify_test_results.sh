#!/bin/bash
set -u

if [ $# -ne 1 ]; then
	echo "Usage: $0 test_dir "
	exit 1
fi

TEST_DIR=${1}


function interactive_ok () {
	MSG="spawn id exp. not open"

	echo "========================================"
	echo "Check strings..."
	echo "========================================"

	RET1=$(grep -E "${MSG}" -rl $TEST_DIR/*.log)
	if [ -z "${RET1}" ]; then
		return
	fi

	for log in "${RET1}"; do
		tail -n+1 ${log}
	done

	exit 1
}


function main () {
	interactive_ok
}

### main function ###
main "$@"

