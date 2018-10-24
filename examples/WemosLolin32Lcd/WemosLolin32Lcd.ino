#include <WiFi.h>
#include <SimcomGsmLib.h>
#include <GsmDebugHelpers.h>
#include <OperatorNameHelper.h>
#include <MappingHelpers.h>
#include <SSD1306.h>
#include "Gui.h"

void UpdateBaudRate(int baudRate)
{
	Serial2.end();
	Serial2.begin(baudRate, SERIAL_8N1, 19, 18, false);
}
SimcomGsm gsm(Serial2, UpdateBaudRate);
SSD1306 display(0x3c, 5, 4);
Gui gui(display);

void OnLog(const char* gsmLog)
{
	Serial.print("[GSM]");
	Serial.println(gsmLog);
}

void setup()
{
	gui.init();
	Serial.begin(500000);
	gsm.SetLogCallback(OnLog);
}

void loop()
{
	display.setColor(OLEDDISPLAY_COLOR::BLACK);
	display.fillRect(0, 0, 128, 64);

	if (!gsm.EnsureModemConnected(460800))
	{
		display.drawString(0, 2, "No shield");
		display.display();
		delay(200);
		return;
	}

	int16_t signalQuality;
	BatteryStatus batteryInfo;
	FixedString20 operatorName;
	IncomingCallInfo callInfo;
	SimcomIpState ipStatus;

	if (gsm.GetSignalQuality(signalQuality) == AtResultType::Timeout)
	{
		return;
	}
	if (gsm.GetBatteryStatus(batteryInfo) == AtResultType::Timeout)
	{
		return;
	}
	if (OperatorNameHelper::GetRealOperatorName(gsm, operatorName) == AtResultType::Timeout)
	{
		return;
	}
	if (gsm.GetIncomingCall(callInfo) == AtResultType::Timeout)
	{
		return;
	}

	gsm.GetIpState(ipStatus);


	bool hasCipmux;
	if (gsm.GetCipmux(hasCipmux) == AtResultType::Success)
	{
		if (!hasCipmux)
		{
			Serial.println("Cipmux disabled, attempting to enable");
			if (gsm.SetCipmux(true) == AtResultType::Error)
			{
				Serial.println("Failed to set cipmux");
				gsm.Cipshut();
				gsm.SetCipmux(true);
				
			}
		}
	}

	if (ipStatus == SimcomIpState::PdpDeact)
	{
		gsm.Cipshut();
	}

	if (ipStatus == SimcomIpState::PdpDeact ||
		ipStatus == SimcomIpState::IpInitial ||
		ipStatus == SimcomIpState::IpGprsact ||
		ipStatus == SimcomIpState::IpStart)
	{
		gsm.SetApn("virgin-internet", "", "");
		display.clear();
		display.drawString(0, 0, "Connecting to gprs..");
		display.display();

		gsm.AttachGprs();
	}
	

	GsmIp ipAddress;

	gsm.GetIpAddress(ipAddress);
	ConnectionInfo info;
	if (gsm.GetConnectionInfo(0, info) == AtResultType::Success)
	{
		Serial.printf("Conn info: bearer=%d, ctx=%d,proto=%s  endpoint = [%s:%d] state = [%s]\n", 
			info.Mux, info.Bearer, 
			ProtocolToStr(info.Protocol), info.RemoteAddress.ToString().c_str(), 

			info.Port, ConnectionStateToStr(info.State));

		if (info.State == ConnectionState::Closed || info.State == ConnectionState::Initial)
		{
			gsm.BeginConnect(ProtocolType::Tcp, 0, "conti.ml", 12668);
		}
	}
	else
	{
		Serial.println("Connection info failed");
	}

	GsmRegistrationState gsmRegStatus;
	auto registrationStatus = gsm.GetRegistrationStatus(gsmRegStatus);


	gui.drawBattery(batteryInfo.Percent, batteryInfo.Voltage);
	gui.drawGsmInfo(signalQuality, gsmRegStatus, operatorName);
	gui.DisplayIp(ipAddress);
	gui.DisplayBlinkIndicator();
	gui.DisplayIncomingCall(callInfo);

	display.display();

	gsm.wait(1000);
}
