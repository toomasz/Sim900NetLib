#pragma once

#include "SimcomAtCommands.h"
#include <FixedString.h>
#include <WString.h>
#include "Network/GsmAsyncSocket.h"
#include "Network/SocketManager.h"
#include "GsmLogger.h"
#include "SimcomGsmTypes.h"
#include <vector>

enum class GsmState :uint8_t
{
	Initial,
	NoShield,
	Initializing,
	SimError,
	SearchingForNetwork,
	RegistrationDenied,
	RegistrationUnknown,
	ConnectingToGprs,
	ConnectedToGprs,
	Error
};


class GsmModule
{
	SimcomAtCommands& _gsm;	
	SocketManager _socketManager;
	GsmLogger& _logger;
	FixedString100 _error;
	bool _isInSleepMode;
	void GetStateStringFromProg(char* stateStr, GsmState state)
	{
		strcpy_P(stateStr, (PGM_P)StateToStr(state));
	}
	void ChangeState(GsmState newState)
	{
		if (_state == GsmState::Error)
		{
			return;
		}
		if (_state != newState)
		{
			char inState[20];
			char outState[20];
			GetStateStringFromProg(inState, _state);
			GetStateStringFromProg(outState, newState);
			_logger.Log(F("State changed %s -> %s"), inState, outState);
				
			_state = newState;
			_socketManager.SetIsNetworkAvailable(_state == GsmState::ConnectedToGprs);
			_lastStateChange = millis();
		}
		if (_state == GsmState::NoShield)
		{
			ModuleConnectCount++;			
		}
	}
	GsmState _state;
	bool ReadModemProperties();
	bool RequestSleepIfEnabled();
	bool ExitSleepIfEnabled();
	uint64_t _lastStateChange = 0;
public:
	GsmModule(SimcomAtCommands &gsm);

	
	bool SleepEnabled = false;
	uint16_t TickInterval = 100;
	uint16_t SimStatusInterval = 1000;
	uint16_t GetPropertiesInterval = 1000;
	uint16_t GetTemperatureInterval = 5000;
	char *ApnName;
	char* ApnUser;
	char* ApnPassword;
	int ModuleConnectCount = 0;

	SimcomAtCommands& At()
	{
		return _gsm;
	}
	void OnGsmModuleEvent(GsmModuleEventType eventType);

	FixedStringBase& Error()
	{
		return _error;
	}

	GsmAsyncSocket* CreateSocket(uint8_t mux, ProtocolType protocolType)
	{
		return _socketManager.CreateSocket(mux, protocolType);
	}
	uint64_t BaudRate;
	GsmState GetState()
	{
		return _state;
	}
	uint64_t GetTimeSinceStateChange()
	{
		return millis() - _lastStateChange;
	}
	const __FlashStringHelper* StateToStr(GsmState state);
	int GarbageOnSerialDetected();
	int16_t signalQuality;
	BatteryStatus batteryInfo;
	FixedString20 operatorName;
	IncomingCallInfo callInfo;
	GsmRegistrationState gsmRegStatus;
	GsmIp ipAddress;
	SimcomIpState ipStatus;
	SimState simStatus;
	float Temperature = 0;
	void OnLog(GsmLogCallback onLog);
	void Loop();
	void Wait(uint64_t delayInMs)
	{
		_gsm.wait(delayInMs);
	}
};

