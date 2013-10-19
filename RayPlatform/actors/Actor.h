/*
    Copyright 2013 Sébastien Boisvert
    Copyright 2013 Université Laval
    Copyright 2013 Centre Hospitalier Universitaire de Québec

    This file is part of the actor model implementation in RayPlatform.

    RayPlatform is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, version 3 of the License.

    RayPlatform is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with RayPlatform.  If not, see <http://www.gnu.org/licenses/>.
*/

// author: Sébastien Boisvert
// Université Laval 2013-10-10

#ifndef ActorHeader

#define ActorHeader

class ComputeCore;
class Message;
class Playground;

#include <RayPlatform/communication/Message.h>

/**
 * actor model.
 *
 * \author Sébastien Boisvert
 * \see https://github.com/sebhtml/BioActors
 */
class Actor {

private:
	int m_name;
	Playground * m_core;
	bool m_dead;

	void send(int destination, Message * message);
public:
	
	enum {
		BOOT = 10000
	};

	Actor();
	virtual ~Actor();

	virtual void receive(Message & message) = 0;
	void send(int destination, Message & message);
	void spawn(Actor * actor);
	void configureStuff(int name, Playground * kernel);
	int getName() const;
	void printName() const;

	int getRank() const;
	int getSize() const;

	void die();
	bool isDead() const;
};

#endif
