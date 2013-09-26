
#include "ProcessStatus.h"

#include <fstream>
#include <sstream>
#include <vector>
using namespace std;

void ProcessStatus::getProcessStatus() {

	m_values.clear();

#ifdef __linux__

	ifstream file("/proc/self/status");

	if(!file) {
		file.close();
		return;
	}

	while(!file.eof()) {

		char line[1024];
		file.getline(line, 1024);

		istringstream lineStream(line);

		string key = "";
		lineStream >> key;

		string value = lineStream.str();
		int position = lineStream.tellg();
		while(position < (int)value.length()) {
			char symbol = value[position];
			if(symbol == ' ' || symbol == '\t' || symbol == '\n')
				position++;
			else
				break;
		}

		string newValue = value.c_str() + position;
		//cout << " newValue " << newValue << " position= " << position << endl;

		m_values[key] = newValue;
	}

	file.close();

#endif

}

void ProcessStatus::printMemoryMetrics() {

	vector<string> metrics;

	metrics.push_back("VmLck:");
	metrics.push_back("VmPin:");
	metrics.push_back("VmRSS:");
	metrics.push_back("VmData:");
	metrics.push_back("VmStk:");
	metrics.push_back("VmExe:");
	metrics.push_back("VmLib:");
	metrics.push_back("VmPTE:");
	metrics.push_back("VmSwap:");

	for(vector<string>::iterator i = metrics.begin();
			i != metrics.end();
			++i) {

		string & metric = *i;

		/*
		if(!hasKey(metric))
			continue;
		*/

		cout << " " << metric << " " << getValue(metric);
		//cout << "" << metric << "";
	}
}

bool ProcessStatus::hasKey(string & key) {
	return m_values.count(key) > 0;
}

string & ProcessStatus::getValue(string & metric) {

	if(hasKey(metric))
		return m_values[metric];

	return m_nothing;
}

ProcessStatus::ProcessStatus(){

}

ProcessStatus::~ProcessStatus() {

}
