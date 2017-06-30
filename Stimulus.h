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
#ifndef _Stimulus_h__
#define _Stimulus_h__

#ifndef NULL_SIG64
#define		NULL_SIG64		((1 << 64) -1)
#define     NULL_SIG(_nBit) ((1 << _nBit) -1)
#endif


#define     RETURN_VAL(_class, _major , _minor)		(((uint32_t)((_class) & 0x00000001) << 31) |\
                                                     ((uint32_t)((_major) & 0x00007FFF) << 16) | \
                                                     (uint32_t)((_minor) & 0x0000FFFF))


#define     STIMUL_VAL                              (0)
#define     STIMUL_SUCCESS                          RETURN_VAL(STIMUL_VAL, 0, 0)
#define     STIMUL_ERROR                            RETURN_VAL(STIMUL_VAL, 0x1, 0)
#define     STIMUL_ERROR_DRAM_BUSY                  RETURN_VAL(STIMUL_VAL, 0x1, 0x1)
#define     STIMUL_ERROR_INVALID_BUFFER             RETURN_VAL(STIMUL_VAL, 0x1, 0x2)
#define     STIMUL_ERROR_INVALID_IOTYPE             RETURN_VAL(STIMUL_VAL, 0x1, 0x4)

typedef enum {
    STIMUL_IO_READ,
    STIMUL_IO_WRITE,
    STIMUL_IO_MAXVAL
} STIMUL_IO_TYPE;


#define		UNIT_TEST_SEQUENTIAL_DATA				(0)


typedef     void (*GETRESP_PF)(uint32_t nTransId, uint32_t nAddr, uint32_t nData, STIMUL_IO_TYPE nIoType);

SC_MODULE(Stimulus)
{
	/************************************************************************/
	/* SYSTEM C PORTS                                                       */
	/************************************************************************/
	//
	// ports for out
	//
	sc_out<bool>		_prtWE;
	sc_out<uint64_t>	_prtAO;
	sc_out<bool>		_prtDE;
	sc_out<uint64_t>	_prtDataOut[4];

	//
	// port for in
	//
	sc_in<bool>			_prtCLK;
	sc_in<bool>			_prtRB;
	sc_in<uint64_t>		_prtAI;
	sc_in<bool>			_prtCA;
	sc_in<bool>			_prtWI;
	sc_in<uint64_t>		_prtDataIn[4];


	/************************************************************************/
	/* INTERNAL STATE                                                       */
	/************************************************************************/
	uint64_t			_nClockCycle;
	ifstream			*_ptraceFile;
	uint64_t			_nReadBuffer[4];
    uint64_t			_nWriteBuffer[4];
	TraceType			_traceType;
    GETRESP_PF          _pfCallback;

    std::map<uint32_t, uint32_t>    _writeTransIdMap;  // address, trans id

#if (UNIT_TEST_SEQUENTIAL_DATA == 1)
	uint64_t			_nAddrKeeper;
	bool				_bWrite;
#endif

    /************************************************************************/
    /* public                                                               */
    /************************************************************************/
    void        Initialize();
	void	    AttachTracefile(ifstream *pfStream, TraceType traceType) { _ptraceFile = pfStream; _traceType = traceType;};
    void        RegisterCallback(GETRESP_PF pfCallback);
    uint32_t    IssueRequest(uint32_t nTransId, uint32_t nAddr, uint32_t nData, STIMUL_IO_TYPE nIoType);
    void        Reset();


    /************************************************************************/
    /* private member   
       Even though users don't need to care about these functions starting
       from small letter (private member), For compatibility to SystemC kernel
       , I do not use private keyword here*/
    /************************************************************************/
    void        run();
	void	    sendIo( uint64_t nDestAddr, TransactionType transType, void * pBuffer ); 
	void	    setDataOutPort(uint64_t *pData);
	void	    resetDataOutPort();
	void	    getDataPort(uint64_t *pData);
	void	    idleState();
	void*	    parseTraceFileLine(string &line, uint64_t &addr, TransactionType &transType, uint64_t &clockCycle, TraceType type);
#if (UNIT_TEST_SEQUENTIAL_DATA == 1)
	void	    verifyData();
	bool	    sequentialIo(uint64_t nMaxAddrTest);
#endif

    SC_CTOR(Stimulus) : _nClockCycle(0)
    {
        Initialize();

        SC_THREAD(run);
        sensitive << _prtCLK;
    }
};



#endif // _Stimulus_h__

