/*
 	RayPlatform: a message-passing development framework
    Copyright (C) 2010, 2011, 2012 Sébastien Boisvert

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

#ifndef _RingAllocator
#define _RingAllocator

#include "DirtyBuffer.h"

#include <RayPlatform/core/types.h> 

#include <set>
#include <stdint.h>
#include <mpi.h>
using namespace std;


/**
 * This class is a ring buffer. No !, it is an allocator. Thus, referred to as a ring allocator.
 *
 * This is an allocator that can allocate up to <m_chunks> allocations of exactly <m_max> bytes.
 * allocation and free are done both in constant time (yeah!)
 * \author Sébastien Boisvert
 */
class RingAllocator{

	int m_minimumNumberOfDirtyBuffersForSweep;
	int m_minimumNumberOfDirtyBuffersForWarning;
	uint64_t m_linearSweeps;
/** prints dirty buffers **/
	void printDirtyBuffers();

	Rank m_rank;

	// this contain information about dirty buffers.
	// the state are stored in m_bufferStates

	DirtyBuffer * m_dirtyBuffers;

	int m_numberOfDirtyBuffers;
	int m_maximumDirtyBuffers;
	int m_dirtyBufferSlots;


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

/**
 * marks a buffer as used
 */
	void markBufferAsDirty(void*buffer);

/**
 * marks a buffer as available
 */

	void salvageBuffer(void*buffer);


public:

	void setRegisteredBufferAttributes(void * buffer,
		Rank source,
		Rank destination, MessageTag tag);

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
 * returns the number of buffers in the ring
 */
	int getNumberOfBuffers();

/**
 * get the handle for a buffer
 */
	int getBufferHandle(void*buffer);

	void checkDirtyBuffer(int i);
	void cleanDirtyBuffers();
	void initializeDirtyBuffers();
	DirtyBuffer*getDirtyBuffers();
	MPI_Request*registerBuffer(void*buffer);

	void printStatus();

	bool isRegistered(int handle);
};


#endif

