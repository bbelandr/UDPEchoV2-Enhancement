#!/bin/bash
#####################################################
#
# script: runTests.sh : 
#
# Explanation: This automates tests using the tool 
#
# Inputs:
#   numberOfTests:  specifies the number of tests to perform
#
# Last update  4/1/2025 
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
   echo "$0:  <numberOfTests>  "
   exit 
fi

numberOfTests="$1"

testOutLog="test.log"
resultsOutputFile="serverResults.dat"

echo "$0:$myDate: Running  $numberOfTests,  server results output:$resultsOutputFile  and this script output file: $testOutLog" 
echo "$0:$myDate: Running  $numberOfTests,  server results output:$resultsOutputFile  and this script output file: $testOutLog"   > $testOutLog

echo "duration meanOWD  minOWD  maxOWD  avgTh   avgLR2  avgGapSz  avgLER   numOfGps   totLost2    avgLR1   totLost1   rxCount " > $resultsOutputFile 
echo "" &>>$testOutLog 

sleep 1
#First test 

echo "" &>>$testOutLog 
echo "$0:$myDate:  TEST 1::  client localhost 6205 0.0 60000 100000 1 100000000000 "   &>>$testOutLog 
echo "" &>>$testOutLog 
nohup ./server 6205 &>>$testOutLog  &  >/dev/null 
./client localhost 6205 0.0 60000 100000 1 100000000000   &>>$testOutLog 
cat  serverResults.dat
sleep 1

echo "" &>>$testOutLog 
echo "$0:$myDate:  TEST 2:  client localhost 6205 0.0 60000 100000 1 50000000000 "   &>>$testOutLog 
echo "" &>>$testOutLog 
nohup ./server 6205 &>>$testOutLog  &  >/dev/null 
./client localhost 6205 0.0 60000 100000 1 50000000000   &>>$testOutLog 
cat  serverResults.dat
sleep 1

echo "" &>>$testOutLog 
echo "$0:$myDate:  TEST 3:  client localhost 6205 0.0 60000 100000 1 30000000000 "   &>>$testOutLog 
echo "" &>>$testOutLog 
nohup ./server 6205 &>>$testOutLog  &  >/dev/null 
./client localhost 6205 0.0 60000 100000 1 30000000000   &>>$testOutLog 
cat  serverResults.dat
sleep 1

echo "" &>>$testOutLog 
echo "$0:$myDate:  TEST 4::  client localhost 6205 0.0 60000 100000 1 20000000000 "   &>>$testOutLog 
echo "" &>>$testOutLog 
nohup ./server 6205 &>>$testOutLog  &  >/dev/null 
./client localhost 6205 0.0 60000 100000 1 20000000000   &>>$testOutLog 
cat  serverResults.dat

sleep 1

ps aux | grep "6205"

echo "" &>>$testOutLog 
echo "$0:$myDate:  TEST 5::  client localhost 6205 0.0 60000 100000 1 10000000000 "   &>>$testOutLog 
echo "" &>>$testOutLog 
nohup ./server 6205 &>>$testOutLog  &  >/dev/null 
./client localhost 6205 0.0 60000 100000 1 12000000000   &>>$testOutLog 
cat  serverResults.dat
sleep 1


exit


cp  gapArray.dat /tmp/UDPEchoV2/gapArray1.dat
#cp  serverSamplesArray.dat /tmp/UDPEchoV2/samplesArray1.dat 


#Second test 
echo "" &>>$testOutLog 
echo "$0:$myDate:  SECOND TEST:  client localhost 6205 0.0 1472 1000000 1 1000000000"   &>>$testOutLog 
echo "" &>>$testOutLog 
nohup ./server 6205 &>>$testOutLog  &  >/dev/null 
./client localhost 6205 0.0 1472 1000000 1 1000000000   &>>$testOutLog 
cp  gapArray.dat /tmp/UDPEchoV2/gapArray2.dat
#cp  serverSamplesArray.dat /tmp/UDPEchoV2/samplesArray2.dat 
 
#Third test 
echo "" &>>$testOutLog 

echo "$0:$myDate:  THIRD TEST:  client localhost 6205 0.0 32 1000000 1 1000000000 "  &>>$testOutLog 
echo "" &>>$testOutLog 
nohup ./server 6205 &>>$testOutLog  &  >/dev/null 
./client localhost 6205 0.0 32 1000000 1 1000000000   &>>$testOutLog 

cp  gapArray.dat /tmp/UDPEchoV2/gapArray3.dat
#cp  serverSamplesArray.dat /tmp/UDPEchoV2/samplesArray3.dat 
 
echo "" &>>$testOutLog 
echo "$0:$myDate: COMPLETED  "  &>>$testOutLog 


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

