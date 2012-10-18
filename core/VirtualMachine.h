/*
 	RayPlatform
    Copyright (C) 2012  Sébastien Boisvert

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

#ifndef _VirtualMachine_h

#define _VirtualMachine_h

#include "communication/MessagesHandler.h"
#include <pthread.h>

#define MAXIMUM_NUMBER_OF_MINIRANKS_PER_RANK 64

/**
 * The VirtualMachine is part of the layered minirank
 * architecture.
 *
 * \author Sébastien Boisvert
 */
class VirtualMachine{

	pthread_mutex_t m_mutex;

	int m_deadMiniRanks;

	/** middleware to handle messages */
	MessagesHandler m_messagesHandler;

	MiniRank*m_miniRanks[MAXIMUM_NUMBER_OF_MINIRANKS_PER_RANK];
	pthread_t m_threads[MAXIMUM_NUMBER_OF_MINIRANKS_PER_RANK];

	int m_numberOfMiniRanksPerRank;
	int m_numberOfRanks;
	int m_numberOfInstalledMiniRanks;
	int m_numberOfMiniRanks;

	/** get the middleware object */
	MessagesHandler*getMessagesHandler();

public:
	void constructor(int numberOfMiniRanksPerRank);
	void addMiniRank(MiniRank*miniRank);

	void run();
};

#endif /* _VirtualMachine_h */

