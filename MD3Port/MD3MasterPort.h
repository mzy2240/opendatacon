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
 * MD3ClientPort.h
 *
 *  Created on: 01/04/2018
 *      Author: Scott Ellis <scott.ellis@novatex.com.au>
 */

#ifndef MD3MASTERPORT_H_
#define MD3MASTERPORT_H_

#include <queue>
#include <utility>
#include <opendnp3/master/ISOEHandler.h>
#include <opendatacon/ASIOScheduler.h>

#include "MD3.h"
#include "MD3Engine.h"
#include "MD3Port.h"


/*
template <typename T, typename F>
class capture_impl
{
    T x;
    F f;
public:
    capture_impl( T && x, F && f )
    : x{std::forward<T>(x)}, f{std::forward<F>(f)}
    {}

    template <typename ...Ts> auto operator()( Ts&&...args )
    -> decltype(f( x, std::forward<Ts>(args)... ))
    {
        return f( x, std::forward<Ts>(args)... );
    }

    template <typename ...Ts> auto operator()( Ts&&...args ) const
    -> decltype(f( x, std::forward<Ts>(args)... ))
    {
        return f( x, std::forward<Ts>(args)... );
    }
};

template <typename T, typename F>
capture_impl<T,F> capture( T && x, F && f )
{
    return capture_impl<T,F>(
                             std::forward<T>(x), std::forward<F>(f) );
}*/


class MD3MasterPort: public MD3Port
{
public:
	MD3MasterPort(std::string aName, std::string aConfFilename, const Json::Value aConfOverrides);
	~MD3MasterPort() override;

	void Enable() override;
	void Disable() override;

	void BuildOrRebuild(IOManager& IOMgr, openpal::LogFilters& LOG_LEVEL) override;

	void SocketStateHandler(bool state);

	void SetAllPointsQualityToCommsLost();
	void SendAllPointEvents();

	uint8_t CalculateBinaryQuality(bool enabled);
	uint8_t CalculateAnalogQuality(bool enabled, uint16_t meas);

	// Implement some IOHandler - parent MD3Port implements the rest to return NOT_SUPPORTED
	std::future<CommandStatus> Event(const ControlRelayOutputBlock& arCommand, uint16_t index, const std::string& SenderName) override;
	std::future<CommandStatus> Event(const AnalogOutputInt16& arCommand, uint16_t index, const std::string& SenderName) override;
	std::future<CommandStatus> Event(const AnalogOutputInt32& arCommand, uint16_t index, const std::string& SenderName) override;
	std::future<CommandStatus> Event(const AnalogOutputFloat32& arCommand, uint16_t index, const std::string& SenderName) override;
	std::future<CommandStatus> Event(const AnalogOutputDouble64& arCommand, uint16_t index, const std::string& SenderName) override;
	std::future<CommandStatus> ConnectionEvent(ConnectState state, const std::string& SenderName) override;
	template<typename T> std::future<CommandStatus> EventT(T& arCommand, uint16_t index, const std::string& SenderName);


private:
//	template<class T>
//	CommandStatus WriteObject(const T& command, uint16_t index);

	void DoPoll(uint32_t pollgroup);
	void ProcessMD3Message(std::vector<MD3BlockData>& CompleteMD3Message);

	void HandleError(int errnum, const std::string& source);
	CommandStatus HandleWriteError(int errnum, const std::string& source);

	std::unique_ptr<ASIOScheduler> PollScheduler;
	void ProcessAnalogUnconditionalReturn(MD3BlockFormatted & Header, std::vector<MD3BlockData>& CompleteMD3Message);
	void ProcessAnalogDeltaScaReturn(MD3BlockFormatted & Header, std::vector<MD3BlockData>& CompleteMD3Message);

};

#endif /* MD3MASTERPORT_H_ */
