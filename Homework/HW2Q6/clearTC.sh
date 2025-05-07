#!/bin/bash
#####################################################
#
# script: clearTC.sh 
#
# Explanation:  This script issues the tc command 
#             to remove the root qdisc on the input 
#              IF
#
# Last update  12/9/2024 
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


#####################################################
#
# script: function askToExit() {
#   Called to query the user to exit or proceed
#   This will do an exit if the user choses
#
#####################################################
function askToExit() {

rc=$NOERROR
choice="n"
echo "$0 Do you want to continue or quit ? (y to continue anything else quits) "
read choice
if [ "$choice" == "y" ]; then
  rc=$NOERROR
else
   echo "$0: ok...we will exit "
  exit
fi
 return $rc
}


myIF="lo"

echo "$0: about to issue tc qdisc del dev $myIF root " 
askToExit

# ensure at least  args were passed
if [ $# -lt "1"  ]
  then
   echo "$0: Usage:  <IF name>  "
   echo "   Ex:  $0 lo "
   exit -1
fi

  myLatency="$1ms"


myIF=$1
echo "$0: rmove qdisc for dev $myIF "


#sudo tc qdisc del dev enp3s0 root

tc qdisc show
tc qdisc del dev $myIF  root
tc qdisc show

exit 
