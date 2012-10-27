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

#ifndef _RankProcess_h

#define _RankProcess_h

#include "communication/MessagesHandler.h"

#define MAXIMUM_NUMBER_OF_MINIRANKS_PER_RANK 64

/*
 * The Rank is part of the layered minirank
 * architecture.
 *
 * \author Sébastien Boisvert
 */
class RankProcess{

	int m_argc;
	char**m_argv;

	bool m_communicate;

	bool m_mustWait[MAXIMUM_NUMBER_OF_MINIRANKS_PER_RANK];

/*
 * Middleware to handle messages.
 */
	MessagesHandler m_messagesHandler;

	MiniRank*m_miniRanks[MAXIMUM_NUMBER_OF_MINIRANKS_PER_RANK];
	pthread_t m_threads[MAXIMUM_NUMBER_OF_MINIRANKS_PER_RANK];
	ComputeCore*m_cores[MAXIMUM_NUMBER_OF_MINIRANKS_PER_RANK];

/**
 * The number of this rank.
 */
	int m_rank;

/** 
 * The number of ranks.
 */
	int m_numberOfRanks;

	int m_numberOfMiniRanksPerRank;
	int m_numberOfInstalledMiniRanks;

/*
 * The total number of mini-ranks in ranks.
 */
	int m_numberOfMiniRanks;

	void sendMessages();
	void receiveMessages();
public:
	void constructor(int numberOfMiniRanksPerRank,int*argc,char***argv);
	void addMiniRank(MiniRank*miniRank);
	void destructor();

	void run();

/*
 * Get the middleware object 
 */
	MessagesHandler*getMessagesHandler();
};

#endif /* _Rank_h */

