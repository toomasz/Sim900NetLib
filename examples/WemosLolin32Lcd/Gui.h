#pragma once

#include <SSD1306.h>
#include <SimcomGsmLib.h>

class Gui
{
	SSD1306& _lcd;
public:
	Gui(SSD1306 &lcd):
	_lcd(lcd)
	{

	}
	void init()
	{
		_lcd.init();
		_lcd.setFont(ArialMT_Plain_16);
		_lcd.clear();
		_lcd.resetDisplay();
	}
	void drawBatterySymbol(int percent, float voltage)
	{
		const int width = 12;
		const int height = 25;
		const int batteryPlusHeight = 3;
		const int batteryPlusPadding = 2;

		_lcd.setColor(OLEDDISPLAY_COLOR::BLACK);

		_lcd.fillRect(0, 0, width, height);
		_lcd.setColor(OLEDDISPLAY_COLOR::WHITE);

		// main battery rect
		_lcd.drawRect(0, batteryPlusHeight, width, height - batteryPlusHeight);

		// battery plus rect
		_lcd.drawRect(batteryPlusPadding, 0,
			width - 2 * batteryPlusPadding, batteryPlusHeight + 1);

		_lcd.setColor(OLEDDISPLAY_COLOR::BLACK);
		_lcd.drawHorizontalLine(batteryPlusPadding + 1, batteryPlusHeight,
			width - 2 * (batteryPlusPadding + 1));

		int fillHeightWhen100Percent = height - batteryPlusHeight - 2;

		int batteryUsedInPercent = 100 - percent;

		int batteryLeftPixels = ((double)batteryUsedInPercent * (double)fillHeightWhen100Percent) / 100.0;



		_lcd.setColor(OLEDDISPLAY_COLOR::WHITE);
		_lcd.fillRect(2, batteryPlusHeight + 2 + batteryLeftPixels, width - 2 * 2, fillHeightWhen100Percent - 2 - batteryLeftPixels);

	};
	void drawBattery(int percent, float voltage)
	{
		drawBatterySymbol(percent, voltage);

		const int paddingLeft = 14;
		const int leftTextPadding = 2;
		int textX = paddingLeft + leftTextPadding;

		_lcd.setColor(OLEDDISPLAY_COLOR::WHITE);
		_lcd.setFont(ArialMT_Plain_16);

		char buffer[20];
		snprintf_P(buffer, 20, (PGM_P)F("%d%%"), percent);
		_lcd.drawString(textX, 0, buffer);
		_lcd.setFont(ArialMT_Plain_10);

		snprintf_P(buffer, 20, (PGM_P)F("%.2f V"), voltage);
		_lcd.drawString(textX, 14, buffer);
	}


	void drawSignalQuality(int csqQuality)
	{
		const double triangleWidth = 28;
		const double triangleHeight = 16;

		_lcd.setColor(OLEDDISPLAY_COLOR::WHITE);
		_lcd.drawHorizontalLine(128 - triangleWidth, triangleHeight, triangleWidth);
		_lcd.drawVerticalLine(128 - 1, 0, triangleHeight);
		_lcd.drawLine(128 - triangleWidth, triangleHeight, 128 - 1, 0);

		auto qualityToPixel = (double)csqQuality / 32.0 * triangleWidth;

		for (int i = 0; i < qualityToPixel; i++)
		{
			int lineHeight = (double)i / triangleWidth * triangleHeight;
			_lcd.drawVerticalLine(128-triangleWidth + i, triangleHeight - lineHeight, lineHeight);
		}
		_lcd.setFont(ArialMT_Plain_10);

		char buffer[10];
		snprintf_P(buffer, 10, (PGM_P)F("%d"), csqQuality);
		_lcd.drawString(128 - triangleWidth-2, 0, buffer);
	}

	const char* RegistrationStatusToStr(GsmNetworkStatus regStatus)
	{
		switch (regStatus)
		{
		case GsmNetworkStatus::SearchingForNetwork:
			return "Net Search...";
		case GsmNetworkStatus::RegistrationDenied:
			return "Reg denied";
		case GsmNetworkStatus::RegistrationUnknown:
			return "Reg unknown";
		default:
			break;
		}
		return "";
	}

	void lcd_label(int y, int heigth, int fontSize, const __FlashStringHelper* format, ...)
	{
		va_list argptr;
		va_start(argptr, format);

		if (fontSize == 16)
		{
			_lcd.setFont(ArialMT_Plain_16);
		}
		if (fontSize == 10)
		{
			_lcd.setFont(ArialMT_Plain_10);
		}

		char buffer[200];
		vsnprintf_P(buffer, 200, (PGM_P)format, argptr);
		_lcd.drawString(0, y, buffer);
		va_end(argptr);
	}
	void lcd_label(int x, int y, const __FlashStringHelper* format, ...)
	{
		va_list argptr;
		va_start(argptr, format);
		char buffer[200];
		vsnprintf_P(buffer, 200, (PGM_P)format, argptr);
		_lcd.drawString(x, y, buffer);
		va_end(argptr);
	}

	void drawGsmInfo(int signalQuality, GsmNetworkStatus gsmNetworkStatus, FixedString20& operatorName)
	{
		_lcd.setColor(OLEDDISPLAY_COLOR::WHITE);
		if (gsmNetworkStatus == GsmNetworkStatus::HomeNetwork || 
			gsmNetworkStatus == GsmNetworkStatus::Roaming)
		{
			_lcd.setFont(ArialMT_Plain_10);
			auto opWidth = _lcd.getStringWidth(operatorName.c_str());
			_lcd.drawString(128 - opWidth, 16, operatorName.c_str());
			drawSignalQuality(signalQuality);
			return;
		}
		
		auto gsmStatusStr = RegistrationStatusToStr(gsmNetworkStatus);
		auto statusStrWidth = _lcd.getStringWidth(gsmStatusStr);
		_lcd.drawString(128 - statusStrWidth, 0, operatorName.c_str());
	}

	void DisplayIncomingCall(IncomingCallInfo &callInfo)
	{
		if (callInfo.HasIncomingCall)
		{
			_lcd.setColor(OLEDDISPLAY_COLOR::WHITE);
			_lcd.drawRect(5, 10, 128 - 10, 45);
			_lcd.setColor(OLEDDISPLAY_COLOR::BLACK);
			_lcd.fillRect(6, 11, 128 - 10 - 2, 45 - 2);
			_lcd.setColor(OLEDDISPLAY_COLOR::WHITE);
			_lcd.setFont(ArialMT_Plain_16);
			lcd_label(10, 14, F("Calling: "));
			lcd_label(10, 32, F("%s"), callInfo.CallerNumber.c_str());
		}
	}

	void DisplayIp(FixedString20& ip)
	{
		lcd_label(26, 13, 10, F("ip: %s"), ip.c_str());
	}

	void DisplayBlinkIndicator()
	{
		static bool rectState = false;
		rectState = !rectState;
		if (rectState)
		{
			_lcd.setColor(OLEDDISPLAY_COLOR::WHITE);
		}
		else
		{
			_lcd.setColor(OLEDDISPLAY_COLOR::BLACK);
		}

		const int rectSize= 10;
		_lcd.fillCircle(128 - 50, 8, 8);
	}
};
