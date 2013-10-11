// author: Sébastien Boisvert
// Université Laval 2013-10-10

#include "Actor.h"

#include <RayPlatform/communication/Message.h>
#include <RayPlatform/core/ComputeCore.h>

#include <iostream>
using namespace std;

Actor::Actor() {
}

Actor::~Actor() {
}

void Actor::send(int destination, Message * message) {

	message->setSourceActor(getName());
	message->setDestinationActor(destination);

	m_core->sendActorMessage(message);
}

void Actor::spawn(Actor * actor) {
	
	m_core->spawnActor(actor);
}

void Actor::configureStuff(int name, ComputeCore * kernel) {

	m_name = name;
	m_core = kernel;
}

int Actor::getName() const {
	return m_name;
}

void Actor::printName() const {

	cout << "/actors/" << getName();
	cout << " -> ";
}
