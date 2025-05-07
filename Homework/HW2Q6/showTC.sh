#!/bin/bash
#####################################################
#
# script: showTC.sh 
#
#       This script simples issues a tc show 
#
# Last Update: 12/9/2024
#
#####################################################

# Useful examples:
# To have the script quit on first error- otherwise it ignores all errors
set -e
#
#Turn on debugging
#set -x
#
#If a script  is not behaving, try :  bash -u script.sh #
#Also use shellcheck -  super helpful
# A script can call set to set options


if [ $# -gt 0 ] ; then
  echo "$0: issue tc qdisc show dev $1 "
  sudo tc -s qdisc show dev  "$1"
else
  echo "$0: issue tc qdisc show "
  tc -s qdisc show
fi 


exit 
