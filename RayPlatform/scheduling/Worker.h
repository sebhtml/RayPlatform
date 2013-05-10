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

#ifndef _Worker_h
#define _Worker_h

#include <RayPlatform/memory/RingAllocator.h>
#include <RayPlatform/communication/VirtualCommunicator.h>

#include <stdint.h>

/** a general worker class 
 * \author Sébastien Boisvert
 */
class Worker{

public:

	/** work a little bit 
	 * the class Worker provides no implementation for that 
	*/
	virtual void work() = 0;

	/** is the worker done doing its things */
	virtual bool isDone() = 0;

	/** get the worker number */
	virtual WorkerHandle getWorkerIdentifier() = 0 ;

	virtual ~Worker(){}
};

#endif
