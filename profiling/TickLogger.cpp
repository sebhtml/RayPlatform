/*
 	Ray
    Copyright (C) 2012 Sébastien Boisvert

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

#include <profiling/TickLogger.h>
#include <assert.h>
#include <iostream>
#include <time.h>
#include <core/OperatingSystem.h>
using namespace std;

//#define CONFIG_DISPLAY_TICKS
#define CONFIG_TICK_STEPPING 1000000

void TickLogger::logSlaveTick(SlaveMode i){
	#ifdef ASSERT
	assert(i!=INVALID_HANDLE);
	#endif

	// this case occurs once during the first call
	if(m_lastSlaveMode==INVALID_HANDLE){
		// start the new entry
		m_lastSlaveMode=i;
		m_slaveCount=0;
		m_slaveStarts.push_back(getTheTime());

	// this is the same as the last one
	}else if(i==m_lastSlaveMode){
	
		// nothing to do

	// this is a new slave mode
	}else{
		// add the last entry
		m_slaveModes.push_back(m_lastSlaveMode);
		m_slaveCounts.push_back(m_slaveCount);

		uint64_t stamp=getTheTime();
		m_slaveEnds.push_back(stamp);
		m_receivedMessageCounts.push_back(m_receivedMessageCount);
		m_receivedMessageCount=0;
		m_sentMessageCounts.push_back(m_sentMessageCount);
		m_sentMessageCount=0;

		// start the new entry
		m_lastSlaveMode=i;
		m_slaveCount=0;
		m_slaveStarts.push_back(stamp);
	}

	// increment the ticks
	m_slaveCount++;

	#ifdef CONFIG_DISPLAY_TICKS
	if(m_slaveCount% CONFIG_TICK_STEPPING == 0)
		cout<<"[TickLogger::addSlaveTick] "<<SLAVE_MODES[i]<<" "<<m_slaveCount<<endl;
	#endif
}

void TickLogger::logMasterTick(MasterMode i){
	#ifdef ASSERT
	assert(i!=INVALID_HANDLE);
	#endif

	// this case occurs once during the first call
	if(m_lastMasterMode==INVALID_HANDLE){
		// start the new entry
		m_lastMasterMode=i;
		m_masterCount=0;
		m_masterStarts.push_back(getTheTime());

	// this is the same as the last one
	}else if(i==m_lastMasterMode){
	
		// Nothing to do

	// this is a new slave mode
	}else{

		uint64_t stamp=getTheTime();

		// add the last entry
		m_masterModes.push_back(m_lastMasterMode);
		m_masterCounts.push_back(m_masterCount);
		m_masterEnds.push_back(stamp);

		// start the new entry
		m_lastMasterMode=i;
		m_masterCount=0;
		m_masterStarts.push_back(stamp);
	}

	m_masterCount++;

	#ifdef CONFIG_DISPLAY_TICKS
	if(m_masterCount% CONFIG_TICK_STEPPING == 0)
		cout<<"[TickLogger::addMasterTick] "<<MASTER_MODES[i]<<" "<<m_masterCount<<endl;
	#endif

}

void TickLogger::printSlaveTicks(ofstream*file){
	// add the last entry
	m_slaveModes.push_back(m_lastSlaveMode);
	m_slaveCounts.push_back(m_slaveCount);
	m_slaveEnds.push_back(getTheTime());
	m_receivedMessageCounts.push_back(m_receivedMessageCount);
	m_receivedMessageCount=0;
	m_sentMessageCounts.push_back(m_sentMessageCount);
	m_sentMessageCount=0;

	// reset the entry
	m_lastSlaveMode=INVALID_HANDLE;
	m_slaveCount=0;

	// print stuff
	
	#ifdef ASSERT
	assert(m_slaveModes.size()==m_slaveCounts.size());
	assert(m_slaveModes.size()==m_slaveStarts.size());
	assert(m_slaveModes.size()==m_slaveEnds.size());
	assert(m_slaveModes.size()==m_receivedMessageCounts.size());
	assert(m_slaveModes.size()==m_sentMessageCounts.size());
	#endif

	uint64_t total=0;

	for(int i=0;i<(int)m_slaveModes.size();i++){
		total+=m_slaveCounts[i];
	}

	bool firstSet=false;
	uint64_t firstTime=0;

	(*file)<<"Index	Slave mode	Start time in milliseconds	End time in milliseconds	Duration in milliseconds	Number of ticks	Average granularity in nanoseconds	Received messages	Average number of received messages per second	Sent messages	Average number of sent messages per second"<<endl;
	for(int i=0;i<(int)m_slaveModes.size();i++){
		uint64_t startTime=m_slaveStarts[i];
		uint64_t endTime=m_slaveEnds[i];

		if(!firstSet){
			firstSet=true;
			firstTime=startTime;
		}

		int realStart=startTime-firstTime;
		int realEnd=endTime-firstTime;
		int delta=realEnd-realStart;

		double ratio=m_slaveCounts[i];
		if(total!=0)
			ratio/=total;

		uint64_t ticks=m_slaveCounts[i];

		uint64_t granularity=delta;
		granularity*=1000*1000;

		#ifdef ASSERT
		assert(ticks!=0);
		#endif

		granularity/=ticks;

		uint64_t receptionSpeed=m_receivedMessageCounts[i];

		if(delta!=0){
			receptionSpeed*=1000;
			receptionSpeed/=delta;
		}

		uint64_t sendingSpeed=m_sentMessageCounts[i];

		if(delta!=0){
			sendingSpeed*=1000;
			sendingSpeed/=delta;
		}

		(*file)<<i<<"	"<<SLAVE_MODES[m_slaveModes[i]];
		(*file)<<"	"<<realStart<<"	"<<realEnd<<"	"<<delta;
		(*file)<<"	"<<ticks;
		(*file)<<"	"<<granularity;
		//<<"	"<<ratio<<endl;
		(*file)<<"	"<<m_receivedMessageCounts[i]<<"	";
		(*file)<<receptionSpeed<<"	";
		(*file)<<m_sentMessageCounts[i];
		(*file)<<"	"<<sendingSpeed;
		(*file)<<endl;
	}

	m_slaveModes.clear();
	m_slaveCounts.clear();
	m_slaveStarts.clear();
	m_slaveEnds.clear();
}

void TickLogger::printMasterTicks(ofstream*file){

	// add the last entry
	m_masterModes.push_back(m_lastMasterMode);
	m_masterCounts.push_back(m_masterCount);
	m_masterEnds.push_back(getTheTime());

	// reset the entry
	m_lastMasterMode=INVALID_HANDLE;
	m_masterCount=0;

	// print stuff
	
	#ifdef ASSERT
	assert(m_masterModes.size()==m_masterCounts.size());
	assert(m_masterModes.size()==m_masterStarts.size());
	assert(m_masterModes.size()==m_masterEnds.size());
	#endif
	
	(*file)<<"Index	Master mode	Start time in milliseconds	End time in milliseconds	Duration in milliseconds	Number of ticks	Average granularity in nanoseconds"<<endl;

	uint64_t total=0;

	for(int i=0;i<(int)m_masterModes.size();i++){
		total+=m_masterCounts[i];
	}

	bool firstSet=false;
	uint64_t firstTime=0;

	for(int i=0;i<(int)m_masterModes.size();i++){

		uint64_t startTime=m_masterStarts[i];
		uint64_t endTime=m_masterEnds[i];
		
		if(!firstSet){
			firstSet=true;
			firstTime=startTime;
		}

		int realStart=startTime-firstTime;
		int realEnd=endTime-firstTime;
		int delta=endTime-startTime;
		
		double ratio=m_masterCounts[i];
		if(total!=0)
			ratio/=total;

		uint64_t ticks=m_masterCounts[i];

		uint64_t granularity=delta;
		granularity*=1000*1000;

		#ifdef ASSERT
		assert(ticks!=0);
		#endif

		granularity/=ticks;

		(*file)<<i<<"	"<<MASTER_MODES[m_masterModes[i]];
		(*file)<<"	"<<realStart<<"	"<<realEnd<<"	";
		(*file)<<delta<<"	"<<ticks<<"	"<<granularity<<endl;
	}

	m_masterModes.clear();
	m_masterCounts.clear();
	m_masterStarts.clear();
	m_masterEnds.clear();
}

TickLogger::TickLogger(){
	m_lastSlaveMode=INVALID_HANDLE;
	m_lastMasterMode=INVALID_HANDLE;
	m_slaveCount=0;
	m_masterCount=0;
}

void TickLogger::logReceivedMessage(MessageTag i){
	m_receivedMessageCount++;
}

void TickLogger::logSendMessage(MessageTag i){
	m_sentMessageCount++;
}

uint64_t TickLogger::getTheTime(){
	return getMilliSeconds();
}
