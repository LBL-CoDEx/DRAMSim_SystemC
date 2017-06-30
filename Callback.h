/****************************************************************************
*	 DRAMSim2: A Cycle Accurate DRAM simulator 
*	 
*	 Copyright (C) 2010   	Elliott Cooper-Balis
*									Paul Rosenfeld 
*									Bruce Jacob
*									University of Maryland
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
#ifndef CALLBACK_H
#define CALLBACK_H

namespace DRAMSim
{

template <typename ReturnT, typename Param1T, typename Param2T,
typename Param3T>
class CallbackBase_3Param
{
public:
	virtual ~CallbackBase_3Param() = 0;
	virtual ReturnT operator()(Param1T, Param2T, Param3T) = 0;
};

template <typename Return, typename Param1T, typename Param2T, typename Param3T>
DRAMSim::CallbackBase_3Param<Return,Param1T,Param2T,Param3T>::~CallbackBase_3Param() {}

template <typename ConsumerT, typename ReturnT,
typename Param1T, typename Param2T, typename Param3T >
class Callback_3Param: public CallbackBase_3Param<ReturnT,Param1T,Param2T,Param3T>
{
private:
	typedef ReturnT (ConsumerT::*PtrMember)(Param1T,Param2T,Param3T);

public:
	Callback_3Param( ConsumerT* const object, PtrMember member) :
			object(object), member(member)
	{
	}

	Callback_3Param( const Callback_3Param<ConsumerT,ReturnT,Param1T,Param2T,Param3T>& e ) :
			object(e.object), member(e.member)
	{
	}

	ReturnT operator()(Param1T param1, Param2T param2, Param3T param3)
	{
		return (const_cast<ConsumerT*>(object)->*member)
		       (param1,param2,param3);
	}

private:

	ConsumerT* const object;
	const PtrMember  member;
};

typedef CallbackBase_3Param <void, uint, uint64_t, uint64_t> TransactionCompleteCB_legacy;
///------------------------------------
// Extend for more parameters

template <typename ReturnT, typename Param1T, typename Param2T,
typename Param3T, typename Param4T>
class CallbackBase_4Param
{
public:
	virtual ~CallbackBase_4Param() = 0;
	virtual ReturnT operator()(Param1T, Param2T, Param3T, Param4T) = 0;
};

template <typename Return, typename Param1T, typename Param2T, typename Param3T, typename Param4T>
DRAMSim::CallbackBase_4Param<Return,Param1T,Param2T,Param3T, Param4T>::~CallbackBase_4Param() {}

template <typename ConsumerT, typename ReturnT,
typename Param1T, typename Param2T, typename Param3T, typename Param4T >
class Callback_4Param: public CallbackBase_4Param<ReturnT,Param1T,Param2T,Param3T, Param4T>
{
private:
	typedef ReturnT (ConsumerT::*PtrMember)(Param1T,Param2T,Param3T, Param4T);

public:
	Callback_4Param( ConsumerT* const object, PtrMember member) :
			object(object), member(member)
	{
	}

	Callback_4Param( const Callback_4Param<ConsumerT,ReturnT,Param1T,Param2T,Param3T, Param4T>& e ) :
			object(e.object), member(e.member)
	{
	}

	ReturnT operator()(Param1T param1, Param2T param2, Param3T param3, Param4T param4)
	{
		return (const_cast<ConsumerT*>(object)->*member)
		       (param1,param2,param3, param4);
	}

private:

	ConsumerT* const object;
	const PtrMember  member;
};

typedef CallbackBase_4Param <void, uint, uint64_t, uint64_t, uint64_t> TransactionCompleteCB;
} // namespace DRAMSim


#endif
