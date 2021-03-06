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

#ifndef _MessageTagHandler_h
#define _MessageTagHandler_h

#include <RayPlatform/core/types.h>
#include <RayPlatform/communication/Message.h>
#include <RayPlatform/communication/mpi_tags.h>

#ifdef CONFIG_MINI_RANKS

#define MessageTagHandlerReference MessageTagHandler*

#define __CreateMessageTagAdapter ____CreateMessageTagAdapterImplementation
#define __DeclareMessageTagAdapter ____CreateMessageTagAdapterDeclaration

/* this is a macro to create the header code for an adapter */
#define ____CreateMessageTagAdapterDeclaration(corePlugin,handle) \
class Adapter_ ## handle : public MessageTagHandler{ \
	corePlugin *m_object; \
public: \
	void setObject(corePlugin *object); \
	void call(Message*message); \
};

/* this is a macro to create the cpp code for an adapter */
#define ____CreateMessageTagAdapterImplementation(corePlugin,handle)\
void Adapter_ ## handle ::setObject( corePlugin *object){ \
	m_object=object; \
} \
 \
void Adapter_ ## handle ::call(Message*message){ \
	m_object->call_ ## handle(message); \
}

/**
 * base class for handling message tags
 * \author Sébastien Boisvert
 * with help from Élénie Godzaridis for the design
 */
class MessageTagHandler{
public:

	virtual void call(Message*message) = 0;

	virtual ~MessageTagHandler(){}
};

#else

/*
 * Without mini-ranks.
 */

#define __DeclareMessageTagAdapter(plugin, handle)

/* this is a macro to create the cpp code for an adapter */
#define __CreateMessageTagAdapter( corePlugin, handle ) \
void __GetAdapter( corePlugin, handle) ( Message*message ) { \
	__GetPlugin( corePlugin ) -> __GetMethod( handle ) ( message ) ; \
} 

/**
 * base class for handling message tags
 * \author Sébastien Boisvert
 * with help from Élénie Godzaridis for the design
 * \date 2012-08-08 replaced this with function pointers
 */
typedef void (*MessageTagHandler) (Message*message) /* */ ;

#define MessageTagHandlerReference MessageTagHandler

#endif /* CONFIG_MINI_RANKS */

#define __ConfigureMessageTagHandler(pluginName, handlerHandle) \
	handlerHandle= m_core->allocateMessageTagHandle(m_plugin); \
	m_core->setMessageTagObjectHandler(m_plugin, handlerHandle, \
		__GetAdapter(pluginName,handlerHandle)); \
	m_core->setMessageTagSymbol(m_plugin, handlerHandle, # handlerHandle); \
	__BindAdapter(pluginName, handlerHandle);


#endif
