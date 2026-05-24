#!/bin/bash

set -eu -o pipefail
trap 'echo "ERROR: l.$LINENO, exit status = $?" >&2; exit 1' ERR

source tests/common.sh

IMAGE="exfat.img"

function main() {
	init_image "${IMAGE}"

	./debugfatfs -r "${IMAGE}"
	./debugfatfs -r "${IMAGE}" /00/FILE1.TXT

	OUTPUT=$(./debugfatfs -rqu a "${IMAGE}")
	echo "${OUTPUT}"
	echo "${OUTPUT}" | grep -F "Convert: a -> A"
}

### main function ###
main "$@"

