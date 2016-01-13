#!/bin/bash
#
# Run through a series of tests to try out the various capability
# manipulations posible through exec.
#
# [Run this as root in a root-enabled process tree.]

try_capsh () {
    echo "TEST: ./capsh $*"
    ./capsh "$@"
    if [ $? -ne 0 ]; then
	echo FAILED
	return 1
    else
	echo PASSED
	return 0
    fi
}

fail_capsh () {
    echo -n "EXPECT FAILURE: "
    try_capsh "$@"
    if [ $? -eq 1 ]; then
	echo "[WHICH MEANS A PASS!]"
	return 0
    else
	echo "Undesired result - aborting"
	echo "PROBLEM TEST: $*"
	exit 1
    fi
}

pass_capsh () {
    echo -n "EXPECT SUCCESS: "
    try_capsh "$@"
    if [ $? -eq 0 ]; then
	return 0
    else
	echo "Undesired result - aborting"
	echo "PROBLEM TEST: $*"
	exit 1
    fi
}

pass_capsh --print


# Make a local non-setuid-0 version of capsh and call it privileged
cp ./capsh ./privileged && chmod -s ./privileged
if [ $? -ne 0 ]; then
    echo "Failed to copy capsh for capability manipulation"
    exit 1
fi

# Give it the forced capability it could need
./setcap all=ep ./privileged
if [ $? -ne 0 ]; then
    echo "Failed to set all capabilities on file"
    exit 1
fi
./setcap cap_setuid,cap_setgid=ep ./privileged
if [ $? -ne 0 ]; then
    echo "Failed to set limited capabilities on privileged file"
    exit 1
fi

# Explore keep_caps support
pass_capsh --keep=0 --keep=1 --keep=0 --keep=1 --print

rm -f tcapsh
cp capsh tcapsh
chown root.root tcapsh
chmod u+s tcapsh
ls -l tcapsh

# leverage keep caps maintain capabilities accross a change of uid
# from setuid root to capable luser (as per wireshark/dumpcap 0.99.7)
pass_capsh --uid=500 -- -c "./tcapsh --keep=1 --caps=\"cap_net_raw,cap_net_admin=ip\" --uid=500 --caps=\"cap_net_raw,cap_net_admin=pie\" --print"

# This fails, on 2.6.24, but shouldn't
pass_capsh --uid=500 -- -c "./tcapsh --keep=1 --caps=\"cap_net_raw,cap_net_admin=ip\" --uid=500 --forkfor=10 --caps= --print --killit=9 --print"

# only continue with these if --secbits is supported
./capsh --secbits=0x2f > /dev/null 2>&1
if [ $? -ne 0 ]; then
    echo "unable to test securebits manipulation - assume not supported (PASS)"
    rm -f tcapsh
    rm -f privileged
    exit 0
fi

pass_capsh --secbits=42 --print
fail_capsh --secbits=32 --keep=1 --keep=0 --print
pass_capsh --secbits=10 --keep=0 --keep=1 --print
fail_capsh --secbits=47 -- -c "./tcapsh --user=nobody"

rm -f tcapsh

# Suppress uid=0 privilege
fail_capsh --secbits=47 --print -- -c "./capsh --user=nobody"

# suppress uid=0 privilege and test this privileged
pass_capsh --secbits=0x2f --print -- -c "./privileged --user=nobody"

# observe that the bounding set can be used to suppress this forced capability
fail_capsh --drop=cap_setuid --secbits=0x2f --print -- -c "./privileged --user=nobody"

# change the way the capability is obtained (make it inheritable)
./setcap cap_setuid,cap_setgid=ei ./privileged

# Note, the bounding set (edited with --drop) only limits p
# capabilities, not i's.
pass_capsh --secbits=47 --inh=cap_setuid,cap_setgid --drop=cap_setuid \
    --uid=500 --print -- -c "./privileged --user=nobody"

rm -f ./privileged

# test that we do not support capabilities on setuid shell-scripts
cat > hack.sh <<EOF
#!/bin/bash
mypid=\$\$
caps=\$(./getpcaps \$mypid 2>&1 | cut -d: -f2)
if [ "\$caps" != " =" ]; then
  echo "Shell script got [\$caps] - you should upgrade your kernel"
  exit 1
else
  ls -l \$0
  echo "Good, no capabilities [\$caps] for this setuid-0 shell script"
fi
exit 0
EOF
chmod +xs hack.sh
./capsh --uid=500 --inh=none --print -- ./hack.sh
status=$?
rm -f ./hack.sh
if [ $status -ne 0 ]; then
    echo "shell scripts can have capabilities (bug)"
    exit 1
fi

# Max lockdown
pass_capsh --keep=1 --user=nobody --caps=cap_setpcap=ep \
    --drop=all --secbits=0x2f --caps= --print

# Verify we can chroot
pass_capsh --chroot=$(/bin/pwd)
pass_capsh --chroot=$(/bin/pwd) ==
fail_capsh --chroot=$(/bin/pwd) -- -c "echo oops"
