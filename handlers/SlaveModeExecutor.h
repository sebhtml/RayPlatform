/*
 	Ray
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

#ifndef _SlaveModeExecutor_h
#define _SlaveModeExecutor_h

#include <handlers/SlaveModeHandler.h>
#include <core/slave_modes.h>
#include <core/types.h>

/**
 * This class is responsible to handling event 
 * for master modes.
 * The technology uses aggressive caching.
 *
 * \author Sébastien Boisvert
 */
class SlaveModeExecutor{

#ifdef CONFIG_CACHE_OPERATION_CODES
	SlaveMode m_cachedOperationCode;
	SlaveModeHandlerReference m_cachedOperationHandler;
#endif /* CONFIG_CACHE_OPERATION_CODES */

/** table of slave objects */
	SlaveModeHandlerReference m_objects[MAXIMUM_NUMBER_OF_SLAVE_HANDLERS];

public:

/** call the handler for a given slave mode */
	void callHandler(SlaveMode mode);

/** initialise default object and method handlers */
	SlaveModeExecutor();

/** set the object to call for a slave mode */
	void setObjectHandler(SlaveMode mode,SlaveModeHandlerReference object);

};

#endif

