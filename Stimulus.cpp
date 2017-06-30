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

#include <iostream>
#include <fstream>
#include <sstream>
#include <getopt.h>
#include <map>
#include <list>

#include "SystemConfiguration.h"
using namespace DRAMSim;

#include "assert.h"
#include "systemc.h"
#include "Transaction.h"
#include "Stimulus.h"

#ifndef NO_STORAGE
#include "BusPacket.h"
#endif



//////////////////////////////////////////////////////////////////////////////// 
//
// Method:    run
// FullName:  Stimulus::run
// Access:    public 
// Returns:   void
//
// Descriptions - A CPU model's SystemC thread
// 
//////////////////////////////////////////////////////////////////////////////
void Stimulus::run() 
{
	resetDataOutPort();
	idleState();

	wait(SC_ZERO_TIME);
	if(_ptraceFile == NULL)
	{
		ERROR("No trace input file")
			exit(0); 
	}

	std::string				line;
	void *					pBuffer;
	TransactionType			transType;
	uint64_t				nDestAddr;
	uint64_t				nSyncClk = 0;

	while(1)
	{
		if(_prtCA->read() == true)
		{
#if (UNIT_TEST_SEQUENTIAL_DATA == 1)
			if(_prtWI->read() == true)
			{
				verifyData();
			}
#else
			getDataPort(_nReadBuffer);
            if(_pfCallback != NULL)
            {
                uint32_t        nTransId   = NULL_SIG(32);

                if (_prtWI->read() == true)
                {
                    std::map<uint32_t, uint32_t>::iterator iterElem = _writeTransIdMap.find(_nReadBuffer[0]);
                    assert(iterElem != _writeTransIdMap.end());
                    nTransId   = iterElem->second;
                    _writeTransIdMap.erase(iterElem);
                }
                else
                {
                    nTransId   = _nReadBuffer[1];
                }
                
                assert(nTransId != NULL_SIG(32));
                _pfCallback(nTransId, _prtAI->read() , _nReadBuffer[0], (_prtWI->read() == true) ? STIMUL_IO_WRITE : STIMUL_IO_READ);
            }
#endif
		}

#if (UNIT_TEST_SEQUENTIAL_DATA == 1)
		if(sequentialIo(4*1024*1024) == false)
		{
			idleState();
		}

#else
        if(_pfCallback == NULL)
        {
            if(_ptraceFile->eof() != true)
            {
                if(_prtRB->read() == false)
                {
                    //
                    // stimulus issues I/O to memory only if the bus interface unit has room
                    //
                    getline(*_ptraceFile, line);

                    if (line.size() > 0)
                    {
                        pBuffer = parseTraceFileLine(line, nDestAddr, transType, nSyncClk, _traceType);

                        //DEBUG("Stimulus issues (address) :" << std::hex << nDestAddr << std::dec);

                        sendIo(nDestAddr, transType, pBuffer);
                    }
                }
            }
            else 
            {
                //
                // there is no trace input to run anymore
                //
                idleState();
            }
        }
#endif

		_nClockCycle++;
		wait();
	}

}


void Stimulus::RegisterCallback(GETRESP_PF pfCallback)
{
    _pfCallback = pfCallback;    
}


uint32_t Stimulus::IssueRequest(uint32_t nTransId, uint32_t nAddr, uint32_t nData, STIMUL_IO_TYPE nIoType)
{
    uint32_t    nResult     = STIMUL_ERROR;
   if(nIoType >= STIMUL_IO_MAXVAL)
   {
       nResult  |= STIMUL_ERROR_INVALID_IOTYPE;
   }
   else if (_prtRB->read() == false)
   {
       nResult  |= STIMUL_ERROR_DRAM_BUSY;
   }
   else
   {
       _nWriteBuffer[0] = nData;
       _nWriteBuffer[1] = nTransId;

       if(nIoType == STIMUL_IO_WRITE)
       {
           _writeTransIdMap.insert(std::make_pair(nAddr, nTransId));
       }

       sendIo(nAddr, (nIoType == STIMUL_IO_READ) ? DATA_READ : DATA_WRITE, (void *)_nWriteBuffer);
       nResult          = STIMUL_SUCCESS;
   }
   return nResult;
}


#if (UNIT_TEST_SEQUENTIAL_DATA == 1)
//////////////////////////////////////////////////////////////////////////////// 
//
// Method:    sequentialIo
// FullName:  Stimulus::sequentialIo
// Access:    public 
// Returns:   bool
// Parameter: uint64_t nMaxAddrTest
//
// Descriptions -
// 
//////////////////////////////////////////////////////////////////////////////
bool Stimulus::sequentialIo(uint64_t nMaxAddrTest)
{
	if(_bWrite == true)
	{
		if(_nAddrKeeper < nMaxAddrTest)
		{
			_nReadBuffer[0] = _nAddrKeeper;
			sendIo(_nAddrKeeper, DATA_WRITE, &_nReadBuffer);
			_nAddrKeeper++;
		}
		else
		{
			_nAddrKeeper = 0;
			_bWrite = true;
		}
	}
	else
	{
		if(_nAddrKeeper < nMaxAddrTest)
		{
			sendIo(_nAddrKeeper, DATA_READ, &_nReadBuffer);
			_nAddrKeeper++;
		}
		else
		{
			return false;
		}

	}

	return true;
}

//////////////////////////////////////////////////////////////////////////////// 
//
// Method:    verifyData
// FullName:  Stimulus::verifyData
// Access:    public 
// Returns:   void
//
// Descriptions -
// 
//////////////////////////////////////////////////////////////////////////////
void Stimulus::verifyData()
{
	getDataPort(_nReadBuffer);	
	if(_prtAI->read() != _nReadBuffer[0])
	{
		std::cerr << "Data mismatch !! write was " << hex << _prtAI->read() << " read data is " << hex << _nReadBuffer[0] << std::endl;
	}

}
#endif

//////////////////////////////////////////////////////////////////////////////// 
//
// Method:    sendIo
// FullName:  Stimulus::sendIo
// Access:    public 
// Returns:   void
// Parameter: uint64_t nDestAddr
// Parameter: TransactionType transType
// Parameter: void * pBuffer
//
// Descriptions -
// 
//////////////////////////////////////////////////////////////////////////////
void Stimulus::sendIo( uint64_t nDestAddr, TransactionType transType, void * pBuffer ) 
{
	_prtAO->write(nDestAddr);
	if(transType == DATA_WRITE)
	{
		_prtWE->write(true);
		if(pBuffer == NULL)
		{
			resetDataOutPort();
		}
		else
		{
			setDataOutPort((uint64_t *)pBuffer);	
		}
	}
	else if(transType == DATA_READ)
	{
		_prtWE->write(false);
		resetDataOutPort();
	}

	//
	// issue I/O transaction
	//
	_prtDE->write(true);
}

//////////////////////////////////////////////////////////////////////////////// 
//
// Method:    idleState
// FullName:  Stimulus::idleState
// Access:    public 
// Returns:   void
//
// Descriptions -
// 
//////////////////////////////////////////////////////////////////////////////
void Stimulus::idleState()
{
	_prtDE->write(false);
}


//////////////////////////////////////////////////////////////////////////////// 
//
// Method:    resetDataOutPort
// FullName:  Stimulus::resetDataOutPort
// Access:    public 
// Returns:   void
//
// Descriptions -
// 
//////////////////////////////////////////////////////////////////////////////
void Stimulus::resetDataOutPort()
{
	for (int buffIdx = 0; buffIdx < 4; ++buffIdx)
	{
		_prtDataOut[buffIdx]->write(NULL_SIG64);
	}
	
}

//////////////////////////////////////////////////////////////////////////////// 
//
// Method:    setDataOutPort
// FullName:  Stimulus::setDataOutPort
// Access:    public 
// Returns:   void
// Parameter: uint64_t * pData
//
// Descriptions -
// 
//////////////////////////////////////////////////////////////////////////////
void Stimulus::setDataOutPort(uint64_t *pData)
{
	assert(pData != NULL);
	for (int buffIdx = 0; buffIdx < 4; ++buffIdx)
	{
		_prtDataOut[buffIdx]->write(pData[buffIdx]);
	}
}


//////////////////////////////////////////////////////////////////////////////// 
//
// Method:    getDataPort
// FullName:  Stimulus::getDataPort
// Access:    public 
// Returns:   void
// Parameter: uint64_t * pData
//
// Descriptions -
// 
//////////////////////////////////////////////////////////////////////////////
void Stimulus::getDataPort(uint64_t *pData)
{
	assert(pData != NULL);
	for (int buffIdx = 0; buffIdx < 4; ++buffIdx)
	{
		pData[buffIdx] = _prtDataIn[buffIdx]->read();
	}
}



//////////////////////////////////////////////////////////////////////////////// 
//
// Method:    parseTraceFileLine
// FullName:  Stimulus::parseTraceFileLine
// Access:    public 
// Returns:   void *
// Parameter: string & line
// Parameter: uint64_t & addr
// Parameter: enum TransactionType & transType
// Parameter: uint64_t & clockCycle
// Parameter: TraceType type
//
// Descriptions -
// 
//////////////////////////////////////////////////////////////////////////////
void * Stimulus::parseTraceFileLine(string &line, uint64_t &addr, enum TransactionType &transType, uint64_t &clockCycle, TraceType type)
{
	size_t previousIndex=0;
	size_t spaceIndex=0;
	uint64_t *dataBuffer = NULL;
	string addressStr="", cmdStr="", dataStr="", ccStr="";

#if (USE_SYSTEM_C_INTERFACE == 1)
	bool useClockCycle = true;
#else
#ifndef _SIM_ 
	bool useClockCycle = false;
#else
	bool useClockCycle = true;
#endif
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

void Stimulus::Initialize()
{
    _pfCallback     = NULL;
#if (UNIT_TEST_SEQUENTIAL_DATA == 1)
    _nAddrKeeper		= 0;
    _bWrite				= false;
#endif
}

void Stimulus::Reset()
{
    Initialize();
    resetDataOutPort();
}




