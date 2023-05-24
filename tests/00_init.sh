#!/bin/bash

set -eu -o pipefail
trap 'echo "ERROR: l.$LINENO, exit status = $?" >&2; exit 1' ERR

# md5sum
declare -A images=(
	["fat12"]="fcef9fc31b1b5b9c33236053a408e145"
	["fat16"]="dca95fc0446cdb62ba99ab1518196fa6"
	["fat32"]="342fe173ee5ad2797a69d3da412ec8bb"
	["exfat"]="81740a8e624141c1459a439db2311eb9"
)

for image in $( ls tests/sample | grep .tar.bz2$ ); do
	FSNAME=`echo ${image} | cut -d"." -f 1`
	HASH="${images[$(eval echo $FSNAME)]}"
	IMG=${FSNAME}.img
	if [ -e ${IMG} ]; then
		A=`md5sum ${IMG} | cut -d" " -f 1`
		if [ "$A" = "$HASH" ]; then
			continue
		fi
	fi
	echo "Extruct original filesystem image"
	tar -xf tests/sample/${image}
done
