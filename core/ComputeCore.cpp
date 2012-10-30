/*
 	Ray
    Copyright (C) 2010, 2011, 2012  Sébastien Boisvert

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

#include <core/ComputeCore.h>
#include <core/OperatingSystem.h>
#include <cryptography/crypto.h>
#include <stdlib.h>
#include <string.h> /* for strcpy */

#ifdef ASSERT
#include <assert.h>
#endif

#include <iostream>
using namespace std;

//#define CONFIG_DEBUG_SLAVE_SYMBOLS
//#define CONFIG_DEBUG_MASTER_SYMBOLS
//#define CONFIG_DEBUG_TAG_SYMBOLS
//#define CONFIG_DEBUG_processData
//#define CONFIG_DEBUG_CORE

ComputeCore::ComputeCore(){
}

void ComputeCore::setSlaveModeObjectHandler(PluginHandle plugin,SlaveMode mode,SlaveModeHandler object){
	if(!validationPluginAllocated(plugin))
		return;

	if(!validationSlaveModeRange(plugin,mode))
		return;

	if(!validationSlaveModeOwnership(plugin,mode)){
		cout<<"was trying to set object handler for mode "<<mode<<endl;
		return;
	}

	#ifdef CONFIG_DEBUG_CORE
	cout<<"setSlaveModeObjectHandler "<<SLAVE_MODES[mode]<<" to "<<object<<endl;
	#endif

	#ifdef ASSERT
	assert(m_plugins.count(plugin)>0);
	assert(m_plugins[plugin].hasSlaveMode(mode));
	#endif

	m_slaveModeExecutor.setObjectHandler(mode,object);

	m_plugins[plugin].addRegisteredSlaveModeHandler(mode);
}

void ComputeCore::setMasterModeObjectHandler(PluginHandle plugin,MasterMode mode,MasterModeHandler object){
	if(!validationPluginAllocated(plugin))
		return;

	if(!validationMasterModeRange(plugin,mode))
		return;

	if(!validationMasterModeOwnership(plugin,mode))
		return;

	#ifdef CONFIG_DEBUG_CORE
	cout<<"setMasterModeObjectHandler "<<MASTER_MODES[mode]<<" to "<<object<<endl;
	#endif

	#ifdef ASSERT
	assert(m_plugins.count(plugin)>0);
	assert(m_plugins[plugin].hasMasterMode(mode));
	#endif

	m_masterModeExecutor.setObjectHandler(mode,object);

	m_plugins[plugin].addRegisteredMasterModeHandler(mode);
}

void ComputeCore::setMessageTagObjectHandler(PluginHandle plugin,MessageTag tag,MessageTagHandler object){
	if(!validationPluginAllocated(plugin))
		return;

	if(!validationMessageTagRange(plugin,tag))
		return;

	if(!validationMessageTagOwnership(plugin,tag))
		return;

	#ifdef CONFIG_DEBUG_CORE
	cout<<"setMessageTagObjectHandler "<<MESSAGE_TAGS[tag]<<" to "<<object<<endl;
	#endif

	#ifdef ASSERT
	assert(m_plugins.count(plugin)>0);
	assert(m_plugins[plugin].hasMessageTag(tag));
	#endif

	m_messageTagExecutor.setObjectHandler(tag,object);

	m_plugins[plugin].addRegisteredMessageTagHandler(tag);
}


/**
 * runWithProfiler if -run-profiler is provided
 * otherwise, run runVanilla()
 */
void ComputeCore::run(){

	// ask the router if it is enabled
	// the virtual router will disable itself if there were
	// problems during configuration
	m_routerIsEnabled=m_router.isEnabled();

	if(!m_resolvedSymbols)
		resolveSymbols();

	if(m_hasFatalErrors){
		cout<<"Exiting because of previously reported errors."<<endl;
		return;
	}

	m_startingTimeMicroseconds = getMicroseconds();

	if(m_routerIsEnabled)
		m_router.getGraph()->start(m_rank);

	if(m_runProfiler){
		runWithProfiler();
	}else{
		runVanilla();
	}

	if(m_routerIsEnabled)
		m_router.getGraph()->printStatus();

}

/**
 * the while loop is *the* main loop of Ray for each 
 * processor.
 * it is similar to the main loop of a video game, actually, but without a display.
 */
void ComputeCore::runVanilla(){

	#ifdef CONFIG_DEBUG_CORE
	cout<<"m_alive= "<<m_alive<<endl;
	#endif

	while(m_alive || (m_routerIsEnabled && !m_router.hasCompletedRelayEvents())){
		
		// 1. receive the message (0 or 1 message is received)
		// blazing fast, receives 0 or 1 message, never more, never less, other messages will wait for the next iteration !
		receiveMessages(); 

		// 2. process the received message, if any
		// consume the one message received, if any, also very fast because it is done with an array mapping tags to function pointers
		processMessages();

		// 3. process data according to current slave and master modes
		// should be fast, but apparently call_RAY_SLAVE_MODE_EXTENSION is slowish sometimes...
		processData();

		// 4. send messages
		// fast, sends at most 17 messages. In most case it is either 0 or 1 message.,..
		sendMessages();
	}

	#ifdef CONFIG_DEBUG_CORE
	cout<<"m_alive= "<<m_alive<<endl;
	#endif
}

/*
 * This is the main loop of the program.
 * One instance on each MPI rank.
 */
void ComputeCore::runWithProfiler(){
	// define some variables that hold life statistics of this
	// MPI rank
	int ticks=0;
	int sentMessages=0;
	int sentMessagesInProcessMessages=0;
	int sentMessagesInProcessData=0;
	int receivedMessages=0;
	map<int,int> receivedTags;
	map<int,int> sentTagsInProcessMessages;
	map<int,int> sentTagsInProcessData;
	
	int resolution=100;// milliseconds
	int parts=1000/resolution;

	uint64_t startingTime=getMilliSeconds();

	uint64_t lastTime=getMilliSeconds();

/*
	uint64_t lastTickWhenSentMessageInProcessMessage=lastTime;
	uint64_t lastTickWhenSentMessageInProcessData=lastTime;
*/

	vector<int> distancesForProcessMessages;
	vector<int> distancesForProcessData;

	bool profilerVerbose=m_profilerVerbose; 

	while(m_alive  || (m_routerIsEnabled && !m_router.hasCompletedRelayEvents())){
		uint64_t t=getMilliSeconds();
		if(t>=(lastTime+resolution)/parts*parts){

			double seconds=(t-startingTime)/1000.0;

			int balance=sentMessages-receivedMessages;

			if(profilerVerbose){
				printf("Rank %i: %s Time= %.2f s Speed= %i Sent= %i (processMessages: %i, processData: %i) Received= %i Balance= %i\n",
					m_rank,SLAVE_MODES[m_switchMan.getSlaveMode()],
					seconds,ticks,sentMessages,sentMessagesInProcessMessages,sentMessagesInProcessData,
					receivedMessages,balance);
				fflush(stdout);

				m_profiler.printGranularities(m_rank);
			}

			m_profiler.clearGranularities();

			if(receivedTags.size() > 0 && profilerVerbose){
				cout<<"Rank "<<m_messagesHandler.getRank()<<" received in receiveMessages:"<<endl;
				for(map<int,int>::iterator i=receivedTags.begin();i!=receivedTags.end();i++){
					int tag=i->first;
					int count=i->second;
					cout<<"Rank "<<m_messagesHandler.getRank()<<"        "<<MESSAGE_TAGS[tag]<<"	"<<count<<endl;
				}
			}

			if(sentTagsInProcessMessages.size() > 0 && profilerVerbose){
				cout<<"Rank "<<m_messagesHandler.getRank()<<" sent in processMessages:"<<endl;
				for(map<int,int>::iterator i=sentTagsInProcessMessages.begin();i!=sentTagsInProcessMessages.end();i++){
					int tag=i->first;
					int count=i->second;
					cout<<"Rank "<<m_messagesHandler.getRank()<<"        "<<MESSAGE_TAGS[tag]<<"	"<<count<<endl;
				}

/*
				int average1=getAverage(&distancesForProcessMessages);
				int deviation1=getStandardDeviation(&distancesForProcessMessages);
			
				cout<<"Rank "<<m_messagesHandler.getRank()<<" distance between processMessages messages: average= "<<average1<<", stddev= "<<deviation1<<
					", n= "<<distancesForProcessMessages.size()<<endl;
				
*/
				#ifdef FULL_DISTRIBUTION
				map<int,int> distribution1;
				for(int i=0;i<(int)distancesForProcessMessages.size();i++){
					distribution1[distancesForProcessMessages[i]]++;
				}
				cout<<"Rank "<<m_messagesHandler.getRank()<<" distribution: "<<endl;
				for(map<int,int>::iterator i=distribution1.begin();i!=distribution1.end();i++){
					cout<<i->first<<" "<<i->second<<endl;
				}
				#endif

			}

			distancesForProcessMessages.clear();

			if(sentTagsInProcessData.size() > 0 && profilerVerbose){
				cout<<"Rank "<<m_messagesHandler.getRank()<<" sent in processData:"<<endl;
				for(map<int,int>::iterator i=sentTagsInProcessData.begin();i!=sentTagsInProcessData.end();i++){
					int tag=i->first;
					int count=i->second;
					cout<<"Rank "<<m_messagesHandler.getRank()<<"        "<<MESSAGE_TAGS[tag]<<"	"<<count<<endl;
				}
/*
				int average2=getAverage(&distancesForProcessData);
				int deviation2=getStandardDeviation(&distancesForProcessData);
	
				cout<<"Rank "<<m_messagesHandler.getRank()<<" distance between processData messages: average= "<<average2<<", stddev= "<<deviation2<<
					", n= "<<distancesForProcessData.size()<<endl;
				
*/
				#ifdef FULL_DISTRIBUTION
				map<int,int> distribution2;
				for(int i=0;i<(int)distancesForProcessData.size();i++){
					distribution2[distancesForProcessData[i]]++;
				}
				cout<<"Rank "<<m_messagesHandler.getRank()<<" distribution: "<<endl;
				for(map<int,int>::iterator i=distribution2.begin();i!=distribution2.end();i++){
					cout<<i->first<<" "<<i->second<<endl;
				}
				#endif

			}

			distancesForProcessData.clear();

			sentMessages=0;
			sentMessagesInProcessMessages=0;
			sentMessagesInProcessData=0;
			receivedMessages=0;
			receivedTags.clear();
			sentTagsInProcessMessages.clear();
			sentTagsInProcessData.clear();
			ticks=0;

			lastTime=t;
		}

		/* collect some statistics for the profiler */

		// 1. receive the message (0 or 1 message is received)
		receiveMessages(); 
		receivedMessages+=m_inbox.size();
		
		for(int i=0;i<(int)m_inbox.size();i++){
			// stript routing information, if any
			uint8_t tag=m_inbox[i]->getTag();
			receivedTags[tag]++;
		}

		// 2. process the received message, if any
		processMessages();

		int messagesSentInProcessMessages=m_outbox.size();
		sentMessagesInProcessMessages += messagesSentInProcessMessages;
		sentMessages += messagesSentInProcessMessages;

/*
		if(messagesSentInProcessMessages > 0){
			int distance=t- lastTickWhenSentMessageInProcessMessage;
			lastTickWhenSentMessageInProcessMessage=t;
			distancesForProcessMessages.push_back(distance);
		}
*/

		// 3. process data according to current slave and master modes

		int currentSlaveMode=m_switchMan.getSlaveMode();

		uint64_t startingTime = getThreadMicroseconds();
		processData();
		uint64_t endingTime = getThreadMicroseconds();

		int difference = endingTime - startingTime;
		
		m_profiler.addGranularity(currentSlaveMode,difference);

		/* threshold to say something is taking too long */
		/* in microseconds */
		int tooLong=m_profiler.getThreshold();

		if(difference >= tooLong){
			cout<<"Warning, SlaveMode= "<<SLAVE_MODES[currentSlaveMode]<<" GranularityInMicroseconds= "<<difference<<""<<endl;
			m_profiler.printStack();
		}

		m_profiler.resetStack();

		int messagesSentInProcessData = m_outbox.size() - messagesSentInProcessMessages;
		sentMessagesInProcessData += messagesSentInProcessData;
		sentMessages += messagesSentInProcessData;

		for(int i=0;i<messagesSentInProcessMessages;i++){
			// stript routing information, if any
			uint8_t tag=m_outbox[i]->getTag();
			sentTagsInProcessMessages[tag]++;
		}

		for(int i=messagesSentInProcessMessages;i<(int)m_outbox.size();i++){
			// stript routing information, if any
			uint8_t tag=m_outbox[i]->getTag();
			sentTagsInProcessData[tag]++;
		}

		// 4. send messages
		sendMessages();

		/* increment ticks */
		ticks++;
	}

	m_profiler.printAllGranularities();
}

void ComputeCore::processMessages(){
	#ifdef ASSERT
	assert(m_inbox.size()>=0&&m_inbox.size()<=1);
	#endif

	if(m_inbox.size()==0)
		return;


	// if routing is enabled, we want to strip the routing tags if it
	// is required
	if(m_routerIsEnabled){
		if(m_router.routeIncomingMessages()){
			// if the message has routing tag, we don't need to process it...

/*
 * If a message is routed, the original message needs to be destroyed 
 * with fire. Otherwise, a slave mode or a master mode will pick it up, 
 * which will cause silly behaviors.
 */
			m_inbox.clear();

			return;
		}
	}

	Message*message=m_inbox[0];
	MessageTag messageTag=message->getTag();

	#ifdef ASSERT
	assert(messageTag!=INVALID_HANDLE);
	assert(m_allocatedMessageTags.count(messageTag)>0);
	string symbol=MESSAGE_TAGS[messageTag];
	assert(m_messageTagSymbols.count(symbol));
	assert(m_messageTagSymbols[symbol]==messageTag);
	#endif

	// check if the tag is in the list of slave switches
	m_switchMan.openSlaveModeLocally(messageTag,m_rank);

	m_messageTagExecutor.callHandler(messageTag,message);
}

void ComputeCore::sendMessages(){
	// assert that we did not overflow the ring
	#ifdef ASSERT
	if(m_outboxAllocator.getCount() > m_maximumAllocatedOutboxBuffers){
		cout<<"Rank "<<m_rank<<" Error, allocated "<<m_outboxAllocator.getCount()<<" buffers, but maximum is ";
		cout<<m_maximumAllocatedOutboxBuffers<<endl;
		cout<<" outboxSize= "<<m_outbox.size()<<endl;
		cout<<"This means that too many messages were created in this time slice."<<endl;
	}

	assert(m_outboxAllocator.getCount()<=m_maximumAllocatedOutboxBuffers);
	m_outboxAllocator.resetCount();
	int messagesToSend=m_outbox.size();
	if(messagesToSend>m_maximumNumberOfOutboxMessages){
		cout<<"Fatal: "<<messagesToSend<<" messages to send, but max is "<<m_maximumNumberOfOutboxMessages<<endl;
		cout<<"tags=";
		for(int i=0;i<(int)m_outbox.size();i++){
			MessageTag tag=m_outbox[i]->getTag();
			cout<<" "<<MESSAGE_TAGS[tag]<<" handle= "<<tag<<endl;
		}
		cout<<endl;
	}

	assert(messagesToSend<=m_maximumNumberOfOutboxMessages);
	if(messagesToSend>m_maximumNumberOfOutboxMessages){
		uint8_t tag=m_outbox[0]->getTag();
		cout<<"Tag="<<tag<<" n="<<messagesToSend<<" max="<<m_maximumNumberOfOutboxMessages<<endl;
	}

	#endif

	// route messages if the router is enabled
	if(m_routerIsEnabled){
		// if message routing is enabled,
		// generate routing tags.
		m_router.routeOutcomingMessages();
	}

	// parameters.showCommunicationEvents() 
	if( m_showCommunicationEvents && m_outbox.size() > 0){
		uint64_t microseconds=getMicroseconds() - m_startingTimeMicroseconds;
		for(int i=0;i<(int)m_outbox.size();i++){
			cout<<"[Communication] "<<microseconds<<" microseconds, SEND ";
			m_outbox[i]->print();
			cout<<endl;
		}
	}

	for(int i=0;i<(int)m_outbox.size();i++){
		m_tickLogger.logSendMessage(INVALID_HANDLE);
	}

	// add checksums if necessary
	if(m_doChecksum){
		addMessageChecksums();
	}

	// finally, send the messages
	m_messagesHandler.sendMessages(&m_outbox,&m_outboxAllocator);
}

void ComputeCore::addMessageChecksums(){

	int count=m_outbox.size();
	for(int i=0;i<count;i++){
		MessageUnit*data=m_outbox.at(i)->getBuffer();
		int numberOfUnits=m_outbox.at(i)->getCount();

		// don't compute checksum for empty messages
		if(numberOfUnits==0){
			continue;
		}

		int numberOfBytes=sizeof(MessageUnit)*numberOfUnits;

		uint8_t*bytes=(uint8_t*)data;

		uint32_t crc32=computeCyclicRedundancyCode32(bytes,numberOfBytes);

		data[numberOfUnits]=crc32;

		m_outbox.at(i)->setCount(numberOfUnits+1);
	}

}

void ComputeCore::verifyMessageChecksums(){

	int count=m_inbox.size();
	for(int i=0;i<count;i++){
		MessageUnit*data=m_inbox.at(i)->getBuffer();

		// this contains the checksum unit too.
		int numberOfUnits=m_inbox.at(i)->getCount();

		// don't compute checksum for empty messages
		if(numberOfUnits==0){
			continue;
		}

		// the last MessageUnit contains the crc32.
		// note that the CRC32 feature only works for MessageUnit at least
		// 32 bits.
		int numberOfBytes=sizeof(MessageUnit)*(numberOfUnits-1);

		uint8_t*bytes=(uint8_t*)data;

		// compute the checksum, excluding the reference checksum
		// from the data
		uint32_t crc32=computeCyclicRedundancyCode32(bytes,numberOfBytes);

		uint32_t expectedChecksum=data[numberOfUnits-1];

		#ifdef CONFIG_SHOW_CHECKSUM
		cout<<" Expected checksum (CRC32): "<<hex<<expectedChecksum<<endl;
		cout<<" Actual checksum (CRC32): "<<hex<<crc32<<dec<<endl;
		#endif

		if(crc32 != expectedChecksum){
			Message*message=m_inbox.at(i);
			cout<<"Error: RayPlatform detected a message corruption !"<<endl;
			cout<<" Tag: "<<MESSAGE_TAGS[message->getTag()]<<endl;
			cout<<" Source: "<<message->getSource()<<endl;
			cout<<" Destination: "<<message->getDestination()<<endl;
			cout<<" sizeof(MessageUnit): "<<sizeof(MessageUnit)<<endl;
			cout<<" Count (excluding checksum): "<<numberOfUnits-1<<endl;
			cout<<" Expected checksum (CRC32): "<<hex<<expectedChecksum<<endl;
			cout<<" Actual checksum (CRC32): "<<hex<<crc32<<dec<<endl;
		}

		#ifdef ASSERT
		assert(numberOfUnits>=2);
		#endif

		// remove the checksum
		m_inbox.at(i)->setCount(numberOfUnits-1);

		#ifdef ASSERT
		assert(numberOfUnits>=1);
		#endif
	}

}

/**
 * receivedMessages receives 0 or 1 messages.
 * If more messages are available to be pumped, they will wait until the
 * next ComputeCore cycle.
 */
void ComputeCore::receiveMessages(){
	m_inbox.clear();
	m_messagesHandler.receiveMessages(&m_inbox,&m_inboxAllocator);

	// verify checksums and remove them
	if(m_doChecksum){
		verifyMessageChecksums();
	}

	for(int i=0;i<(int)m_inbox.size();i++){
		m_tickLogger.logReceivedMessage(INVALID_HANDLE);
	}

	#ifdef ASSERT
	int receivedMessages=m_inbox.size();
	assert(receivedMessages<=m_maximumNumberOfInboxMessages);
	#endif

	if(m_inbox.size() > 0 && m_showCommunicationEvents){
		uint64_t theTime=getMicroseconds();
		uint64_t microseconds=theTime - m_startingTimeMicroseconds;
		for(int i=0;i<(int)m_inbox.size();i++){
			cout<<"[Communication] "<<microseconds<<" microseconds, RECEIVE ";
			m_inbox[i]->print();
			cout<<endl;
		}
	}
}

/** process data my calling current slave and master methods */
void ComputeCore::processData(){

	// call the master method first
	MasterMode master=m_switchMan.getMasterMode();

	#ifdef RayPlatform_ASSERT
	assert(master!=INVALID_HANDLE);
	assert(m_allocatedMasterModes.count(master)>0);
	string masterSymbol=MASTER_MODES[master];
	assert(m_masterModeSymbols.count(masterSymbol)>0);
	assert(m_masterModeSymbols[masterSymbol]==master);
	#endif

	#ifdef CONFIG_DEBUG_processData
	cout<<"master mode -> "<<MASTER_MODES[master]<<" handle is "<<master<<endl;
	#endif

	m_masterModeExecutor.callHandler(master);
	m_tickLogger.logMasterTick(master);

	// then call the slave method
	SlaveMode slave=m_switchMan.getSlaveMode();

	#ifdef RayPlatform_ASSERT
	assert(slave!=INVALID_HANDLE);
	assert(m_allocatedSlaveModes.count(slave)>0);
	string slaveSymbol=SLAVE_MODES[slave];
	assert(m_slaveModeSymbols.count(slaveSymbol)>0);
	assert(m_slaveModeSymbols[slaveSymbol]==slave);
	#endif

	#ifdef CONFIG_DEBUG_processData
	cout<<"slave mode -> "<<SLAVE_MODES[slave]<<" handle is "<<slave<<endl;
	#endif

	m_slaveModeExecutor.callHandler(slave);
	m_tickLogger.logSlaveTick(slave);
}

void ComputeCore::constructor(int*argc,char***argv){

	m_doChecksum=false;

	// checksum calculation is only tested
	// for cases with sizeof(MessageUnit)>=4 bytes

	const char verifyMessages[]="-verify-message-integrity";
	const char router[]="-route-messages";

	m_routerIsEnabled=false;

	int match=0;

	for(int i=0;i<(*argc);i++){
		if(strcmp( ((*argv)[i]), verifyMessages) == match){
			m_doChecksum=true;
		}else if(strcmp( ((*argv)[i]),router) == match){
			m_routerIsEnabled=true;
		}
	}

	m_argumentCount=*argc;
	m_argumentValues=*argv;


	m_resolvedSymbols=false;

	m_hasFatalErrors=false;

	m_firstRegistration=true;

	m_hasFirstMode=false;

	srand(portableProcessId() * getMicroseconds());

	m_alive=true;

	m_messagesHandler.constructor(argc,argv);

	m_runProfiler=false;
	m_showCommunicationEvents=false;
	m_profilerVerbose=false;

	m_rank=m_messagesHandler.getRank();
	m_size=m_messagesHandler.getSize();

	if(m_doChecksum){
		cout<<"[RayPlatform] Rank "<<m_rank<<" will compute a CRC32 checksum for any non-empty message."<<" ("<<verifyMessages<<")"<<endl;
	}

	// set the number of buffers to use
	int minimumNumberOfBuffers=128;

	int availableBuffers=minimumNumberOfBuffers;

	// even a message with a NULL buffer requires a buffer for routing
	if(m_routerIsEnabled)
		availableBuffers=m_size*2;

	// this will occur when using the virtual router with a few processes
	if(availableBuffers<minimumNumberOfBuffers)
		availableBuffers=minimumNumberOfBuffers;


	m_switchMan.constructor(m_rank,m_size);

	m_virtualCommunicator.constructor(m_rank,m_size,&m_outboxAllocator,&m_inbox,&m_outbox);

	/***********************************************************************************/
	/** initialize the VirtualProcessor */
	m_virtualProcessor.constructor(&m_outbox,&m_inbox,&m_outboxAllocator,
		&m_virtualCommunicator);

	m_maximumAllocatedInboxBuffers=1;
	m_maximumAllocatedOutboxBuffers=availableBuffers;
	
	m_maximumNumberOfInboxMessages=1;
	m_maximumNumberOfOutboxMessages=availableBuffers;

	// broadcasting messages
	// this case will occur when the number of requested outbox buffers is < the number of processor cores.
	if(m_maximumNumberOfOutboxMessages < m_size){
		m_maximumNumberOfOutboxMessages=m_size;
	}

	getInbox()->constructor(m_maximumNumberOfInboxMessages,"RAY_MALLOC_TYPE_INBOX_VECTOR",false);
	
	#if 0
	cout<<"[ComputeCore] the inbox capacity is "<<m_maximumNumberOfInboxMessages<<" message"<<endl;
	#endif

	getOutbox()->constructor(m_maximumNumberOfOutboxMessages,"RAY_MALLOC_TYPE_OUTBOX_VECTOR",false);

	#if 0
	cout<<"[ComputeCore] the outbox capacity is "<<m_maximumNumberOfOutboxMessages<<" message"<<endl;
	#endif

	int maximumMessageSizeInByte=MAXIMUM_MESSAGE_SIZE_IN_BYTES;

	// add a message unit to store the checksum or the routing information
	// with 64-bit integers as MessageUnit, this is 4008 bytes or 501 MessageUnit maximum
	// TODO: RayPlatform can not do both routing and checksums 
	if(m_doChecksum || m_routerIsEnabled){
		if(sizeof(MessageUnit)>=4){
			maximumMessageSizeInByte+=sizeof(MessageUnit);
		}else if(sizeof(MessageUnit)>=2){
			maximumMessageSizeInByte+=2*sizeof(MessageUnit);
		}else{
			maximumMessageSizeInByte+=4*sizeof(MessageUnit);
		}
	}

	m_inboxAllocator.constructor(m_maximumAllocatedInboxBuffers,
		maximumMessageSizeInByte,
		"RAY_MALLOC_TYPE_INBOX_ALLOCATOR",false);

	#if 0
	cout<<"[ComputeCore] allocated "<<m_maximumAllocatedInboxBuffers<<" buffers of size "<<maximumMessageSizeInByte<<" for inbox messages"<<endl;
	#endif

	m_outboxAllocator.constructor(m_maximumAllocatedOutboxBuffers,
		maximumMessageSizeInByte,
		"RAY_MALLOC_TYPE_OUTBOX_ALLOCATOR",false);

	#if 0
	cout<<"[ComputeCore] allocated "<<m_maximumAllocatedOutboxBuffers<<" buffers of size "<<maximumMessageSizeInByte<<" for outbox messages"<<endl;
	#endif

	for(int i=0;i<MAXIMUM_NUMBER_OF_MASTER_HANDLERS;i++){
		strcpy(MASTER_MODES[i],"UnnamedMasterMode");
	}
	for(int i=0;i<MAXIMUM_NUMBER_OF_SLAVE_HANDLERS;i++){
		strcpy(SLAVE_MODES[i],"UnnamedSlaveMode");
	}
	for(int i=0;i<MAXIMUM_NUMBER_OF_TAG_HANDLERS;i++){
		strcpy(MESSAGE_TAGS[i],"UnnamedMessageTag");
	}

	m_currentSlaveModeToAllocate=0;
	m_currentMasterModeToAllocate=0;
	m_currentMessageTagToAllocate=0;

}

void ComputeCore::enableProfiler(){
	m_runProfiler=true;
}

void ComputeCore::showCommunicationEvents(){
	m_showCommunicationEvents=true;
}

void ComputeCore::enableProfilerVerbosity(){
	m_profilerVerbose=true;
}

MessagesHandler*ComputeCore::getMessagesHandler(){
	return &m_messagesHandler;
}

Profiler*ComputeCore::getProfiler(){
	return &m_profiler;
}

TickLogger*ComputeCore::getTickLogger(){
	return &m_tickLogger;
}

SwitchMan*ComputeCore::getSwitchMan(){
	return &m_switchMan;
}

StaticVector*ComputeCore::getOutbox(){
	return &m_outbox;
}

StaticVector*ComputeCore::getInbox(){
	return &m_inbox;
}

MessageRouter*ComputeCore::getRouter(){
	return &m_router;
}

RingAllocator*ComputeCore::getOutboxAllocator(){
	return &m_outboxAllocator;
}

RingAllocator*ComputeCore::getInboxAllocator(){
	return &m_inboxAllocator;
}

bool*ComputeCore::getLife(){
	return &m_alive;
}

VirtualProcessor*ComputeCore::getVirtualProcessor(){
	return &m_virtualProcessor;
}

VirtualCommunicator*ComputeCore::getVirtualCommunicator(){
	return &m_virtualCommunicator;
}

void ComputeCore::registerPlugin(CorePlugin*plugin){
	
	if(m_firstRegistration){

		m_firstRegistration=false;

		// register some built-in plugins

		registerPlugin(&m_switchMan);

	}

	m_listOfPlugins.push_back(plugin);

	plugin->registerPlugin(this);
}

void ComputeCore::resolveSymbols(){
	if(m_resolvedSymbols)
		return;

	registerPlugin(&m_messagesHandler); // must be the last registered

	for(int i=0;i<(int)m_listOfPlugins.size();i++){
		CorePlugin*plugin=m_listOfPlugins[i];
		plugin->resolveSymbols(this);
	}

	m_resolvedSymbols=true;
}

void ComputeCore::destructor(){
	getMessagesHandler()->destructor();
}

void ComputeCore::stop(){
	m_alive=false;
}

void ComputeCore::setSlaveModeSymbol(PluginHandle plugin,SlaveMode mode,const char*symbol){
	if(!validationPluginAllocated(plugin))
		return;
	
	if(!validationSlaveModeRange(plugin,mode))
		return;

	if(!validationSlaveModeOwnership(plugin,mode)){
		cout<<"was trying to set symbol "<<symbol<<" to mode "<<mode<<endl;
		return;
	}

	if(!validationSlaveModeSymbolAvailable(plugin,symbol)){
		return;
	}

	if(!validationSlaveModeSymbolNotRegistered(plugin,mode))
		return;

	if(!validationSlaveModeRange(plugin,mode))
		return;

	#ifdef ASSERT
	assert(mode>=0);
	assert(mode<MAXIMUM_NUMBER_OF_SLAVE_HANDLERS);
	#endif

	#ifdef ASSERT
	assert(m_plugins.count(plugin)>0);
	assert(m_plugins[plugin].hasSlaveMode(mode));
	#endif

	strcpy(SLAVE_MODES[mode],symbol);

	m_plugins[plugin].addRegisteredSlaveModeSymbol(mode);

	m_registeredSlaveModeSymbols.insert(mode);

	m_slaveModeSymbols[symbol]=mode;

	#ifdef CONFIG_DEBUG_SLAVE_SYMBOLS
	cout<<"Registered slave mode "<<mode<<" to symbol "<<symbol<<endl;
	#endif
}

void ComputeCore::setMasterModeSymbol(PluginHandle plugin,MasterMode mode,const char*symbol){
	
	if(!validationPluginAllocated(plugin))
		return;

	if(!validationMasterModeRange(plugin,mode))
		return;

	if(!validationMasterModeOwnership(plugin,mode))
		return;

	if(!validationMasterModeSymbolAvailable(plugin,symbol)){
		return;
	}

	if(!validationMasterModeSymbolNotRegistered(plugin,mode))
		return;

	if(!validationMasterModeRange(plugin,mode))
		return;

	#ifdef ASSERT
	assert(mode>=0);
	assert(mode<MAXIMUM_NUMBER_OF_MASTER_HANDLERS);
	#endif

	#ifdef ASSERT
	assert(m_plugins.count(plugin)>0);
	assert(m_plugins[plugin].hasMasterMode(mode));
	#endif

	strcpy(MASTER_MODES[mode],symbol);

	m_plugins[plugin].addRegisteredMasterModeSymbol(mode);

	m_registeredMasterModeSymbols.insert(mode);

	m_masterModeSymbols[symbol]=mode;
}

void ComputeCore::setMessageTagSymbol(PluginHandle plugin,MessageTag tag,const char*symbol){

	if(!validationPluginAllocated(plugin))
		return;

	if(!validationMessageTagRange(plugin,tag))
		return;

	if(!validationMessageTagOwnership(plugin,tag))
		return;

	if(!validationMessageTagSymbolAvailable(plugin,symbol))
		return;

	if(!validationMessageTagSymbolNotRegistered(plugin,tag))
		return;

	if(!validationMessageTagRange(plugin,tag))
		return;

	#ifdef ASSERT
	assert(tag>=0);
	assert(tag<MAXIMUM_NUMBER_OF_TAG_HANDLERS);
	#endif

	#ifdef ASSERT
	assert(m_plugins.count(plugin)>0);
	assert(m_plugins[plugin].hasMessageTag(tag));
	#endif

	strcpy(MESSAGE_TAGS[tag],symbol);

	m_plugins[plugin].addRegisteredMessageTagSymbol(tag);

	m_registeredMessageTagSymbols.insert(tag);

	m_messageTagSymbols[symbol]=tag;
}

PluginHandle ComputeCore::allocatePluginHandle(){
	PluginHandle handle=generatePluginHandle();
	
	while(m_plugins.count(handle)>0){
		handle=generatePluginHandle();
	}

	RegisteredPlugin plugin;

	m_plugins[handle]=plugin;

	return handle;
}

SlaveMode ComputeCore::allocateSlaveModeHandle(PluginHandle plugin){

	if(!validationPluginAllocated(plugin))
		return INVALID_HANDLE;


	#ifdef ASSERT
	assert(m_plugins.count(plugin)>0);
	#endif

	SlaveMode handle=m_currentSlaveModeToAllocate;

	if(!(0<=handle && handle<MAXIMUM_NUMBER_OF_SLAVE_HANDLERS))
		handle=m_currentSlaveModeToAllocate;

	while(m_allocatedSlaveModes.count(handle)>0){
		handle=m_currentSlaveModeToAllocate++;
	}

	if(!validationSlaveModeRange(plugin,handle))
		return INVALID_HANDLE;

	#ifdef ASSERT
	assert(m_allocatedSlaveModes.count(handle)==0);
	#endif

	m_allocatedSlaveModes.insert(handle);

	m_plugins[plugin].addAllocatedSlaveMode(handle);

	#ifdef ASSERT
	assert(m_allocatedSlaveModes.count(handle)>0);
	assert(m_plugins[plugin].hasSlaveMode(handle));
	#endif
	
	return handle;
}

MasterMode ComputeCore::allocateMasterModeHandle(PluginHandle plugin){

	if(!validationPluginAllocated(plugin))
		return INVALID_HANDLE;

	#ifdef ASSERT
	assert(m_plugins.count(plugin)>0);
	#endif

	MasterMode handle=m_currentMasterModeToAllocate;

	if(!(0<=handle && handle<MAXIMUM_NUMBER_OF_MASTER_HANDLERS))
		handle=m_currentMasterModeToAllocate;

	while(m_allocatedMasterModes.count(handle)>0){
		handle=m_currentMasterModeToAllocate++;
	}

	if(!validationMasterModeRange(plugin,handle))
		return INVALID_HANDLE;

	#ifdef ASSERT
	assert(m_allocatedMasterModes.count(handle)==0);
	#endif

	m_allocatedMasterModes.insert(handle);

	m_plugins[plugin].addAllocatedMasterMode(handle);

	#ifdef ASSERT
	assert(m_allocatedMasterModes.count(handle)>0);
	assert(m_plugins[plugin].hasMasterMode(handle));
	#endif

	return handle; 
}

MessageTag ComputeCore::allocateMessageTagHandle(PluginHandle plugin){

	if(!validationPluginAllocated(plugin))
		return INVALID_HANDLE;

	#ifdef ASSERT
	assert(m_plugins.count(plugin)>0);
	#endif

	MessageTag handle=m_currentMessageTagToAllocate;

	if(!(0<=handle && handle < MAXIMUM_NUMBER_OF_TAG_HANDLERS))
		handle=m_currentMessageTagToAllocate;

	while(m_allocatedMessageTags.count(handle)>0){
		handle=m_currentMessageTagToAllocate++;
	}

	if(!validationMessageTagRange(plugin,handle))
		return INVALID_HANDLE;

	#ifdef ASSERT
	assert(m_allocatedMessageTags.count(handle)==0);
	#endif

	m_allocatedMessageTags.insert(handle);

	m_plugins[plugin].addAllocatedMessageTag(handle);

	#ifdef ASSERT
	assert(m_allocatedMessageTags.count(handle)>0);
	assert(m_plugins[plugin].hasMessageTag(handle));
	#endif

	return handle;
}

PluginHandle ComputeCore::generatePluginHandle(){
	uint64_t randomNumber=rand();

	return uniform_hashing_function_1_64_64(randomNumber);
}

bool ComputeCore::validationPluginAllocated(PluginHandle plugin){
	if(!m_plugins.count(plugin)>0){
		cout<<"Error, plugin "<<plugin<<" is not allocated"<<endl;
		setFatalError();
		return false;
	}

	return true;
}

bool ComputeCore::validationSlaveModeOwnership(PluginHandle plugin,SlaveMode handle){
	if(!m_plugins[plugin].hasSlaveMode(handle)){
		cout<<"Error, plugin "<<m_plugins[plugin].getPluginName();
		setFatalError();
		cout<<" ("<<plugin<<") has no ownership on slave mode "<<handle<<endl;
		return false;
	}

	return true;
}

bool ComputeCore::validationMasterModeOwnership(PluginHandle plugin,MasterMode handle){
	if(!m_plugins[plugin].hasMasterMode(handle)){
		cout<<"Error, plugin "<<m_plugins[plugin].getPluginName();
		cout<<" ("<<plugin<<") has no ownership on master mode "<<handle<<endl;
		cout<<" public symbol is "<<MASTER_MODES[handle]<<endl;
		setFatalError();

		return false;
	}

	return true;
}

bool ComputeCore::validationMessageTagOwnership(PluginHandle plugin,MessageTag handle){
	if(!m_plugins[plugin].hasMessageTag(handle)){
		cout<<"Error, plugin "<<m_plugins[plugin].getPluginName();
		cout<<" ("<<plugin<<") has no ownership on message tag mode "<<handle<<endl;
		setFatalError();
		return false;
	}

	return true;
}

void ComputeCore::setPluginName(PluginHandle plugin,const char*name){
	if(!validationPluginAllocated(plugin))
		return;

	m_plugins[plugin].setPluginName(name);
}

void ComputeCore::printPlugins(string directory){

/*
	ostringstream list;
	list<<directory<<"/list.txt";

	ofstream f1(list.str().c_str());

	for(map<PluginHandle,RegisteredPlugin>::iterator i=m_plugins.begin();
		i!=m_plugins.end();i++){
		
		f1<<i->first<<"	plugin_"<<i->second.getPluginName()<<endl;
	}

	f1.close();
*/

	for(map<PluginHandle,RegisteredPlugin>::iterator i=m_plugins.begin();
		i!=m_plugins.end();i++){

		ostringstream file;
		file<<directory<<"/plugin_"<<i->second.getPluginName()<<".txt";

		ofstream f2(file.str().c_str());
		i->second.print(&f2);
		f2.close();
	}

}

SlaveMode ComputeCore::getSlaveModeFromSymbol(PluginHandle plugin,const char*symbol){
	if(!validationPluginAllocated(plugin))
		return INVALID_HANDLE;

	if(!validationSlaveModeSymbolRegistered(plugin,symbol))
		return INVALID_HANDLE;

	string key=symbol;

	if(m_slaveModeSymbols.count(key)>0){
		SlaveMode handle=m_slaveModeSymbols[key];
		
		m_plugins[plugin].addResolvedSlaveMode(handle);

		#ifdef CONFIG_DEBUG_SLAVE_SYMBOLS
		cout<<"Plugin "<<m_plugins[plugin].getPluginName()<<" resolved symbol "<<symbol<<" to slave mode "<<handle<<endl;
		#endif

		return handle;
	}

	cout<<"Invalid handle returned!"<<endl;
	return INVALID_HANDLE;
}

MasterMode ComputeCore::getMasterModeFromSymbol(PluginHandle plugin,const char*symbol){

	if(!validationPluginAllocated(plugin))
		return INVALID_HANDLE;

	if(!validationMasterModeSymbolRegistered(plugin,symbol))
		return INVALID_HANDLE;

	string key=symbol;

	if(m_masterModeSymbols.count(key)>0){
		MasterMode handle=m_masterModeSymbols[key];

		m_plugins[plugin].addResolvedMasterMode(handle);

		#ifdef CONFIG_DEBUG_MASTER_SYMBOLS
		cout<<"symbol "<<symbol<<" is resolved as master mode "<<handle<<endl;
		#endif

		return handle;
	}

	return INVALID_HANDLE;
}

MessageTag ComputeCore::getMessageTagFromSymbol(PluginHandle plugin,const char*symbol){

	if(!validationPluginAllocated(plugin))
		return INVALID_HANDLE;

	if(!validationMessageTagSymbolRegistered(plugin,symbol))
		return INVALID_HANDLE;

	string key=symbol;

	if(m_messageTagSymbols.count(key)>0){
		MessageTag handle=m_messageTagSymbols[key];

		m_plugins[plugin].addResolvedMessageTag(handle);

		#ifdef CONFIG_DEBUG_TAG_SYMBOLS
		cout<<" symbol "<<symbol<<" is resolved as message tag handle "<<handle<<endl;
		#endif

		return handle;
	}

	return INVALID_HANDLE;
}

bool ComputeCore::validationMessageTagSymbolAvailable(PluginHandle plugin,const char*symbol){
	string key=symbol;

	if(m_messageTagSymbols.count(key)>0){
		cout<<"Error, plugin "<<plugin<<" can not register symbol "<<symbol<<" because it is already registered."<<endl;
		setFatalError();
		return false;
	}

	return true;
}

bool ComputeCore::validationSlaveModeSymbolAvailable(PluginHandle plugin,const char*symbol){

	string key=symbol;

	if(m_slaveModeSymbols.count(key)>0){
		cout<<"Error, plugin "<<plugin<<" can not register symbol "<<symbol<<" because it is already registered."<<endl;
		setFatalError();
		return false;
	}

	return true;

}

bool ComputeCore::validationMasterModeSymbolAvailable(PluginHandle plugin,const char*symbol){

	string key=symbol;

	if(m_masterModeSymbols.count(key)>0){
		cout<<"Error, plugin "<<plugin<<" can not register symbol "<<symbol<<" because it is already registered."<<endl;
		setFatalError();
		return false;
	}

	return true;
}

bool ComputeCore::validationMessageTagSymbolRegistered(PluginHandle plugin,const char*symbol){
	string key=symbol;

	if(m_messageTagSymbols.count(key)==0){
		cout<<"Error, plugin "<<plugin<<" (name: "<<m_plugins[plugin].getPluginName()<<") can not fetch symbol "<<symbol<<" because it is not registered."<<endl;
		setFatalError();
		return false;
	}

	return true;
}

bool ComputeCore::validationSlaveModeSymbolRegistered(PluginHandle plugin,const char*symbol){

	string key=symbol;

	if(m_slaveModeSymbols.count(key)==0){
		cout<<"Error, plugin "<<plugin<<" (name: "<<m_plugins[plugin].getPluginName()<<") can not fetch symbol "<<symbol<<" because it is not registered."<<endl;
		setFatalError();
		return false;
	}

	return true;

}

bool ComputeCore::validationMasterModeSymbolRegistered(PluginHandle plugin,const char*symbol){

	string key=symbol;

	if(m_masterModeSymbols.count(key)==0){
		cout<<"Error, plugin "<<plugin<<" (name: "<<m_plugins[plugin].getPluginName()<<") can not fetch symbol "<<symbol<<" because it is not registered."<<endl;
		setFatalError();
		return false;
	}

	return true;
}

bool ComputeCore::validationMessageTagSymbolNotRegistered(PluginHandle plugin,MessageTag handle){

	if(m_registeredMessageTagSymbols.count(handle)>0){
		cout<<"Error, plugin "<<plugin<<" can not register symbol for message tag "<<handle <<" because it already has a symbol."<<endl;
		setFatalError();
		return false;
	}

	return true;
}

bool ComputeCore::validationSlaveModeSymbolNotRegistered(PluginHandle plugin,SlaveMode handle){

	if(m_registeredSlaveModeSymbols.count(handle)>0){
		cout<<"Error, plugin "<<plugin<<" can not register symbol for slave mode "<<handle <<" because it already has a symbol."<<endl;
		setFatalError();
		return false;
	}

	return true;

}

bool ComputeCore::validationMasterModeSymbolNotRegistered(PluginHandle plugin,MasterMode handle){

	if(m_registeredMasterModeSymbols.count(handle)>0){
		cout<<"Error, plugin "<<plugin<<" can not register symbol for master mode "<<handle <<" because it already has a symbol."<<endl;
		setFatalError();
		return false;
	}

	return true;
}

void ComputeCore::setPluginDescription(PluginHandle plugin,const char*a){

	if(!validationPluginAllocated(plugin))
		return;

	m_plugins[plugin].setPluginDescription(a);
}

void ComputeCore::setMasterModeToMessageTagSwitch(PluginHandle plugin,MasterMode mode,MessageTag tag){
	if(!validationPluginAllocated(plugin))
		return;

	if(!validationMasterModeRange(plugin,mode))
		return;

	if(!validationMasterModeOwnership(plugin,mode))
		return;

	m_switchMan.addMasterSwitch(mode,tag);

	m_plugins[plugin].addRegisteredMasterModeToMessageTagSwitch(mode);
}

void ComputeCore::setPluginAuthors(PluginHandle plugin,const char*text){
	if(!validationPluginAllocated(plugin))
		return;

	m_plugins[plugin].setPluginAuthors(text);
}

void ComputeCore::setPluginLicense(PluginHandle plugin,const char*text){
	if(!validationPluginAllocated(plugin))
		return;

	m_plugins[plugin].setPluginLicense(text);

}

void ComputeCore::setMessageTagToSlaveModeSwitch(PluginHandle plugin,MessageTag tag,SlaveMode mode){
	if(!validationPluginAllocated(plugin))
		return;

	if(!validationMessageTagRange(plugin,tag))
		return;

	if(!validationMessageTagOwnership(plugin,tag))
		return;

	m_switchMan.addSlaveSwitch(tag,mode);

	m_plugins[plugin].addRegisteredMessageTagToSlaveModeSwitch(tag);

}

void ComputeCore::setMessageTagReplyMessageTag(PluginHandle plugin,MessageTag tag,MessageTag reply){
	if(!validationPluginAllocated(plugin))
		return;

	if(!validationMessageTagRange(plugin,tag))
		return;

	if(!validationMessageTagRange(plugin,reply))
		return;

	if(!validationMessageTagOwnership(plugin,tag))
		return;

	m_virtualCommunicator.setReplyType(tag,reply);

	m_plugins[plugin].addRegisteredMessageTagReplyMessageTag(tag);
}

void ComputeCore::setMessageTagSize(PluginHandle plugin,MessageTag tag,int size){
	if(!validationPluginAllocated(plugin))
		return;

	if(!validationMessageTagRange(plugin,tag))
		return;

	if(!validationMessageTagOwnership(plugin,tag))
		return;

	m_virtualCommunicator.setElementsPerQuery(tag,size);

	m_plugins[plugin].addRegisteredMessageTagSize(tag);
}

void ComputeCore::setMasterModeNextMasterMode(PluginHandle plugin,MasterMode current,MasterMode next){

	if(!validationPluginAllocated(plugin))
		return;

	if(!validationMasterModeRange(plugin,current))
		return;

	if(!validationMasterModeRange(plugin,next))
		return;

	if(!validationMasterModeOwnership(plugin,current))
		return;

	m_switchMan.addNextMasterMode(current,next);

	// configure the switch man
	//
	// this is where steps can be added or removed.

	m_plugins[plugin].addRegisteredMasterModeNextMasterMode(current);
}

void ComputeCore::setFirstMasterMode(PluginHandle plugin,MasterMode mode){
	if(!validationPluginAllocated(plugin))
		return;

	if(!validationMasterModeRange(plugin,mode))
		return;

	if(!validationMasterModeOwnership(plugin,mode))
		return;

	if(m_hasFirstMode)
		cout<<"Error, already has a first master mode."<<endl;

	m_hasFirstMode=true;

	/** the computation will start there **/
	if(m_rank == MASTER_RANK){
		m_switchMan.setMasterMode(mode);
	}

	m_plugins[plugin].addRegisteredFirstMasterMode(mode);
}

string ComputeCore::getRayPlatformVersion(){

#ifndef RAYPLATFORM_VERSION
#define RAYPLATFORM_VERSION "Unknown-RayPlatform-Version"
#endif

	return RAYPLATFORM_VERSION;
}

bool ComputeCore::validationMasterModeRange(PluginHandle plugin,MasterMode mode){
	if(!(0<= mode && mode < MAXIMUM_NUMBER_OF_MASTER_HANDLERS)){
		cout<<"Error, master mode "<<mode<<" is not in the allowed range."<<endl;
		cout<<"MAXIMUM_NUMBER_OF_MASTER_HANDLERS= "<<MAXIMUM_NUMBER_OF_MASTER_HANDLERS<<endl;
		setFatalError();
		return false;
	}

	return true;
}

bool ComputeCore::validationSlaveModeRange(PluginHandle plugin,SlaveMode mode){
	if(!(0<= mode && mode < MAXIMUM_NUMBER_OF_SLAVE_HANDLERS)){
		cout<<"Error, slave mode "<<mode<<" is not in the allowed range."<<endl;
		cout<<"MAXIMUM_NUMBER_OF_SLAVE_HANDLERS= "<<MAXIMUM_NUMBER_OF_SLAVE_HANDLERS<<endl;
		setFatalError();
		return false;
	}

	return true;
}

bool ComputeCore::validationMessageTagRange(PluginHandle plugin,MessageTag tag){
	if(!(0<= tag && tag < MAXIMUM_NUMBER_OF_TAG_HANDLERS)){
		cout<<"Error, message tag "<<tag<<" is not in the allowed range."<<endl;
		cout<<"MAXIMUM_NUMBER_OF_TAG_HANDLERS= "<<MAXIMUM_NUMBER_OF_TAG_HANDLERS<<endl;
		setFatalError();
		return false;
	}

	return true;
}

void ComputeCore::setFatalError(){
	cout<<"A fatal error occurred !"<<endl;
	m_hasFatalErrors=true;
}

void ComputeCore::sendEmptyMessageToAll(MessageTag tag){
	m_switchMan.sendToAll(&m_outbox,m_rank,tag);
}

void ComputeCore::setObjectSymbol(PluginHandle plugin,void*object,const char* symbol){

	if(!validationPluginAllocated(plugin))
		return;

	if(!validationObjectSymbolNotRegistered(plugin,symbol))
		return;

	int handle=m_objects.size();

	m_objects.push_back(object);

	m_objectSymbols[symbol]=handle;

	m_plugins[plugin].addAllocatedObject(handle);
	m_plugins[plugin].addRegisteredObjectSymbol(handle);
}

void* ComputeCore::getObjectFromSymbol(PluginHandle plugin,const char*symbol){

	if(!validationPluginAllocated(plugin))
		return NULL;

	if(!validationObjectSymbolRegistered(plugin,symbol))
		return NULL;

	int handle=m_objectSymbols[symbol];

	void*object=m_objects[handle];

	m_plugins[plugin].addResolvedObject(handle);

	return object;
}

bool ComputeCore::validationObjectSymbolRegistered(PluginHandle plugin,const char*symbol){

	string key=symbol;

	if(m_objectSymbols.count(key)==0){
		cout<<"object symbol not registered, symbol: "<<symbol<<endl;
		setFatalError();
		return false;
	}

	return true;
}

bool ComputeCore::validationObjectSymbolNotRegistered(PluginHandle plugin,const char*symbol){

	string key=symbol;

	if(m_objectSymbols.count(key)>0){
		cout<<"object symbol already registered, symbol: "<<symbol<<endl;
		setFatalError();
		return false;
	}

	return true;
}

int ComputeCore::getNumberOfArguments(){
	return m_argumentCount;
}

char**ComputeCore::getArgumentValues(){
	return m_argumentValues;
}
