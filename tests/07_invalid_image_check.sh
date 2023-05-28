#!/bin/bash

set -u

source tests/common.sh

function main() {
	./debugfatfs README.md || exit 0

	echo "ERROR: Mishandling the invalid options." >&2; exit 1
}

### main function ###
main "$@"
