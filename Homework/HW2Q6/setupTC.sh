#!/bin/bash
#####################################################
#
# script: setupTC.sh 
#
#    echo "$0:$#:  Usage:  <latency (ms)>  < loss (percentage : 2.0)>  <IF name> <opCode> "
#
# This script illustrates how to setup an emulated latency 
# and loss process over a specified network IF.
#
#    latency :  one way latency in ms
#    loss rate : average loss rate as a percentage
#    Interface name:  IF name 
#    Optional fourth param :  opCode
#          c :   set up a codel qdisc
#          r :   reset as it originally did
#          s :   show the tc settings for the IF
#
#  Example usage: 30ms 0 loss localhost
#        sudo ./setupTC.sh 30 0 lo 
#
#  To remove the setting:
#     sudo ./setupTC.sh 0 0 lo r 
#
#  To show:
#     sudo ./setupTC.sh 0 0 enp0s8 s 
#
#  To apply codel qdisc
#     sudo ./setupTC.sh 0 0 enp0s8  c
#
#
# NOTE:  
#   To add rate rate control and latency:
#      sudo tc qdisc add dev lo root handle 1: htb default 12 
#      sudo tc class add dev lo parent 1:1 classid 1:12 htb rate 56kbps ceil 128kbps 
#      sudo tc qdisc add dev lo parent 1:12 netem delay 1000ms
#
# and this returns true if last call succeeds  if [ $? -eq 0 ]
#
#
#   NOTE   TODO:  support correlated loss models, RATE and other netem capabilities
#
# HISTORY:
#
#   A1:  fixed problem with calling tc with a loss rate of 0.  Now if we specify 0
#          we call tc without setting any loss.
#   A2:  12/9/24: bugs .... !!
#
#   A3:   Added netem rate 
#         Add 4th param opCode :    r s
#
# Last update  4/8/2025
#
#
############################################################################

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



#init params
Version="v0.9"
myLatency="10ms"
myLoss="6%"
lossFlag=0
myIF=""
param4="r"


echo "$0:$#:Version:$Version: Entered with $# parameters " 

#sudo tc qdisc show dev enp0s8 
#sudo tc qdisc add dev enp0s8 root codel limit 1000  interval 30ms
#sudo tc qdisc del dev enp0s8 root


myRate="0"

# ensure at least  args were passed
if [ $# -lt "3"  ]
  then
   echo "$0: Usage:  <latency (ms)>  < loss (percentage : 2.0)>  <IF name> <opCode> "
   echo "   opCode:   c (add codel)  r (reset qdisc) s (show qdisc )"
   echo " "
   echo "   -->> $0 30 6 lo  "
   echo "   To remove the tc setting, repeat the call but add any 4th param "
   echo "   -->> $0 30 6 lo r "
   echo " "
   echo "   To  add codel qdisc  "
   echo "   -->>  $0 0 0 lo c "
   exit -1
fi

  myLatency="$1ms"
  myLoss="$2"
  myIF="$3"
  myRate="10mbit"

  if [ $myLoss -gt 0 ]; then
      lossFlag=1
      myLoss="$2%"
  else
      lossFlag=0
      myLoss=""
  fi

  if [ $# -gt 3 ]; then
      param4="$4"
  fi


 echo " Params: $myLatency $myLoss $myIF $param4  " 

#Case 1:  handle removing the tc settings back to the default
#if there are 4 params we remove the tc setting for the IF
#  so the first two params can be 0 and 0 
if [ $# -gt 3 ]
  then  
  #
  echo "$0:$#: process 4th param:  $param4 "
  if [ "$param4" == "c" ]; then

    echo "$0:$#:  apply codel qdisc to $myIF "
    # tc qdisc ... codel [ limit PACKETS ] [ target TIME ] [ interval TIME ] [ ecn | noecn ] [ ce_threshold TIME ]
    sudo tc qdisc add dev $myIF root codel limit 100  target 2ms  interval 5ms
    #sudo tc qdisc add dev enp0s8 root codel limit 1000  interval 30ms
  fi
  if [ "$param4" == "r" ]; then
    echo "$0:$#: Remove tc setting on IF $myIF "
    sudo tc qdisc show
    sudo tc qdisc del dev $myIF root
    sudo tc qdisc show
  fi
  if [ "$param4" == "s" ]; then
    echo "$0:$#: Show  tc setting on IF $myIF "
    sudo tc -s qdisc show dev  "$myIF"
  fi
  exit
fi

#Otherwise, we have a latency and loss to set with netem...
if [ $lossFlag -gt 0 ] ; then
  echo "$0:$#: Setting loss rate: $myLoss latency: $myLatency Interface:$myIF "
else
  echo "$0:$#: Just set latency: $myLatency Interface:$myIF "
fi

#A1  
#loss can not be 0 
    if [ $lossFlag -gt 0 ]
     then
        echo " with loss ...$myLoss and myRate:$myRate "
        sudo tc qdisc add dev $myIF root netem limit 2048 delay $myLatency loss $myLoss
    else
        echo " without loss ... and myRate:$myRate "
        sudo tc qdisc add dev $myIF root netem limit 2048 delay $myLatency 
    fi
    #test for success or failure 
    if [ $? -eq 0 ]
       then
       echo "$0:$#: SUCEEDED    "
    else 
       echo "$0:$#: FAILED ??    "
    fi

exit 


# Google netem packet loss
#An optional correlation may also be added. This causes the random number generator to be less random and can be used to emulate packet burst losses.
# tc qdisc change dev eth0 root netem loss 0.3% 25%
# This will cause 0.3% of packets to be lost, and each successive probability depends by a quarter on the last one.
# Probn = .25 * Probn-1 + .75 * Random
#tc qdisc add dev eth0 root netem limit 100 delay 40ms loss 5% 25%
#tc qdisc add dev enp0s8 root netem limit 1000 delay $2ms loss $1%
#tc qdisc add dev enp0s8 root netem limit 1000 delay 30ms loss 10% 
#tc qdisc add dev enp0s17 root netem limit 1000 delay $myLatency loss $myLoss


echo "Setting loss rate of $myLoss latency of $myLatency over interface $myIF "
sudo tc qdisc add dev $myIF root netem limit 2048 delay $myLatency loss $myLoss


