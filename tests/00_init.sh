#!/bin/bash

set -eu -o pipefail
trap 'echo "ERROR: l.$LINENO, exit status = $?" >&2; exit 1' ERR

# md5sum
declare -A images=(
	["fat12"]="4e6295437cb89e037e2ed2da0e905223"
	["fat16"]="58caf1854910887b39e30187ec0848d0"
	["fat32"]="353aa3c5111f0ed8ca1db8ceda06c163"
	["exfat"]="02c3746447149897b941f000d3f89690"
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
