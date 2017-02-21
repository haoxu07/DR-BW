%% Main function
function DR_BW_Decision_Tree()
    [dataX,dataY]=dataPrepare;
    featureSelected=[1:13];
    dataX=TenfoldCrossValidation(dataX, featureSelected,dataY);
    Mdl = fitctree(dataX,dataY);
    testModel(Mdl,featureSelected);
    
end

 %% Prepare the data for training and validation
function [dataX,dataY]=dataPrepare
    load('D:/Dropbox/Project/Bandwidth/Mat/SampleForTraining_sumv.mat')
    load('D:/Dropbox/Project/Bandwidth/Mat/SampleForTraining_countv.mat')
    load('D:/Dropbox/Project/Bandwidth/Mat/SampleForTraining_dotv.mat')
    load('SampleForTraining_bandit.mat')
    bandit_new=zeros(48,17);
    bandit_new(:,1:14) = bandit(:,1:14);
    bandit_new(:,17) = 0;
    SampleForTraining = [sumv;countv;dotv;bandit_new(1:48,:)];
    dataX =SampleForTraining(:,1:14);
    dataY =SampleForTraining(:,17);
    RDRAMLatencyRatio =SampleForTraining(:,12).*SampleForTraining(:,11)./(SampleForTraining(:,10).*SampleForTraining(:,9));
    LFBLatencyRatio = SampleForTraining(:,14).*SampleForTraining(:,13)./(SampleForTraining(:,10).*SampleForTraining(:,9));
    dataX=[dataX, RDRAMLatencyRatio, LFBLatencyRatio];
end
 %% Use 10-fold validation to verfify the algorithm
function dataX=TenfoldCrossValidation(dataX, featureSelected,dataY)
     dataX=dataX(:,featureSelected);
    TotalNum=length(dataY);
    indexShuffle=randperm(TotalNum);
    indexShuffleMapping=zeros(size(indexShuffle));
    for i=1:length(indexShuffle)
        indexShuffleMapping(indexShuffle(i))=i;
    end

    ValidNum=10;
    errorNum=0;
    errorIndexFalsePositive=[];
    errorIndexFalseNegative=[];
    for iValid=1:ValidNum
        indexTrain=indexShuffle([1:ceil(TotalNum*(iValid-1)/ValidNum),ceil(TotalNum*iValid/ValidNum)+1:TotalNum]);
        indexTest=indexShuffle(1+ceil(TotalNum*(iValid-1)/ValidNum):ceil(TotalNum*iValid/ValidNum));
        dataXTrain=dataX(indexTrain,:);
        dataXTest=dataX(indexTest,:);
        dataYTrain=dataY(indexTrain,:);
        dataYTest=dataY(indexTest,:);

        Mdl = fitctree(dataXTrain,dataYTrain);
        newY = predict(Mdl,dataXTest);
        errorNum=errorNum+sum(abs(newY-dataYTest))

        for i=1:length(dataYTest)
            if (newY(i)~=dataYTest(i) && 1==dataYTest(i))
                errorIndexFalsePositive=[errorIndexFalsePositive,indexShuffleMapping(ceil(TotalNum*(iValid-1)/ValidNum)+i)];
            elseif(newY(i)~=dataYTest(i) && 0==dataYTest(i))
                errorIndexFalseNegative=[errorIndexFalseNegative,indexShuffleMapping(ceil(TotalNum*(iValid-1)/ValidNum)+i)];
            end
        end
    end
    errorNum/TotalNum
    length(errorIndexFalsePositive)/length(dataY==1)
    length(errorIndexFalseNegative)/length(dataY==0)


    
end

 %% Test Model by using sampling data from real benchmarks (e.g. NPB,Parsec)
function testModel(Mdl,featureSelected)
    load('D:/Dropbox/Project/Bandwidth/Mat/SampleForBenchmark-NPB.mat');
    benchmark = benchmarkNPB;
    benchmarkNameList={
    'BT',
     'CG',
     'DC',
     'EP',
     'FT',
     'IS',
     'LU',
     'MG',
     'UA',
     'SP',
     'D:/Dropbox/Project/Bandwidth/Resources/Sample/blacksholes',
     'D:/Dropbox/Project/Bandwidth/Resources/Sample/bodytrack',
     'D:/Dropbox/Project/Bandwidth/Resources/Sample/fluidanimate',
     'D:/Dropbox/Project/Bandwidth/Resources/Sample/swaptions-pthreads',
     'D:/Dropbox/Project/Bandwidth/Resources/Sample/x264',
     'D:/Dropbox/Project/Bandwidth/Resources/Sample/ferret',
     'D:/Dropbox/Project/Bandwidth/Resources/Sample/lulesh',
     'D:/Dropbox/Project/Bandwidth/Resources/Sample/freqmine-openmp',
     'D:/Dropbox/Project/Bandwidth/Resources/Sample/IRSmk_v1.0.regroup',
     'D:/Dropbox/Project/Bandwidth/Resources/Sample/AMG2006.orig',
     'D:/Dropbox/Project/Bandwidth/Resources/Sample/streamcluster',
     'D:/Dropbox/Project/Bandwidth/Resources/Sample/nw',
}
logical2str={'false','true'};
benchmarkLabel=cell(1,length(benchmarkNameList));


benchmarkLabel{1,1}=zeros(1,24);
benchmarkLabel{1,2}=zeros(1,24);
benchmarkLabel{1,3}=zeros(1,16);
benchmarkLabel{1,4}=zeros(1,24);
benchmarkLabel{1,5}=zeros(1,24);
benchmarkLabel{1,6}=zeros(1,24);
benchmarkLabel{1,7}=zeros(1,24);
benchmarkLabel{1,8}=zeros(1,24);
benchmarkLabel{1,9}=zeros(1,24);
SPLabel = zeros(1,24); SPLabel(1,5:7)=1;SPLabel(1,9:11)=1;SPLabel(1,13)=1;SPLabel(1,15:17)=1;SPLabel(1,23)=1;
benchmarkLabel{1,10}=SPLabel;


benchmarkLabel{1,11}=zeros(1,32);
benchmarkLabel{1,12}=zeros(1,32);
benchmarkLabel{1,13}=zeros(1,32);
benchmarkLabel{1,14}=zeros(1,32);
benchmarkLabel{1,15}=zeros(1,32);
benchmarkLabel{1,16}=zeros(1,32);
benchmarkLabel{1,17}=ones(1,8);
benchmarkLabel{1,18}=zeros(1,32);


IRSmkLabel=ones(1,24);IRSmkLabel(1,1:2)=0;IRSmkLabel(1,4:6)=0;IRSmkLabel(1,13:14)=0;IRSmkLabel(1,17)=0;IRSmkLabel(1,19)=0;
benchmarkLabel{1,19}=IRSmkLabel;
benchmarkLabel{1,20}=ones(1,8);
streamclusterLabel=ones(1,16);streamclusterLabel(1,4)=0;streamclusterLabel(1,7:8)=0;streamclusterLabel(1,11)=0;
benchmarkLabel{1,21}=streamclusterLabel;
nwLabel = ones(1,24); nwLabel(1,5:6)=0; nwLabel(1,9)=0;nwLabel(1,13:14)=0;nwLabel(1,18:19)=0;nwLabel(1,23)=0;
benchmarkLabel{1,22}=nwLabel;

errorNum=0; TotalNum=520;    
ConditionalPositive=0;
ConditionalNegative=0;
FalsePositive=0;
FalseNegative=0;
TruePositive=0;
TrueNegative=0;
    for iBench=1:22
        AllConfig=benchmark{1,iBench};
        [configNum,pairNum]=size(AllConfig);
        disp(['*************',benchmarkNameList{iBench,1},'*****************']);
        for iConfigNum=1:configNum
            isMemoryContention=false ;
            for iPair=1:pairNum
                if(iPair~=1 && iPair~=6 &&  iPair~=11 && iPair~=16 ) 
                    Stat=AllConfig{iConfigNum,iPair};
                    if (length(Stat)~=16 && length(Stat)~=14 )
                        continue;
                    end
                    rdramLatencyRatio=Stat(:,12).*Stat(:,11)./(Stat(:,10).*Stat(:,9));
                    lfbLatencyRatio=Stat(:,14).*Stat(:,13)./(Stat(:,10).*Stat(:,9));
                    Stat=[Stat,rdramLatencyRatio,lfbLatencyRatio];
                    StatAfterFS=Stat(:,featureSelected);
 %                    You can change the prune level to make the model more
 %                    robust
                        newY = predict(Mdl,StatAfterFS,'SubTrees',max(Mdl.PruneList)-1);
%                      If one channel has been detected as remote memory
%                      contention, we treat it as remote memory contention
%                      detected
                     if(newY==1)
                        isMemoryContention=true;
                        break;
                    else
                        isMemoryContention=false;
                    end
                end
            end
            if(benchmarkLabel{1,iBench}(1,iConfigNum)==1)
                ConditionalPositive=ConditionalPositive+1;
            else
                ConditionalNegative=ConditionalNegative+1;
            end
            if(benchmarkLabel{1,iBench}(1,iConfigNum)==1 && isMemoryContention==1 )
                 TruePositive=TruePositive+1;
            elseif(benchmarkLabel{1,iBench}(1,iConfigNum)==0 && isMemoryContention==0)
                 TrueNegative=TrueNegative+1;
            elseif(benchmarkLabel{1,iBench}(1,iConfigNum)==0 && isMemoryContention==1)
                 FalsePositive=FalsePositive+1;
            elseif(benchmarkLabel{1,iBench}(1,iConfigNum)==1 && isMemoryContention==0)
                  FalseNegative=FalseNegative+1;
            end
            if(isMemoryContention~=benchmarkLabel{1,iBench}(1,iConfigNum))
                errorNum=errorNum+1;
              disp(['*************',benchmarkNameList{iBench,1},iConfigNum,iPair,logical2str(isMemoryContention+1),'*****************']);              
            end
        end


    end
    disp(['Total Benchmark Num is:', num2str(TotalNum), '  Error Num is :',num2str(errorNum),'    Overall accuracy is:',num2str(1-errorNum/TotalNum)])
    disp(['FalseNegative Number is:',num2str(FalseNegative),'    FalseNegative Rate is:',num2str(FalseNegative/ConditionalPositive)])
    disp(['FalsePositive Number is:',num2str(FalsePositive),'    FalsePositive Rate is:',num2str(FalsePositive/ConditionalNegative)])
    disp(['TruePositive Number is:',num2str(TruePositive),'    TruePositive Rate is:',num2str(TruePositive/ConditionalPositive)])
    disp(['TrueNegative Number is:',num2str(TrueNegative),'    TrueNegative Rate is:',num2str(TrueNegative/ConditionalNegative)])
    disp(['ConditionalPositive is:',num2str(ConditionalPositive)])
    disp(['ConditionalNegative is:',num2str(ConditionalNegative)])

end

