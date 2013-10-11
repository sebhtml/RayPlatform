
// author: Sébastien Boisvert
// Université Laval 2013-10-10

#ifndef ActorHeader

#define ActorHeader

class ComputeCore;
class Message;

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
	ComputeCore * m_core;

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
	void configureStuff(int name, ComputeCore * kernel);
	int getName() const;
	void printName() const;

	int getRank() const;
	int getSize() const;
};

#endif
