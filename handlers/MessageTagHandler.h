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

#ifndef _MessageTagHandler_h
#define _MessageTagHandler_h


/* this is a macro to create the cpp code for an adapter */
#define __CreateMessageTagAdapter( corePlugin, handle ) \
void __GetAdapter( corePlugin, handle) ( Message*message ) { \
	__GetPlugin( corePlugin ) -> __GetMethod( handle ) ( message ) ; \
} 



#include <communication/Message.h>
#include <core/types.h>
#include <communication/mpi_tags.h>

/**
 * base class for handling message tags
 * \author Sébastien Boisvert
 * with help from Élénie Godzaridis for the design
 * \date 2012-08-08 replaced this with function pointers
 */
typedef void (*MessageTagHandler) (Message*message) /* */ ;


#endif
