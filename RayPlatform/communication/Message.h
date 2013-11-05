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

#ifndef _Message_H
#define _Message_H

#define MESSAGE_META_DATA_ACTOR_SOURCE		0
#define MESSAGE_META_DATA_ACTOR_DESTINATION	( MESSAGE_META_DATA_ACTOR_SOURCE + sizeof(int) )
#define MESSAGE_META_DATA_ROUTE_SOURCE	 	( MESSAGE_META_DATA_ACTOR_DESTINATION + sizeof(int) )
#define MESSAGE_META_DATA_ROUTE_DESTINATION	( MESSAGE_META_DATA_ROUTE_SOURCE + sizeof(int))
#define MESSAGE_META_DATA_CHECKSUM	 	( MESSAGE_META_DATA_ROUTE_DESTINATION + sizeof(int))
#define MESSAGE_META_DATA_SIZE                  ( MESSAGE_META_DATA_CHECKSUM + sizeof(uint32_t) )



#include <RayPlatform/core/types.h>

#include <stdint.h>

/**
* In Ray, every message is a Message.
* the inbox and the outbox are arrays of Message's
* All the code in Ray utilise Message to communicate information.
* MPI_Datatype is always MPI_UNSIGNED_LONG_LONG
* m_count is >=0 and <= MAXIMUM_MESSAGE_SIZE_IN_BYTES/sizeof(uint64_t)
*  (default is 4000/8 = 500 ).
*
* in the buffer:
*
*  Application payload (works)
*  int source actor (works)
*  int destination actor (works)
*  int routing source (works)
*  int routing destination (works)
*  int checksum (probably broken)
*
 * The offset (see Message.cpp) are relative to the number of bytes *BEFORE*
 * adding this metadata.
 * Note that the m_count is the number of MessageUnit for the message
 * and may include metadata if saveActorMetaData or saveRoutingMetaData
 * was called.
 *
 * Obviously, this is the correct order for calling this methods:
 *
 * setSourceActor
 * setDestinationActor
 * saveActorMetaData  (increases m_count by 1 MessageUnit)
 * setRoutingSource
 * setRoutingDestination
 * saveRoutingMetaData (increases m_count by 1 MessageUnit)
 *
 * then, the message can be sent.
 *
 * On the receiving side:
 *
 * loadActorMetaData
 * getSourceActor
 * getDestinationActor (decreases m_count by 1 MessageUnit)
 * loadRoutingMetaData
 * getRoutingSource
 * getRoutingDestination (decreases m_count by 1 MessageUnit)
 *
 * All this crap is necessary because messages exposed to actors or to MPI ranks
 * do not contain these metadata.
 *
 * The platform allows MAXIMUM_MESSAGE_SIZE_IN_BYTES for buffers of application messages.
 * To this, we add 2 * MessageUnit (1 MessageUnit for actors, 1 MessageUnit for routing).
 *
 * TODO: the checksum code is probably broken by now. check that out.
 *
 * \author Sébastien Boisvert
 */
class Message{
private:

	/** the message body, contains data
 * 	if NULL, m_count must be 0 */
	void * m_buffer;

	/** the number of uint64_t that the m_buffer contains
 * 	can be 0 regardless of m_buffer value
 * 	*/
	//int m_count;
	int m_bytes;

	/** the Message-passing interface rank destination
 * 	Must be >=0 and <= MPI_Comm_size()-1 */
	Rank m_destination;

	/**
 * 	Ray message-passing interface message tags are named RAY_MPI_TAG_<something>
 * 	see mpi_tag_macros.h
 */
	MessageTag m_tag;

	/** the message-passing interface rank source
 * 	Must be >=0 and <= MPI_Comm_size()-1 */
	Rank m_source;

	int m_sourceActor;
	int m_destinationActor;

	int m_routingSource;
	int m_routingDestination;

	void initialize();
public:
	Message();
	~Message();

	/**
	 * numberOf8Bytes is there for historical reasons.
	 * You can call setNumberOfBytes afterward to set the number of bytes anyway.
	 *
	 */
	Message(void* buffer, int numberOf8Bytes, Rank destination, MessageTag tag,
			Rank source);
	MessageUnit *getBuffer();
	char * getBufferBytes();
	int getCount() const;
/**
 * Returns the destination MPI rank
 */
	Rank getDestination() const;

/**
 * Returns the message tag (RAY_MPI_TAG_something)
 */
	MessageTag getTag() const;
/**
 * Gets the source MPI rank
 */
	Rank getSource() const;

	void print();

	void setBuffer(void*buffer);

	void setTag(MessageTag tag);

	void setSource(Rank source);

	void setDestination(Rank destination);

	void setCount(int count);
	void setNumberOfBytes(int bytes);
	int getNumberOfBytes() const;

	// actor model endpoints


	bool isActorModelMessage(int ranks) const;

	int getDestinationActor() const;
	int getSourceActor() const;
	void setSourceActor(int sourceActor);
	void setDestinationActor(int destinationActor);
	void saveActorMetaData();
	void loadActorMetaData();
	int getMetaDataSize() const;
	void printActorMetaData();

	void setRoutingSource(int source);
	void setRoutingDestination(int destination);
	void saveRoutingMetaData();

	int getRoutingSource() const;
	int getRoutingDestination() const;
	void loadRoutingMetaData();
	void displayMetaData();

	void runAssertions();
};

#endif
