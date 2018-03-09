/*	opendatacon
 *
 *	Copyright (c) 2014:
 *
 *		DCrip3fJguWgVCLrZFfA7sIGgvx1Ou3fHfCxnrz4svAi
 *		yxeOtDhDCXf1Z4ApgXvX5ahqQmzRfJ2DoX8S05SqHA==
 *
 *	Licensed under the Apache License, Version 2.0 (the "License");
 *	you may not use this file except in compliance with the License.
 *	You may obtain a copy of the License at
 *
 *		http://www.apache.org/licenses/LICENSE-2.0
 *
 *	Unless required by applicable law or agreed to in writing, software
 *	distributed under the License is distributed on an "AS IS" BASIS,
 *	WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *	See the License for the specific language governing permissions and
 *	limitations under the License.
 */
/*
 * MD3OutstationPort.cpp
 *
 *  Created on: 16/10/2014
 *      Author: Alan Murray
 */

#include <iostream>
#include <future>
#include <regex>
#include <chrono>
#include <asiopal/UTCTimeSource.h>
#include <opendnp3/outstation/IOutstationApplication.h>
#include "MD3OutstationPort.h"
#include "LogMacro.h"

#include <opendnp3/LogLevels.h>

MD3OutstationPort::MD3OutstationPort(std::string aName, std::string aConfFilename, const Json::Value aConfOverrides):
	MD3Port(aName, aConfFilename, aConfOverrides)
{}

MD3OutstationPort::~MD3OutstationPort()
{
	Disable();
//	if (mb != nullptr)
//		MD3_free(mb);
//	if (mb_mapping != nullptr)
//		MD3_mapping_free(mb_mapping);
}

void MD3OutstationPort::Enable()
{
	if(enabled) return;
	enabled = true;

	MD3PortConf* pConf = static_cast<MD3PortConf*>(this->pConf.get());

	// Only change stack state if it is a persistent server
	if (pConf->mAddrConf.ServerType == server_type_t::PERSISTENT)
	{
		this->Connect();
	}

}

void MD3OutstationPort::Connect()
{
	if(!enabled) return;
	if (stack_enabled) return;

	stack_enabled = true;

//	if (mb == NULL)
	{
		LOG("MD3OutstationPort", openpal::logflags::ERR,"", Name + ": Connect error: 'MD3 stack failed'");
		return;
	}

/*	int s = MD3_tcp_pi_listen(mb, 1);
	if (s == -1)
	{
		std::string msg = Name+": Connect error: '" + MD3_strerror(errno) + "'";
		auto log_entry = openpal::LogEntry("MD3OutstationPort", openpal::logflags::WARN,"", msg.c_str(), -1);
		pLoggers->Log(log_entry);
		return;
	}

	int r = MD3_tcp_pi_accept(mb, &s);
	if (r == -1)
	{
		std::string msg = Name+": Connect error: '" + MD3_strerror(errno) + "'";
		auto log_entry = openpal::LogEntry("MD3OutstationPort", openpal::logflags::WARN,"", msg.c_str(), -1);
		pLoggers->Log(log_entry);
		return;
	}
	*/
}

void MD3OutstationPort::Disable()
{
	//cancel the retry timers (otherwise it would tie up the io_service on shutdown)
	Disconnect();
	enabled = false;
}

void MD3OutstationPort::Disconnect()
{
	if (!stack_enabled) return;
	stack_enabled = false;

//	if(mb != nullptr) MD3_close(mb);
}

void MD3OutstationPort::StateListener(ChannelState state)
{
	if(!enabled)
		return;

	if(state == ChannelState::OPEN)
	{
		PublishEvent(ConnectState::CONNECTED, 0);
	}
	else
	{
		PublishEvent(ConnectState::DISCONNECTED, 0);
	}
}
void MD3OutstationPort::BuildOrRebuild(IOManager& IOMgr, openpal::LogFilters& LOG_LEVEL)
{
	MD3PortConf* pConf = static_cast<MD3PortConf*>(this->pConf.get());

	std::string log_id;

	if(pConf->mAddrConf.IP != "")
	{
		log_id = "outst_" + pConf->mAddrConf.IP + ":" + std::to_string(pConf->mAddrConf.Port);

		//TODO: collect these on a collection of MD3 tcp connections
//		mb = MD3_new_tcp_pi(pConf->mAddrConf.IP.c_str(), std::to_string(pConf->mAddrConf.Port).c_str());
//		if (mb == NULL)
		{
			std::string msg = Name + ": Stack error: 'MD3 stack creation failed'";
			auto log_entry = openpal::LogEntry("MD3OutstationPort", openpal::logflags::ERR,"", msg.c_str(), -1);
			pLoggers->Log(log_entry);
			//TODO: should this throw an exception instead of return?
			return;
		}
	}
	else if(pConf->mAddrConf.SerialDevice != "")
	{
		log_id = "outst_" + pConf->mAddrConf.SerialDevice;
//		mb = MD3_new_rtu(pConf->mAddrConf.SerialDevice.c_str(),pConf->mAddrConf.BaudRate,(char)pConf->mAddrConf.Parity,pConf->mAddrConf.DataBits,pConf->mAddrConf.StopBits);
//		if (mb == NULL)
		{
			std::string msg = Name + ": Stack error: 'MD3 stack creation failed'";
			auto log_entry = openpal::LogEntry("MD3OutstationPort", openpal::logflags::ERR,"", msg.c_str(), -1);
			pLoggers->Log(log_entry);
			//TODO: should this throw an exception instead of return?
			return;
		}
//		if(MD3_rtu_set_serial_mode(mb,MD3_RTU_RS232))
		{
			std::string msg = Name + ": Stack error: 'Failed to set MD3 serial mode to RS232'";
			auto log_entry = openpal::LogEntry("MD3OutstationPort", openpal::logflags::ERR,"", msg.c_str(), -1);
			pLoggers->Log(log_entry);
			//TODO: should this throw an exception instead of return?
			return;
		}
	}
	else
	{
		std::string msg = Name + ": No IP interface or serial device defined";
		auto log_entry = openpal::LogEntry("MD3OutstationPort", openpal::logflags::ERR,"", msg.c_str(), -1);
		pLoggers->Log(log_entry);
		//TODO: should this throw an exception instead of return?
		return;
	}


	//Allocate memory for bits, input bits, registers, and input registers */
/*	mb_mapping = MD3_mapping_new(pConf->pPointConf->BitIndicies.Total(),
	                                pConf->pPointConf->InputBitIndicies.Total(),
	                                pConf->pPointConf->RegIndicies.Total(),
	                                pConf->pPointConf->InputRegIndicies.Total());
	if (mb_mapping == NULL)
*/	{
		std::string msg = Name + ": Failed to allocate the MD3 register mapping: ";// +std::string(MD3_strerror(errno));
		auto log_entry = openpal::LogEntry("MD3OutstationPort", openpal::logflags::ERR,"", msg.c_str(), -1);
		pLoggers->Log(log_entry);
		//TODO: should this throw an exception instead of return?
		return;
	}
}

template<typename T>
inline CommandStatus MD3OutstationPort::SupportsT(T& arCommand, uint16_t aIndex)
{
	if(!enabled)
		return CommandStatus::UNDEFINED;

	//FIXME: this is meant to return if we support the type of command
	//at the moment we just return success if it's configured as a control
	/*
	    auto pConf = static_cast<MD3PortConf*>(this->pConf.get());
	    if(std::is_same<T,ControlRelayOutputBlock>::value) //TODO: add support for other types of controls (probably un-templatise when we support more)
	    {
	                    for(auto index : pConf->pPointConf->ControlIndicies)
	                            if(index == aIndex)
	                                    return CommandStatus::SUCCESS;
	    }
	*/
	return CommandStatus::NOT_SUPPORTED;
}
template<typename T>
inline CommandStatus MD3OutstationPort::PerformT(T& arCommand, uint16_t aIndex)
{
	if(!enabled)
		return CommandStatus::UNDEFINED;

	auto future_results = PublishCommand(arCommand, aIndex);

	for(auto& future_result : future_results)
	{
		//if results aren't ready, we'll try to do some work instead of blocking
		while(future_result.wait_for(std::chrono::milliseconds(0)) == std::future_status::timeout)
		{
			//not ready - let's lend a hand to speed things up
			this->pIOS->poll_one();
		}
		//first one that isn't a success, we can return
		if(future_result.get() != CommandStatus::SUCCESS)
			return CommandStatus::UNDEFINED;
	}

	return CommandStatus::SUCCESS;
}

std::future<CommandStatus> MD3OutstationPort::Event(const Binary& meas, uint16_t index, const std::string& SenderName){ return EventT(meas, index, SenderName); }
std::future<CommandStatus> MD3OutstationPort::Event(const DoubleBitBinary& meas, uint16_t index, const std::string& SenderName){ return EventT(meas, index, SenderName); }
std::future<CommandStatus> MD3OutstationPort::Event(const Analog& meas, uint16_t index, const std::string& SenderName){ return EventT(meas, index, SenderName); }
std::future<CommandStatus> MD3OutstationPort::Event(const Counter& meas, uint16_t index, const std::string& SenderName){ return EventT(meas, index, SenderName); }
std::future<CommandStatus> MD3OutstationPort::Event(const FrozenCounter& meas, uint16_t index, const std::string& SenderName){ return EventT(meas, index, SenderName); }
std::future<CommandStatus> MD3OutstationPort::Event(const BinaryOutputStatus& meas, uint16_t index, const std::string& SenderName){ return EventT(meas, index, SenderName); }
std::future<CommandStatus> MD3OutstationPort::Event(const AnalogOutputStatus& meas, uint16_t index, const std::string& SenderName){ return EventT(meas, index, SenderName); }

template<typename T>
int find_index (const MD3ReadGroupCollection<T>& aCollection, uint16_t index)
{
	for(auto group : aCollection)
		for(auto group_index = group.start; group_index < group.start + group.count; group_index++)
			if(group_index + group.index_offset == index)
				return (int)group_index;
	return -1;
}

template<typename T>
inline std::future<CommandStatus> MD3OutstationPort::EventT(T& meas, uint16_t index, const std::string& SenderName)
{
	auto cmd_promise = std::promise<CommandStatus>();

	if(!enabled)
	{
		cmd_promise.set_value(CommandStatus::UNDEFINED);
		return cmd_promise.get_future();
	}

	MD3PortConf* pConf = static_cast<MD3PortConf*>(this->pConf.get());

	if(std::is_same<T,Analog>::value)
	{
		int map_index = find_index(pConf->pPointConf->InputRegIndicies, index);
		if (map_index >= 0)
			;//	*(mb_mapping->tab_input_registers + map_index) = (uint16_t)meas.value;
		else
		{
			map_index = find_index(pConf->pPointConf->RegIndicies, index);
			if (map_index >= 0)
				;//		*(mb_mapping->tab_registers + map_index) = (uint16_t)meas.value;
		}
	}
	else if(std::is_same<T,Binary>::value)
	{
		int map_index = find_index(pConf->pPointConf->InputBitIndicies, index);
		if (map_index >= 0)
			;//		*(mb_mapping->tab_input_bits + index) = (uint8_t)meas.value;
		else
		{
			map_index = find_index(pConf->pPointConf->BitIndicies, index);
			if (map_index >= 0)
				;//				*(mb_mapping->tab_bits + index) = (uint8_t)meas.value;
		}
	}
	//TODO: impl other types

	cmd_promise.set_value(CommandStatus::UNDEFINED);
	return cmd_promise.get_future();
}

