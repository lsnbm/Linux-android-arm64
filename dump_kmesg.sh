#!/system/bin/sh

su -c 'dmesg | grep "lsdriver" | grep -v "lsdriver: fault:" > /storage/emulated/0/kmesg.txt'