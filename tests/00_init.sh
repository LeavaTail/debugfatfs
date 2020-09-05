#!/bin/bash

set -eu -o pipefail
trap 'echo "ERROR: l.$LINENO, exit status = $?" >&2; exit 1' ERR

for image in $( ls tests/sample | grep .tar.xz$ ); do
	tar -Jxf tests/sample/${image}
done
