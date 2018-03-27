#include <array>
#include "catchvs.hpp"
#include "Trompeloeil.hpp"
#include <opendnp3/LogLevels.h>
#include <asiodnp3/ConsoleLogger.h>
#include "MD3OutstationPort.h"
#include "MD3MasterPort.h"
#include "MD3Engine.h"

#define SUITE(name) "MD3Tests - " name

const char *conffilename = "MD3Config.conf";

// Serial connection string...
// std::string  JsonSerialOverride = "{ ""SerialDevice"" : "" / dev / ttyUSB0"", ""BaudRate"" : 115200, ""Parity"" : ""NONE"", ""DataBits"" : 8, ""StopBits" : 1, "MasterAddr" : 0, "OutstationAddr" : 1, "ServerType" : "PERSISTENT"}";

// We actually have the conf file here to match the tests it is used in below. We write out to a file (overwrite) on each test so it can be read back in.
const char *conffile = R"001(
{
	// MD3 Test Configuration - Only need to know what to pass, don't actually need to know what it is?
	// What about analog/digital? Need to know that for MD3 Module 0 has special meaning - not sure if Module == Index??

	"IP" : "127.0.0.1",
	"Port" : 20000,
	"MasterAddr" : 0,
	"OutstationAddr" : 124,
	"ServerType" : "PERSISTENT",
	"LinkNumRetry": 4,

	//-------Point conf--------#
	"Binaries" : [{"Index": 20,  "Module" : 33, "Offset" : 0}, {"Range" : {"Start" : 0, "Stop" : 15}, "Module" : 34, "Offset" : 0}],
	"Analogs" : [{"Range" : {"Start" : 0, "Stop" : 15}, "Module" : 32, "Offset" : 0}],
	"BinaryControls" : [{"Range" : {"Start" : 1, "Stop" : 4}, "Module" : 35, "Offset" : 0}]

})001";

namespace UnitTests
{
	// Write out the conf file information about into a file so that it can be read back in by the code.
	void WriteConfFileToCurrentWorkingDirectory()
	{
		std::ofstream ofs(conffilename);
		if (!ofs) REQUIRE("Could not open conffile for writing");

		ofs << conffile;
		ofs.close();
	}
	std::string Response;

	// Have to fish out of the Packet capture some real DataBlock information to feed into this test.
	TEST_CASE(SUITE("MD3CRCTest"))
	{
		const uint32_t data = 0x0F0F0F0F;
		uint32_t res = MD3CRC(data);
		REQUIRE(res == 0x23);
	}

	TEST_CASE(SUITE("MD3BlockClassConstructor1Test"))
	{
		uint8_t stationaddress = 124;
		bool mastertostation = true;
		MD3_FUNCTION_CODE functioncode = ANALOG_UNCONDITIONAL;
		uint8_t moduleaddress = 0x20;
		uint8_t channels = 16;
		bool lastblock = true;
		bool APL = false;
		bool RSF = false;
		bool HCP = false;
		bool DCP = false;

		MD3BlockArray msg = { 0x7C,0x05,0x20,0x0F,0x52 };	// From a packet capture

		MD3Block b = MD3Block::MD3Block(msg);

		REQUIRE(b.GetStationAddress() == stationaddress);
		REQUIRE(b.IsMasterToStationMessage() == mastertostation);
		REQUIRE(b.GetModuleAddress() == moduleaddress);
		REQUIRE(b.GetFunctionCode() == functioncode);
		REQUIRE(b.GetChannels() == channels);
		REQUIRE(b.IsEndOfMessageBlock() == lastblock);
		REQUIRE(b.IsFormattedBlock());
		REQUIRE(b.GetAPL() == APL);
		REQUIRE(b.GetRSF() == RSF);
		REQUIRE(b.GetHCP() == HCP);
		REQUIRE(b.GetDCP() == DCP);
//		REQUIRE(b.CheckSumPasses());
	}
	TEST_CASE(SUITE("MD3BlockClassConstructor2Test"))
	{
		uint8_t stationaddress = 0x33;
		bool mastertostation = false;
		MD3_FUNCTION_CODE functioncode = ANALOG_UNCONDITIONAL;
		uint8_t moduleaddress = 0x30;
		uint8_t channels = 16;
		bool lastblock = true;
		bool APL = false;
		bool RSF = false;
		bool HCP = false;
		bool DCP = false;

		MD3Block b = MD3Block::MD3Block(stationaddress, mastertostation, functioncode, moduleaddress, channels, lastblock, APL, RSF, HCP, DCP);

		REQUIRE(b.GetStationAddress() == stationaddress);
		REQUIRE(b.IsMasterToStationMessage() == mastertostation);
		REQUIRE(b.GetModuleAddress() == moduleaddress);
		REQUIRE(b.GetFunctionCode() == functioncode);
		REQUIRE(b.GetChannels() == channels);
		REQUIRE(b.IsEndOfMessageBlock() == lastblock);
		REQUIRE(b.IsFormattedBlock());
		REQUIRE(b.GetAPL() == APL);
		REQUIRE(b.GetRSF() == RSF);
		REQUIRE(b.GetHCP() == HCP);
		REQUIRE(b.GetDCP() == DCP);
		REQUIRE(b.CheckSumPasses());

		mastertostation = true;
		b = MD3Block::MD3Block(stationaddress, mastertostation, functioncode, moduleaddress, channels, lastblock, APL, RSF, HCP, DCP);

		REQUIRE(b.GetStationAddress() == stationaddress);
		REQUIRE(b.IsMasterToStationMessage() == mastertostation);
		REQUIRE(b.GetModuleAddress() == moduleaddress);
		REQUIRE(b.GetFunctionCode() == functioncode);
		REQUIRE(b.GetChannels() == channels);
		REQUIRE(b.IsEndOfMessageBlock() == lastblock);
		REQUIRE(b.IsFormattedBlock());
		REQUIRE(b.GetAPL() == APL);
		REQUIRE(b.GetRSF() == RSF);
		REQUIRE(b.GetHCP() == HCP);
		REQUIRE(b.GetDCP() == DCP);
		REQUIRE(b.CheckSumPasses());

		lastblock = false;
		b = MD3Block::MD3Block(stationaddress, mastertostation, functioncode, moduleaddress, channels, lastblock, APL, RSF, HCP, DCP);

		REQUIRE(b.GetStationAddress() == stationaddress);
		REQUIRE(b.IsMasterToStationMessage() == mastertostation);
		REQUIRE(b.GetModuleAddress() == moduleaddress);
		REQUIRE(b.GetFunctionCode() == functioncode);
		REQUIRE(b.GetChannels() == channels);
		REQUIRE(b.IsEndOfMessageBlock() == lastblock);
		REQUIRE(b.IsFormattedBlock());
		REQUIRE(b.GetAPL() == APL);
		REQUIRE(b.GetRSF() == RSF);
		REQUIRE(b.GetHCP() == HCP);
		REQUIRE(b.GetDCP() == DCP);
		REQUIRE(b.CheckSumPasses());

		channels = 1;
		b = MD3Block::MD3Block(stationaddress, mastertostation, functioncode, moduleaddress, channels, lastblock, APL, RSF, HCP, DCP);

		REQUIRE(b.GetStationAddress() == stationaddress);
		REQUIRE(b.IsMasterToStationMessage() == mastertostation);
		REQUIRE(b.GetModuleAddress() == moduleaddress);
		REQUIRE(b.GetFunctionCode() == functioncode);
		REQUIRE(b.GetChannels() == channels);
		REQUIRE(b.IsEndOfMessageBlock() == lastblock);
		REQUIRE(b.IsFormattedBlock());
		REQUIRE(b.GetAPL() == APL);
		REQUIRE(b.GetRSF() == RSF);
		REQUIRE(b.GetHCP() == HCP);
		REQUIRE(b.GetDCP() == DCP);
		REQUIRE(b.CheckSumPasses());

		moduleaddress = 0xFF;
		b = MD3Block::MD3Block(stationaddress, mastertostation, functioncode, moduleaddress, channels, lastblock, APL, RSF, HCP, DCP);

		REQUIRE(b.GetStationAddress() == stationaddress);
		REQUIRE(b.IsMasterToStationMessage() == mastertostation);
		REQUIRE(b.GetModuleAddress() == moduleaddress);
		REQUIRE(b.GetFunctionCode() == functioncode);
		REQUIRE(b.GetChannels() == channels);
		REQUIRE(b.IsEndOfMessageBlock() == lastblock);
		REQUIRE(b.IsFormattedBlock());
		REQUIRE(b.GetAPL() == APL);
		REQUIRE(b.GetRSF() == RSF);
		REQUIRE(b.GetHCP() == HCP);
		REQUIRE(b.GetDCP() == DCP);
		REQUIRE(b.CheckSumPasses());

		stationaddress = 0x7F;
		b = MD3Block::MD3Block(stationaddress, mastertostation, functioncode, moduleaddress, channels, lastblock, APL, RSF, HCP, DCP);

		REQUIRE(b.GetStationAddress() == stationaddress);
		REQUIRE(b.IsMasterToStationMessage() == mastertostation);
		REQUIRE(b.GetModuleAddress() == moduleaddress);
		REQUIRE(b.GetFunctionCode() == functioncode);
		REQUIRE(b.GetChannels() == channels);
		REQUIRE(b.IsEndOfMessageBlock() == lastblock);
		REQUIRE(b.IsFormattedBlock());
		REQUIRE(b.GetAPL() == APL);
		REQUIRE(b.GetRSF() == RSF);
		REQUIRE(b.GetHCP() == HCP);
		REQUIRE(b.GetDCP() == DCP);
		REQUIRE(b.CheckSumPasses());

		stationaddress = 0x01;
		moduleaddress = 0x01;
		APL = true;
		b = MD3Block::MD3Block(stationaddress, mastertostation, functioncode, moduleaddress, channels, lastblock, APL, RSF, HCP, DCP);

		REQUIRE(b.GetStationAddress() == stationaddress);
		REQUIRE(b.IsMasterToStationMessage() == mastertostation);
		REQUIRE(b.GetModuleAddress() == moduleaddress);
		REQUIRE(b.GetFunctionCode() == functioncode);
		REQUIRE(b.GetChannels() == channels);
		REQUIRE(b.IsEndOfMessageBlock() == lastblock);
		REQUIRE(b.IsFormattedBlock());
		REQUIRE(b.GetAPL() == APL);
		REQUIRE(b.GetRSF() == RSF);
		REQUIRE(b.GetHCP() == HCP);
		REQUIRE(b.GetDCP() == DCP);
		REQUIRE(b.CheckSumPasses());

		RSF = true;
		b = MD3Block::MD3Block(stationaddress, mastertostation, functioncode, moduleaddress, channels, lastblock, APL, RSF, HCP, DCP);

		REQUIRE(b.GetStationAddress() == stationaddress);
		REQUIRE(b.IsMasterToStationMessage() == mastertostation);
		REQUIRE(b.GetModuleAddress() == moduleaddress);
		REQUIRE(b.GetFunctionCode() == functioncode);
		REQUIRE(b.GetChannels() == channels);
		REQUIRE(b.IsEndOfMessageBlock() == lastblock);
		REQUIRE(b.IsFormattedBlock());
		REQUIRE(b.GetAPL() == APL);
		REQUIRE(b.GetRSF() == RSF);
		REQUIRE(b.GetHCP() == HCP);
		REQUIRE(b.GetDCP() == DCP);
		REQUIRE(b.CheckSumPasses());

		HCP = true;
		b = MD3Block::MD3Block(stationaddress, mastertostation, functioncode, moduleaddress, channels, lastblock, APL, RSF, HCP, DCP);

		REQUIRE(b.GetStationAddress() == stationaddress);
		REQUIRE(b.IsMasterToStationMessage() == mastertostation);
		REQUIRE(b.GetModuleAddress() == moduleaddress);
		REQUIRE(b.GetFunctionCode() == functioncode);
		REQUIRE(b.GetChannels() == channels);
		REQUIRE(b.IsEndOfMessageBlock() == lastblock);
		REQUIRE(b.IsFormattedBlock());
		REQUIRE(b.GetAPL() == APL);
		REQUIRE(b.GetRSF() == RSF);
		REQUIRE(b.GetHCP() == HCP);
		REQUIRE(b.GetDCP() == DCP);
		REQUIRE(b.CheckSumPasses());

		DCP = true;
		b = MD3Block::MD3Block(stationaddress, mastertostation, functioncode, moduleaddress, channels, lastblock, APL, RSF, HCP, DCP);

		REQUIRE(b.GetStationAddress() == stationaddress);
		REQUIRE(b.IsMasterToStationMessage() == mastertostation);
		REQUIRE(b.GetModuleAddress() == moduleaddress);
		REQUIRE(b.GetFunctionCode() == functioncode);
		REQUIRE(b.GetChannels() == channels);
		REQUIRE(b.IsEndOfMessageBlock() == lastblock);
		REQUIRE(b.IsFormattedBlock());
		REQUIRE(b.GetAPL() == APL);
		REQUIRE(b.GetRSF() == RSF);
		REQUIRE(b.GetHCP() == HCP);
		REQUIRE(b.GetDCP() == DCP);
		REQUIRE(b.CheckSumPasses());
	}
	TEST_CASE(SUITE("MD3BlockClassConstructor3Test"))
	{
		uint16_t firstword = 12;
		uint16_t secondword = 32000;
		bool lastblock = true;

		MD3Block b = MD3Block::MD3Block(firstword, secondword, lastblock);

		REQUIRE(b.GetFirstWord() == firstword);
		REQUIRE(b.GetSecondWord() == secondword);
		REQUIRE(b.IsEndOfMessageBlock() == lastblock);
		REQUIRE(!b.IsFormattedBlock());
		REQUIRE(b.CheckSumPasses());

		MD3BlockArray msg = { 0x7C,0x05,0x20,0x0F,0x82 };	// From a packet capture
		b = MD3Block::MD3Block(msg);
		REQUIRE(b.GetBlockData() == 0x7C05200F);
		REQUIRE(!b.IsEndOfMessageBlock());
		REQUIRE(!b.IsFormattedBlock());
	//	REQUIRE(b.CheckSumPasses());
	}
	TEST_CASE(SUITE("AnalogUnconditionalTest1"))
	{
		WriteConfFileToCurrentWorkingDirectory();

		IOManager IOMgr(1);	// The 1 is for concurrency hint - usually the number of cores.
		asio::io_service IOS(1);

		IOMgr.AddLogSubscriber(asiodnp3::ConsoleLogger::Instance()); // send log messages to the console

		auto MD3Port = new  MD3OutstationPort("TestPLC", conffilename, Json::nullValue);

		MD3Port->SetIOS(&IOS);
		MD3Port->BuildOrRebuild(IOMgr, (openpal::LogFilters)opendnp3::levels::NORMAL);

		MD3Port->Enable();

		// Write to the analog registers that we are going to request the values for.
		for (int i = 0; i < 16; i++)
		{
			const odc::Analog a(4096+i+i*0x100);
			auto res = MD3Port->Event(a, i, "TestHarness");

			REQUIRE((res.get() == odc::CommandStatus::SUCCESS));	// The Get will Wait for the result to be set. 1 is defined
		}

		// Request Analog Unconditional, Station 0x7C, Module 0x20, 16 Channels
		std::array<uint8_t, 6> MD3data{ 0x7C, 0x05, 0x20, 0x0F, 0x52, 0x00 };
		asio::streambuf write_buffer;
		std::ostream output(&write_buffer);
		for (const auto& b : MD3data)
			output << b;

		// Call the Event functions to set the MD3 table data to what we are expecting to get back.

		// Hook the output function with a lambda
		// &Response - had to make response global to get access - having trouble with casting...
		MD3Port->SetSendTCPDataFn([](std::string MD3Message) { Response = MD3Message; });

		MD3Port->ReadCompletionHandler(write_buffer);

		const std::string DesiredResult = { (char)0xfc,0x05,0x20,0x0f,0x33,	// Echoed block
								0x10,0x00,0x11,0x01,(char)0x83,			// Channel 0 and 1
								0x12,0x02,0x13,0x03,(char)0x88,			// Channel 2 and 3 etc
								0x14,0x04,0x15,0x05,(char)0x94,
								0x16,0x06,0x17,0x07,(char)0x9f,
								0x18,0x08,0x19,0x09,(char)0xad,
								0x1A,0x0A,0x1B,0x0B,(char)0xa6,
								0x1C,0x0C,0x1D,0x0D,(char)0xba,
								0x1E,0x0E,0x1F,0x0F,(char)0xf1,0x00 };

		REQUIRE(Response == DesiredResult);

		IOMgr.Shutdown();
	}
	TEST_CASE(SUITE("BinaryEventTest"))
	{
		WriteConfFileToCurrentWorkingDirectory();

		WARN("Trying to construct an OutStationPort");

		// The 1 is for concurrency hint - usually the number of cores.
		IOManager IOMgr(1);
		asio::io_service IOS(1);

		IOMgr.AddLogSubscriber(asiodnp3::ConsoleLogger::Instance()); // send log messages to the console

		auto MD3Port = new  MD3OutstationPort("TestPLC", conffilename, Json::nullValue);

		MD3Port->SetIOS(&IOS);
		MD3Port->BuildOrRebuild(IOMgr, (openpal::LogFilters)opendnp3::levels::NORMAL);

		MD3Port->Enable();

		// TEST EVENTS WITH DIRECT CALL
		// Test on a valid binary point
		const odc::Binary b((bool)true);
		const int index = 1;
		auto res = MD3Port->Event(b, index, "TestHarness");

		REQUIRE((res.get() == odc::CommandStatus::SUCCESS));	// The Get will Wait for the result to be set. 1 is defined

		// Test on an undefined binary point. 40 NOT defined in the config text at the top of this file.
		const int index2 = 40;
		auto res2 = MD3Port->Event(b, index2, "TestHarness");
		REQUIRE((res2.get() == odc::CommandStatus::UNDEFINED));	// The Get will Wait for the result to be set. This always returns this value?? Should be Success if it worked...
		// Wait for some period to do something?? Check that the port is open and we can connect to it?

		IOMgr.Shutdown();
	}
}