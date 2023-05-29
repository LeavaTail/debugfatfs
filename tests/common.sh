#!/bin/bash

# md5sum
declare -A images=(
	["fat12"]="4fae2d03cd312b9f69296a6ed53345ad"
	["fat16"]="1d63e0009104645628b91e90f46cb250"
	["fat32"]="27f1ad30b2a35c63883eefb42de63f46"
	["exfat"]="927ca233c0a75166ea3a11e4c2d2dc48"
)

function init_image() {
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
		echo "Initialize ${FSNAME} filesystem image"
		tar -xf tests/sample/${image}
	done
}
