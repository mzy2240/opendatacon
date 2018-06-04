/*	opendatacon
*
*	Copyright (c) 2018:
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
* MD3MasterPort.cpp
*
*  Created on: 01/04/2018
*      Author: Scott Ellis <scott.ellis@novatex.com.au>
*/


#include <opendnp3/LogLevels.h>
#include <thread>
#include <chrono>
#include <array>
#include <opendnp3/app/MeasurementTypes.h>

#include "MD3.h"
#include "MD3Engine.h"
#include "MD3MasterPort.h"


MD3MasterPort::MD3MasterPort(std::string aName, std::string aConfFilename, const Json::Value aConfOverrides) :
	MD3Port(aName, aConfFilename, aConfOverrides),
	PollScheduler(nullptr)
{}

MD3MasterPort::~MD3MasterPort()
{
	Disable();
	//TODO: SJE Remove any connections that reference this Master so they cant be accessed!
}

void MD3MasterPort::Enable()
{
	if (enabled) return;
	try
	{
		if (pConnection.get() == nullptr)
			throw std::runtime_error("Connection manager uninitilised");

		pConnection->Open();	// Any outstation can take the port down and back up - same as OpenDNP operation for multidrop

		enabled = true;
	}
	catch (std::exception& e)
	{
		LOG("DNP3OutstationPort", openpal::logflags::ERR, "", "Problem opening connection : " + Name + " : " + e.what());
		return;
	}
}
void MD3MasterPort::Disable()
{
	if (!enabled) return;
	enabled = false;

	if (pConnection.get() == nullptr)
		return;
	pConnection->Close(); // Any outstation can take the port down and back up - same as OpenDNP operation for multidrop
}

// Have to fire the SocketStateHandler for all other OutStations sharing this socket.
void MD3MasterPort::SocketStateHandler(bool state)
{
	std::string msg;
	if (state)
	{
		PollScheduler->Start();
		PublishEvent(ConnectState::CONNECTED, 0);
		msg = Name + ": Connection established.";
	}
	else
	{
		PollScheduler->Stop();

		SetAllPointsQualityToCommsLost();	// All the connected points need their quality set to comms lost.

		PublishEvent(ConnectState::DISCONNECTED, 0);
		msg = Name + ": Connection closed.";
	}
	LOG("MD3OutstationPort", openpal::logflags::INFO, "", msg);
}

void MD3MasterPort::BuildOrRebuild(IOManager& IOMgr, openpal::LogFilters& LOG_LEVEL)
{
	//TODO: Do we re-read the conf file - so we can do a live reload? - How do we kill all the sockets and connections properly?
	std::string ChannelID = MyConf()->mAddrConf.IP + ":" + std::to_string(MyConf()->mAddrConf.Port);

	if (PollScheduler == nullptr)
		PollScheduler.reset(new ASIOScheduler(*pIOS));

	pMasterCommandQueue.reset(new StrandProtectedQueue<MasterCommandQueueItem>(*pIOS, 256));	// If we get more than 256 commands in the queue we have a problem.

	pConnection = MD3Connection::GetConnection(ChannelID); //Static method

	if (pConnection == nullptr)
	{
		pConnection.reset(new MD3Connection(pIOS, isServer, MyConf()->mAddrConf.IP,
			std::to_string(MyConf()->mAddrConf.Port), this, true, MyConf()->TCPConnectRetryPeriodms));	// Retry period cannot be different for multidrop outstations

		MD3Connection::AddConnection(ChannelID, pConnection);	//Static method
	}

	pConnection->AddMaster(MyConf()->mAddrConf.OutstationAddr,
		std::bind(&MD3MasterPort::ProcessMD3Message, this, std::placeholders::_1),
		std::bind(&MD3MasterPort::SocketStateHandler, this, std::placeholders::_1));

	PollScheduler->Stop();
	PollScheduler->Clear();
	for (auto pg : MyPointConf()->PollGroups)
	{
		auto id = pg.second.ID;
		auto action = [=]() {
			this->DoPoll(id);
		};
		PollScheduler->Add(pg.second.pollrate, action);
	}
	//	PollScheduler->Start(); Is started and stopped in the socket state handler
}

// Modbus code
void MD3MasterPort::HandleError(int errnum, const std::string& source)
{
	std::string msg = Name + ": " + source + " error: '";// +MD3_strerror(errno) + "'";
	auto log_entry = openpal::LogEntry("MD3MasterPort", openpal::logflags::WARN,"", msg.c_str(), -1);
	pLoggers->Log(log_entry);

	// If not a MD3 error, tear down the connection?
//    if (errnum < MD3_ENOBASE)
//    {
//        this->Disconnect();

//        MD3PortConf* pConf = static_cast<MD3PortConf*>(this->pConf.get());

//        // Try and re-connect if a persistent connection
//        if (pConf->mAddrConf.ServerType == server_type_t::PERSISTENT)
//        {
//            //try again later
//            pTCPRetryTimer->expires_from_now(std::chrono::seconds(5));
//            pTCPRetryTimer->async_wait(
//                                       [this](asio::error_code err_code)
//                                       {
//                                           if(err_code != asio::error::operation_aborted)
//                                               this->Connect();
//                                       });
//        }
//    }
}
//Modbus code
CommandStatus MD3MasterPort::HandleWriteError(int errnum, const std::string& source)
{
	HandleError(errnum, source);
	switch (errno)
	{
		/*
		case EMBXILFUN: //return "Illegal function";
			return CommandStatus::NOT_SUPPORTED;
		case EMBBADCRC:  //return "Invalid CRC";
		case EMBBADDATA: //return "Invalid data";
		case EMBBADEXC:  //return "Invalid exception code";
		case EMBXILADD:  //return "Illegal data address";
		case EMBXILVAL:  //return "Illegal data value";
		case EMBMDATA:   //return "Too many data";
			return CommandStatus::FORMAT_ERROR;
		case EMBXSFAIL:  //return "Slave device or server failure";
		case EMBXMEMPAR: //return "Memory parity error";
			return CommandStatus::HARDWARE_ERROR;
		case EMBXGTAR: //return "Target device failed to respond";
			return CommandStatus::TIMEOUT;
		case EMBXACK:   //return "Acknowledge";
		case EMBXSBUSY: //return "Slave device or server is busy";
		case EMBXNACK:  //return "Negative acknowledge";
		case EMBXGPATH: //return "Gateway path unavailable";
			*/
		default:
			return CommandStatus::UNDEFINED;
	}
}

void MD3MasterPort::ProcessMD3Message(std::vector<MD3BlockData> &CompleteMD3Message)
{
	// We know that the address matches in order to get here, and that we are in the correct INSTANCE of this class.
	assert(CompleteMD3Message.size() != 0);

	MD3BlockFormatted Header = CompleteMD3Message[0];

	if (Header.IsMasterToStationMessage() != false)
	{
		LOG("MD3MasterPort", openpal::logflags::ERR, "", "Received a Master to Station message at the Master - ignoring - " + std::to_string(Header.GetFunctionCode()) + " On Station Address - " + std::to_string(Header.GetStationAddress()));
		//TODO: SJE Trip an error so we dont have to wait for timeout?
		return;
	}

//if (Header.GetStationAddress() != ExpectedAddress)
	{

	}
	// Now based on the Command Function, take action.
	// All are included to allow better error reporting.
	switch (Header.GetFunctionCode())
	{
	case ANALOG_UNCONDITIONAL:	// Command and reply
		ProcessAnalogUnconditionalReturn(Header, CompleteMD3Message);
		break;
	case ANALOG_DELTA_SCAN:		// Command and reply
		ProcessAnalogDeltaScaReturn(Header, CompleteMD3Message);
		break;
	case DIGITAL_UNCONDITIONAL_OBS:
	//	DoDigitalUnconditionalObs(Header);
		break;
	case DIGITAL_DELTA_SCAN:
	//	DoDigitalChangeOnly(Header);
		break;
	case HRER_LIST_SCAN:
	//		DoDigitalHRER(static_cast<MD3BlockFn9&>(Header), CompleteMD3Message);
		break;
	case DIGITAL_CHANGE_OF_STATE:
	//	DoDigitalCOSScan(static_cast<MD3BlockFn10&>(Header));
		break;
	case DIGITAL_CHANGE_OF_STATE_TIME_TAGGED:
	//	DoDigitalScan(static_cast<MD3BlockFn11MtoS&>(Header));
		break;
	case DIGITAL_UNCONDITIONAL:
	//	DoDigitalUnconditional(static_cast<MD3BlockFn12MtoS&>(Header));
		break;
	case ANALOG_NO_CHANGE_REPLY:
		// Master Only
		break;
	case DIGITAL_NO_CHANGE_REPLY:
		// Master Only
		break;
	case CONTROL_REQUEST_OK:
		// Master Only
		break;
	case FREEZE_AND_RESET:
	//	DoFreezeResetCounters(static_cast<MD3BlockFn16MtoS&>(Header));
		break;
	case POM_TYPE_CONTROL:
	//	DoPOMControl(static_cast<MD3BlockFn17MtoS&>(Header), CompleteMD3Message);
		break;
	case DOM_TYPE_CONTROL:
	//	DoDOMControl(static_cast<MD3BlockFn19MtoS&>(Header), CompleteMD3Message);
		break;
	case INPUT_POINT_CONTROL:
		break;
	case RAISE_LOWER_TYPE_CONTROL:
		break;
	case AOM_TYPE_CONTROL:
	//	DoAOMControl(static_cast<MD3BlockFn23MtoS&>(Header), CompleteMD3Message);
		break;
	case CONTROL_OR_SCAN_REQUEST_REJECTED:
		// Master Only
		break;
	case COUNTER_SCAN:
	//	DoCounterScan(Header);
		break;
	case SYSTEM_SIGNON_CONTROL:
	//	DoSystemSignOnControl(static_cast<MD3BlockFn40&>(Header));
		break;
	case SYSTEM_SIGNOFF_CONTROL:
		break;
	case SYSTEM_RESTART_CONTROL:
		break;
	case SYSTEM_SET_DATETIME_CONTROL:
	//	DoSetDateTime(static_cast<MD3BlockFn43MtoS&>(Header), CompleteMD3Message);
		break;
	case FILE_DOWNLOAD:
		break;
	case FILE_UPLOAD:
		break;
	case SYSTEM_FLAG_SCAN:
	//	DoSystemFlagScan(Header, CompleteMD3Message);
		break;
	case LOW_RES_EVENTS_LIST_SCAN:
		break;
	default:
		LOG("MD3MasterPort", openpal::logflags::ERR, "", "Unknown Message Function - " + std::to_string(Header.GetFunctionCode()) + " On Station Address - " + std::to_string(Header.GetStationAddress()));
		break;
	}
	// Send the next command if there is one.
	std::vector<MD3BlockData> NextCommand;
	if (pMasterCommandQueue->sync_front(NextCommand))
	{
		pMasterCommandQueue->sync_pop();
		SendMD3Message(NextCommand);
	}
}

// We have received data from an Analog command - could be  the result of Fn 5 or 6
// Store the decoded data into the point lists. Counter scan comes back in an identical format
void MD3MasterPort::ProcessAnalogUnconditionalReturn(MD3BlockFormatted & Header, std::vector<MD3BlockData>& CompleteMD3Message)
{
	uint8_t ModuleAddress = Header.GetModuleAddress();
	uint8_t Channels = Header.GetChannels();

	int NumberOfDataBlocks = Channels / 2 + Channels % 2;	// 2 --> 1, 3 -->2

	if (NumberOfDataBlocks != CompleteMD3Message.size() - 1)
	{
		LOG("MD3MasterPort", openpal::logflags::ERR, "", "Received a message with the wrong number of blocks - ignoring - " + std::to_string(Header.GetFunctionCode()) + " On Station Address - " + std::to_string(Header.GetStationAddress()));
		//TODO: SJE Trip an error so we dont have to wait for timeout?
		return;
	}

	// Unload the analog values from the blocks
	std::vector<uint16_t> AnalogValues;
	int ChanCount = 0;
	for (int i = 0; i < NumberOfDataBlocks; i++)
	{
		AnalogValues.push_back(CompleteMD3Message[i + 1].GetFirstWord());
		ChanCount++;

		// The last block may only have one reading in it. The second might be filler.
		if (ChanCount < Channels)
		{
			AnalogValues.push_back(CompleteMD3Message[i + 1].GetSecondWord());
			ChanCount++;
		}
	}

	// Now take the returned values and store them into the points
	uint16_t wordres = 0;

	// Search to see if the first value is a counter or analog
	bool FirstModuleIsCounterModule = GetCounterValueUsingMD3Index(ModuleAddress, 0, wordres);

	for (int i = 0; i < Channels; i++)
	{
		// Code to adjust the ModuleAddress and index if the first module is a counter module (8 channels)
		// 16 channels will cover two counters or one counter and 1/2 an analog, or one analog (16 channels).
		// We assume that Analog and Counter modules cannot have the same module address - which we think is a safe assumption.
		int idx = FirstModuleIsCounterModule ? i % 8 : i;
		int maddress = (FirstModuleIsCounterModule && i > 8) ? ModuleAddress+1 : ModuleAddress;

		if (SetAnalogValueUsingMD3Index(maddress, idx, AnalogValues[i]))
		{
			// We have succeeded in setting the value
			//TODO: Trigger an event update on this analog value through ODC if value is 0x8000 - change quality value
		}
		else if (SetCounterValueUsingMD3Index(maddress, idx, AnalogValues[i]))
		{
			// We have succeeded in setting the value
			//TODO: Trigger an event update on this counter value through ODC if value is 0x8000 - change quality value
		}
		else
		{
			LOG("MD3MasterPort", openpal::logflags::ERR, "", "Failed to set an Analog or Counter Value - " + std::to_string(Header.GetFunctionCode())
				+ " On Station Address - " + std::to_string(Header.GetStationAddress())
				+ " Module : " + std::to_string(maddress) + " Channel : " + std::to_string(idx));
			//TODO: SJE Trip an error so we dont have to wait for timeout?
		}
	}
}


void MD3MasterPort::ProcessAnalogDeltaScaReturn(MD3BlockFormatted & Header, std::vector<MD3BlockData>& CompleteMD3Message)
{
}
/*void MD3OutstationPort::GetAnalogModuleValues(AnalogCounterModuleType IsCounterOrAnalog, int Channels, int ModuleAddress, MD3OutstationPort::AnalogChangeType & ResponseType, std::vector<uint16_t> & AnalogValues, std::vector<int> & AnalogDeltaValues)
{
	for (int i = 0; i < Channels; i++)
	{
		uint16_t wordres = 0;
		int deltares = 0;
		bool foundentry = GetAnalogValueAndChangeUsingMD3Index(ModuleAddress, i, wordres, deltares);

		if (IsCounterOrAnalog == CounterModule)
		{
			foundentry = GetCounterValueAndChangeUsingMD3Index(ModuleAddress, i, wordres, deltares);
		}

		if (!foundentry)
		{
			// Point does not exist - need to send analog unconditional as response.
			ResponseType = AllChange;
			AnalogValues.push_back(0x8000);			// Magic value
			AnalogDeltaValues.push_back(0);
		}
		else
		{
			AnalogValues.push_back(wordres);
			AnalogDeltaValues.push_back(deltares);

			if (abs(deltares) > 127)
			{
				ResponseType = AllChange;
			}
			else if (abs(deltares > 0) && (ResponseType != AllChange))
			{
				ResponseType = DeltaChange;
			}
		}
	}
}
*/
//TODO: We will need a strand to make sure we are only sending (and waiting) for one command response pair at a time
// We could have a poll below, along with a set time command that is passed through ODC trying to execute at the same time.
// Do we just build a queue of commands and lambda call backs to handle when we get data back. Then we just process these one by one.
// It eliminates the issue with needing strand protection. May need the ability to flush the queue at some point....
// Only issue is if we do a broadcast message and can get information back from multiple sources... These commands are probably not used?

// We will be called at the appropriate time to trigger an Unconditional or Delta scan
// For digital scans there are two formats we might use. Set in the conf file.
void MD3MasterPort::DoPoll(uint32_t pollgroup)
{
	if(!enabled) return;

	if (MyPointConf()->PollGroups[pollgroup].polltype == AnalogPoints)
	{
		if (MyPointConf()->PollGroups[pollgroup].UnconditionalRequired)
		{
			// Use Unconditional Request Fn 5
			// So now need to work out the start Module address and the number of channels to scan (can be Analog or Counter)
			// For an analog, only one command to get the maximum of 16 channels. For counters it might be two modules that we can get with one command.

			ModuleMapType::iterator mait = MyPointConf()->PollGroups[pollgroup].ModuleAddresses.begin();

			// Request Analog Unconditional, Station 0x7C, Module 0x20, 16 Channels
			int ModuleAddress = mait->first;
			int channels = 16;	// Most we can get in one command
			MD3BlockFormatted commandblock(MyConf()->mAddrConf.OutstationAddr, true, ANALOG_UNCONDITIONAL,ModuleAddress, channels, true);

			QueueMasterCommand(commandblock);
		}
		else
		{
			// Use a delta command Fn 6
		}
	}

	if (MyPointConf()->PollGroups[pollgroup].polltype == BinaryPoints)
	{
		if (NewDigitalCommands)	// Old are 7,8,9,10 - New are 11 and 12
		{
			if (MyPointConf()->PollGroups[pollgroup].UnconditionalRequired)
			{
				// Use Unconditional Request Fn 12
			}
			else
			{
				// Use a delta command Fn 11
			}
		}
		else
		{
			if (MyPointConf()->PollGroups[pollgroup].UnconditionalRequired)
			{
				// Use Unconditional Request Fn 7
			}
			else
			{
				// Use a delta command Fn 8
			}
		}
	}
}

void MD3MasterPort::SetAllPointsQualityToCommsLost()
{
	// Loop through all Binary points.
	for (auto const &Point : MyPointConf()->BinaryODCPointMap)
	{
		int index = Point.first;
		PublishEvent(BinaryQuality::COMM_LOST, index);
	}
	// Analogs
	for (auto const &Point : MyPointConf()->AnalogODCPointMap)
	{
		int index = Point.first;
		PublishEvent(AnalogQuality::COMM_LOST, index);
	}
	// Counters
	for (auto const &Point : MyPointConf()->CounterODCPointMap)
	{
		int index = Point.first;
		PublishEvent(CounterQuality::COMM_LOST, index);
	}
	// Binary Control/Output
	for (auto const &Point : MyPointConf()->BinaryControlODCPointMap)
	{
		int index = Point.first;
		PublishEvent(BinaryOutputStatusQuality::COMM_LOST, index);
	}
}

// When a new device connects to us through ODC (or an exisiting one reconnects), send them everything we currently have.
void MD3MasterPort::SendAllPointEvents()
{
	//TODO: SJE Set a quality of RESTART if we have just started up but not yet received information for a point. Not sure if super usefull...

	// Quality of ONLINE means the data is GOOD.
	for (auto const &Point : MyPointConf()->BinaryODCPointMap)
	{
		int index = Point.first;
		uint8_t meas = Point.second->Binary;
		uint8_t qual = CalculateBinaryQuality(enabled);
		PublishEvent(Binary( meas == 1, qual ),index);	//TODO: SJE Really should have Time as part of this event (bool,  quality, time)
	}
	// Binary Control/Output - the status of which we show as a binary - on our other end we look for the index in both binary lists
	//TODO: SJE Check that we need to report BinaryOutput status, or if it is just assumed?
	for (auto const &Point : MyPointConf()->BinaryControlODCPointMap)
	{
		int index = Point.first;
		uint8_t meas = Point.second->Binary;
		uint8_t qual = CalculateBinaryQuality(enabled);
		PublishEvent(Binary(meas == 1, qual), index);	//TODO: SJE Really should have Time as part of this event (bool,  quality, time)
	}
	// Analogs
	for (auto const &Point : MyPointConf()->AnalogODCPointMap)
	{
		int index = Point.first;
		uint16_t meas = Point.second->Analog;
		// If the measurement is 0x8000 - there is a problem in the MD3 OutStation for that point.
		uint8_t qual = CalculateAnalogQuality(enabled, meas);
		PublishEvent(Analog(meas, qual), index);	//TODO: SJE Really should have Time as part of this event (bool,  quality, time)
	}
	// Counters
	for (auto const &Point : MyPointConf()->CounterODCPointMap)
	{
		int index = Point.first;
		uint16_t meas = Point.second->Analog;
		// If the measurement is 0x8000 - there is a problem in the MD3 OutStation for that point.
		uint8_t qual = CalculateAnalogQuality(enabled, meas);
		PublishEvent(Counter(meas, qual), index);	//TODO: SJE Really should have Time as part of this event (bool,  quality, time)
	}
}

// Binary quality only depends on our link status (at the moment) could also be age related?
uint8_t MD3MasterPort::CalculateBinaryQuality(bool enabled)
{
	return (uint8_t)(enabled ? BinaryQuality::ONLINE : BinaryQuality::COMM_LOST);
}
// Use the measument value and if we are enabled to determine what the quality value should be.
uint8_t MD3MasterPort::CalculateAnalogQuality(bool enabled, uint16_t meas)
{
	return (enabled ? ((meas == 0x8000) ? (uint8_t)AnalogQuality::LOCAL_FORCED : (uint8_t)AnalogQuality::ONLINE) : (uint8_t)AnalogQuality::COMM_LOST);
}

// This will be fired by (typically) an MD3OutStation port on the "other" side of the ODC Event bus.
// We should probably send all the points to the Outstation as we dont know what state the OutStation point table will be in.
std::future<CommandStatus> MD3MasterPort::ConnectionEvent(ConnectState state, const std::string& SenderName)
{
	if (!enabled)
	{
		return IOHandler::CommandFutureUndefined();
	}

	//something upstream has connected
	if(state == ConnectState::CONNECTED)
	{
		LOG("MD3MasterPort", openpal::logflags::INFO, "", "Upstream (other side of ODC) port enabled - Triggering sending of current data ");
		// We dont know the state of the upstream data, so send event information for all points.
		SendAllPointEvents();
	}
	else // ConnectState::DISCONNECTED
	{
		// If we were an on demand connection, we would take down the connection . For MD3 we are using persistent connections only.
		// We have lost an ODC connection, so events we send dont go anywhere.

	}

	return IOHandler::CommandFutureSuccess();
}

//Implement some IOHandler - parent MD3Port implements the rest to return NOT_SUPPORTED
std::future<CommandStatus> MD3MasterPort::Event(const ControlRelayOutputBlock& arCommand, uint16_t index, const std::string& SenderName) { return EventT(arCommand, index, SenderName); }
std::future<CommandStatus> MD3MasterPort::Event(const AnalogOutputInt16& arCommand, uint16_t index, const std::string& SenderName) { return EventT(arCommand, index, SenderName); }
std::future<CommandStatus> MD3MasterPort::Event(const AnalogOutputInt32& arCommand, uint16_t index, const std::string& SenderName) { return EventT(arCommand, index, SenderName); }
std::future<CommandStatus> MD3MasterPort::Event(const AnalogOutputFloat32& arCommand, uint16_t index, const std::string& SenderName) { return EventT(arCommand, index, SenderName); }
std::future<CommandStatus> MD3MasterPort::Event(const AnalogOutputDouble64& arCommand, uint16_t index, const std::string& SenderName) { return EventT(arCommand, index, SenderName); }

// So we have received an event, which for the Master will result in a write to the Outstation, so the command is a Binary Output or Analog Output
// see all 5 possible definitions above.
// We will have to translate from the float values to the uint16_t that MD3 actually handles, and then it is only a 12 bit number.
template<typename T>
inline std::future<CommandStatus> MD3MasterPort::EventT(T& arCommand, uint16_t index, const std::string& SenderName)
{
	std::unique_ptr<std::promise<CommandStatus> > cmd_promise{ new std::promise<CommandStatus>() };
	auto cmd_future = cmd_promise->get_future();

	if (!enabled)
	{
		cmd_promise->set_value(CommandStatus::UNDEFINED);
		return cmd_future;
	}

	//	cmd_promise->set_value(WriteObject(arCommand, index));
	/*
	auto lambda = capture( std::move(cmd_promise),
	[=]( std::unique_ptr<std::promise<CommandStatus>> & cmd_promise ) {
	cmd_promise->set_value(WriteObject(arCommand, index));
	} );
	pIOS->post([&](){ lambda(); });
	*/
	return cmd_future;
}

/*
template<>
CommandStatus MD3MasterPort::WriteObject(const ControlRelayOutputBlock& command, uint16_t index)
{
	if (
	      (command.functionCode == ControlCode::NUL) ||
	      (command.functionCode == ControlCode::UNDEFINED)
	      )
	{
		return CommandStatus::FORMAT_ERROR;
	}

	// MD3 function code 0x01 (read coil status)
	MD3ReadGroup<Binary>* TargetRange = GetRange(index);
	if (TargetRange == nullptr) return CommandStatus::UNDEFINED;

	int rc;
	if (
	      (command.functionCode == ControlCode::LATCH_OFF) ||
	      (command.functionCode == ControlCode::TRIP_PULSE_ON)
	      )
	{
//		rc = MD3_write_bit(mb, index, false);
	}
	else
	{
		//ControlCode::PULSE_CLOSE || ControlCode::PULSE || ControlCode::LATCH_ON
//		rc = MD3_write_bit(mb, index, true);
	}

	// If the index is part of a non-zero pollgroup, queue a poll task for the group
	if (TargetRange->pollgroup > 0)
		pIOS->post([=](){ DoPoll(TargetRange->pollgroup); });

//	if (rc == -1) return HandleWriteError(errno, "write bit");
	return CommandStatus::SUCCESS;
}

template<>
CommandStatus MD3MasterPort::WriteObject(const AnalogOutputInt16& command, uint16_t index)
{
	MD3ReadGroup<Binary>* TargetRange = GetRange(index);
	if (TargetRange == nullptr) return CommandStatus::UNDEFINED;

//	int rc = MD3_write_register(mb, index, command.value);

	// If the index is part of a non-zero pollgroup, queue a poll task for the group
	if (TargetRange->pollgroup > 0)
		pIOS->post([=](){ DoPoll(TargetRange->pollgroup); });

//	if (rc == -1) return HandleWriteError(errno, "write register");
	return CommandStatus::SUCCESS;
}

template<>
CommandStatus MD3MasterPort::WriteObject(const AnalogOutputInt32& command, uint16_t index)
{
	MD3ReadGroup<Binary>* TargetRange = GetRange(index);
	if (TargetRange == nullptr) return CommandStatus::UNDEFINED;

//	int rc = MD3_write_register(mb, index, command.value);

	// If the index is part of a non-zero pollgroup, queue a poll task for the group
	if (TargetRange->pollgroup > 0)
		pIOS->post([=](){ DoPoll(TargetRange->pollgroup); });

//	if (rc == -1) return HandleWriteError(errno, "write register");
	return CommandStatus::SUCCESS;
}

template<>
CommandStatus MD3MasterPort::WriteObject(const AnalogOutputFloat32& command, uint16_t index)
{
	MD3ReadGroup<Binary>* TargetRange = GetRange(index);
	if (TargetRange == nullptr) return CommandStatus::UNDEFINED;

//	int rc = MD3_write_register(mb, index, command.value);

	// If the index is part of a non-zero pollgroup, queue a poll task for the group
	if (TargetRange->pollgroup > 0)
		pIOS->post([=](){ DoPoll(TargetRange->pollgroup); });

//	if (rc == -1) return HandleWriteError(errno, "write register");
	return CommandStatus::SUCCESS;
}

template<>
CommandStatus MD3MasterPort::WriteObject(const AnalogOutputDouble64& command, uint16_t index)
{
	MD3ReadGroup<Binary>* TargetRange = GetRange(index);
	if (TargetRange == nullptr) return CommandStatus::UNDEFINED;

//	int rc = MD3_write_register(mb, index, command.value);

	// If the index is part of a non-zero pollgroup, queue a poll task for the group
	if (TargetRange->pollgroup > 0)
		pIOS->post([=](){ DoPoll(TargetRange->pollgroup); });

//	if (rc == -1) return HandleWriteError(errno, "write register");
	return CommandStatus::SUCCESS;
}
*/





