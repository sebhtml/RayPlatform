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



#include "DirtyBuffer.h"

void DirtyBuffer::setBuffer(void * buffer) {

	m_buffer = buffer;
}

void DirtyBuffer::setSource(Rank source) {

	m_source = source;
}

void DirtyBuffer::setDestination(Rank destination) {

	m_destination = destination;
}

void DirtyBuffer::setTag(MessageTag tag) {

	m_messageTag = tag;
}

Rank DirtyBuffer::getSource() {

	return m_source;
}

Rank DirtyBuffer::getDestination() {

	return m_destination;
}

MessageTag DirtyBuffer::getTag() {

	return m_messageTag;
}

MPI_Request * DirtyBuffer::getRequest() {

	return & m_messageRequest;
}

void * DirtyBuffer::getBuffer() {

	return m_buffer;
}

