#!/bin/bash
echo " === before clean up === "
echo "~~~~~~ ls -l /tmp"
ls -l /tmp
echo "~~~~~~ ls -l /dev/shm"
ls -l /dev/shm
echo " === clean up ==="
rm -rf /tmp/*
rm -rf /dev/shm/*
ls -l /tmp
ls -l /dev/shm/
echo " === done ==="
