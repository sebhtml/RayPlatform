/*
 	RayPlatform
    Copyright (C) 2012  Sébastien Boisvert

	http://github.com/sebhtml/RayPlatform

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, version 3 of the License.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You have received a copy of the GNU Lesser General Public License
    along with this program (lgpl-3.0.txt).  
	see <http://www.gnu.org/licenses/>

*/

#include "core/RankProcess.h"

void RankProcess::constructor(int numberOfMiniRanksPerRank,int*argc,char***argv){

	m_numberOfMiniRanksPerRank=numberOfMiniRanksPerRank;
	m_numberOfMiniRanks=m_numberOfRanks*m_numberOfMiniRanksPerRank;
	m_numberOfInstalledMiniRanks=0;

	m_messagesHandler.constructor(argc,argv);

	m_rank=m_messagesHandler.getRank();
	m_numberOfRanks=m_messagesHandler.getSize();
}

void RankProcess::addMiniRank(MiniRank*miniRank){

	#ifdef ASSERT
	assert(m_numberOfInstalledMiniRanks<m_numberOfMiniRanksPerRank);
	#endif

	m_miniRanks[m_numberOfInstalledMiniRanks]=miniRank;
	m_cores[m_numberOfInstalledMiniRanks]=miniRank->getCore();

	m_cores[m_numberOfInstalledMiniRanks]->initLock();

	m_numberOfInstalledMiniRanks++;

	#ifdef ASSERT
	assert(m_numberOfInstalledMiniRanks<=m_numberOfMiniRanksPerRank);
	#endif

}

void*Rank_startMiniRank(void*object){

	((MiniRank*)object)->run();

	return object;
}

void RankProcess::run(){

	#ifdef ASSERT
	assert(m_numberOfMiniRanksPerRank==m_numberOfInstalledMiniRanks);
	#endif

	for(int i=0;i<m_numberOfInstalledMiniRanks;i++){
		pthread_create(m_threads+i,NULL,Rank_startMiniRank,m_miniRanks[i]);
	}

	bool communicate=true;

	int i=0;
	int periodForCheckingDeadMiniRanks=100000;

	while(communicate){
		receiveMessages();

		sendMessages();

		if(i%periodForCheckingDeadMiniRanks==0)
			if(allMiniRanksAreDead())
				communicate=false;

		i++;
	}

	for(int i=0;i<m_numberOfInstalledMiniRanks;i++)
		m_cores[i]->destroyLock();
}

bool RankProcess::allMiniRanksAreDead(){

	int count=0;

	for(int i=0;i<m_numberOfInstalledMiniRanks;i++){
		ComputeCore*core=m_cores[i];

		core->lock();
		if(core->hasFinished())
			count++;
		core->unlock();
	}

	return count==m_numberOfInstalledMiniRanks;
}

void RankProcess::destructor(){
	getMessagesHandler()->destructor();
}

MessagesHandler*RankProcess::getMessagesHandler(){
	return &m_messagesHandler;
}

void RankProcess::receiveMessages(){

	m_messagesHandler.receiveMessagesForMiniRanks(m_cores,m_numberOfMiniRanksPerRank);

}

void RankProcess::sendMessages(){

	m_messagesHandler.sendMessagesForMiniRanks(m_cores,m_numberOfMiniRanksPerRank);
}


