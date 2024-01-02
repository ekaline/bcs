#!/bin/sh

echo "base=0x92c00000 size= 1MB type=write-combining" >| /proc/mtrr
