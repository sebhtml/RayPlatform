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

#ifndef _RingAllocator
#define _RingAllocator

#include<set>
#include<stdint.h>
#include <core/types.h> 
using namespace std;

/**
 * This class is a ring buffer. No !, it is an allocator. Thus, referred to as a ring allocator.
 *
 * This is an allocator that can allocate up to <m_chunks> allocations of exactly <m_max> bytes.
 * allocation and free are done both in constant time (yeah!)
 * \author Sébastien Boisvert
 */
class RingAllocator{
/** the number of call to allocate() since the last hard reset */
	int m_count;

/** the number of memory cells */
	int m_chunks;

/** the number of bytes, linear in the number of chunks/cells */
	int m_numberOfBytes;

/** the maximum size of a buffer */
	int m_max;

/** the number of available buffers */
	int m_availableBuffers;

/** the head */
	int m_current;

	bool m_show;
/** memory block */

	uint8_t*m_memory;

	char m_type[100];
	
	uint8_t*m_bufferStates;


public:
	RingAllocator();
	void constructor(int chunks,int size,const char*type,bool show);

/** allocate memory */
	void*allocate(int a);

/**
 * Gets the size of each buffer
 */
	int getSize();

	void clear();
	void resetCount();
	int getCount();

/**
 * marks a buffer as available
 */
	void salvageBuffer(void*buffer);

/**
 * marks a buffer as used
 */
	void markBufferAsDirty(void*buffer);

/**
 * returns the number of buffers in the ring
 */
	int getNumberOfBuffers();

/**
 * get the handle for a buffer
 */
	int getBufferHandle(void*buffer);
};


#endif

