
#ifndef ProcessStatusHeader
#define ProcessStatusHeader

#include <map>
#include <iostream>
#include <string>
using namespace std;

class ProcessStatus {

private:
	map<string,string> m_values;
	string m_nothing;

public:

	ProcessStatus();
	~ProcessStatus();

	bool hasKey(string & key);
	void getProcessStatus();
	void printMemoryMetrics();
	string & getValue(string & metric);

};

#endif
