/****************************************************************************
*	 DRAMSim2: A Cycle Accurate DRAM simulator 
*	 
*	 Copyright (C) 2010   	Elliott Cooper-Balis
*									Paul Rosenfeld 
*									Bruce Jacob
*									University of Maryland
*
*	 SCIC: A System C Interface Converter for DRAMSim
*	 
*	 Copyright (C) 2011  	Myoungsoo Jung
*									Pennsylvania State University
*							David Donofrio
*							John Shalf
*									Lawrence Berkeley National Lab.
*
*	 This program is free software: you can redistribute it and/or modify
*	 it under the terms of the GNU General Public License as published by
*	 the Free Software Foundation, either version 3 of the License, or
*	 (at your option) any later version.
*
*	 This program is distributed in the hope that it will be useful,
*	 but WITHOUT ANY WARRANTY; without even the implied warranty of
*	 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*	 GNU General Public License for more details.
*
*	 You should have received a copy of the GNU General Public License
*	 along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*****************************************************************************/

//TraceBasedSim.cpp
//
//File to run a trace-based simulation
//

#include <iostream>
#include <fstream>
#include <sstream>
#include <getopt.h>
#include <map>
#include <list>

#include "SystemConfiguration.h"
#include "MemorySystem.h"
#include "Transaction.h"

//#define USE_SYSTEM_C_INTERFACE					(1)

#if (USE_SYSTEM_C_INTERFACE == 1)
#include "systemc.h"
#include "SCIC.h"
#include "Stimulus.h"
#endif


using namespace DRAMSim;
using namespace std;

//#define RETURN_TRANSACTIONS 1

#ifndef _SIM_
int SHOW_SIM_OUTPUT = 1;
ofstream visDataOut; //mostly used in MemoryController

#ifdef RETURN_TRANSACTIONS
class TransactionReceiver
{
	private: 
		map<uint64_t, list<uint64_t> > pendingReadRequests; 
		map<uint64_t, list<uint64_t> > pendingWriteRequests; 

	public: 
		void add_pending(const Transaction &t, uint64_t cycle)
		{
			// C++ lists are ordered, so the list will always push to the back and
			// remove at the front to ensure ordering
			if (t.transactionType == DATA_READ)
			{
				pendingReadRequests[t.address].push_back(cycle); 
			}
			else if (t.transactionType == DATA_WRITE)
			{
				pendingWriteRequests[t.address].push_back(cycle); 
			}
			else
			{
				ERROR("This should never happen"); 
				exit(-1);
			}
		}

		void read_complete(uint id, uint64_t address, uint64_t done_cycle)
		{
			map<uint64_t, list<uint64_t> >::iterator it;
			it = pendingReadRequests.find(address); 
			if (it == pendingReadRequests.end())
			{
				ERROR("Cant find a pending read for this one"); 
				exit(-1);
			}
			else
			{
				if (it->second.size() == 0)
				{
					ERROR("Nothing here, either"); 
					exit(-1); 
				}
			}

			uint64_t added_cycle = pendingReadRequests[address].front();
			uint64_t latency = done_cycle - added_cycle;

			pendingReadRequests[address].pop_front();
			cout << "Read Callback:  0x"<< std::hex << address << std::dec << " latency="<<latency<<"cycles ("<< done_cycle<< "->"<<added_cycle<<")"<<endl;
		}
		void write_complete(uint id, uint64_t address, uint64_t done_cycle)
		{
			map<uint64_t, list<uint64_t> >::iterator it;
			it = pendingWriteRequests.find(address); 
			if (it == pendingWriteRequests.end())
			{
				ERROR("Cant find a pending read for this one"); 
				exit(-1);
			}
			else
			{
				if (it->second.size() == 0)
				{
					ERROR("Nothing here, either"); 
					exit(-1); 
				}
			}

			uint64_t added_cycle = pendingWriteRequests[address].front();
			uint64_t latency = done_cycle - added_cycle;

			pendingWriteRequests[address].pop_front();
			cout << "Write Callback: 0x"<< std::hex << address << std::dec << " latency="<<latency<<"cycles ("<< done_cycle<< "->"<<added_cycle<<")"<<endl;
		}
};
#endif

void usage()
{
	cout << "DRAMSim2 Usage: " << endl;
	cout << "DRAMSim -t tracefile -s system.ini -d ini/device.ini [-c #] [-p pwd] -q" <<endl;
	cout << "\t-t, --tracefile=FILENAME \tspecify a tracefile to run  "<<endl;
	cout << "\t-s, --systemini=FILENAME \tspecify an ini file that describes the memory system parameters  "<<endl;
	cout << "\t-d, --deviceini=FILENAME \tspecify an ini file that describes the device-level parameters"<<endl;
	cout << "\t-c, --numcycles=# \t\tspecify number of cycles to run the simulation for [default=30] "<<endl;
	cout << "\t-q, --quiet \t\t\tflag to suppress simulation output (except final stats) [default=no]"<<endl;
	cout << "\t-o, --option=OPTION_A=234\t\t\toverwrite any ini file option from the command line"<<endl;
	cout << "\t-p, --pwd=DIRECTORY\t\tSet the working directory (i.e. usually DRAMSim directory where ini/ and results/ are)"<<endl;
	cout << "\t-S, --size=# \t\t\tSize of the memory system in megabytes"<<endl;
}
#endif

void *parseTraceFileLine(string &line, uint64_t &addr, enum TransactionType &transType, uint64_t &clockCycle, TraceType type)
{
	size_t previousIndex=0;
	size_t spaceIndex=0;
	uint64_t *dataBuffer = NULL;
	string addressStr="", cmdStr="", dataStr="", ccStr="";
#ifndef _SIM_
	bool useClockCycle = false;
#else
	bool useClockCycle = true;
#endif

	switch (type)
	{
	case k6:
	{
		spaceIndex = line.find_first_of(" ", 0);

		addressStr = line.substr(0, spaceIndex);
		previousIndex = spaceIndex;

		spaceIndex = line.find_first_not_of(" ", previousIndex);
		cmdStr = line.substr(spaceIndex, line.find_first_of(" ", spaceIndex) - spaceIndex);
		previousIndex = line.find_first_of(" ", spaceIndex);

		spaceIndex = line.find_first_not_of(" ", previousIndex);
		ccStr = line.substr(spaceIndex, line.find_first_of(" ", spaceIndex) - spaceIndex);

		if (cmdStr.compare("P_MEM_WR")==0 ||
		        cmdStr.compare("BOFF")==0)
		{
			transType = DATA_WRITE;
		}
		else if (cmdStr.compare("P_FETCH")==0 ||
		         cmdStr.compare("P_MEM_RD")==0 ||
		         cmdStr.compare("P_LOCK_RD")==0 ||
		         cmdStr.compare("P_LOCK_WR")==0)
		{
			transType = DATA_READ;
		}
		else
		{
			ERROR("== Unknown Command : "<<cmdStr);
			exit(0);
		}

		istringstream a(addressStr.substr(2));//gets rid of 0x
		a>>hex>>addr;

		//if this is set to false, clockCycle will remain at 0, and every line read from the trace
		//  will be allowed to be issued
		if (useClockCycle)
		{
			istringstream b(ccStr);
			b>>clockCycle;
		}
		break;
	}
	case mase:
	{
		spaceIndex = line.find_first_of(" ", 0);

		addressStr = line.substr(0, spaceIndex);
		previousIndex = spaceIndex;

		spaceIndex = line.find_first_not_of(" ", previousIndex);
		cmdStr = line.substr(spaceIndex, line.find_first_of(" ", spaceIndex) - spaceIndex);
		previousIndex = line.find_first_of(" ", spaceIndex);

		spaceIndex = line.find_first_not_of(" ", previousIndex);
		ccStr = line.substr(spaceIndex, line.find_first_of(" ", spaceIndex) - spaceIndex);

		if (cmdStr.compare("IFETCH")==0||
		        cmdStr.compare("READ")==0)
		{
			transType = DATA_READ;
		}
		else if (cmdStr.compare("WRITE")==0)
		{
			transType = DATA_WRITE;
		}
		else
		{
			ERROR("== Unknown command in tracefile : "<<cmdStr);
		}

		istringstream a(addressStr.substr(2));//gets rid of 0x
		a>>hex>>addr;

		//if this is set to false, clockCycle will remain at 0, and every line read from the trace
		//  will be allowed to be issued
		if (useClockCycle)
		{
			istringstream b(ccStr);
			b>>clockCycle;
		}

		break;
	}
	case misc:
		spaceIndex = line.find_first_of(" ", spaceIndex+1);
		if (spaceIndex == string::npos)
		{
			ERROR("Malformed line: '"<< line <<"'");
		}

		addressStr = line.substr(previousIndex,spaceIndex);
		previousIndex=spaceIndex;

		spaceIndex = line.find_first_of(" ", spaceIndex+1);
		if (spaceIndex == string::npos)
		{
			cmdStr = line.substr(previousIndex+1);
		}
		else
		{
			cmdStr = line.substr(previousIndex+1,spaceIndex-previousIndex-1);
			dataStr = line.substr(spaceIndex+1);
		}

		//convert address string -> number
		istringstream b(addressStr.substr(2)); //substr(2) chops off 0x characters
		b >>hex>> addr;

		// parse command
		if (cmdStr.compare("read") == 0)
		{
			transType=DATA_READ;
		}
		else if (cmdStr.compare("write") == 0)
		{
			transType=DATA_WRITE;
		}
		else
		{
			ERROR("INVALID COMMAND '"<<cmdStr<<"'");
			exit(-1);
		}
		if (SHOW_SIM_OUTPUT)
		{
			DEBUGN("ADDR='"<<hex<<addr<<dec<<"',CMD='"<<transType<<"'");//',DATA='"<<dataBuffer[0]<<"'");
		}

		//parse data
		//if we are running in a no storage mode, don't allocate space, just return NULL
#ifndef NO_STORAGE
		if (dataStr.size() > 0 && transType == DATA_WRITE)
		{
			// 32 bytes of data per transaction
			dataBuffer = (uint64_t *)calloc(sizeof(uint64_t),4);
			size_t strlen = dataStr.size();
			for (int i=0; i < 4; i++)
			{
				size_t startIndex = i*16;
				if (startIndex > strlen)
				{
					break;
				}
				size_t charsLeft = min(((size_t)16), strlen - startIndex + 1);
				string piece = dataStr.substr(i*16,charsLeft);
				istringstream iss(piece);
				iss >> hex >> dataBuffer[i];
			}
			PRINTN("\tDATA=");
			BusPacket::printData(dataBuffer);
		}

		PRINT("");
#endif
		break;
	}
	return dataBuffer;
}

#ifndef _SIM_

void alignTransactionAddress(Transaction &trans)
{
	// zero out the low order bits which correspond to the size of a transaction

	unsigned throwAwayBits = dramsim_log2((BL*JEDEC_DATA_BUS_BITS/8));

	trans.address >>= throwAwayBits;
	trans.address <<= throwAwayBits;
}

#if ((USE_SYSTEM_C_INTERFACE == 1) && (USE_SCIC_MEMSYS_QUERY == 1) )
void ExampleforDisplayingStatistics( SCIC &memSystemSc ) 
{

	//
	// test for displaying performance
	// In practice, you don't need buch of these vector to query memory system information.
	// It is just for test (comparison to original statistics)
	//

	// energy test
	vector<double> energyInfos[SCIC_NUMS_ENERGY_TYPE];
	for(int nQueryType = 0; nQueryType < SCIC_NUMS_ENERGY_TYPE; nQueryType++)
	{
		energyInfos[nQueryType] = vector<double>(NUM_RANKS,0.0);
		memSystemSc.GetElapsedEnergyInfo(energyInfos[nQueryType], (SCIC_ENERGY_QUERY)nQueryType);
	}
	vector<double> averageEnergyInfo					= vector<double>(NUM_RANKS,0.0);
	memSystemSc.GetElapsedEnergyInfo(averageEnergyInfo, SCIC_ENERGY_AVERAGE);

	// performance test
	vector<double> latencyInfo						= vector<double>(NUM_RANKS*NUM_BANKS,0.0);
	vector<double> bandwidthInfo					= vector<double>(NUM_RANKS*NUM_BANKS,0.0);

	// statistics test
	vector<uint64_t> numsRead						= vector<uint64_t>(NUM_RANKS*NUM_BANKS,0.0);
	vector<uint64_t> numsWrite						= vector<uint64_t>(NUM_RANKS*NUM_BANKS,0.0);


	PRINT( " =====================SCIC==============================" );

	PRINTN( "   Total Return Transactions : " << memSystemSc.GetTotalNumsTransactions() );
	PRINT( " ("<<memSystemSc.GetTotalNumsTransactions() * memSystemSc.GetBytePerTransaction() <<" bytes) aggregate average bandwidth "<<memSystemSc.GetElapsedPerfromanceInfo(bandwidthInfo, SCIC_PERF_BANDWIDTH)<<"GB/s");


	cout << "Total write requests : " << memSystemSc.GetNumsElapsedIo(numsWrite, SCIC_STAT_WRITE)  << endl;
	cout << "Total read requests : " << memSystemSc.GetNumsElapsedIo(numsRead, SCIC_STAT_READ)  << endl;

	memSystemSc.GetElapsedPerfromanceInfo(latencyInfo, SCIC_PERF_LATENCY);
	for (size_t i=0;i<NUM_RANKS;i++)
	{

		PRINT( "      -Rank   "<<i<<" : ");
		PRINTN( "        -Reads  : " << numsRead[i]);
		PRINT( " ("<<numsRead[i] * memSystemSc.GetBytePerTransaction()<<" bytes)");
		PRINTN( "        -Writes : " << numsWrite[i]);
		PRINT( " ("<<numsWrite[i] * memSystemSc.GetBytePerTransaction()<<" bytes)");

		for (size_t j=0;j<NUM_BANKS;j++)
		{
			PRINT( "        -Bandwidth / Latency  (Bank " <<j<<"): " <<bandwidthInfo[SEQUENTIAL(i,j)] << " GB/s\t\t" <<latencyInfo[SEQUENTIAL(i,j)] << " ns");
		}

		PRINT( " == Power Data for Rank        " << i );
		PRINT( "   Average Power (watts)     : " << averageEnergyInfo[i] );
		PRINT( "     -Background (watts)     : " << energyInfos[SCIC_ENERGY_BACKGROUND][i] );
		PRINT( "     -Act/Pre    (watts)     : " << energyInfos[SCIC_ENERGY_PRECHARGE][i] );
		PRINT( "     -Burst      (watts)     : " << energyInfos[SCIC_ENERGY_BURST][i]);
		PRINT( "     -Refresh    (watts)     : " << energyInfos[SCIC_ENERGY_REFRESH][i] );


	}
}

#endif


int main(int argc, char **argv)
{
	int c;
	string traceFileName = "";
	TraceType traceType;
	string systemIniFilename = "system.ini";
	string deviceIniFilename = "";
	string pwdString = "";
	unsigned megsOfMemory=2048;

	bool overrideOpt = false;
	string overrideKey = "";
	string overrideVal = "";
	string tmp = "";
	size_t equalsign;

	uint numCycles=1000;
	//getopt stuff
	while (1)
	{
		static struct option long_options[] =
		{
			{"deviceini", required_argument, 0, 'd'},
			{"tracefile", required_argument, 0, 't'},
			{"systemini", required_argument, 0, 's'},
			{"pwd", required_argument, 0, 'p'},
			{"numcycles",  required_argument,	0, 'c'},
			{"option",  required_argument,	0, 'o'},
			{"quiet",  no_argument, &SHOW_SIM_OUTPUT, 'q'},
			{"help", no_argument, 0, 'h'},
			{"size", required_argument, 0, 'S'},
			{0, 0	, 0, 0}
		};
		int option_index=0; //for getopt
		c = getopt_long (argc, argv, "t:s:c:d:o:p:S:bkq", long_options, &option_index);
		if (c == -1)
		{
			break;
		}
		switch (c)
		{
		case 0: //TODO: figure out what the hell this does, cuz it never seems to get called
			if (long_options[option_index].flag != 0) //do nothing on a flag
			{
				printf("setting flag\n");
				break;
			}
			printf("option %s",long_options[option_index].name);
			if (optarg)
			{
				printf(" with arg %s", optarg);
			}
			printf("\n");
			break;
		case 'h':
			usage();
			exit(0);
			break;
		case 't':
			traceFileName = string(optarg);
			break;
		case 's':
			systemIniFilename = string(optarg);
			break;
		case 'd':
			deviceIniFilename = string(optarg);
			break;
		case 'c':
			numCycles = atoi(optarg);
			break;
		case 'S':
			megsOfMemory=atoi(optarg);
			break;
		case 'p':
			pwdString = string(optarg);
			break;
		case 'q':
			SHOW_SIM_OUTPUT=false;
			break;
		case 'o':
			tmp = string(optarg);
			equalsign = tmp.find_first_of('=');
			overrideKey = tmp.substr(0,equalsign);
			overrideVal = tmp.substr(equalsign+1,tmp.size()-equalsign+1);
			overrideOpt = true;
			break;
		case '?':
			usage();
			exit(-1);
			break;
		}
	}

	// get the trace filename
	string temp = traceFileName.substr(traceFileName.find_last_of("/")+1);

	//get the prefix of the trace name
	temp = temp.substr(0,temp.find_first_of("_"));
	if (temp=="mase")
	{
		traceType = mase;
	}
	else if (temp=="k6")
	{
		traceType = k6;
	}
	else if (temp=="misc")
	{
		traceType = misc;
	}
	else
	{
		ERROR("== Unknown Tracefile Type : "<<temp);
		exit(0);
	}


	// no default value for the default model name
	if (deviceIniFilename.length() == 0)
	{
		ERROR("Please provide a device ini file");
		usage();
		exit(-1);
	}


	//ignore the pwd argument if the argument is an absolute path
	if (pwdString.length() > 0 && traceFileName[0] != '/')
	{
		traceFileName = pwdString + "/" +traceFileName;
	}

	DEBUG("== Loading trace file '"<<traceFileName<<"' == ");

	ifstream traceFile;
	string line;

	MemorySystem *memorySystem = new MemorySystem(0, deviceIniFilename, systemIniFilename, pwdString, traceFileName, megsOfMemory);



#ifdef RETURN_TRANSACTIONS
	TransactionReceiver transactionReceiver; 
	/* create and register our callback functions */
	Callback_t *read_cb = new Callback<TransactionReceiver, void, uint, uint64_t, uint64_t>(&transactionReceiver, &TransactionReceiver::read_complete);
	Callback_t *write_cb = new Callback<TransactionReceiver, void, uint, uint64_t, uint64_t>(&transactionReceiver, &TransactionReceiver::write_complete);
	memorySystem->RegisterCallbacks(read_cb, write_cb, NULL);
#endif

	uint64_t	addr;
	uint64_t	clockCycle	=	0;
	enum TransactionType transType;

	void *data	= NULL;
	int lineNumber = 0;
	Transaction trans;
	bool pendingTrans = false;

	traceFile.open(traceFileName.c_str());

	if (!traceFile.is_open())
	{
		cout << "== Error - Could not open trace file"<<endl;
		exit(0);
	}



#if (USE_SYSTEM_C_INTERFACE  == 1)

	sc_set_time_resolution(1, SC_NS);
	sc_set_default_time_unit(1, SC_NS);
	sc_clock				sysClk("clock", 2, SC_NS, 0.5, 0, SC_NS, true);
	sc_signal<uint64_t>		sgnDestAddr;
	sc_signal<uint64_t>		sgnBuffAddr;
	sc_signal<bool>			sgnWriteEnable;

	sc_signal<bool>			sgnDimmEnable;
	sc_signal<bool>			sgnBiuBusy;
	sc_signal<uint64_t>		sgnAddrOut;
	sc_signal<bool>			sgnWriteOut;
	sc_signal<bool>			sgnCompAck;


	sc_signal<uint64_t>		sgnDataMemIn[4];
	sc_signal<uint64_t>		sgnDataMemOut[4];

	//
	// make connection between system c interface and memory system
	//
	SCIC memSystemSc("SC_MemorySystem");
	memSystemSc.AttachLegacyMemorySystem(memorySystem);
	memSystemSc._prtCLK(sysClk.signal());

	memSystemSc._prtDA(sgnDestAddr);
	memSystemSc._prtWE(sgnWriteEnable);

	memSystemSc._prtDE(sgnDimmEnable);
	memSystemSc._prtRB(sgnBiuBusy);
	memSystemSc._prtAO(sgnAddrOut);
	memSystemSc._prtWO(sgnWriteOut);
	memSystemSc._prtCA(sgnCompAck);

	//
	// Initializing Stimulus
	//
	Stimulus stimulus("Stimulus");
	stimulus.AttachTracefile(&traceFile, traceType);
	stimulus._prtAO(sgnDestAddr);
	stimulus._prtRB(sgnBiuBusy);
	stimulus._prtCLK(sysClk.signal());
	stimulus._prtDE(sgnDimmEnable);
	stimulus._prtWE(sgnWriteEnable);
	stimulus._prtCA(sgnCompAck);
	stimulus._prtAI(sgnAddrOut);
	stimulus._prtWI(sgnWriteOut);

	//
	// System C trace
	//
	sc_trace_file *pfScTrace = sc_create_vcd_trace_file("wave");
	sc_trace(pfScTrace, sysClk, "CLK");
	sc_trace(pfScTrace, sgnDestAddr, "DA");
	sc_trace(pfScTrace, sgnBuffAddr, "buffAddr");
	sc_trace(pfScTrace, sgnWriteEnable, "WE");

	sc_trace(pfScTrace, sgnDimmEnable, "DE");
	sc_trace(pfScTrace, sgnBiuBusy, "RB");
	sc_trace(pfScTrace, sgnAddrOut, "AO");
	sc_trace(pfScTrace, sgnWriteOut, "WO");
	sc_trace(pfScTrace, sgnCompAck, "CA");

	//
	// make connection of data port between memory and stimulus
	//
	for(int nBuffIdx = 0; nBuffIdx < 4; nBuffIdx++)
	{
		stringstream strStreamName;

		memSystemSc._prtDOUT[nBuffIdx](sgnDataMemOut[nBuffIdx]);
		stimulus._prtDataIn[nBuffIdx](sgnDataMemOut[nBuffIdx]);
		
		strStreamName << "DOUT(" << nBuffIdx << ")";
		sc_trace(pfScTrace, sgnDataMemOut[nBuffIdx], strStreamName.str());
		
		memSystemSc._prtDIN[nBuffIdx](sgnDataMemIn[nBuffIdx]);
		stimulus._prtDataOut[nBuffIdx](sgnDataMemIn[nBuffIdx]);
		
		strStreamName.str("");
		strStreamName << "DIN(" << nBuffIdx << ")";
		sc_trace(pfScTrace, sgnDataMemIn[nBuffIdx], strStreamName.str());
		
	}


#if (USE_CALIBRATION_1CYCLE != 1)
	//
	// Since System C interface for DRAMSim works on double data rate, calibration for ignoring the end of cycle is needed
	//
	sc_start(numCycles, SC_NS);
#else
	sc_start(numCycles+1, SC_NS);
#endif

#else

	for (size_t i=0;i<numCycles;i++)
	{
		if (!pendingTrans)
		{
			if (!traceFile.eof())
			{
				getline(traceFile, line);

				if (line.size() > 0)
				{
					data = parseTraceFileLine(line, addr, transType,clockCycle, traceType);
					trans = Transaction(transType, addr, data);
					alignTransactionAddress(trans); 

					if (i>=clockCycle)
					{
						if (!(*memorySystem).addTransaction(trans))
						{
							pendingTrans = true;
						}
						else
						{
#ifdef RETURN_TRANSACTIONS
							transactionReceiver.add_pending(trans, i); 
#endif
						}
					}
					else
					{
						pendingTrans = true;
					}
				}
				else
				{
					DEBUG("WARNING: Skipping line "<<lineNumber<< " ('" << line << "') in tracefile");
				}
				lineNumber++;
			}
			else
			{
				//we're out of trace, set pending=false and let the thing spin without adding transactions
				pendingTrans = false; 
			}
		}

		else if (pendingTrans && i >= clockCycle)
		{
			pendingTrans = !(*memorySystem).addTransaction(trans);
			if (!pendingTrans)
			{
#ifdef RETURN_TRANSACTIONS
				transactionReceiver.add_pending(trans, i); 
#endif
			}
		}

		(*memorySystem).update();
	}
#endif

	traceFile.close();
	(*memorySystem).printStats(true);
	ExampleforDisplayingStatistics(memSystemSc);


	// make valgrind happy
	delete(memorySystem);
}
#endif
