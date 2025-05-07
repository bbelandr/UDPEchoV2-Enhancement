function easyPDF(dataFileP, plotFlagP,maxXP, maxYP)
%
%
%  script:  easyPDF.m 
%
%  explanation: 
%    This was cloned from an earlier, simpler, version of plotDataPDF.m 
%
%    It will work with the usual defaults for ping:  pingmonRAW.dat and
%          its reduced file format RTT3Columns.dat 
%
%    It is a companion to easyPlot.m 
%
%    See DISPLAY PLOT or NOT DISPLAY PLOT 
%
%    This program plots the distribution associated with 
%    a data set contained in an input file.
%    Or it can plot the evolution of  a timeseries 
%
%   The norm use case assumes the data file consists of (x,y)
%    (Timestamp, sample )  where the sample is an observed sample of
%    a random variable X.  Let's assume we are talking about RTTs.
%   This script visualizes the PDF, CDF of the samples of X
% 
%   Assume the timestamps have been 'fixed' so they are relative to time 0
%   which is the timestamp of the first entry in the data  file.
% 
%   We assume the second field is in units mss
%
%   params:
%       dataFile:   name of the input data file.  The format must be:
%                timestamp  value   
%                A default is RTT3Columns.dat 
%                 Timestamp RTT seqNumber
%
%        There can be any number of fields following  seqNumber...
%        This script only looks at the first two fields  
%
%       plotFlag:  set to 0 to plot the CDF
%                         1 to plot the PDF (the default)
%                         2 to plot the X-Y data points
%
%       maxXP  sets the max allowed X value.  For example if you know the time spans from 0 to 100000 seconds
%                 setting this param to 100 will have this script look at the data up to 100 seconds.
%                 This sets the variable maxX
%              If not specified, maxX uses the entire range of X  
%
%       maxYP:    sets the max allowed value for the Y axis   (< 1) 
%                 This sets the variable maxY
%              If not specified, maxY is 1.0
%
%  invocation:    easyPDF()
%                 easyPDF(0)
%                 easyPDF(1, 50, 0.25)
%
%
%  Last updated:10/4/24
%
%

%if 1, we issue plots to autoscale
%if a max X or Y param is input, we set this to 0
%HACK FO RNOW ENABLE IT
autoScale=1;
fprintf(1,'easyPDF  Entered with nargin:%d, enabling AUTOSCALE  \n', nargin);

% Summary of  max and  min variables.
% The goal is to pass 2 parameters that will be the maxX and maxY  values plotted.
% Assume we are plotting (x,y) where x is time and y is a measure such at RTT 
% For now I am not using the last two params 
% I set maxX and maxY  in the code 
DEFAULT_MAX_X= 100;
%DEFAULT_MAX_Y= 1.0;
DEFAULT_MAX_Y= 0.05;


%HACK: A1 :  HAVE autoSize override maxX and maxY at the time of plot

%The following adjusts input params based on current state of this script....
if nargin < 1
  dataFileP='RTT3Columns.dat';
  plotFlagP=1;
  maxXP=0;
  maxYP=1;
end

if nargin < 2
  plotFlagP=1;
  maxXP=0;
  maxYP=1;
end

if nargin < 3
  maxXP=0;
  maxYP=1;
end

if nargin < 4
  maxYP=1;
end


  dataFile=dataFileP;
  plotFlag=plotFlagP;
  maxX = maxXP;
  minX = 0;
  maxY = maxYP;
  minY = 0;

fprintf(1,'easyPDF (nargin:%d): dataFile:%s plotFlag:%d  maxX:%f,  maxY:%f \n', nargin,dataFile, plotFlag, maxX,maxY);


rawData = load (dataFile);


sizeOfData=size(rawData);
sampleSize=sizeOfData(1);
numberFields=sizeOfData(2);
startTime = rawData(1,1);
timeStamps=rawData(:,[1]);

firstRTT = rawData(1,2);
dataSet=rawData(:,[2]);


fprintf(1,'easyPDF numberFields:%d startTime:%f firstRTT:%f sampleSize:%d \n', numberFields, startTime, firstRTT, sampleSize);



%obtain stats of the dataset
meanStat = mean(dataSet);
medianStat = median(dataSet);
stdStat = std(dataSet);
maxStat= max(dataSet);
minStat= min(dataSet);
y=prctile(dataSet,[2.5 25 50 75 97.5]);

%To place all values greater than maxX in a single bin
%maxX=1000;
%Set to 97.5%
%Or to maxStat
%HACK A1  autoScale overrides this
if (maxX == 0)
  maxX=maxStat;
%  maxX=y(5);
end

%maxY already set...


%counts number of samples we limit....
maxCount=0;
%if we want to count the number of 0 values ...
zeroCount=0;
if (maxX > 0)
  for i = 1 : sampleSize
    if (dataSet(i) == 0)
      zeroCount=zeroCount+1;
    end
    if (dataSet(i) > maxX)
      dataSet(i) = maxX;
      maxCount=maxCount+1;
    end
  end
end


fprintf(1,'easyPDF  sampleSize:%d,zeroCount:%d maxCount:%d  \n', sampleSize,zeroCount, maxCount);


fprintf(1,'#samples:%d Mean:%6.6f milliseconds  median: %6.6f,  std: %6.6f, max:%6.6f, min:%6.6f, maxCount:%d zeroCnt:%d\n',sampleSize,meanStat,medianStat,stdStat,maxStat,minStat,maxCount,zeroCount);

fprintf(1,'percentiles (2.5 25 50 75 97.5): :%6.6f %6.6f %6.6f %6.6f %6.6f\n', y(1),y(2),y(3),y(4),y(5));

numberBins = 100;

%
%DISPLAY PLOT or NOT DISPLAY PLOT 
%Comment out to have the plot displayed - else just goes to file 
f = figure('visible','off');

[N,X] = hist(dataSet,numberBins);
N=N/sampleSize;
S=0;
for i = 1:numberBins
   S = S + N(i);
   CDF(i) =S;
end

if (plotFlag == 0)
  bar(X,CDF);
  hold on
  grid
  ylabel('Cumulative Distribution')
  xlabel('Delay (milliseconds)')
  title('CDF RTT Samples')
  if (autoScale == 0 )
    axis([0 maxX 0 maxY]);
  end
  grid
end

if (plotFlag == 1)
  bar(X,N);
  hold on
  grid
  ylabel('Probability density')
  xlabel('Delay (milliseconds)')
  title('PDF RTT Samples')
  if (autoScale == 0 )
    axis([0 maxX 0 maxY]);
  end
end

if (plotFlag == 2)

  dataTime = (rawData(:,[1]) - startTime);
  dataSample = (1000000 * rawData(:,[2]));

  plot(dataTime,dataSample,'k-');
  hold on
  plot(dataTime,dataSample,'k+');
  hold on
  grid;
  if (autoScale == 0 )
    axis([0 maxX 0 maxY]);
  end
  xlabel('Time (seconds) ')
  ylabel('RTT (milliseconds)');
end

saveas(1,'easyPDFFigure');
saveas(1,'easyPDFFigure.png');



