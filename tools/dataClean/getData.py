# -*- coding: utf-8 -*-
"""
Created on Sat Mar 05 03:10:40 2016

@author: Hunter
"""
import pickle
from os import walk

samplingFiles = []
# Key: Name of Benchmark, Value: directory of the sampling files
fileHeadDict={
'streamcluster':'/home/haoxu/benchmarks/streamcluster/paperstat/sample_result/',
};

cpuNodeMap=[
0,0,0,0,0,0,0,0,
1,1,1,1,1,1,1,1,
2,2,2,2,2,2,2,2,
3,3,3,3,3,3,3,3,
0,0,0,0,0,0,0,0,
1,1,1,1,1,1,1,1,
2,2,2,2,2,2,2,2,
3,3,3,3,3,3,3,3
];
for benchmarkName in fileHeadDict.keys():
	fileHead=fileHeadDict[benchmarkName]
	samplingFiles=[]
	# Get all data files name in a directory
	for (dirpath, dirnames, filenames) in walk(fileHead):
	    samplingFiles.extend(filenames)
	    break
	TwoDMatList={}
	dictVariableTwoDMatList={}
	for pathtail in samplingFiles:
		L1Counter=[0]*64
		L1CounterForNode=[0]*4
		dictVariable={};
		dictVariableTime={}
		dictVariableR_DRAM={}
		dictVariableL_DRAM={}
		dictVariableAll={}
		dictLatencyNodePair={}
		dictLFBNodePair={}
		dictRDRAMNodePair={}
		dictLDRAMNodePair={}
		dictStatNodePair={}
		for i in [0,1,2,3]:
			for j in [0,1,2,3]:
				dictLatencyNodePair[str(i)+'->'+str(j)]=[]
				dictLFBNodePair[str(i)+'->'+str(j)]=[]
				dictRDRAMNodePair[str(i)+'->'+str(j)]=[]
				dictStatNodePair[str(i)+'->'+str(j)]=[]
				dictLDRAMNodePair[str(i)+'->'+str(j)]=[]
		remoteAccessLatency=[]
		remoteDRAMLatency=[]
		L_LFBLatency=[]
		LatencyTotal=[] 
		sample_statics = []
		path=fileHead+pathtail
		print path
		# Get information all data object generated during the profiling
		with open(path) as f:
			for line in f:
				line=line.strip('\n')
				Temp=line.split(',')
				# find a line like: "mallocID: 4 thread ID:0"
				if len(Temp)==1 and Temp[0][0:8]=='mallocID':
					MallocID=Temp[0].split(' ')[1]
					ThreadID=Temp[0].split(' ')[3].split(':')[1]
					dictVariable[MallocID+':'+ThreadID]=0.0
					dictVariableTime[MallocID+':'+ThreadID]=0.0
					dictVariableR_DRAM[MallocID+':'+ThreadID]={}
					dictVariableL_DRAM[MallocID+':'+ThreadID]={}
					for i in [0,1,2,3]:
						for j in [0,1,2,3]:
							dictVariableR_DRAM[MallocID+':'+ThreadID][str(i)+'->'+str(j)]=[]
							dictVariableL_DRAM[MallocID+':'+ThreadID][str(i)+'->'+str(j)]=[]
		dictVariable['-1:-1']=0
		dictVariableTime['-1:-1']=0
		dictVariableR_DRAM['-1:-1']={}
		dictVariableL_DRAM['-1:-1']={}
		for i in [0,1,2,3]:
			for j in [0,1,2,3]:
				dictVariableR_DRAM['-1:-1'][str(i)+'->'+str(j)]=[]
				dictVariableL_DRAM['-1:-1'][str(i)+'->'+str(j)]=[]
                dictVariableKeys = dictVariable.keys()
		# Count all type of memory access for each channel
		with open(path) as f:
			lineNum=0;     
			for line in f:
				line=line.strip('\n')
				Temp=line.split(',')
				if lineNum>0 and len(Temp)>3 and Temp[0]!='timestamp':
					LatencyValue=int(Temp[10])
					LatencyTotal.append(LatencyValue)
					if int(Temp[4])>=0 and int(Temp[4])<=3 and cpuNodeMap[int(Temp[1])]>=0 and cpuNodeMap[int(Temp[1])]<=3 and (Temp[11]+':'+Temp[12] in dictVariableKeys):
						if Temp[9]=='R_DRAM':
							dictRDRAMNodePair[str(cpuNodeMap[int(Temp[1])])+'->'+Temp[4]].append(LatencyValue)
							dictVariableR_DRAM[Temp[11]+':'+Temp[12]][str(cpuNodeMap[int(Temp[1])])+'->'+Temp[4]].append(LatencyValue)
						elif Temp[9]=='L_LFB':
							dictLFBNodePair[str(cpuNodeMap[int(Temp[1])])+'->'+Temp[4]].append(LatencyValue)
						elif Temp[9]=='L_DRAM':
							dictLDRAMNodePair[str(cpuNodeMap[int(Temp[1])])+'->'+Temp[4]].append(LatencyValue) 
							dictVariableL_DRAM[Temp[11]+':'+Temp[12]][str(cpuNodeMap[int(Temp[1])])+'->'+Temp[4]].append(LatencyValue)       
						dictLatencyNodePair[str(cpuNodeMap[int(Temp[1])])+'->'+Temp[4]].append(LatencyValue)
						dictVariable[Temp[11]+':'+Temp[12]]=dictVariable[Temp[11]+':'+Temp[12]]+LatencyValue
						dictVariableTime[Temp[11]+':'+Temp[12]]=dictVariableTime[Temp[11]+':'+Temp[12]]+1.0
				elif Temp[0]=='L1Counter' and lineNum>0 and len(Temp)==3:
					if long(Temp[2])>L1Counter[int(Temp[1])]:
						L1Counter[int(Temp[1])]=long(Temp[2])					
				lineNum=lineNum+1
		for cpuid in range(64):
			L1CounterForNode[cpuNodeMap[cpuid]]+=L1Counter[cpuid];
		# Save all the data in ArrayList and print some stastic information 
		for key, value in dictLatencyNodePair.iteritems():
			Latency=value
			Latency_Sorted=sorted(Latency)  
			ArrayList=[]        
			memoryAccessNum=len(Latency)
			Latency_Sorted_Length=float(len(Latency_Sorted))
			if Latency_Sorted_Length==0:
				continue
			ArrayList.append(sum([1 for item in Latency_Sorted if item > 1000])/Latency_Sorted_Length)
			ArrayList.append(sum([1 for item in Latency_Sorted if item > 500])/Latency_Sorted_Length)
			ArrayList.append(sum([1 for item in Latency_Sorted if item > 200])/Latency_Sorted_Length)
			ArrayList.append(sum([1 for item in Latency_Sorted if item > 100])/Latency_Sorted_Length)
			ArrayList.append(sum([1 for item in Latency_Sorted if item > 50])/Latency_Sorted_Length)
			ArrayList.append(sum([1 for item in Latency_Sorted if item > 10])/Latency_Sorted_Length)
			ArrayList.append(Latency_Sorted[int(Latency_Sorted_Length/2)])
			ArrayList.append(sum(Latency_Sorted)/Latency_Sorted_Length)
			print key,"Latency>1000 ratio:",ArrayList[0]
			print key,"Latency>500 ratio:",ArrayList[1]    
			print key,"Latency>200 ratio:",ArrayList[2]
			print key,"Latency>100 ratio:",ArrayList[3]
			print key,"Latency>50 ratio:",ArrayList[4]
			print key,"Latency>10 ratio:",ArrayList[5]
			print key,"Median of the latency is:",ArrayList[6]
			print key,"Average of the latency is:",ArrayList[7]
			print key,"Memory Access # is:", memoryAccessNum
			accessNum=len(value)
			L1CounterValue=L1CounterForNode[int(key.split('->')[0])]	
			print key,'L1 access number is:',L1CounterForNode
			if accessNum!=0:
				averageLatency=float(sum(Latency))/float(accessNum)
			else:
				averageLatency=0.0
			print key,'The sampled access number is:',accessNum,'Average Latency is:',averageLatency
			R_DRAMNum=len(dictRDRAMNodePair[key])
			if R_DRAMNum!=0:
				averageR_DRAMLatency=float(sum(dictRDRAMNodePair[key]))/float(R_DRAMNum)
			else:
				averageR_DRAMLatency=0.0
			print key,'The sampled R_DRAM number is:',R_DRAMNum,'Average R_DRAM Latency is:',averageR_DRAMLatency
			
			L_DRAMNum=len(dictLDRAMNodePair[key])
			if L_DRAMNum!=0:
				averageL_DRAMLatency=float(sum(dictLDRAMNodePair[key]))/float(L_DRAMNum)
			else:
				averageL_DRAMLatency=0.0
			print key,'The sampled L_DRAM number is:',L_DRAMNum,'Average L_DRAM Latency is:',averageL_DRAMLatency

			L_LFBNum=len(dictLFBNodePair[key])
			if L_LFBNum!=0:
				averageL_LFBLatency=float(sum(dictLFBNodePair[key]))/float(L_LFBNum)
			else:
				averageL_LFBLatency=0.0
			print key,'The sampled L_LFB number is:',L_LFBNum,'Average Remote Latency is:',averageL_LFBLatency
			ArrayList.append(accessNum)
			ArrayList.append(averageLatency)
			ArrayList.append(R_DRAMNum)
			ArrayList.append(averageR_DRAMLatency)
			ArrayList.append(L_LFBNum)
			ArrayList.append(averageL_LFBLatency)
			ArrayList.append(L_DRAMNum)
			ArrayList.append(averageL_DRAMLatency)
			
			dictStatNodePair[key]=ArrayList
		TwoDMatList[pathtail]=dictStatNodePair
		dictVariableAll['R_DRAM']=dictVariableR_DRAM
		dictVariableAll['L_DRAM']=dictVariableL_DRAM
		dictVariableAll['TotalLatency']=dictVariable
		dictVariableAll['Num']=dictVariableTime
		dictVariableTwoDMatList[pathtail]=dictVariableAll
		PrintNum=5;
		iPrint=0;
		LatencySum=sum(LatencyTotal)
		# Print top 5 data objects contribute to the total latency
		for key, value in sorted(dictVariable.iteritems(), key=lambda (k,v): (v,k), reverse = True):
			if iPrint<PrintNum:
				iPrint=iPrint+1
				if dictVariableTime[key]!=0 and LatencySum!=0:
					print key,float(value)/LatencySum,value/dictVariableTime[key]
				for i in [0,1,2,3]:
					for j in [0,1,2,3]:
						NumSampleR_DRAM=len(dictVariableR_DRAM[key][str(i)+'->'+str(j)])
						if NumSampleR_DRAM!=0:
							print 'R_DRAM',key,str(i)+'->'+str(j),sum(dictVariableR_DRAM[key][str(i)+'->'+str(j)])*1.0/NumSampleR_DRAM,NumSampleR_DRAM
                                                NumSampleL_DRAM=len(dictVariableL_DRAM[key][str(i)+'->'+str(j)])
                                                if NumSampleL_DRAM!=0:
                                                        print 'L_DRAM',key,str(i)+'->'+str(j),sum(dictVariableL_DRAM[key][str(i)+'->'+str(j)])*1.0/NumSampleL_DRAM,NumSampleL_DRAM
			else: 
				break;
	# Dump  objects to files
	pickle.dump(TwoDMatList, open( benchmarkName, "wb" ) )
	pickle.dump(dictVariableTwoDMatList, open( benchmarkName+"Var", "wb" ) )
