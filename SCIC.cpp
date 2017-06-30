/****************************************************************************
*	 SCIC: A System C Interface Converter for DRAMSim
*	 
*	 Copyright (C) 2011  	Myoungsoo Jung
*									Pennsylvania State University
*							David Donofrio
*							John Shalf
*									Lawrence Berkeley National Lab.
*
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

#include "systemc.h"
#include "PrintMacros.h"
#include "MemorySystem.h"
#include "SCIC.h"
#include "assert.h"



#if (USE_SCIC_MEMSYS_QUERY == 1)
extern unsigned NUM_DEVICES;
extern unsigned NUM_RANKS;
extern uint BL;
#endif

#ifndef _SIM_
int SHOW_SIM_OUTPUT = 1;
ofstream visDataOut; //mostly used in MemoryController
#endif
//////////////////////////////////////////////////////////////////////////////// 
//
// Method:    runSystem
// FullName:  SCIC::runSystem
// Access:    public 
// Returns:   void
//
// Descriptions - SCIC's SystemC thread.
// 
//////////////////////////////////////////////////////////////////////////////
void SCIC::runSystem()
{
	bool bIssued	= false;
	resetDataOut();
	wait(SC_ZERO_TIME);

	while(true)
	{
		if(_pLegacyMemorySystem == NULL)
		{
			//
			// Since there is no module for emulating system c interface, this thread yields a control to other process until 
			// legacy memory system assigned.
			//
			//wait(SC_ZERO_TIME);
			wait();
			continue;
		}

		//
		// clear signal
		//
		if(_prtCA->read() == true)
		{
			_prtCA->write(false);
			resetDataOut();
		}

		//
		// CPU issued an I/O requests to BIU
		//
		if(_prtDE->read() == true)
		{
			TransactionType type	= _prtWE->read() ? DATA_WRITE : DATA_READ;

			uint64_t	*	pBuffer = NULL;

#ifndef		NO_STORAGE
			if(type == DATA_WRITE)
			{
				pBuffer = getDataFromPort();
				if(pBuffer != NULL)
				{
					SCIC_PRINT("DIN[0] : 0x" << hex << pBuffer[0] << " ADDR: 0x" << hex <<_prtDA->read());
				}
				else
				{
					//
					// A CPU model use no storage trace or input without no_stroage option.
					// Based what original DRAMSim use, SCIC handles this one to avoid 
					// a memory leak on scoreboard.
					//
					_bUserDataHandlingFault = true;
				}
			}
			else
			{
				//
				// Typically, if there is a request which is pending, 
				// no CPU commit the same request until memory system releases the request.
				// Just in case, however, if an internal buffer was allocated already for same address,
				// SCIC allocates no storage for this transaction. 
				//
				ScoreBoard::iterator entry = _scoreBoard.find(_prtDA->read());
				if(entry != _scoreBoard.end() && entry->second.WasWrite() != true)
				{
					pBuffer = entry->second.AllocatedMem();
					entry->second.IncRefCnt();
				}
				else
				{
					pBuffer = new uint64_t[4];
					ScoreBoardElement memElement(pBuffer, (type == DATA_WRITE) ? true : false);
					_scoreBoard.insert(make_pair(_prtDA->read(), memElement));
				}
			}

#endif
			

			Transaction trans(type, _prtDA->read(), (void *)pBuffer, _prtReqTransID->read());
			//DEBUG("SC Memory System received address :" << std::hex << trans.address << std::dec);

			alignTransactionAddress(trans);
			bIssued	= _pLegacyMemorySystem->addTransaction(trans);

			if(bIssued == false)
			{
				//
				// Transaction queue is full
				//
				_prtRB->write(true);

			}
		}


#if (USE_CALIBRATION_1CYCLE != 1)
		//
		// No matter what, the this system interface provides clock signal to memory system.
		//
		_pLegacyMemorySystem->update();
		_nCurrentClk++;
#else
		if(_bInitialTime == false)
		{
			//
			// No matter what, the this system interface provides clock signal to memory system.
			//
			_pLegacyMemorySystem->update();
			_nCurrentClk++;

		}
		else
		{
			_bInitialTime = false;
		}
#endif
		wait();
	}
}



void power_callback(double a, double b, double c, double d)
{
	//	printf("power callback: %0.3f, %0.3f, %0.3f, %0.3f\n",a,b,c,d);
}




//////////////////////////////////////////////////////////////////////////////// 
//
// Method:    AttachLegacyMemorySystem
// FullName:  SCIC::AttachLegacyMemorySystem
// Access:    public 
// Returns:   void
// Parameter: MemorySystem * pMemorySystem
//
// Descriptions - attach legacy memory system (DRAMSim instance0
// The SCIC should provide SystemC interface for this instance, so a CPU model,
// or user have to attach the memory system first before running sc_main
// 
//////////////////////////////////////////////////////////////////////////////
void SCIC::AttachLegacyMemorySystem(MemorySystem *pMemorySystem)
{
	if(_pLegacyMemorySystem != NULL)
	{
		ERROR("SystemC Interface has already legacy memory system");
		exit(0);
	}
	else if(pMemorySystem == NULL)
	{
		ERROR("Memory System that you want to attach is not available");
		exit(0);		
	}
	else
	{
		_pLegacyMemorySystem = pMemorySystem;

		//
		// register callbacks
		//
		Callback_t *pReadCallback		= new Callback_4Param<SCIC, void, uint, uint64_t, uint64_t, uint64_t>(this, &SCIC::readComplete);
		Callback_t *pWriteCallback		= new Callback_4Param<SCIC, void, uint, uint64_t, uint64_t, uint64_t>(this, &SCIC::writeComplete);
#if (USE_SCIC_MEMSYS_QUERY == 1)
		CB_HIST *pHistgramCallback	= new Callback_3Param<SCIC, void, uint, uint, uint>(this, &SCIC::measureIndividualLatency);
		_pLegacyMemorySystem->RegisterCallbacks(pReadCallback, pWriteCallback, pHistgramCallback, power_callback);
#else
		_pLegacyMemorySystem->RegisterCallbacks(pReadCallback, pWriteCallback, power_callback);
#endif
		if(_pLegacyMemorySystem->memoryController == NULL)
		{
			ERROR("Memory System that you want to attach is not available");
			exit(0);		
		}

#if (USE_SCIC_MEMSYS_QUERY == 1)
		//
		// build references
		//
		_vctpEnergy[SCIC_ENERGY_BACKGROUND]	= _pLegacyMemorySystem->memoryController->BackgroundEnergy();
		_vctpEnergy[SCIC_ENERGY_BURST]		= _pLegacyMemorySystem->memoryController->BurstEnergy();
		_vctpEnergy[SCIC_ENERGY_PRECHARGE]	= _pLegacyMemorySystem->memoryController->ActpreEnergy();
		_vctpEnergy[SCIC_ENERGY_REFRESH]	= _pLegacyMemorySystem->memoryController->RefreshEnergy();
#endif
	}
}




//////////////////////////////////////////////////////////////////////////////// 
//
// Method:    getDataFromPort
// FullName:  SCIC::getDataFromPort
// Access:    public 
// Returns:   uint64_t *
//
// Descriptions - read data from DIN using a storage. User does not consider
// memory management because it is managed by the scoreboard manager.
// 
//////////////////////////////////////////////////////////////////////////////
uint64_t * SCIC::getDataFromPort()
{
	uint64_t *pBuffer			= NULL;

	if(_prtDIN[0]->read() != NULL_SIG64)
	{
		pBuffer = new uint64_t[4];
		for (int nBuffIdx = 0; nBuffIdx < 4; ++nBuffIdx)
		{
			pBuffer[nBuffIdx] = _prtDIN[nBuffIdx]->read();
		}
	}

	return pBuffer;
}



//////////////////////////////////////////////////////////////////////////////// 
//
// Method:    freeMemoryElement
// FullName:  SCIC::freeMemoryElement
// Access:    public 
// Returns:   void
// Parameter: uint64_t key
// Parameter: bool bWrite
//
// Descriptions - this method will release an entry of scoreboard, which are
// matched with an associated key which comprises a key and type of transaction
// 
//////////////////////////////////////////////////////////////////////////////
void SCIC::freeMemoryElement(uint64_t key, bool bWrite)
{
#ifndef		NO_STORAGE

	ScoreBoard::iterator iter = _scoreBoard.end();
	//
	// For the case that user uses k6 or mase without NO_STORAGE option
	//
	if (bWrite == true && _bUserDataHandlingFault == true )
	{
		return;
	}

	bool bReleased	= false;

	if(_scoreBoard.count(key) == 1 )
	{
		iter= _scoreBoard.find(key);
		iter->second.DecRefCnt();
		if(iter->second.RefCnts() == 0)
		{
			iter->second.Free();
			bReleased = true;
		}
	}
	else
	{
		pair<ScoreBoard::iterator, ScoreBoard::iterator> ret_pair = _scoreBoard.equal_range(key);

		for(iter = ret_pair.first; iter != ret_pair.second; ++iter)
		{
			if(iter->second.WasWrite() == bWrite)
			{
				iter->second.DecRefCnt();
				if(iter->second.RefCnts() == 0)
				{
					iter->second.Free();
					bReleased = true;
				}
				break;
			}
		}
	}

	if(iter != _scoreBoard.end() && bReleased == true)
	{
		_scoreBoard.erase(iter);
	}

#endif
}

//////////////////////////////////////////////////////////////////////////////// 
//
// Method:    setDataOutPort
// FullName:  SCIC::setDataOutPort
// Access:    public 
// Returns:   void
// Parameter: uint64_t key
//
// Descriptions - write data to DOUTs based on the provided key
// The key is a memory address used in a request transaction.
// 
//////////////////////////////////////////////////////////////////////////////
void SCIC::setDataOutPort(uint64_t key)
{
#ifndef		NO_STORAGE
	if (key == NULL_SIG64)
	{
		for(int nBuffIdx = 0; nBuffIdx < 4; nBuffIdx++)
		{
			_prtDOUT[nBuffIdx]->write(NULL_SIG64);
		}		
	}
	else
	{
		uint64_t	*pBuffer = NULL;

		//
		// look up scoreboard.
		//
		if(_scoreBoard.count(key) == 1)
		{
				pBuffer = _scoreBoard.find(key)->second.AllocatedMem();
		}
		else
		{
			pair<ScoreBoard::iterator, ScoreBoard::iterator> ret_pair = _scoreBoard.equal_range(key);
			for(ScoreBoard::iterator iter = ret_pair.first; iter != ret_pair.second; ++iter)
			{
				if(iter->second.WasWrite() == false)
				{
					pBuffer = iter->second.AllocatedMem();
					break;
				}

			}
		}

		if(pBuffer == NULL)
		{
			ERROR("get lost of allocated internal stroage");
			assert(pBuffer != NULL);
		}

		SCIC_PRINT("DOUT[0] : 0x" << hex << pBuffer[0] << " ADDR: 0x" << hex << key);
		for(int nBuffIdx = 0; nBuffIdx < 4; nBuffIdx++)
		{
			_prtDOUT[nBuffIdx]->write(pBuffer[nBuffIdx]);
		}

	}
#else
	for(int nBuffIdx = 0; nBuffIdx < 4; nBuffIdx++)
	{
		_prtDOUT[nBuffIdx]->write(NULL_SIG64);
	}
#endif
}



//////////////////////////////////////////////////////////////////////////////// 
//
// Method:    initializeInterface
// FullName:  SCIC::initializeInterface
// Access:    public 
// Returns:   void
//
// Descriptions - Initialize Internal States
// 
//////////////////////////////////////////////////////////////////////////////
void SCIC::initializeInterface()
{
	_pLegacyMemorySystem	= NULL;
	_bUserDataHandlingFault = false;
	_bInitialTime	= true;

#if (USE_SCIC_MEMSYS_QUERY == 1)
	_nCycleTracker = NULL_SIG64;
    _vctRtLatencyReport = vector<uint64_t>(NUM_RANKS*NUM_BANKS,0);
#endif

}


void SCIC::Reset()
{
    if(_pLegacyMemorySystem == NULL)
    {
        throw "SCIC::Reset:: Invalid Pointer for MemorySystem instance";
    }
#if (USE_SCIC_MEMSYS_QUERY == 1)
    if(!((NUM_RANKS > 0) && (NUM_BANKS >0)) ) { throw "SCIC::Reset:: Invalid RANK / BANK Configuration - must be > 0";};
	_nCycleTracker = NULL_SIG64;
        _vctRtLatencyReport = vector<uint64_t>(NUM_RANKS*NUM_BANKS,0);
#endif
    //initializeInterface();
    resetDataOut();
}


//////////////////////////////////////////////////////////////////////////////// 
//
// Method:    readComplete
// FullName:  SCIC::readComplete
// Access:    public 
// Returns:   void
// Parameter: uint nSystemId
// Parameter: uint64_t nTargatAddr
// Parameter: uint64_t nClockCycle
// Parameter: uint64_t nTransID
//
// Descriptions - read completion (This method releases scoreboard entry associated with the callback request.
// 
//////////////////////////////////////////////////////////////////////////////
void SCIC::readComplete(uint nSystemId, uint64_t nTargatAddr, uint64_t nClockCycle, uint64_t nTransID)
{
	_prtRB->write(false);
	_prtAO->write(nTargatAddr);
	_prtWO->write(false);
	_prtCA->write(true);
	_prtRespTransID->write(nTransID);
	m_ptrCompAckEvent->notify(SC_ZERO_TIME);

	setDataOutPort(nTargatAddr);

#if (USE_CALIBRATION_1CYCLE == 1)
	wait(SC_ZERO_TIME);
#endif

#if (USE_SCIC_MEMSYS_QUERY == 1)
	_nCycleTracker = nClockCycle;
#endif

	freeMemoryElement(nTargatAddr, false);
}



//////////////////////////////////////////////////////////////////////////////// 
//
// Method:    writeComplete
// FullName:  SCIC::writeComplete
// Access:    public 
// Returns:   void
// Parameter: uint nSystemId
// Parameter: uint64_t nTargatAddr
// Parameter: uint64_t nClockCycle
// Parameter: uint64_t nTransID
//
// Descriptions - callback for write completion.
// 
//////////////////////////////////////////////////////////////////////////////
void SCIC::writeComplete(uint nSystemId, uint64_t nTargatAddr, uint64_t nClockCycle, uint64_t nTransID) 
{
	_prtRB->write(false);
	_prtAO->write(nTargatAddr);
	_prtWO->write(true);
	_prtCA->write(true);
	m_ptrCompAckEvent->notify(SC_ZERO_TIME);
	_prtRespTransID->write(nTransID);

	setDataOutPort(NULL_SIG64);

#if (USE_SCIC_MEMSYS_QUERY == 1)
	_nCycleTracker = nClockCycle;
#endif

#if (USE_CALIBRATION_1CYCLE == 1)
	wait(SC_ZERO_TIME);
#endif
	freeMemoryElement(nTargatAddr, true);
}


void SCIC::alignTransactionAddress(Transaction &trans)
{
	// zero out the low order bits which correspond to the size of a transaction

	unsigned throwAwayBits = dramsim_log2((BL*JEDEC_DATA_BUS_BITS/8));

	trans.address >>= throwAwayBits;
	trans.address <<= throwAwayBits;
}





//////////////////////////////////////////////////////////////////////////
// Function-style inquiry methods for memory status
// These method enable to get latency and bandwidth for each memory request at real-time
//
// For compatibility to future version of DRAMSim, 
// I recommend you to disable USE_SCIC_MEMSYS_QUERY preprocessor.
// If you need to use these methods to get statistics from memory, 
// you is asked to port code fragments surrounded by the USE_SCIC_MEMSYS_QUERY macro
// to new version of DRAMSim.
//////////////////////////////////////////////////////////////////////////
#if (USE_SCIC_MEMSYS_QUERY == 1)


//////////////////////////////////////////////////////////////////////////////// 
//
// Method:    measureIndividualLatency
// FullName:  SCIC::measureIndividualLatency
// Access:    public 
// Returns:   void
// Parameter: uint nLatency
// Parameter: uint nRank
// Parameter: uint nBank
//
// Descriptions - 
// This method measures latency for each memory instruction.
// This is not going to elapse whole memory latencies for running. 
// Instead, this method elapses latency for each rank and bank from the last point
// that you queried latency information using GetLatencyandMarkTimepoint().
// 
//////////////////////////////////////////////////////////////////////////////
void SCIC::measureIndividualLatency(uint nLatency, uint nRank, uint nBank)
{
	_vctRtLatencyReport[SEQUENTIAL(nRank, nBank)] += nLatency; 
}




//////////////////////////////////////////////////////////////////////////////// 
//
// Method:    GetLatencyandMarkTimepoint
// FullName:  SCIC::GetLatencyandMarkTimepoint
// Access:    public 
// Returns:   uint64_t
// Parameter: vector<uint64_t> & perfInfo
//
// Descriptions -
// 
//////////////////////////////////////////////////////////////////////////////
uint64_t SCIC::GetLatencyandMarkTimepoint(vector<uint64_t> &perfInfo)
{
	uint64_t nLatency = 0;

	for (size_t i=0;i<NUM_RANKS;i++)
	{
		perfInfo[i] = 0;
		for (size_t j=0; j<NUM_BANKS; j++)
		{
			perfInfo[i] += _vctRtLatencyReport[i + j];
			nLatency += perfInfo[i];
			//
			// mark a time point
			//
			_vctRtLatencyReport[i + j] = 0;
		}
	}
	
	return nLatency;
}


//////////////////////////////////////////////////////////////////////////////// 
//
// Method:    GetNumsElapsedIo
// FullName:  SCIC::GetNumsElapsedIo
// Access:    public 
// Returns:   uint64_t
// Parameter: vector<uint64_t> & perfInfo
// Parameter: SCIC_STAT_QUERY queryType
//
// Descriptions -
// The information regarding the nums of transaction requests is available for each bank and rank.
// For identifying available performance class that you can measure using this, please check 
// SCIC_STAT_QUERY enum values.
// If a user want to go over specific data for each bank, the user should iterate the vector information based on the number of rank * bank.
// Otherwise, the user can query the information, which be elapsed by all banks' data.
// The simple way addressing a rank is # of ranks * NUMS_RANKS + # of bank.
// (The size of the vector should be NUMS_RANKS * NUMS_BANKS)
// 
//////////////////////////////////////////////////////////////////////////////
uint64_t SCIC::GetNumsElapsedIo(vector<uint64_t> &perfInfo, SCIC_STAT_QUERY queryType)
{
	vector<uint64_t> &vctIoPerBank = (queryType == SCIC_STAT_READ) ? _pLegacyMemorySystem->memoryController->TotalReadsPerBank(): _pLegacyMemorySystem->memoryController->TotalWritesPerBank();
	uint64_t nNumsIos = 0;
	
	for (size_t i=0;i<NUM_RANKS;i++)
	{
		perfInfo[i] = 0;
		for (size_t j=0; j<NUM_BANKS; j++)
		{
			perfInfo[i] += vctIoPerBank[SEQUENTIAL(i,j)];
			nNumsIos += perfInfo[i];
		}
	}

	return nNumsIos;
}

//////////////////////////////////////////////////////////////////////////////// 
//
// Method:    GetElapsedPerfromanceInfo
// FullName:  SCIC::GetElapsedPerfromanceInfo
// Access:    public 
// Returns:   double
// Parameter: vector<double> & perfInfo
// Parameter: SCIC_PERF_QUERY queryType
//
// Descriptions -
// Perfromance information is available for each bank and rank.
// For identifying available performance class that you can measure using this, please check 
// SCIC_PERF_QUERY enum values.
// If a user want to go over specific data for each bank, the user should iterate the vector information based on the number of rank * bank.
// Otherwise, the user can query the information, which be elapsed by all banks' data.
// The simple way addressing a rank is # of ranks * NUMS_RANKS + # of bank.
// (The size of the vector should be NUMS_RANKS * NUMS_BANKS)
// 
//////////////////////////////////////////////////////////////////////////////
double SCIC::GetElapsedPerfromanceInfo(vector<double> &perfInfo, SCIC_PERF_QUERY queryType)
{
	double		secondsThisEpoch	= (double)_pLegacyMemorySystem->currentClockCycle * tCK * 1E-9;
	uint		bytesPerTransaction = (64*BL)/8;
	double		nTotalPerfInfo		= 0;
	
	vector<uint64_t> &vctTotalEpochLatency	= _pLegacyMemorySystem->memoryController->TotalEpochLatency();
	vector<uint64_t> &vctTotalReadPerBank	= _pLegacyMemorySystem->memoryController->TotalReadsPerBank();
	vector<uint64_t> &vctTotalWritePerBank	= _pLegacyMemorySystem->memoryController->TotalWritesPerBank();

	if(queryType == SCIC_PERF_LATENCY)
	{
		for (size_t i=0;i<NUM_RANKS;i++)
		{
			for (size_t j=0; j<NUM_BANKS; j++)
			{
				perfInfo[SEQUENTIAL(i,j)] = ((double)vctTotalEpochLatency[SEQUENTIAL(i,j)] / (double)(vctTotalReadPerBank[SEQUENTIAL(i,j)])) * tCK;
				nTotalPerfInfo += perfInfo[SEQUENTIAL(i,j)];
			}
		}
	}
	else if (queryType == SCIC_PERF_BANDWIDTH)
	{
		for (size_t i=0;i<NUM_RANKS;i++)
		{
			for (size_t j=0; j<NUM_BANKS; j++)
			{
				perfInfo[SEQUENTIAL(i,j)] = (((double)(vctTotalReadPerBank[SEQUENTIAL(i,j)]+vctTotalWritePerBank[SEQUENTIAL(i,j)]) * (double)bytesPerTransaction)/(1024.0*1024.0*1024.0)) / secondsThisEpoch;
				nTotalPerfInfo += perfInfo[SEQUENTIAL(i,j)];
			}
		}
	}

	return nTotalPerfInfo;
}



//////////////////////////////////////////////////////////////////////////////// 
//
// Method:    GetElapsedEnergyInfo
// FullName:  SCIC::GetElapsedEnergyInfo
// Access:    public 
// Returns:   double
// Parameter: vector<double> & energyInfos
// Parameter: SCIC_ENERGY_QUERY queryType
//
// Descriptions - 
// Energy information will be delivered trhough energyInfos vector;
// users can specify enerygy type that they want using SCIC_ENERGY_QUERY enum value.
// (for example, SCIC_ENERGY_BACKGROUND means bacgrkound energy, SCIC_ENERGY_BURST means burst energy and so on.)
// The energy information (Watts) is accumulated for each rank; therefore
// users should be aware the number of rank.

// If a user want to go over specific data for each rank, the user should iterate the vector information based on the number of rank.
// Otherwise, the user can query the information, which be elapsed by all ranks' data.
// (The size of the vector should be NUMS_RANKS)
// 
//////////////////////////////////////////////////////////////////////////////
double SCIC::GetElapsedEnergyInfo(vector<double> &energyInfos, SCIC_ENERGY_QUERY queryType)
{
	double	 nTotalEnergyInfo	=	0;
	uint64_t cyclesElapsed		= (_pLegacyMemorySystem->currentClockCycle % EPOCH_LENGTH == 0) ? EPOCH_LENGTH : _pLegacyMemorySystem->currentClockCycle % EPOCH_LENGTH;

	if(queryType == SCIC_ENERGY_AVERAGE)	
	{
		for (size_t i=0;i<NUM_RANKS;i++)
		{
			//
			// The average latency will be different compared to original DRAMSim because
			// the DRAMSim miscalculate average poewr (there are two redundant burst poewr used)
			//
			energyInfos[i] = 0;
			for (int nTypeIdx = 0; nTypeIdx < SCIC_NUMS_ENERGY_TYPE; ++nTypeIdx)
			{
				energyInfos[i] += (double)((*(_vctpEnergy[nTypeIdx]))[i]);
			}
			energyInfos[i] = (energyInfos[i] / (double)(cyclesElapsed)) * Vdd / 1000.0;
			nTotalEnergyInfo += energyInfos[i];
		}
	}
	else {
		for (size_t i=0;i<NUM_RANKS;i++)
		{
			energyInfos[i] = ((double)((*(_vctpEnergy[queryType]))[i]) / (double)(cyclesElapsed)) * Vdd / 1000.0;
			nTotalEnergyInfo += energyInfos[i];
		}
	}

	return nTotalEnergyInfo;
}
#endif

uint64_t SCIC::GetTotalNumsTransactions()
{
	return _pLegacyMemorySystem->memoryController->TotalTransactions();
}

uint SCIC::GetBytePerTransaction() 
{
	return (64*BL)/8;
}
