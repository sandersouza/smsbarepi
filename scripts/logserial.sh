#!/usr/bin/env bash
LOG="/tmp/rpiserial.log"
DEV="/dev/cu.usbserial-21230"
BAUD="115200"

if [ -f "$LOG" ]; then
    echo "Clearing serial log $LOG"
    rm -Rf $LOG
fi

echo "Logging serial output to $LOG"
TERM=xterm minicom -o -D $DEV -b $BAUD -8 -C $LOG