/*
 *  RayPlatform: a message-passing development framework
 *  Copyright (C) 2013 Sébastien Boisvert
 *
 *  http://github.com/sebhtml/RayPlatform
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation, version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You have received a copy of the GNU Lesser General Public License
 *  along with this program (lgpl-3.0.txt).
 *  see <http://www.gnu.org/licenses/>
 *
 * HeaderVersion_ 2013-08-19
 * /sebseb
 */

#ifndef DirtyBufferHeader
#define DirtyBufferHeader

#include <mpi.h>

#include <RayPlatform/core/types.h>

/**
 * A data model for storing dirty buffers
 */
class DirtyBuffer{

	void * m_buffer;
	MPI_Request m_messageRequest;
	Rank m_destination;
	MessageTag m_messageTag;
	Rank m_source;

public:

	void setBuffer(void * buffer);
	void setSource(Rank  source);
	void setDestination(Rank  destination);
	void setTag(MessageTag tag);

	Rank getSource();
	Rank getDestination();
	MessageTag getTag();
	MPI_Request * getRequest();
	void * getBuffer();
};

#endif // DirtyBufferHeader
