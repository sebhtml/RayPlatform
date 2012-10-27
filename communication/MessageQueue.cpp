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

#include <communication/MessageQueue.h>
#include <iostream>

using namespace std;

#ifdef ASSERT
#include <assert.h>
#endif /* ASSERT */

void MessageQueue::constructor(int bins){

	#ifdef ASSERT
	assert(bins);
	assert(bins>0);
	#endif /* ASSERT */

	m_size=bins;
	m_headForPushOperations=0;
	m_tailForPopOperations=0;
	m_numberOfMessages=0;
	m_ring=(Message*)__Malloc(m_size*sizeof(Message),"/MessageQueue",false);

	m_dead=false;

	#ifdef ASSERT
	assert(m_ring!=NULL);
	assert(m_headForPushOperations<m_size);
	assert(m_tailForPopOperations<m_size);
	#endif /* ASSERT */

	pthread_spin_init(&m_spinlock,PTHREAD_PROCESS_PRIVATE);
}

void MessageQueue::push(Message*message){

	#ifdef DEBUG_MESSAGE_QUEUE
	cout<<"Push message"<<endl;
	#endif

	#ifdef ASSERT
	if(isFull()){
		cout<<"Error, MessageQueue is full with m_numberOfMessages "<<m_numberOfMessages<<endl;
	}
	assert(!isFull());
	assert(message!=NULL);
	assert(m_headForPushOperations<m_size);
	#endif /* ASSERT */

	m_ring[m_headForPushOperations++]=(*message);

	if(m_headForPushOperations==m_size)
		m_headForPushOperations=0;

	m_numberOfMessages++;
}

bool MessageQueue::isFull(){
	return m_numberOfMessages==m_size;
}

void MessageQueue::pop(Message*message){
	#ifdef ASSERT
	assert(hasContent());
	assert(message!=NULL);
	assert(m_tailForPopOperations<m_size);
	#endif /* ASSERT */

	(*message)=m_ring[m_tailForPopOperations++];

	if(m_tailForPopOperations==m_size)
		m_tailForPopOperations=0;

	m_numberOfMessages--;

	#ifdef ASSERT
	assert(m_tailForPopOperations<m_size);
	#endif /* ASSERT */
}

bool MessageQueue::hasContent(){
	return m_numberOfMessages>0;
}

void MessageQueue::destructor(){
	if(m_size==0)
		return;

	#ifdef ASSERT
	assert(m_size>0);
	assert(m_ring!=NULL);
	#endif /* ASSERT */

	m_size=0;
	m_headForPushOperations=0;
	m_tailForPopOperations=0;
	__Free(m_ring,"/MessageQueue",false);
	m_ring=NULL;

	#ifdef ASSERT
	assert(m_size==0);
	assert(m_ring==NULL);
	#endif /* ASSERT */

	pthread_spin_destroy(&m_spinlock);
}

bool MessageQueue::isDead(){
	return m_dead;
}

void MessageQueue::lock(){
	pthread_spin_lock(&m_spinlock);
}

void MessageQueue::unlock(){
	pthread_spin_unlock(&m_spinlock);
}

void MessageQueue::sendKillSignal(){
	m_dead=true;
}

