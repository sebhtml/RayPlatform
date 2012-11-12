/*
 	RayPlatform: a message-passing development framework
    Copyright (C) 2010, 2011, 2012 Sébastien Boisvert

	http://DeNovoAssembler.SourceForge.Net/

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

#ifndef _MasterModeExecutor_h
#define _MasterModeExecutor_h

#include "MasterModeHandler.h"

#include <RayPlatform/core/types.h>
#include <RayPlatform/core/master_modes.h>

/**
 * This class is responsible to handling event 
 * for master modes.
 * The technology uses aggressive caching.
 *
 * \author Sébastien Boisvert
 */
class MasterModeExecutor{

#ifdef CONFIG_CACHE_OPERATION_CODES
	MasterMode m_cachedOperationCode;
	MasterModeHandlerReference m_cachedOperationHandler;
#endif /* CONFIG_CACHE_OPERATION_CODES */
	
/** a list of objects to use for calling methods */
	MasterModeHandlerReference m_objects[MAXIMUM_NUMBER_OF_MASTER_HANDLERS];

public:
	/** call the handler */
	void callHandler(MasterMode mode);

/** set the correct object to call for a given master mode */
	void setObjectHandler(MasterMode mode, MasterModeHandlerReference object);

/** initialise default object and method handlers */
	MasterModeExecutor();
};

#endif

