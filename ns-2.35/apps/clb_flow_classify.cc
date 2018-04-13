#include "clb_flow_classify.h"

void CLBFlowClassifier::init()
{
	maxActiveFlowNumber=0;
	invalidEntryNumber=0;
}

CLBTunnelAgent* CLBFlowClassifier::getCLBTunnelAgent(int otherIP)
{
	// printf("%lf-NODE %d: Begin CLBFlowClassifier! maxActiveFlowNumber=%u\n",Scheduler::instance().clock(),ParentClassifier_->nodeID(), maxActiveFlowNumber);
	for(unsigned short i=0;i<maxActiveFlowNumber;i++)
	{
		if(flowTable[i].valid==0)
		{
			invalidEntryNumber++;
		}
		if(flowTable[i].otherIP==otherIP)
		{
			flowTable[i].age=1;
			flowTable[i].hitTimes++;
			// printf("%lf-NODE %d: Hit flowTable[%u]! hitTimes=%d, otherIP=%d\n"
			// 	,Scheduler::instance().clock(),ParentClassifier_->nodeID()
			// 	, i, flowTable[i].hitTimes,otherIP);
			fflush(stdout);
			if (flowTable[i].clbTunnelAgent->flowID != i)
			{
				printf("ERROR: flowTable[%u].clbTunnelAgent->flowID=%d!\n", i, flowTable[i].clbTunnelAgent->flowID);
				fflush(stdout);
				exit(0);
			}
			return flowTable[i].clbTunnelAgent;
		}
	}

	expandOneFlowTableEntry(otherIP);
	unsigned short maxTmp=maxActiveFlowNumber;
	// printf("%lf-NODE %d: Hit flowTable[%u]! hitTimes=%d, otherIP=%d\n"
	// 	,Scheduler::instance().clock(),ParentClassifier_->nodeID()
	// 	, maxTmp - 1, flowTable[maxTmp - 1].hitTimes,otherIP);
	fflush(stdout);
	if (flowTable[maxTmp - 1].clbTunnelAgent->flowID != maxTmp - 1)
	{
		printf("ERROR: flowTable[%u].clbTunnelAgent->flowID=%d!\n", maxTmp - 1, flowTable[maxTmp - 1].clbTunnelAgent->flowID);
		fflush(stdout);
		exit(0);
	}
	return flowTable[maxActiveFlowNumber-1].clbTunnelAgent;
}

void CLBFlowClassifier::expandOneFlowTableEntry(int otherIP)
{
	// printf("%lf-NODE %d: Begin expandOneFlowTableEntry otherIP=%d\n"
	// 	,Scheduler::instance().clock(),ParentClassifier_->nodeID(), otherIP);
	if(invalidEntryNumber!=0)
	{
		printf("There are %u unused flow table entry, should not be expanded now!\n",invalidEntryNumber);
		fflush(stdout);
		exit(0);
	}

	maxActiveFlowNumber++;

	if(maxActiveFlowNumber==1)
	{
		flowTable=new FLOW_TABLE [maxActiveFlowNumber];
		flowTable[maxActiveFlowNumber-1].age=1;
		flowTable[maxActiveFlowNumber-1].otherIP=otherIP;
		flowTable[maxActiveFlowNumber-1].valid=1;
		flowTable[maxActiveFlowNumber-1].hitTimes = 1;
		flowTable[maxActiveFlowNumber-1].clbTunnelAgent=new CLBTunnelAgent(ParentClassifier_, BlockSize_N, BlockSize_P);
		flowTable[maxActiveFlowNumber-1].clbTunnelAgent->flowID = maxActiveFlowNumber - 1;
	}
	else
	{
		FLOW_TABLE* tmp=new FLOW_TABLE [maxActiveFlowNumber];
		if (tmp==NULL)
		{
			printf("expandOneFlowTableEntry error! Can't new memeory\n");
			fflush(stdout);
			exit(0);
		}
		memset(tmp,0,maxActiveFlowNumber*sizeof(FLOW_TABLE));
		memcpy(tmp,flowTable,(maxActiveFlowNumber-1)*sizeof(FLOW_TABLE));
		delete [] flowTable;

		flowTable=new FLOW_TABLE [maxActiveFlowNumber];
		if (flowTable==NULL)
		{
			printf("expandOneFlowTableEntry error! Can't new memeory\n");
			fflush(stdout);
			exit(0);
		}
		memcpy(flowTable,tmp,(maxActiveFlowNumber-1)*sizeof(FLOW_TABLE));

		flowTable[maxActiveFlowNumber-1].age=1;
		flowTable[maxActiveFlowNumber-1].otherIP=otherIP;
		flowTable[maxActiveFlowNumber-1].valid=1;
		flowTable[maxActiveFlowNumber-1].hitTimes = 1;
		flowTable[maxActiveFlowNumber-1].clbTunnelAgent=new CLBTunnelAgent(ParentClassifier_, BlockSize_N, BlockSize_P);
		flowTable[maxActiveFlowNumber-1].clbTunnelAgent->flowID = maxActiveFlowNumber - 1;

		delete [] tmp;
	}


	// printf("%lf-NODE %d: expandOneFlowTableEntry, maxActiveFlowNumber expand to %u! flowTable[%u].clbTunnelAgent->flowID =%d\n"
	// 	,Scheduler::instance().clock(),flowTable[maxActiveFlowNumber-1].clbTunnelAgent->ParentClassifier_->nodeID()
	// 	,maxActiveFlowNumber
	// 	,maxActiveFlowNumber-1,flowTable[maxActiveFlowNumber-1].clbTunnelAgent->flowID);
	fflush(stdout);
}

void CLBFlowClassifier::shrinkFlowTable()
{
	//printf("===========Begin shrinkFlowTable!==========\n");
fflush(stdout);

	unsigned short old_maxActiveFlowNumber=maxActiveFlowNumber;
	for(unsigned short i=0;i<maxActiveFlowNumber;i++)
	{
		//printf("flowTable[%u] age=%d, valid=%d\n",i,flowTable[i].age,flowTable[i].valid);
fflush(stdout);
		if(flowTable[i].age==0)
		{
			invalidEntryNumber++;
			flowTable[i].valid=0;
			flowTable[i].hitTimes = 0;
			// flowTable[i].clbTunnelAgent->uninitialize();
			delete flowTable[i].clbTunnelAgent;
		}
		flowTable[i].age=0;
	}
	maxActiveFlowNumber=maxActiveFlowNumber-invalidEntryNumber;

	printf("old_maxActiveFlowNumber=%u, invalidEntryNumber=%u, shrink to %u!\n"
		,old_maxActiveFlowNumber,invalidEntryNumber,maxActiveFlowNumber);
fflush(stdout);

	if(invalidEntryNumber==0)
	{
		//printf("===========No need to shrink!==========\n");
fflush(stdout);
		return;
	}

	invalidEntryNumber=0;
	FLOW_TABLE* tmp=new FLOW_TABLE [maxActiveFlowNumber];
	if (tmp==NULL)
	{
		//printf("expandOneFlowTableEntry error! Can't new memeory\n");
fflush(stdout);
		exit(0);
	}
	memset(tmp,0,maxActiveFlowNumber*sizeof(FLOW_TABLE));

	unsigned short k=0;
	for(unsigned short i=0;i<old_maxActiveFlowNumber;i++)
	{
		if(flowTable[i].valid==1)
		{
			memcpy(&tmp[k],&flowTable[i],sizeof(FLOW_TABLE));
			flowTable[i].clbTunnelAgent->flowID = i;
			k++;
		}
	}
	if(k != maxActiveFlowNumber)
	{
		//printf("k=%u\n",k);
fflush(stdout);
		//printf("shrinkFlowTable error!\n");
fflush(stdout);
		getchar();
		exit(0);
	}
	delete [] flowTable;

	flowTable=new FLOW_TABLE [maxActiveFlowNumber];
	if (flowTable==NULL)
	{
		//printf("expandOneFlowTableEntry error! Can't new memeory\n");
fflush(stdout);
		exit(0);
	}
	memcpy(flowTable,tmp,maxActiveFlowNumber*sizeof(FLOW_TABLE));
	delete [] tmp;

	//printf("===========Finish shrinkFlowTable!==========\n");
fflush(stdout);
}
