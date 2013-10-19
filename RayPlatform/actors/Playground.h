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

#ifndef PlaygroundHeader
#define PlaygroundHeader

#include <RayPlatform/actors/Actor.h>

#include <vector>
using namespace std;

class ComputeCore;
class RingAllocator;

class Playground {

private:

	vector<Actor*> m_actors;
	int m_actorIterator;
	int m_aliveActors;
	ComputeCore * m_computeCore;

public:

	Playground();
	~Playground();

	void initialize(ComputeCore * core);
	bool hasAliveActors() const;

	// actor stuff !!!

	/**
	 * assign a unique identifier to the actor
	 * and add it to the party team.
	 */
	void spawnActor(Actor * actor);

	/**
	 * send a message to an actor.
	 */
	void sendActorMessage(Message * message);

	/**
	 * receive a message for an actor.
	 */
	void receiveActorMessage(Message * message);
	void bootActors();
	int getActorRank(int name) const;

	RingAllocator * getOutboxAllocator();
	ComputeCore * getComputeCore();

	int getRank() const;
	int getSize() const;
};

#endif
