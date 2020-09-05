#!/bin/bash

set -x

./debugfatfs ntfs.img || { echo "Failed case: OK"; exit 0; }
exit 1;
