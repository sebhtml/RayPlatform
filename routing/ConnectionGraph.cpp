/*
 	RayPlatform
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

#include <routing/ConnectionGraph.h>

enum {
__COMPLETE,
__GROUP,
__RANDOM,
__DEBRUIJN,
__KAUTZ,
__EXPERIMENTAL,
__HYPERCUBE
};

/**
 * Print a route
 */
void ConnectionGraph::printRoute(Rank source,Rank destination){
	cout<<"[printRoute] Source: "<<source<<"	Destination: "<<destination<<"	";

	vector<Rank> route;
	m_implementation->getRoute(source,destination,&route);

	cout<<"Size: "<<route.size()<<"	Route: ";

	for(int i=0;i<(int)route.size();i++){
		if(i!=0)
			cout<<" ";
		cout<<route[i];
	}
	cout<<"	Hops: "<<route.size()-1<<endl;
}


/**
 * a rank can only speak to things listed in connections
 */
bool ConnectionGraph::isConnected(Rank source,Rank destination){
	// TODO: replace by a switch case to avoid virtual calls
	return m_implementation->isConnected(source,destination);
}

/**
 * Write files.
 */
void ConnectionGraph::writeFiles(string prefix){
	// dump the connections in a file
	ostringstream file;
	file<<prefix<<"Connections.txt";
	ofstream f(file.str().c_str());

	f<<"#Rank	Count	Connections"<<endl;

	for(Rank rank=0;rank<m_size;rank++){
		vector<Rank> connections;
		m_implementation->getOutcomingConnections(rank,&connections);

		f<<rank<<"	"<<connections.size()<<"	";

		for(vector<Rank>::iterator i=connections.begin();
			i!=connections.end();i++){
			if(i!=connections.begin())
				f<<" ";
			f<<*i;
		}
		f<<endl;
	}

	f.close();

	// dump the routes in a file
	ostringstream file2;
	file2<<prefix<<"Routes.txt";
	ofstream f2(file2.str().c_str());
	
	if(m_typeCode==__HYPERCUBE){
		f2<<"Routes are dynamically determined with real-time load-balancing."<<endl;
	}else{
		f2<<"#Source	Destination	Hops	Route"<<endl;

		for(Rank rank=0;rank<m_size;rank++){
			for(Rank i=0;i<m_size;i++){
				vector<Rank> route;
				m_implementation->getRoute(rank,i,&route);
				f2<<rank<<"	"<<i<<"	"<<route.size()-1<<"	";
	
				for(int i=0;i<(int)route.size();i++){
					if(i!=0)
						f2<<" ";
					f2<<route[i];
				}
	
				f2<<endl;
			}
		}
	}

	f2.close();

	// write relay events
	ostringstream file3;
	file3<<prefix<<"RelayEvents.txt";
	ofstream f3(file3.str().c_str());
	f3<<"#Source	RelayEvents"<<endl;

	vector<int> relayEvents;

	for(Rank rank=0;rank<m_size;rank++){
		int relays=m_implementation->getRelays(rank);
		f3<<rank<<"	"<<relays<<endl;
		relayEvents.push_back(relays);
	}

	f3.close();

	// dump the routes in a file
	ostringstream file4;
	file4<<prefix<<"Summary.txt";
	ofstream f4(file4.str().c_str());

	int numberOfVertices=m_size;

	int numberOfEdges=0;

	vector<int> connectivities;

	for(Rank i=0;i<m_size;i++){
		vector<Rank> connections;

		m_implementation->getOutcomingConnections(i,&connections);

		connectivities.push_back(connections.size());
		numberOfEdges+=connections.size();

	}

	f4<<"Type: "<<m_type<<endl;
	f4<<endl;

	f4<<"NumberOfVertices: "<<numberOfVertices<<endl;
	f4<<"NumberOfArcs: "<<numberOfEdges<<endl;
	f4<<"NumberOfArcsInCompleteGraph: "<<numberOfVertices*(numberOfVertices-1)<<endl;
	f4<<endl;
	f4<<"NumberOfOutcomingArcsPerVertex"<<endl;
	f4<<"   Frequencies:"<<endl;

	map<int,int> connectionFrequencies;
	for(Rank i=0;i<m_size;i++){
		vector<Rank> connections;
		m_implementation->getOutcomingConnections(i,&connections);
		connectionFrequencies[connections.size()]++;
	}

	int totalForEdges=0;

	for(map<int,int>::iterator i=connectionFrequencies.begin();
		i!=connectionFrequencies.end();i++){
		totalForEdges+=i->second;
	}

	for(map<int,int>::iterator i=connectionFrequencies.begin();
		i!=connectionFrequencies.end();i++){
		f4<<"        "<<i->first<<"    "<<i->second<<"    "<<i->second*100.0/totalForEdges<<"%"<<endl;
	}
	
	f4<<"        "<<"Total"<<"    "<<totalForEdges<<"    100.00%"<<endl;

	f4<<"   Average: "<<getAverage(&connectivities)<<endl;
	f4<<"   StandardDeviation: "<<getStandardDeviation(&connectivities)<<endl;

	f4<<endl;
	f4<<"NumberOfRelayEventsPerVertex"<<endl;

	f4<<"   Average: "<<getAverage(&relayEvents)<<endl;
	f4<<"   StandardDeviation: "<<getStandardDeviation(&relayEvents)<<endl;

	f4<<endl;
	f4<<"RouteLength"<<endl;

	f4<<"   Frequencies:"<<endl;
	map<int,int> pathLengths;

	for(Rank i=0;i<m_size;i++){
		for(Rank j=0;j<m_size;j++){
			vector<Rank> route;
			m_implementation->getRoute(i,j,&route);

			// we remove the source vertex
			pathLengths[route.size()-1]++;
		}
	}

	int totalForPaths=0;
	for(map<int,int>::iterator i=pathLengths.begin();
		i!=pathLengths.end();i++){
		totalForPaths+=i->second;
	}

	for(map<int,int>::iterator i=pathLengths.begin();
		i!=pathLengths.end();i++){
		f4<<"        "<<i->first<<"    "<<i->second<<"    "<<i->second*100.0/totalForPaths<<"%"<<endl;
	}
	f4<<"        "<<"Total"<<"    "<<totalForPaths<<"    100.00%"<<endl;

	f4.close();

}

int ConnectionGraph::getNextRankInRoute(Rank source,Rank destination,Rank rank){
	// TODO: replace by a switch case to avoid virtual calls
	return m_implementation->getNextRankInRoute(source,destination,rank);
}

void ConnectionGraph::getIncomingConnections(Rank source,vector<Rank>*connections){
	// TODO: replace by a switch case to avoid virtual calls
	m_implementation->getIncomingConnections(source,connections);
}

void ConnectionGraph::buildGraph(int numberOfRanks,string type,bool verbosity,
int degree){
	m_verbose=verbosity;

	m_size=numberOfRanks;

	/** provide the user-provided degree for those
 * requiring it */
	m_deBruijn.setDegree(degree);
	m_hypercube.setDegree(degree);

	if(type==""){
		type="debruijn";
	}

	m_implementation=NULL;

	if(type=="random"){
		m_implementation=&m_random;
		m_typeCode=__RANDOM;
	}else if((type=="hypercube"||type=="polytope")
		 && m_hypercube.isValid(numberOfRanks)){

		m_implementation=&m_hypercube;
		m_typeCode=__HYPERCUBE;
	}else if(type=="group"){
		m_implementation=&m_group;
		m_typeCode=__GROUP;
	}else if(type=="debruijn" && m_deBruijn.isValid(numberOfRanks)){
		m_implementation=&m_deBruijn;
		m_typeCode=__DEBRUIJN;
	}else if(type=="complete"){
		m_implementation=&m_complete;
		m_typeCode=__COMPLETE;
	}else if(type=="kautz" && m_kautz.isValid(numberOfRanks)){
		m_implementation=&m_kautz;
		m_typeCode=__KAUTZ;
	}else if(type=="experimental" && m_experimental.isValid(numberOfRanks)){
		m_implementation=&m_experimental;
		m_typeCode=__EXPERIMENTAL;
	}else{
		cout<<"Warning: using a complete graph because type "<<type<<" can not be used with "<<numberOfRanks<<" vertices"<<endl;
		type="complete";
		m_implementation=&m_complete;
		m_typeCode=__COMPLETE;
	}

	m_type=type;
	
	// TODO: replace by a switch case to avoid virtual calls
	m_implementation->setVerbosity(m_verbose);

	// TODO: replace by a switch case to avoid virtual calls
	m_implementation->makeConnections(m_size);

	// TODO: replace by a switch case to avoid virtual calls
	m_implementation->makeRoutes();
}

/**
 * TODO: remove me
 * */
int ConnectionGraph::getRelaysFrom0(Rank rank){
	return m_implementation->getRelaysFrom0(rank);
}

/**
 * TODO: remove me
 */
int ConnectionGraph::getRelaysTo0(Rank rank){
	return m_implementation->getRelaysTo0(rank);
}

void ConnectionGraph::printStatus(){

	if(m_typeCode==__HYPERCUBE)
		m_hypercube.printStatus(m_rank);
}

void ConnectionGraph::start(Rank rank){

	m_rank=rank;

	if(m_typeCode==__HYPERCUBE)
		m_hypercube.start();
}
