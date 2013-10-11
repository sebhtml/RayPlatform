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

#define ACTOR_MODEL_NOBODY -1

/** buffer must be allocated or else it will CORE DUMP. */
Message::Message(MessageUnit*b,int c,Rank dest,MessageTag tag,Rank source){
	m_buffer=b;
	m_count=c;
	m_destination=dest;
	m_tag=tag;
	m_source=source;

	m_sourceActor = ACTOR_MODEL_NOBODY;
	m_destinationActor = ACTOR_MODEL_NOBODY;
}

MessageUnit*Message::getBuffer(){
	return m_buffer;
}

int Message::getCount(){
	return m_count;
}

Rank Message::getDestination(){
	return m_destination;
}

MessageTag Message::getTag(){
	return m_tag;
}

Message::Message(){}

int Message::getSource(){
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

	return m_sourceActor != ACTOR_MODEL_NOBODY ||
		m_destinationActor == ACTOR_MODEL_NOBODY;
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
