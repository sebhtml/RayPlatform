/*
 	Ray
    Copyright (C) 2010, 2011  Sébastien Boisvert

	http://DeNovoAssembler.SourceForge.Net/

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

#include <memory/RingAllocator.h>
#include <memory/allocator.h>

#include <string.h>
#include <assert.h>
#include <iostream>
using namespace std;

//#define CONFIG_RING_VERBOSE

#define BUFFER_STATE_AVAILABLE 0x0
#define BUFFER_STATE_DIRTY 0x1

#define __NOT_SET -1


void RingAllocator::constructor(int chunks,int size,const char*type,bool show){

	#ifdef CONFIG_RING_VERBOSE
	cout<<"[RingAllocator::constructor] "<<type<<" this "<<hex<<this<<dec<<endl;
	#endif

	resetCount();

	/* m_max should never be 0 */
	#ifdef ASSERT
	assert(size>0);
	assert(chunks>0);
	#endif /* ASSERT */

	m_chunks=chunks;// number of buffers

	m_max=size;// maximum buffer size in bytes

	#ifdef ASSERT
	assert(m_chunks==chunks);
	assert(m_max==size);
	#endif /* ASSERT */

	strcpy(m_type,type);

	m_numberOfBytes=m_chunks*m_max;

	#ifdef ASSERT
	assert(m_numberOfBytes>0);
	#endif /* ASSERT */

	#ifdef ASSERT
	assert(m_memory==NULL);
	#endif

	m_memory=(uint8_t*)__Malloc(sizeof(uint8_t)*m_numberOfBytes,m_type,show);

	#ifdef ASSERT
	assert(m_memory!=NULL);
	#endif

	m_bufferStates=(uint8_t*)__Malloc(m_chunks*sizeof(uint8_t),m_type,show);

	#ifdef CONFIG_RING_VERBOSE
	cout<<"[RingAllocator::constructor] memory= "<<(void*)m_memory<<endl;
	#endif /* CONFIG_RING_VERBOSE */

	for(int i=0;i<m_chunks;i++)
		m_bufferStates[i]= BUFFER_STATE_AVAILABLE;

	m_current=0;// the current head for allocation operations

	m_show=show;

	// in the beginning, all buffers are available
	m_availableBuffers=m_chunks;

	#if 0
	cout<<"[RingAllocator] m_chunks: "<<m_chunks<<" m_max: "<<m_max<<endl;
	#endif

	#ifdef ASSERT
	assert(size>0);
	assert(chunks>0);

	if(m_chunks==0)
		cout<<"Error: chunks: "<<chunks<<" m_chunks: "<<m_chunks<<endl;

	assert(m_chunks>0);
	assert(m_max>0);
	#endif /* ASSERT */

	m_linearSweeps=0;

/**
 * internally, there are N buffers for MPI_Isend. However,
 * these slots become dirty when they are used and become
 * clean again when MPI_Test says so.
 * But we don't want to do too much sweep operations. Instead,
 * we want amortized operations.
 */

	m_minimumNumberOfDirtyBuffersForSweep=__NOT_SET;
	m_minimumNumberOfDirtyBuffersForWarning=__NOT_SET;

	m_numberOfDirtyBuffers=0;

	m_maximumDirtyBuffers=m_numberOfDirtyBuffers;

	m_dirtyBuffers=NULL;

}

RingAllocator::RingAllocator(){

	#if 0
	cout<<"[RingAllocator::RingAllocator] "<<hex<<this<<dec<<endl;
	#endif

	m_memory=NULL;
}

/*
 * allocate a chunk of m_max bytes in constant time
 */
void*RingAllocator::allocate(int a){

	m_count++;

	#ifdef ASSERT
	assert(m_chunks>0);
	
	if(m_memory==NULL)
		cout<<"Error: you must call constructor() before calling allocate(), type="<<m_type<<" this "<<hex<<this<<dec<<endl;

	assert(m_memory!=NULL);
	
	if(a>m_max){
		cout<<"Request "<<a<<" but maximum is "<<m_max<<endl;
	}
	assert(a<=m_max);
	#endif

	int origin=m_current;

	// first half of the circle
	// from origin to N-1
	while(m_bufferStates[m_current] == BUFFER_STATE_DIRTY
	&& m_current < m_chunks){

		m_current++;

	}

	// start from 0 if we completed.
	// set i to 0
	if(m_current==m_chunks){
		m_current=0;
	}

	// from 0 to origin-1
	while(m_bufferStates[m_current] == BUFFER_STATE_DIRTY
	&& m_current < origin){

		m_current++;
	}

	// if all buffers are dirty, we throw a runtime error
	#ifdef ASSERT
	if(m_current==origin && m_bufferStates[m_current]==BUFFER_STATE_DIRTY){
		cout<<"Error: all buffers are dirty !, chunks: "<<m_chunks<<endl;
		assert(m_current!=origin);
	}
	#endif

	void*address=(void*)(m_memory+m_current*m_max);


	m_current++;

	// depending on the architecture
	// branching (if) can be faster than integer division/modulo

	if(m_current==m_chunks){
		m_current=0;
	}

	#ifdef ASSERT
	assert(address!=NULL);
	#endif

	return address;
}

void RingAllocator::salvageBuffer(void*buffer){
	int bufferNumber=getBufferHandle(buffer);

	m_bufferStates[bufferNumber]= BUFFER_STATE_AVAILABLE;

	#ifdef CONFIG_RING_VERBOSE
	cout<<"[RingAllocator::salvageBuffer] "<<bufferNumber<<" -> BUFFER_STATE_AVAILABLE"<<endl;
	#endif

	m_availableBuffers++;

	#ifdef ASSERT
	assert(m_availableBuffers>=1);
	#endif

}

void RingAllocator::markBufferAsDirty(void*buffer){
	int bufferNumber=getBufferHandle(buffer);

	m_bufferStates[bufferNumber]= BUFFER_STATE_DIRTY;

	#ifdef CONFIG_RING_VERBOSE
	cout<<"[RingAllocator::markBufferAsDirty] "<<bufferNumber<<" -> BUFFER_STATE_DIRTY"<<endl;
	#endif

	#ifdef ASSERT
	assert(m_availableBuffers>=1);
	#endif

	m_availableBuffers--;

	#ifdef ASSERT
	assert(m_availableBuffers>=0);
	#endif
}

int RingAllocator::getBufferHandle(void*buffer){

	void*origin=m_memory;

	uint64_t originValue=(uint64_t)origin;
	uint64_t bufferValue=(uint64_t)buffer;

	uint64_t difference=bufferValue-originValue;

	int index=difference/(m_max*sizeof(uint8_t));

	return index;
}

int RingAllocator::getSize(){
	return m_max;
}

void RingAllocator::clear(){

	#ifdef CONFIG_RING_VERBOSE
	cout<<"[RingAllocator::clear] "<<m_type<<" memory is "<<hex<<(void*)m_memory<<dec<<endl;
	#endif

	#ifdef ASSERT
	assert(m_memory!=NULL);
	#endif

	__Free(m_memory,m_type,m_show);
	m_memory=NULL;
}

void RingAllocator::resetCount(){
	m_count=0;
}

int RingAllocator::getCount(){
	return m_count;
}

int RingAllocator::getNumberOfBuffers(){
	return m_chunks;
}

void RingAllocator::initializeDirtyBuffers(){

	m_dirtyBufferSlots=getNumberOfBuffers();

	m_dirtyBuffers=(DirtyBuffer*)__Malloc(m_dirtyBufferSlots*sizeof(DirtyBuffer),
		"m_dirtyBuffers",false);

	for(int i=0;i<m_dirtyBufferSlots;i++){
		m_dirtyBuffers[i].m_buffer=NULL;
	}

	// configure the real-time sweeper.

	m_minimumNumberOfDirtyBuffersForSweep=m_dirtyBufferSlots/4;
	m_minimumNumberOfDirtyBuffersForWarning=m_dirtyBufferSlots/2;

}

DirtyBuffer*RingAllocator::getDirtyBuffers(){
	return m_dirtyBuffers;
}

/**
 * TODO Instead of a true/false state, increase and decrease requests
 * using a particular buffer. Otherwise, there may be a problem when 
 * a buffer is re-used several times for many requests.
 */
void RingAllocator::checkDirtyBuffer(int index){

	#ifdef ASSERT
	assert(m_numberOfDirtyBuffers>0);
	#endif

	if(m_dirtyBuffers[index].m_buffer==NULL)// this entry is empty...
		return;

	// check the buffer and free it if it is finished.
	MPI_Status status;
	MPI_Request*request=&(m_dirtyBuffers[index].m_messageRequest);

	int flag=0;

	MPI_Test(request,&flag,&status);

	if(!flag)// this slot is not ready
		return;

	#ifdef ASSERT
	assert( flag );
	#endif /* ASSERT */

	void*buffer=m_dirtyBuffers[index].m_buffer;
	salvageBuffer(buffer);
	m_numberOfDirtyBuffers--;

	#ifdef COMMUNICATION_IS_VERBOSE
	cout<<"From checkDirtyBuffer flag= "<<flag<<endl;
	#endif /* COMMUNICATION_IS_VERBOSE */

	#ifdef ASSERT
	assert(*request == MPI_REQUEST_NULL);
	#endif /* ASSERT */

	m_dirtyBuffers[index].m_buffer=NULL;
}

void RingAllocator::cleanDirtyBuffers(){

/**
 * don't do any linear sweep if we still have plenty of free cells
 */
	if(m_numberOfDirtyBuffers<m_minimumNumberOfDirtyBuffersForSweep)
		return;

	#ifdef ASSERT
	assert(m_numberOfDirtyBuffers>0);
	#endif

	m_linearSweeps++;

	// update the dirty buffer list.
	for(int i=0;i<m_dirtyBufferSlots;i++){
		if(m_numberOfDirtyBuffers==0)
			return;

		checkDirtyBuffer(i);
	}

	if(m_numberOfDirtyBuffers>=m_minimumNumberOfDirtyBuffersForWarning){
		cout<<"[MessagesHandler] Warning: dirty buffers are still dirty after linear sweep."<<endl;
		printDirtyBuffers();
	}
}

void RingAllocator::printDirtyBuffers(){

	#if 0
	cout<<"[MessagesHandler] Dirty buffers: "<<m_numberOfDirtyBuffers<<"/";
	cout<<m_dirtyBufferSlots<<endl;

	for(int i=0;i<m_dirtyBufferSlots;i++){
		cout<<"DirtyBuffer # "<<i<<"    State: ";
		if(m_dirtyBuffers[i].m_buffer==NULL){
			cout<<"Available"<<endl;
		}else{
			cout<<"Dirty"<<endl;

			MessageTag tag=m_dirtyBuffers[i].m_messageTag;
			Rank destination=m_dirtyBuffers[i].m_destination;
			Rank routingSource=m_rank;
			Rank routingDestination=destination;

			bool isRoutingTagValue=false;

/* TODO we don't have easy access to this information */
#if 0
			RoutingTag routingTag=tag;

			if(isRoutingTag(tag)){
				tag=getMessageTagFromRoutingTag(routingTag);
				routingSource=getSourceFromRoutingTag(routingTag);
				routingDestination=getDestinationFromRoutingTag(routingTag);

				isRoutingTagValue=true;
			}
#endif

			uint8_t index=tag;
			cout<<" MessageTag: "<<MESSAGE_TAGS[index]<<" ("<<(int)index<<") ";
			if(index<tag){
				cout<<"[this is a routing tag]"<<endl;
			}
			cout<<" Source: "<<m_rank<<endl;
			cout<<" Destination: "<<destination<<endl;

			if(isRoutingTagValue){
				cout<<" RoutingSource: "<<routingSource<<endl;
				cout<<" RoutingDestination: "<<routingDestination<<endl;
			}
		}
	}

	#endif
}

MPI_Request*RingAllocator::registerBuffer(void*buffer){

	int handle=this->getBufferHandle(buffer);
	bool mustRegister=false;

	MPI_Request*request=NULL;

	// this buffer is not registered.
	if(handle >=0 && m_dirtyBuffers[handle].m_buffer==NULL)
		mustRegister=true;

	#ifdef ASSERT
	assert(m_dirtyBuffers[handle].m_buffer==NULL);
	#endif

	/* register the buffer for processing */
	if(mustRegister){
		#ifdef ASSERT
		assert(m_dirtyBuffers[handle].m_buffer==NULL);
		#endif

		m_dirtyBuffers[handle].m_buffer=buffer;

		#if 0
		m_dirtyBuffers[handle].m_destination=destination;
		m_dirtyBuffers[handle].m_messageTag=tag;
		#endif

		#ifdef ASSERT
		assert(m_dirtyBuffers[handle].m_buffer!=NULL);
		#endif

		request=&(m_dirtyBuffers[handle].m_messageRequest);

		/* this is O(1) */
		this->markBufferAsDirty(buffer);

		m_numberOfDirtyBuffers++;

		// update the maximum number of dirty buffers
		// observed since the beginning.
		if(m_numberOfDirtyBuffers > m_maximumDirtyBuffers)
			m_maximumDirtyBuffers=m_numberOfDirtyBuffers;
	}

	return request;
}

void RingAllocator::printStatus(){
	
	#if 0
	cout<<"Rank "<<m_rank<<": the maximum number of dirty buffers was "<<m_maximumDirtyBuffers<<endl;
	cout<<"Rank "<<m_rank<<": "<<m_linearSweeps<<" linear sweep operations (threshold: ";
	cout<<m_minimumNumberOfDirtyBuffersForSweep<<" dirty buffers)"<<endl;
	#endif

}
