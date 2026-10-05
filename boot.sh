#!/bin/sh
mount -t proc proc /proc 2>/dev/null
mount -t sysfs sys /sys 2>/dev/null
mount -t devtmpfs dev /dev 2>/dev/null
mount -o remount,rw / 2>/dev/null
dmesg -c >/dev/null 2>&1
insmod /home/user/mod/fuzz.ko 2>/home/user/INSMOD-ERR.txt
sleep 3
dmesg > /home/user/KEXEC-FUZZ-LOG.txt 2>&1
sync
exec /sbin/init
