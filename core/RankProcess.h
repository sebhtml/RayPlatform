/*
 	RayPlatform: a message-passing development framework
    Copyright (C) 2012 Sébastien Boisvert

	http://github.com/sebhtml/RayPlatform: a message-passing development framework

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

#ifndef _RankProcess_h
#define _RankProcess_h

#include "MiniRank.h"

#include <RayPlatform/communication/MessagesHandler.h>

#if defined(CONFIG_DEBUG_MPI_RANK) || defined(ASSERT)
#include <iostream>
using namespace std;
#endif

#ifdef CONFIG_DEBUG_MPI_RANK
#endif /* CONFIG_DEBUG_MPI_RANK */

#ifdef ASSERT
#include <assert.h>
#endif /* ASSERT */


#define MAXIMUM_NUMBER_OF_MINIRANKS_PER_RANK 64

/*
 * The Rank is part of the layered minirank
 * architecture.
 *
 * \author Sébastien Boisvert
 */
template<class Application>
class RankProcess{

	int m_argc;
	char**m_argv;

	bool m_communicate;
	bool m_destroyed;

/*
 * Middleware to handle messages.
 */
	MessagesHandler m_messagesHandler;

	MiniRank*m_miniRanks[MAXIMUM_NUMBER_OF_MINIRANKS_PER_RANK];
	pthread_t m_threads[MAXIMUM_NUMBER_OF_MINIRANKS_PER_RANK];
	ComputeCore*m_cores[MAXIMUM_NUMBER_OF_MINIRANKS_PER_RANK];

/**
 * The number of this rank.
 */
	int m_rank;

/** 
 * The number of ranks.
 */
	int m_numberOfRanks;

	int m_numberOfMiniRanksPerRank;
	int m_numberOfInstalledMiniRanks;

/*
 * The total number of mini-ranks in ranks.
 */
	int m_numberOfMiniRanks;

	void startMiniRanks();
	void startMiniRank();

	void spawnApplicationObjects(int argc,char**argv);
	void addMiniRank(MiniRank*miniRank);

/*
 * Get the middleware object 
 */
	MessagesHandler*getMessagesHandler();

public:
	void constructor(int*argc,char***argv);
	void destructor();
	void run();
};


/*
 * Below are the implementation of these template methods.
 */

//#define CONFIG_DEBUG_MPI_RANK

template<class Application>
void RankProcess<Application>::spawnApplicationObjects(int argc,char**argv){

	int miniRanksPerRank=1;

/*
 * Get the number of mini-ranks per rank.
 */
	for(int i=0;i<argc;i++){
		if(strcmp(argv[i],"-mini-ranks-per-rank")==0 && i+1<argc)
			miniRanksPerRank=atoi(argv[i+1]);
	}

	if(miniRanksPerRank<1)
		miniRanksPerRank=1;

	m_numberOfMiniRanksPerRank=miniRanksPerRank;

/*
 * Add the mini-ranks.
 */
	for(int i=0;i<m_numberOfMiniRanksPerRank;i++){
		
		MiniRank*miniRank=new Application(argc,argv);
		addMiniRank(miniRank);
	}


}

template<class Application>
void RankProcess<Application>::constructor(int*argc,char***argv){

	m_numberOfInstalledMiniRanks=0;

	m_messagesHandler.constructor(argc,argv);

	m_destroyed=false;

	spawnApplicationObjects(*argc,*argv);

	#ifdef CONFIG_DEBUG_MPI_RANK
	cout<<"[RankProcess] m_numberOfMiniRanksPerRank= "<<m_numberOfMiniRanksPerRank<<endl;
	#endif

	m_numberOfMiniRanks=m_numberOfRanks*m_numberOfMiniRanksPerRank;

	m_argc=*argc;
	m_argv=*argv;

	m_rank=m_messagesHandler.getRank();
	m_numberOfRanks=m_messagesHandler.getSize();

	#ifdef CONFIG_DEBUG_MPI_RANK
	cout<<"[RankProcess] rank= "<<m_rank<<" size= "<<m_numberOfRanks<<endl;
	#endif
}

template<class Application>
void RankProcess<Application>::addMiniRank(MiniRank*miniRank){

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

template<class Application>
void RankProcess<Application>::run(){

	#ifdef ASSERT
	if(m_numberOfMiniRanksPerRank!=m_numberOfInstalledMiniRanks){
		cout<<"Error: "<<m_numberOfInstalledMiniRanks<<" installed mini-ranks, but need "<<m_numberOfMiniRanksPerRank<<endl;
	}

	assert(m_numberOfMiniRanksPerRank==m_numberOfInstalledMiniRanks);
	#endif

	bool useMiniRanks=m_numberOfMiniRanksPerRank>1;

	if(useMiniRanks){
		startMiniRanks();
	}else{
		startMiniRank();
	}

	#ifdef CONFIG_DEBUG_MPI_RANK
	cout<<"All mini-ranks are dead."<<endl;
	#endif

	for(int i=0;i<m_numberOfInstalledMiniRanks;i++)
		m_cores[i]->destructor();

	destructor();
}

template<class Application>
void RankProcess<Application>::startMiniRanks(){

	int miniRankNumber=m_messagesHandler.getRank()*m_numberOfMiniRanksPerRank;
	int numberOfMiniRanks=m_messagesHandler.getSize()*m_numberOfMiniRanksPerRank;

	for(int i=0;i<m_numberOfInstalledMiniRanks;i++){
		m_cores[i]->constructor(m_argc,m_argv,miniRankNumber+i,numberOfMiniRanks,
			m_numberOfMiniRanksPerRank,&m_messagesHandler);
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

		m_messagesHandler.sendAndReceiveMessagesForRankProcess(m_cores,m_numberOfInstalledMiniRanks,&m_communicate);
	}

}

template<class Application>
void RankProcess<Application>::destructor(){

	if(m_destroyed)
		return;

	getMessagesHandler()->destructor();

	m_destroyed=true;
}

template<class Application>
MessagesHandler*RankProcess<Application>::getMessagesHandler(){
	return &m_messagesHandler;
}

template<class Application>
void RankProcess<Application>::startMiniRank(){

	int miniRankIndex=0;

	m_cores[miniRankIndex]->constructor(m_argc,m_argv,m_rank,m_numberOfRanks,
		m_numberOfMiniRanksPerRank,&m_messagesHandler);

	m_miniRanks[miniRankIndex]->run();
}


#endif /* _Rank_h */

