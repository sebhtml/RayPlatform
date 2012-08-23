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

// #define CONFIG_RING_VERBOSE

#define BUFFER_STATE_AVAILABLE 0x0
#define BUFFER_STATE_DIRTY 0x1

void RingAllocator::constructor(int chunks,int size,const char*type,bool show){

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

	m_memory=(uint8_t*)__Malloc(sizeof(uint8_t)*m_numberOfBytes,m_type,show);
	m_bufferStates=(uint8_t*)__Malloc(m_chunks*sizeof(uint8_t),m_type,show);

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
}

RingAllocator::RingAllocator(){}

/*
 * allocate a chunk of m_max bytes in constant time
 */
void*RingAllocator::allocate(int a){
	#ifdef ASSERT
	m_count++;
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
