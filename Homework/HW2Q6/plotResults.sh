#!/bin/bash
#####################################################
#
# script: plotResults.sh : 
#
# Explanation: This script plots the time series contained in the 
#              first parameter 
#
# Last update  9/28/2024 
#
####################################################

# To have the script quit on first error- otherwise it ignores all errors
set -e
#
#Turn on debugging
#set -x
#
#If a script  is not behaving, try :  bash -u script.sh #
#Also use shellcheck -  super helpful


myDate=$(date)

if [ $# -lt "1"  ]  ; then
   echo "$0:  <inputFile>  "
   exit 
fi

myFile="$1"

echo "$0: Ploting $myFile "  
echo -e " ---->>>>Next, call octave easyPDF ---- see easyPDFFigure.png   " 
octave --persist --eval "easyPDF('$myFile')"
#octave --persist --eval "easyPDF('RTT3.dat')"
#Produces easyPDFFigure.png

exit


echo -e " ---->>>>Next, call octave easyPlot ---- see easyPlotFigure.png   " 
#octave --persist --eval "easyPlot('RTT3.dat')"
#easyPlot('RTT3.dat', 0.0, 600.0, 0.50, 2.0)
#octave --eval "easyPlot('RTT3.dat')"
octave --eval "easyPlot('$myFile')"
#PRoduces easyPlotFigure.png

exit 

echo -e " ---->>>>Next, call octave easyPDF ---- see easyPDFFigure.png   " 
octave --eval "easyPDF('$myFile')"
#octave --persist --eval "easyPDF('RTT3.dat')"
#Produces easyPDFFigure.png


exit

