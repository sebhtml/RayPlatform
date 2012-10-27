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
	m_argc=*argc;
	m_argv=*argv;

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

	bool useMiniRanks=m_numberOfMiniRanksPerRank>1;

	int miniRankNumber=m_messagesHandler.getRank()*m_numberOfMiniRanksPerRank;
	int numberOfMiniRanks=m_messagesHandler.getSize()*m_numberOfMiniRanksPerRank;

	for(int i=0;i<m_numberOfInstalledMiniRanks;i++){

		m_cores[i]->constructor(m_argc,m_argv,miniRankNumber+i,numberOfMiniRanks,useMiniRanks);
	}

	for(int i=0;i<m_numberOfInstalledMiniRanks;i++){

		#ifdef CONFIG_DEBUG_MPI_RANK
		cout<<"[RankProcess::run] starting mini-rank in parallel # "<<i<<""<<endl;
		#endif

/*
 * TODO: possibly change the stack size -> pthread_attr_setstacksize
 *
 * pthread_attr_t attributes;
 * int stackSize=8*1024*1024;
 * int errorCode=pthread_attr_setstacksize(&attributes,stackSize);
 * // manage error
 *
 * pthread_create(m_threads+i,&attributes,Rank_startMiniRank,m_miniRanks[i]);
 *
 * on Colosse (CentOS 5.7), the default is 10240 KiB
 * on Fedora 16 and 17, it is 8192 KiB
 *
 */
		pthread_create(m_threads+i,NULL,Rank_startMiniRank,m_miniRanks[i]);
	}

	m_communicate=true;

	while(m_communicate){

		receiveMessages();

		sendMessages();
	}

	#ifdef CONFIG_DEBUG_MPI_RANK
	cout<<"All mini-ranks are dead."<<endl;
	#endif

	for(int i=0;i<m_numberOfInstalledMiniRanks;i++)
		m_cores[i]->destructor();

	destructor();
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

	#ifdef CONFIG_DEBUG_MPI_RANK
	cout<<"[RankProcess::sendMessages]"<<endl;
	#endif

	m_messagesHandler.sendMessagesForMiniRanks(m_cores,m_numberOfMiniRanksPerRank,&m_communicate);
}


