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

#ifndef _SCIC_h__
#define _SCIC_h__

#include "MemorySystem.h"
#include <systemc.h>

#ifndef NULL_SIG64
#define		NULL_SIG64		((1 << 64) -1)
#endif

#ifdef	USE_DEBUG_SYSTEMC
#define	SCIC_PRINT(str) std::cout << str << std::endl	
#else
#define	SCIC_PRINT(str)
#endif


#define	USE_CALIBRATION_1CYCLE		(1)


#if (USE_SCIC_MEMSYS_QUERY == 1)
typedef enum {
	SCIC_ENERGY_BACKGROUND,
	SCIC_ENERGY_BURST,
	SCIC_ENERGY_PRECHARGE,
	SCIC_ENERGY_REFRESH,
	SCIC_NUMS_ENERGY_TYPE,
	SCIC_ENERGY_AVERAGE
} SCIC_ENERGY_QUERY;

typedef enum {
	SCIC_PERF_LATENCY,
	SCIC_PERF_BANDWIDTH,
	SCIC_NUMS_PERF_TYPE
} SCIC_PERF_QUERY;

typedef enum {
	SCIC_STAT_READ,
	SCIC_STAT_WRITE
} SCIC_STAT_QUERY;

#ifndef SEQUENTIAL
#define SEQUENTIAL(rank,bank) (rank*NUM_BANKS)+bank
#endif
#endif



SC_MODULE(SCIC)
{
	class ScoreBoardElement {
		uint64_t		*_pAllocatedMem;
		bool			_bWrite;
		unsigned int	_nRefCnts;
	public:

		uint64_t*		AllocatedMem() const						{ return _pAllocatedMem; }
		bool			WasWrite() const							{ return _bWrite; }
		void			Free()										{delete _pAllocatedMem;};
		unsigned int	RefCnts() const { return _nRefCnts; }
		void			RefCnts(unsigned int val) { _nRefCnts = val; }
		void			IncRefCnt() {_nRefCnts++;};
		void			DecRefCnt() {_nRefCnts--;};


		ScoreBoardElement(uint64_t *pAllocatedMem, bool bWrite)		{_pAllocatedMem = pAllocatedMem; bWrite; _nRefCnts = 1;};

	};

	/************************************************************************/
	/* system C primitive                                                   */
	/************************************************************************/
	sc_in<bool>			_prtWE;
	sc_in<uint64_t>		_prtDA;
	//sc_in<bool>			_prtCLK;
	sc_port <sc_signal_in_if<bool> > _prtCLK;
	sc_in<bool>			_prtDE;
	sc_out<uint64_t>	_prtAO;
	sc_out<bool>		_prtWO;
	sc_out<bool>		_prtCA;
	sc_out<uint64_t>	_prtReqTransID;
	sc_out<uint64_t>	_prtRespTransID;
	//
	// RB will be high when the queue in legacy memory does not have any available room.
	//
	sc_out<bool>		_prtRB;
	
	//
	// Data I/O port
	//
	sc_out<uint64_t>	_prtDOUT[4];
	sc_in<uint64_t>		_prtDIN[4];
	sc_event*		m_ptrCompAckEvent;


	/************************************************************************/
	/* internal states                                                      */
	/************************************************************************/
	MemorySystem		*_pLegacyMemorySystem;
	uint64_t			_nCurrentClk;
	bool				_bUserDataHandlingFault;
	bool				_bInitialTime;

#if (USE_SCIC_MEMSYS_QUERY == 1)
	vector<uint64_t>* 	_vctpEnergy[SCIC_NUMS_ENERGY_TYPE];
	vector<uint64_t>	_vctRtLatencyReport;
	//
	// For measuring real-time performance
	// this tracker check interval time between request begins and completes for each memory transaction.
	//
	uint64_t			_nCycleTracker;

#endif
	//
	// For minimizing modification of DRAMSim, this member filed explicitly helps to manage memory resource. 
	// The key of scoreboard is destination address 
	//
	typedef multimap<uint64_t, ScoreBoardElement> ScoreBoard;		
	ScoreBoard		_scoreBoard;


    /************************************************************************/
    /* private member   
       Even though users don't need to care about these functions starting
       from small letter (private member), For compatibility to SystemC kernel
       , I do not use private keyword here*/
    /************************************************************************/
    void			runSystem();
	uint64_t*		getDataFromPort();
	void			setDataOutPort(uint64_t key);
	void			resetDataOut() {setDataOutPort(NULL_SIG64);}
	void			freeMemoryElement(uint64_t key, bool bWrite);
	void			alignTransactionAddress(Transaction &trans);
	void			initializeInterface();
	void			readComplete(uint nSystemId, uint64_t nTargatAddr, uint64_t nClockCycle, uint64_t nTransID);
	void			writeComplete(uint nSystemId, uint64_t nTargatAddr, uint64_t nClockCycle, uint64_t nTransID);
  
    /************************************************************************/
    /* public                                                               */
    /************************************************************************/    
    void			AttachLegacyMemorySystem(MemorySystem *pMemorySystem);    // methods for compatibility with DRAMSim and a CPU model
    void            Reset();


#if (USE_SCIC_MEMSYS_QUERY == 1)
	uint64_t		GetLatencyandMarkTimepoint(vector<uint64_t> &perfInfo);
	uint64_t		GetNumsElapsedIo(vector<uint64_t> &perfInfo, SCIC_STAT_QUERY queryType);
	double			GetElapsedPerfromanceInfo(vector<double> &perfInfo, SCIC_PERF_QUERY queryType);
	double			GetElapsedEnergyInfo(vector<double> &energyInfos, SCIC_ENERGY_QUERY queryType);
	uint64_t		GetTotalNumsTransactions();
	uint			GetBytePerTransaction();

	//
	// callback, this is not exported to public
	//
	void		    measureIndividualLatency(uint nLatency, uint nRank, uint nBank);
#endif

	SC_CTOR(SCIC)
	{
		initializeInterface();

		SC_THREAD(runSystem);
		//
		// double data rate
		//
		sensitive << _prtCLK;
	}
};

#endif // _SCIC_h__

