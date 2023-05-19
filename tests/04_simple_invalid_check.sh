#!/bin/bash

set -u

OUTPUT=data.dat

function test_options () {
	./debugfatfs --invalid $1 || return

	echo "ERROR: Mishandling the invalid options." >&2; exit 1
}

function test_shell () {
	expect -c "
	set timeout 5
	spawn ./debugfatfs -iq $1
	expect \"/> \"
	send \"cluster\n\"
	expect \"/> \"
	send \"entry\n\"
	expect \"/> \"
	send \"alloc\n\"
	expect \"/> \"
	send \"release\n\"
	expect \"/> \"
	send \"fat\n\"
	expect \"/> \"
	send \"create\n\"
	expect \"/> \"
	send \"remove\n\"
	expect \"/> \"
	send \"update\n\"
	expect \"/> \"
	send \"exit\n\"
	"
	echo ""
	expect -c "
	set timeout 5
	spawn ./debugfatfs -iq $1
	expect \"/> \"
	send \"cd A B\n\"
	expect \"/> \"
	send \"cluster A B\n\"
	expect \"/> \"
	send \"entry A B\n\"
	expect \"/> \"
	send \"alloc A B\n\"
	expect \"/> \"
	send \"release A B\n\"
	expect \"/> \"
	send \"fat A B C\n\"
	expect \"/> \"
	send \"create A B\n\"
	expect \"/> \"
	send \"remove A B\n\"
	expect \"/> \"
	send \"update A B\n\"
	expect \"/> \"
	send \"update A B\n\"
	expect \"/> \"
	send \"trim A\n\"
	expect \"/> \"
	send \"fill A B\n\"
	expect \"/> \"
	send \"nothing\n\"
	expect \"/> \"
	send \"exit\n\"
	"
	echo ""
}

### main function ###
test_options exfat.img
test_shell exfat.img
