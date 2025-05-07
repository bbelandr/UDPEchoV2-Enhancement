function easyPlot(myFileP, minXP, maxXP, minYP, maxYP)
%
%  This is a simple example of plotting a  time series
%         timestamp      sample   seqNo
%    1727620148.344327927 0.989000000 1 
%
% Input: 
%   myFile:  The input file with data
%   myFil2:  A second file with data to plot on the same graph 
%   minX=minStat;
%   maxX=maxStat
%   maxY=maxYStat;
%
%  See dataNormalizer to adjust the values (e.g., ms's to secs)
%  See MAKE RELATIVE to not adjust the timestamps from absolute to relative to startTime
%   See DISPLAY PLOT or NOT DISPLAY PLOT 
%
% A1 1/17/25: Added max X Y and min X
%
%  function easyPlot(myFileP, minXP, maxXP, minYP, maxYP)
%  Example invocattt
% 
%   easyPlot('RTT3.dat', 0.0, 600.0, 0.50, 2.0)
%
% Last update: 1/17/2025
%
%

myFile2="RTT3.dat";

myFile="RTT3.dat";

numberGraphs=1;
if (nargin < 1)
  fprintf(1,'easyPlot <myfile>   <minXP> <maxXP> <minYP> <maxYP> ');
  return;
end

%Do not display if set to 0,   display if set to 1
DISPLAY_PLOT="0"

myFile=myFileP;
%Remove second file...
numberGraphs=1;


%A1
minX=0.0;
maxX=0.0;
minY=0.0;
maxY=0.0;

if (nargin > 1)
  minX=minXP;
end

if (nargin > 2)
  maxX=maxXP;
end

if (nargin > 3)
  minY=minYP;
end

if (nargin > 4)
  maxY=maxYP;
end
%function easyPlot(myFileP, minXP, maxXP, minYP, maxYP)

fprintf(1,'easyPlot: nargin:%d  About to plot  myFile: %s, minX:%f maxX:%f minY:%f maxY:%f   \n', nargin, myFile, minX, maxX, minY,  maxY);


%Assume the data is :  Timestamp sample seqno  where samples are times in ms
%We want to plot    (Timestamp,sample)
%To normalize time interval samples in ms to secs
%dataNormalizer=1000000;

rawData = load (myFile);
sizeOfData=size(rawData);
numberSamples=sizeOfData(1);
numberFields=sizeOfData(2);
startTime = rawData(1,1);

fprintf(1,'easyPlot: About to plot  myFile: %s, numberSamples%d numberFields:%d  maxX:%f maxY:%f   \n', myFile, numberSamples, numberFields, maxX, maxY);

%timestamps
tmpData=rawData(:,[1]);
%To normalize timestamps ... 
fprintf(1,'Make timestamps related to startTime:%f \n', startTime);
XData = (tmpData(:) - startTime);

%Samples
%tmpData=rawData(:,[2]);
%Adjust - put in seconds 
%YData = (dataNormalizer * tmpData(:));
YData=rawData(:,[2]);

SeqNoData=rawData(:,[3]);
curSeqNo=SeqNoData(1);
prevSeqNo=curSeqNo;
totalDrops=0.0;
totalArrivals=0.0;
gapSum=0.0;
numberGaps=0.0;

y=prctile(YData,[2.5 25 50 75 97.5]);
fprintf(1,'YData percentiles (2.5 25 50 75 97.5): :%6.6f %6.6f %6.6f %6.6f %6.6f\n', y(1),y(2),y(3),y(4),y(5));


%Take a pass through the data create
% gapData:   TS gap
dropArrayEncoding(1)=0.0;
totalArrivals=1;
for i = 2 : numberSamples
 totalArrivals++;
 %fix for after a seqno wrap...
 prevSeqNo = curSeqNo;
 curSeqNo =  SeqNoData(i);
 if (curSeqNo == 0)
   fprintf(1,'easyPlot: WARNING: After wrap?   i :%d curSeqNo:%d prevSeqNo:%d  curGap:%d \n',i, curSeqNo,prevSeqNo,curGap);
   curGap = 0;
 else
   curGap = curSeqNo - prevSeqNo - 1;
 end
 if (curGap < 0)
   fprintf(1,'easyPlot: WARNING: wrapped seqNo  - i :%d curSeqNo:%d prevSeqNo:%d  curGap:%d \n',i, curSeqNo,prevSeqNo,curGap);
   curGap=0;
   prevSeqNo=0;
 end 
 if (curGap == 0)
   dropArrayEncoding(i)= 0.0;
 else
   numberGaps++;
   encodedValue = maxY;
   totalDrops+=curGap;
   gapSum+=curGap;
   if (curGap > 1)
     encodedValue+=curGap*(maxY / 10.0);
     fprintf(1,'easyPlot:i:%d  curGap %d encodedValue:%f totalArrivals:%d totalDrops:%d \n',i, curGap, encodedValue, totalArrivals,totalDrops);
   end
   dropArrayEncoding(i)=encodedValue;
 end

%END FOR 
end

fprintf(1,' totalArrivals:%f  totalDrops:%f \n', totalArrivals,totalDrops);

if (totalArrivals > 0)
  avgLR=totalDrops / (totalDrops+totalArrivals);
else
  avgLR=0.0;
end

if (numberGaps > 0)
  avgMBL=gapSum /numberGaps;
else
  avgMBL=0.0;
end

fprintf(1,'easyPlot: :%d  Stats:  %4.3f %4.3f %9.0f %9.0f %9.0f \n',i, avgLR, avgMBL, totalArrivals, totalDrops, numberGaps);

DISPLAY_PLOT="0"
%    See DISPLAY PLOT or NOT DISPLAY PLOT 
%f = figure('visible','off');


fprintf(1,' -->> about to plot: set axis %f %f %f %f \n', minX, maxX, minY, maxY);
%function easyPlot(myFileP, minXP, maxXP, minYP, maxYP)

plot(XData, YData,'k-');
hold on
plot(XData, YData,'r+');
hold on
%A1
if ((maxX  > 0) && (maxY > 0) )
  fprintf(1,' -->> set axis %f %f %f %f \n', minX, maxX, minY, maxY);
  axis([minX maxX minY maxY]);
end
hold on 
grid;
xlabel('Time (seconds) ')
ylabel('Samples (seconds)');

saveas(1,'easyPlotFigure');
saveas(1,'easyPlotFigure.png');


return;













