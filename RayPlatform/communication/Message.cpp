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

#include "Message.h"
#include "mpi_tags.h"

#include <iostream>
using namespace std;

#include <string.h>



#define ACTOR_MODEL_NOBODY -1

/**
 * We always pad the buffer with actor source and actor destination.
 * If routing is enabled, we also pad with the route source and the route destination.
 * Finally, the buffer may be padded with a checksum too !
 */
#define MESSAGE_META_DATA_ACTOR_SOURCE		0x000
#define MESSAGE_META_DATA_ACTOR_DESTINATION	0x004
#define MESSAGE_META_DATA_ROUTE_SOURCE	 	0x008
#define MESSAGE_META_DATA_ROUTE_DESTINATION	0x012
#define MESSAGE_META_DATA_CHECKSUM	 	0x016


Message::Message() {

	initialize();
}

void Message::initialize() {
	m_buffer = NULL;
	m_count = 0;
	m_tag = 0;
	m_source = 0;
	m_destination = 0;
	m_sourceActor = ACTOR_MODEL_NOBODY;
	m_destinationActor = ACTOR_MODEL_NOBODY;
}

Message::~Message() {
}

/** buffer must be allocated or else it will CORE DUMP. */
Message::Message(MessageUnit*b,int c,Rank dest,MessageTag tag,Rank source){

	initialize();

	m_buffer=b;
	m_count=c;
	m_destination=dest;
	m_tag=tag;
	m_source=source;

	/*
	cout << "DEBUG in constructor -> ";
	printActorMetaData();
	cout << endl;
	*/
}

MessageUnit*Message::getBuffer(){
	return m_buffer;
}

int Message::getCount() const{
	return m_count;
}

Rank Message::getDestination() const{
	return m_destination;
}

MessageTag Message::getTag() const{
	return m_tag;
}

//Message::Message(){}

int Message::getSource() const{
	return m_source;
}

void Message::print(){
	uint8_t shortTag=getTag();

	cout<<"Source: "<<getSource()<<" Destination: "<<getDestination()<<" Tag: "<<MESSAGE_TAGS[shortTag];
	cout<<" RealTag: "<<getTag();
	cout<<" Count: "<<getCount();

	if(m_count > 0){
		cout<<" Overlay: "<<m_buffer[0];
	}
}

void Message::setBuffer(MessageUnit*buffer){
	m_buffer = buffer;
}

void Message::setTag(MessageTag tag){
	m_tag=tag;
}

void Message::setCount(int count){
	m_count=count;
}

void Message::setSource(Rank source){
	m_source=source;
}

void Message::setDestination(Rank destination){
	m_destination=destination;
}

bool Message::isActorModelMessage() const {

	return ( m_sourceActor != ACTOR_MODEL_NOBODY
		       	&& m_destinationActor != ACTOR_MODEL_NOBODY );
}


int Message::getDestinationActor() const {
	return m_destinationActor;
}

int Message::getSourceActor() const {
	return m_sourceActor;
}

void Message::setSourceActor(int sourceActor) {
	m_sourceActor = sourceActor;
}

void Message::setDestinationActor(int destinationActor) {
	m_destinationActor = destinationActor;
}

void Message::saveActorMetaData() {

	int offset = getCount() * sizeof(MessageUnit);
	char * memory = (char*) getBuffer();

	memcpy(memory + offset + MESSAGE_META_DATA_ACTOR_SOURCE, &m_sourceActor, sizeof(int));
	memcpy(memory + offset + MESSAGE_META_DATA_ACTOR_DESTINATION, &m_destinationActor, sizeof(int));
	//printActorMetaData();

	m_count += 1; // add 1 uint64_t

	/*
	cout << "DEBUG after saveActorMetaData ";
	printActorMetaData();
	cout << endl;
	*/
}

void Message::printActorMetaData() {
	cout << "DEBUG tag= " << getTag();
	cout << " saveActorMetaData m_sourceActor = " << getSourceActor();
	cout << " m_destinationActor = " << getDestinationActor() << " ";
	cout << " bytes= " << m_count * sizeof(MessageUnit);

}

void Message::loadActorMetaData() {
/*
	cout << "DEBUG before loadActorMetaData -> ";
	printActorMetaData();
	cout << endl;
*/
	int offset = getCount() * sizeof(MessageUnit);
	offset -= 2 * sizeof(int);

	char * memory = (char*) getBuffer();

	memcpy(&m_sourceActor, memory + offset + MESSAGE_META_DATA_ACTOR_SOURCE, sizeof(int));
	memcpy(&m_destinationActor, memory + offset + MESSAGE_META_DATA_ACTOR_DESTINATION, sizeof(int));


	/*
	cout << "DEBUG loadActorMetaData m_sourceActor= " << m_sourceActor;
	cout << " m_destinationActor= " << m_destinationActor << endl;
*/
	// remove 1 uint64_t
	
	m_count -= 1;
/*
	cout << "DEBUG after loadActorMetaData -> ";
	printActorMetaData();
	cout << endl;
	*/
}

int Message::getMetaDataSize() const {
	return 4 * sizeof(int);
}
