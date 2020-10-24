#!/bin/bash
set -eu -o pipefail
trap 'echo "ERROR: l.$LINENO, exit status = $?" >&2; exit 1' ERR

if [ $# -ne 1 ]; then
	echo "Usage: $0 version "
	exit 1
fi

FILE=CHANGELOG.md
VER=`echo $1 | tr -d "refs/tags/"`
VERSION=`echo ${VER} | tr -d v`

grep ${VERSION} ${FILE} >/dev/null 2>&1
read sline eline <<< \
	$( grep -n "^## " ${FILE} | \
	awk -F: -v version=${VERSION} '/'"${VERSION}"'/ \
		{ start = $1 + 1; getline; end = $1 - 1 } \
		END { print start, end }' )

sed -n ${sline},${eline}p ${FILE}
