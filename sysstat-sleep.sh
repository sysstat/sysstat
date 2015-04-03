#!/bin/sh
# System Activity Accounting Restart-on-Resume hack
# for systemd's /usr/lib/systemd/system-sleep/ directory


case $1 in
	pre)  systemctl stop  sysstat ;;
	post) systemctl start sysstat ;;
esac
