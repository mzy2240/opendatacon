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

  /* The out station port is connected to the Overall System Scada master, so the master thinks it is talking to an outstation.
   This code then fires off events to the connector, which the connected master port(s) (of some type DNP3/ModBus/MD3) will turn back into scada commands and send out to the "real" Outstation.
   So it makes sense to connect the SIM (which generates data) to a DNP3 Outstation which will feed the data back to the SCADA master.
   So an Event to an outstation will be data that needs to be sent up to the scada master.
   An event from an outstation will be a master control signal to turn something on or off.
  */
#include <iostream>
#include <future>
#include <regex>
#include <chrono>
#include <asiopal/UTCTimeSource.h>
#include <opendnp3/outstation/IOutstationApplication.h>
#include "MD3OutstationPort.h"
#include "LogMacro.h"
#include "MD3.h"
#include "CRC.h"

#include <opendnp3/LogLevels.h>

MD3OutstationPort::MD3OutstationPort(std::string aName, std::string aConfFilename, const Json::Value aConfOverrides) :
	MD3Port(aName, aConfFilename, aConfOverrides),
	pOutstation(nullptr)
{
}

MD3OutstationPort::~MD3OutstationPort()
{
	// in DNP3Port - but dont think we use. ChannelStateSubscriber::Unsubscribe(this);
	Disable();
	//	if (mb != nullptr)
	//		MD3_free(mb);
	//	if (mb_mapping != nullptr)
	//		MD3_mapping_free(mb_mapping);
}

void MD3OutstationPort::Enable()
{
	if (enabled)
		return;
	if (nullptr == pOutstation)
	{
		LOG("DNP3OutstationPort", openpal::logflags::ERR, "", Name + ": Port not configured.");
		return;
	}
	pOutstation->Enable();
	enabled = true;

	PublishEvent(ConnectState::PORT_UP, 0);
}
void MD3OutstationPort::Disable()
{
	if (!enabled)
		return;
	enabled = false;

	pOutstation->Disable();
}

void MD3OutstationPort::OnLinkDown()
{
	if (!link_dead)
	{
		link_dead = true;
		PublishEvent(ConnectState::DISCONNECTED, 0);
	}
}

TCPClientServer MD3OutstationPort::ClientOrServer()
{
	MD3PortConf* pConf = static_cast<MD3PortConf*>(this->pConf.get());
	if (pConf->mAddrConf.ClientServer == TCPClientServer::DEFAULT)
		return TCPClientServer::SERVER;
	return pConf->mAddrConf.ClientServer;
}

void MD3OutstationPort::BuildOrRebuild(IOManager& IOMgr, openpal::LogFilters& LOG_LEVEL)
{
	MD3PortConf* pConf = static_cast<MD3PortConf*>(this->pConf.get());

	pChannel = GetChannel(IOMgr);

	if (pChannel == nullptr)
	{
		LOG("MD3OutstationPort", openpal::logflags::ERR, "", Name + ": Channel not found for outstation.");
		return;
	}

	// We want these methods on the channel - which talk to the physical layer
	// Throw and exception if no physical layer
	pChannel->BeginWrite(const RSlice& arBuffer);	// Make sure the data exists until the OnSuccess callback
	pChannel->SetHandler(IPhysicalLayerCallbacks* apHandler); // BUt this might overwrite the handler that OpenDNP3 uses to reconnect - need to trace it through the code
	// There is a PhsyicalLayerMonitor class that does this.

	// Cheap console logger!! Remove later
	pChannel->AddStateListener([](ChannelState state)
	{
		std::cout << "channel state: " << ChannelStateToString(state) << std::endl;
	});

	// TODO: SJE Now we have to hook the pChannel would just like access to something that we could write to or calls us when we have data.

	//pOutstation = pChannel->AddOutstation(Name.c_str(), *this, *this, StackConfig);
	//ChannelStateSubscriber::Subscribe(this, pChannel);

	//Allocate memory for bits, input bits, registers, and input registers */
/*	mb_mapping = MD3_mapping_new(pConf->pPointConf->BitIndicies.Total(),
									pConf->pPointConf->InputBitIndicies.Total(),
									pConf->pPointConf->RegIndicies.Total(),
									pConf->pPointConf->InputRegIndicies.Total());
	if (mb_mapping == NULL)
 {
		LOG("MD3OutstationPort", openpal::logflags::ERR, "", Name + ": Failed to allocate the MD3 register mapping: "); // +std::string(MD3_strerror(errno));
		//TODO: should this throw an exception instead of return?
		return;
	}*/
}

//Similar to the command below, but this one is just asking if something is supported.
//At the moment, I assume we respond based on how we are configured (controls and data points) and dont wait to see what happens down the line.
template<typename T>
inline CommandStatus MD3OutstationPort::SupportsT(T& arCommand, uint16_t aIndex)
{
	if (!enabled)
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

// We are going to send a command to the opendatacon connector to do some kind of operation.
// If there is a master on that connector it will then send the command on down to the "real" outstation.
// This method will be called in response to data appearing on our TCP connection.
// TODO: SJE The question is, how do we respond up the line - do we need to wait for a response from down the line first?
template<typename T>
inline CommandStatus MD3OutstationPort::PerformT(T& arCommand, uint16_t aIndex)
{
	if (!enabled)
		return CommandStatus::UNDEFINED;

	auto future_results = PublishCommand(arCommand, aIndex);

	for (auto& future_result : future_results)
	{
		//if results aren't ready, we'll try to do some work instead of blocking
		while (future_result.wait_for(std::chrono::milliseconds(0)) == std::future_status::timeout)
		{
			//not ready - let's lend a hand to speed things up
			this->pIOS->poll_one();
		}
		//first one that isn't a success, we can return
		if (future_result.get() != CommandStatus::SUCCESS)
			return CommandStatus::UNDEFINED;
	}

	return CommandStatus::SUCCESS;
}

std::future<CommandStatus> MD3OutstationPort::Event(const Binary& meas, uint16_t index, const std::string& SenderName) { return EventT(meas, index, SenderName); }
std::future<CommandStatus> MD3OutstationPort::Event(const DoubleBitBinary& meas, uint16_t index, const std::string& SenderName) { return EventT(meas, index, SenderName); }
std::future<CommandStatus> MD3OutstationPort::Event(const Analog& meas, uint16_t index, const std::string& SenderName) { return EventT(meas, index, SenderName); }
std::future<CommandStatus> MD3OutstationPort::Event(const Counter& meas, uint16_t index, const std::string& SenderName) { return EventT(meas, index, SenderName); }
std::future<CommandStatus> MD3OutstationPort::Event(const FrozenCounter& meas, uint16_t index, const std::string& SenderName) { return EventT(meas, index, SenderName); }
std::future<CommandStatus> MD3OutstationPort::Event(const BinaryOutputStatus& meas, uint16_t index, const std::string& SenderName) { return EventT(meas, index, SenderName); }
std::future<CommandStatus> MD3OutstationPort::Event(const AnalogOutputStatus& meas, uint16_t index, const std::string& SenderName) { return EventT(meas, index, SenderName); }

int find_index(const std::vector<uint32_t> &aCollection, uint16_t index)
{
	for (auto group : aCollection)
		if (group == index)
			return (int)index;
	return -1;
}

// We received a change in data from an Event (from the opendatacon Connector) now store it so that it can be produced when the Scada master polls us
// for a group or iindividually on our TCP connection.
// What we return here is not used in anyway that I can see.

template<typename T>
inline std::future<CommandStatus> MD3OutstationPort::EventT(T& meas, uint16_t index, const std::string& SenderName)
{
	auto cmd_promise = std::promise<CommandStatus>();

	if (!enabled)
	{
		cmd_promise.set_value(CommandStatus::UNDEFINED);
		return cmd_promise.get_future();
	}

	MD3PortConf* pConf = static_cast<MD3PortConf*>(this->pConf.get());

	if (std::is_same<T, Analog>::value)
	{
		int map_index = find_index(pConf->pPointConf->AnalogIndicies, index);
//		if (map_index >= 0)
			//	*(mb_mapping->tab_input_registers + map_index) = (uint16_t)meas.value;
	}
	else if (std::is_same<T, Binary>::value)
	{
		int map_index = find_index(pConf->pPointConf->BinaryIndicies, index);
	//	if (map_index >= 0)
			//		*(mb_mapping->tab_input_bits + index) = (uint8_t)meas.value;
	}
	//TODO: impl other types

	cmd_promise.set_value(CommandStatus::UNDEFINED);
	return cmd_promise.get_future();
}

