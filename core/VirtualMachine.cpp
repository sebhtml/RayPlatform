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

#include "core/VirtualMachine.h"

void VirtualMachine::constructor(int numberOfMiniRanksPerRank,int ranks){

	m_numberOfMiniRanksPerRank=numberOfMiniRanksPerRank;
	m_numberOfRanks=ranks;
	m_numberOfMiniRanks=m_numberOfRanks*m_numberOfMiniRanksPerRank;
	m_numberOfInstalledMiniRanks=0;
}

void VirtualMachine::addMiniRank(MiniRank*miniRank){

	#ifdef ASSERT
	assert(m_numberOfInstalledMiniRanks<m_numberOfMiniRanksPerRank);
	#endif

	m_miniRanks[m_numberOfInstalledMiniRanks++]=miniRank;

	#ifdef ASSERT
	assert(m_numberOfInstalledMiniRanks<=m_numberOfMiniRanksPerRank);
	#endif

}

void VirtualMachine_startMiniRank(void*object){

	((MiniRank*)object)->run();

	lock();

	m_deadMiniRanks++;

	unlock();
}

void VirtualMachine::run(){

	pthread_spin_init(&m_lock,NULL);

	#ifdef ASSERT
	assert(m_numberOfMiniRanksPerRank==m_numberOfInstalledMiniRanks);
	#endif

	for(int i=0;i<m_numberOfInstalledMiniRanks;i++){
		pthread_create(m_threads+i,NULL,VirtualMachine_startMiniRank,m_miniRanks[i]);
	}

	bool communicate=true;

	while(communicate){
		receiveMessages();

		sendMessages();

		lock();

		if(m_deadMiniRanks==m_numberOfMiniRanksPerRank)
			communicate=false;

		unlock();
	}

	pthread_spin_destroy(&m_lock,NULL);
}

void VirtualMachine::destructor(){
	getMessagesHandler()->destructor();
}

MessagesHandler*ComputeCore::getMessagesHandler(){
	return &m_messagesHandler;
}

void VirtualMachine::receiveMessages(){

	m_messagesHandler.receiveMessagesForMiniRanks(MiniRank**miniRanks,int miniRanksPerRank);

}

void VirtualMachine::sendMessages(){

	m_messagesHandler.sendMessagesForMiniRanks(MiniRank**miniRanks,int miniRanksPerRank);
}

void VirtualMachine::lock(){
	pthread_spin_lock(&m_lock);
}

void VirtualMachine::unlock(){
	pthread_spin_unlock(&m_lock);
}
