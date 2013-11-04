/*
    RayPlatform: a message-passing development framework
    Copyright (C) 2010, 2011, 2012, 2013 Sébastien Boisvert

	http://github.com/sebhtml/RayPlatform: a message-passing development framework

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

#include "Playground.h"

#include <RayPlatform/core/ComputeCore.h>
#include <RayPlatform/memory/RingAllocator.h>

#include <stdlib.h>

#ifdef CONFIG_ASSERT
#include <assert.h>
#endif

Playground::Playground() {

	m_actorIterator = 0;
	m_aliveActors = 0;

	m_zombieActors = 0;
	m_deadActors = 0;
	m_bornActors = 0;
}

Playground::~Playground() {

}

void Playground::spawnActor(Actor * actor) {

	// actor 0 on rank 0 is the rank itself !
	// actor i on rank i is the rank itself too.
	if(m_actorIterator == 0) {
		m_actorIterator++;
		Actor * actor = NULL;
		m_actors.push_back(actor);
	}

	int identifier = m_computeCore->getRank() + m_actorIterator * getSize();
	actor->configureStuff(identifier, this);

	//cout << "DEBUG ... spawnActor name= " << identifier << endl;

	m_actors.push_back(actor);

	m_actorIterator++;

	m_aliveActors ++;

	m_bornActors ++;
}

void Playground::sendActorMessage(Message * message) {

	int sourceActor = message->getSourceActor();
	int destinationActor = message->getDestinationActor();

	int sourceRank = getActorRank(sourceActor);
	int destinationRank = getActorRank(destinationActor);

#ifdef CONFIG_ASSERT
	assert(sourceActor >= 0);
	assert(destinationActor >= 0);
	assert(sourceRank >= 0);
	assert(destinationRank >= 0);
	assert(sourceRank < getSize());
	assert(destinationRank < getSize());
#endif

#if 0
	cout << "DEBUG ... " << message << " sendActorMessage sourceActor= ";
	cout << sourceActor << " destinationActor= ";
	cout << destinationActor << " sourceRank= " << sourceRank;
	cout << " tag= " << message->getTag();
	cout << " bytes= " << message->getNumberOfBytes();
	cout << " destinationRank= " << destinationRank << endl;
#endif

	message->setSource(sourceRank);
	message->setDestination(destinationRank);

	getComputeCore()->send(message);
}

int Playground::getActorRank(int name) const {

	return name % getSize();
}

int Playground::getRank() const {
	return m_computeCore->getRank();
}

int Playground::getSize() const {

	return m_computeCore->getSize();
}

void Playground::receiveActorMessage(Message * message) {

	int actorName = message->getDestinationActor();

	int index = actorName / getSize();

#ifdef CONFIG_ASSERT
	if(index < 0) {
		cout << "Error: negative actor name.";
		cout << " actorName " << actorName;
		cout << " getSize " << getSize();
		cout << " index " << index;
		message->printActorMetaData();
		cout << endl;
	}
	assert(index >= 0);
#endif

#if 0
	cout << "DEBUG .... Rank= " << getRank();
	cout << " tag= " << message->getTag();
	cout << " receiveActorMessage actorName= " << actorName;
	cout << " index=> " << index;
	message->printActorMetaData();
	cout << endl;
#endif

	if(!(index < (int) m_actors.size()))
		return;

	Actor * actor = m_actors[index];

	if(actor == NULL)
		return;

#ifdef CONFIG_ASSERT
	assert( message != NULL );
#endif

	actor->receive(*message);

	if(actor->isDead()) {

		m_aliveActors --;

		m_deadActors ++;
		m_zombieActors += 0;

		/*
		cout << "DEBUG Playground actor " << actor->getName() << " died, remaining: ";
		cout << m_aliveActors << endl;
		*/

		delete actor;
		actor = NULL;
		m_actors[index] = NULL;
	}
}

bool Playground::hasAliveActors() const {

	return m_aliveActors > 0;
}

void Playground::bootActors() {

	//cout << "DEBUG ... bootActors" << endl;

	int toBoot = m_actors.size();

	for(int i = 0 ; i < toBoot ; ++i) {

		Actor * actor = m_actors[i];
		if(actor == NULL) {
			continue;
		}

		int name = actor->getName();

		Message message;
		message.setTag(Actor::BOOT);
		message.setSource(m_computeCore->getRank());
		message.setDestination(m_computeCore->getRank());
		message.setSourceActor(name);
		message.setDestinationActor(name);

		//cout << "DEBUG booting actor # " << name << endl;

		receiveActorMessage(&message);
	}
}

void Playground::initialize(ComputeCore * core) {

	m_computeCore = core;
}

ComputeCore * Playground::getComputeCore() {
	return m_computeCore;
}
RingAllocator * Playground::getOutboxAllocator() {

	return getComputeCore()->getOutboxAllocator();
}

int Playground::getNumberOfAliveActors() const {

	return m_aliveActors;
}

void Playground::printStatus() const {

	cout << "[Playground] Actors: ";
	cout << "| Born " << m_bornActors;
	cout << "| Dead " << m_deadActors;
	cout << "| Defunct " << m_zombieActors;
	cout << "| Alive " << m_aliveActors;

	cout << endl;

}
