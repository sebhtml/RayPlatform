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

#include <iostream>
using namespace std;

#ifdef ASSERT
#include <assert.h>
#endif

//#define CONFIG_DEBUG_MPI_RANK

void RankProcess::constructor(int numberOfMiniRanksPerRank,int*argc,char***argv){

	for(int i=0;i<numberOfMiniRanksPerRank;i++){
		m_mustWait[i]=false;
	}

	#ifdef CONFIG_DEBUG_MPI_RANK
	cout<<"[RankProcess] numberOfMiniRanksPerRank= "<<numberOfMiniRanksPerRank<<endl;
	#endif

	m_numberOfMiniRanksPerRank=numberOfMiniRanksPerRank;
	m_numberOfMiniRanks=m_numberOfRanks*m_numberOfMiniRanksPerRank;
	m_numberOfInstalledMiniRanks=0;

	m_messagesHandler.constructor(argc,argv);

	m_rank=m_messagesHandler.getRank();
	m_numberOfRanks=m_messagesHandler.getSize();

	#ifdef CONFIG_DEBUG_MPI_RANK
	cout<<"[RankProcess] rank= "<<m_rank<<" size= "<<m_numberOfRanks<<endl;
	#endif
}

void RankProcess::addMiniRank(MiniRank*miniRank){

	#ifdef CONFIG_DEBUG_MPI_RANK
	cout<<"Adding mini-rank"<<endl;
	#endif

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

		#ifdef CONFIG_DEBUG_MPI_RANK
		cout<<"[RankProcess::run] starting mini-rank in parallel # "<<i<<""<<endl;
		#endif

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

	cout<<"All mini-ranks are dead."<<endl;

	for(int i=0;i<m_numberOfInstalledMiniRanks;i++)
		m_cores[i]->destroyLock();

	destructor();
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

/*
 * We must make sure that the last mini-rank that received
 * a message has consumed its message.
 * We need to do that because we can not receive a message for
 * this mini-rank before it has consumed the other message.
 */

	for(int i=0;i<m_numberOfMiniRanksPerRank;i++){

		if(!m_mustWait[i])
			continue;

		ComputeCore*core=m_cores[i];

		core->lock();
		int inboxSize=core->getInbox()->size();
		core->unlock();

		if(inboxSize==0){
			m_mustWait[i]=false;

			#ifdef CONFIG_DEBUG_MPI_RANK
			cout<<"Mini-rank # "<<i<<" consumed its message";
			cout<<"inboxSize= "<<inboxSize<<endl;
			#endif
		}else{
			return;// do something else, we can not receive anything
		}

	}


	#ifdef CONFIG_DEBUG_MPI_RANK
	cout<<"[RankProcess::receiveMessages]"<<endl;
	#endif

	m_messagesHandler.receiveMessagesForMiniRanks(m_cores,m_numberOfMiniRanksPerRank);

	int lastMiniRank=-1;

	bool received=m_messagesHandler.hasReceivedMessage(&lastMiniRank);

	if(received){
		m_mustWait[lastMiniRank]=true;

		#ifdef CONFIG_DEBUG_MPI_RANK
		cout<<"mustWait -> "<<m_mustWait[lastMiniRank]<<" mini-rank "<<lastMiniRank<<endl;
		#endif
	}
}

void RankProcess::sendMessages(){

	#ifdef CONFIG_DEBUG_MPI_RANK
	cout<<"[RankProcess::sendMessages]"<<endl;
	#endif

	m_messagesHandler.sendMessagesForMiniRanks(m_cores,m_numberOfMiniRanksPerRank);
}


