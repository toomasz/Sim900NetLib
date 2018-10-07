#include "Sim900.h"

Sim900::Sim900(Stream& serial, int powerPin, Stream &debugStream) :
serial(serial),
ds(debugStream), 
parser(*(new ParserSim900(ds)))
{
	_powerPin = powerPin;
	lastDataWrite = 0;
	dataBufferTail = 0;
	dataBufferHead  = 0;
	pinMode(powerPin, OUTPUT);
	parser.gsm = this;
	parser.ctx = this;
	memset(_dataBuffer,0, DATA_BUFFER_SIZE);
	UpdateBaudeRate = nullptr;
	_currentBaudRate = 0;
}

AtResultType Sim900::GetRegistrationStatus(GsmNetworkStatus& networkStatus)
{
	SendAt_P(AT_CREG,F("AT+CREG?"));

	auto result = PopCommandResult(AT_DEFAULT_TIMEOUT);
	if (result == AtResultType::Success)
	{
		networkStatus = parser._lastGsmResult;
	}
	return result;
}

AtResultType Sim900::GetOperatorName()
{
	operatorName[0] = 0;
	SendAt_P(AT_COPS, F("AT+COPS?"));	
	auto result = PopCommandResult(AT_DEFAULT_TIMEOUT);
	return result;
}

AtResultType Sim900::getSignalQuality()
{
	SendAt_P(AT_CSQ, F("AT+CSQ"));	
	auto result = PopCommandResult(AT_DEFAULT_TIMEOUT);

	return result;
}

AtResultType Sim900::GetBatteryStatus()
{
	SendAt_P(AT_CBC,F("AT+CBC"));
	auto result = PopCommandResult(AT_DEFAULT_TIMEOUT);
	return result;
}

void Sim900::PrintEscapedChar( char c )
{
	if(c=='\r')
		ds.print("\\r");
	else if(c=='\n')
		ds.print("\\n");
	else
		ds.print(c);	
}
/*
Get pdp context status using AT+CIPSTATUS
Return values S900_ERR, S900_TIMEOUT
IP_INITIAL, IP_START, IP_CONFIG, IP_GPRSACT, IP_STATUS, TCP_CONNECTING, TCP_CLOSED, PDP_DEACT, CONNECT_OK
*/
AtResultType Sim900::GetIpStatus()
{
	SendAt_P(AT_CIPSTATUS, F("AT+CIPSTATUS"));
	return PopCommandResult(AT_DEFAULT_TIMEOUT);
}

AtResultType Sim900::GetIpAddress( )
{
	SendAt_P(AT_CIFSR, F("AT+CIFSR"));
	return PopCommandResult(AT_DEFAULT_TIMEOUT);
}

AtResultType Sim900::AttachGprs()
{
	SendAt_P(AT_DEFAULT, F("AT+CIICR"));
	return PopCommandResult(60000);
}

AtResultType Sim900::StartTransparentIpConnection(const char *address, int port, S900Socket *socket = 0 )
{
	dataBufferHead = dataBufferTail = 0;

	// Execute command like AT+CIPSTART="TCP","example.com","80"
	SendAt_P(AT_CIPSTART, F("AT+CIPSTART=\"TCP\",\"%s\",\"%d\""), address, port);

	if (socket != 0)
	{
		socket->s900 = this;
	}
	return PopCommandResult(60000);
}

AtResultType Sim900::CloseConnection()
{
	SendAt_P(AT_CIPCLOSE, F("AT+CIPCLOSE=1"));
	return PopCommandResult(AT_DEFAULT_TIMEOUT);
}


/* returns true if any data is available to read from transparent connection */
bool Sim900::DataAvailable()
{
	if(dataBufferHead != dataBufferTail)
		return true;
	if(serial.available())
		return true;
	return false;
}
void Sim900::PrintDataByte(uint8_t data) // prints 8-bit data in hex
{
	char tmp[3];
	byte first;
	byte second;

	first = (data >> 4) & 0x0f;
	second = data & 0x0f;

	tmp[0] = first+48;
	tmp[1] = second+48;
	if (first > 9) tmp[0] += 39;
	if (second > 9) tmp[1] += 39;
	
	tmp[2] = 0;
	ds.write(' ');
	ds.print(tmp);
	ds.write(' ');
}

int Sim900::DataRead()
{
	int ret = ReadDataBuffer();
	if(ret != -1)
	{
		//PrintDataByte(ret);
		return ret;
	}
	ret = serial.read();
	if(ret != -1)
	{
	//	PrintDataByte(ret);
	}
	return ret;
}

AtResultType Sim900::SwitchToCommandMode()
{
	parser.SetCommandType(AT_SWITH_TO_COMMAND);
	commandBeforeRN = true;
	// +++ escape sequence needs to wait 1000ms after last data was sent via transparent connection
	// in the meantime data from tcp connection may arrive so we read it here to dataBuffer
	// ex: lastDataWrite = 1500
	// loop will exit when millis()-1500<1000 so when millis is 2500
	
	delay(1000);
  /* while(ser->available() || (millis()-lastDataWrite) < 1000)
	{
		while(ser->available())
		{
			int c = ser->read();
			pr("\nsc_data: %c\n", (char)c);
			WriteDataBuffer(c);
		}
	}*/
	
	serial.print(F("+++"));
	lastDataWrite = millis();
	return PopCommandResult(500);
}

AtResultType Sim900::SwitchToCommandModeDropData()
{
	parser.SetCommandType(AT_SWITH_TO_COMMAND);
	serial.flush();
	while (serial.available())
	{
		serial.read();
	}
	delay(1500);
	while (serial.available())
	{
		serial.read();
	}

	serial.print(F("+++"));

	return PopCommandResult(500);
}

/*
Switches to data mode ATO
Return values S900_OK, S900_ERROR, S900_TIMEOUT
*/
AtResultType Sim900::SwitchToDataMode()
{
	SendAt_P(AT_SWITCH_TO_DATA, F("ATO"));

	auto result = PopCommandResult(AT_DEFAULT_TIMEOUT);
	if(result == AtResultType::Success)
	{
		delay(100);
		while(serial.available())
		{
			int c = serial.read();
			//pr("\nsd_data: %c\n", (char)c);
			//ds.print("s_data: "); ds.println((int)c);
			WriteDataBuffer(c);
		}
		
	}
	lastDataWrite = millis();
	return result;
}

AtResultType Sim900::PopCommandResult( int timeout )
{
	unsigned long start = millis();
	while(parser.commandReady == false && (millis()-start) < (unsigned long)timeout)
	{
		if(serial.available())
		{
			char c = serial.read();
			parser.FeedChar(c);
		}
	}

	auto commandResult = parser.GetAtResultType();
	parser.SetCommandType(0);
	ds.println("  command ready");
	return commandResult;
}
/*
Disables/enables echo on serial port
*/
AtResultType Sim900::SetEcho(bool echoEnabled)
{
	parser.SetCommandType(AT_DEFAULT);
	if (echoEnabled)
	{
		SendAt_P(AT_DEFAULT, F("ATE1"));
	}
	else
	{
		SendAt_P(AT_DEFAULT, F("ATE0"));
	}

	auto r = PopCommandResult(AT_DEFAULT_TIMEOUT);
	delay(100); // without 100ms wait, next command failed, idk wky
	return r;
}
/*
Set gsm modem to use transparent mode
*/
AtResultType Sim900::SetTransparentMode( bool transparentMode )
{	
	if (transparentMode)
	{
		SendAt_P(AT_DEFAULT, F("AT+CIPMODE=1"));
	}
	else
	{
		SendAt_P(AT_DEFAULT, F("AT+CIPMODE=0"));
	}
		
	return PopCommandResult(AT_DEFAULT_TIMEOUT);
}

AtResultType Sim900::SetApn(const char *apnName, const char *username,const char *password )
{
	parser.SetCommandType(AT_DEFAULT);

	SendAt_P(AT_DEFAULT, F("AT+CSTT=\"%s\",\"%s\",\"%s\""), apnName, username, password);
	return PopCommandResult(AT_DEFAULT_TIMEOUT);
}

void Sim900::DataWrite( const __FlashStringHelper* data )
{
	serial.print(data);
	lastDataWrite = millis();
}

void Sim900::DataWrite( char* data )
{
	serial.print(data);
	lastDataWrite = millis();
}

void Sim900::DataWrite( char *data, int length )
{
	serial.write((unsigned char*)data, length);
	lastDataWrite = millis();
}

void Sim900::DataWrite( char c )
{
	serial.write(c);
	lastDataWrite = millis();
}


void Sim900::DataEndl()
{
	serial.print(F("\r\n"));
	serial.flush();
	lastDataWrite = millis();
}
AtResultType Sim900::At()
{
	SendAt_P(AT_DEFAULT, F("AT"));
	return PopCommandResult(30);
}

AtResultType Sim900::SetBaudRate(uint32_t baud)
{
	SendAt_P(AT_DEFAULT, F("AT+IPR=%d"), baud);
	return PopCommandResult(AT_DEFAULT_TIMEOUT);
}
bool Sim900::EnsureModemConnected()
{
	auto atResult = At();

	if (_currentBaudRate == 0 || atResult != AtResultType::Success)
	{
		_currentBaudRate = FindCurrentBaudRate();
		if (_currentBaudRate != 0)
		{
			printf("Found baud rate = %d\n", _currentBaudRate);
			if (SetBaudRate(115200) == AtResultType::Success)
			{
				_currentBaudRate = 115200;
				printf("set baud rate to = %d\n", _currentBaudRate);
				At();
				SetEcho(false);
				At();
				return true;
			}
			printf("Failed to update baud rate = %d\n", _currentBaudRate);
			return true;
		}
	}

	printf("EnsureModemConnected return %d\n", atResult == AtResultType::Success);
	return atResult == AtResultType::Success;
}
AtResultType Sim900::GetIMEI()
{
	SendAt_P(AT_GSN, F("AT+GSN"));
	return PopCommandResult(100);
}

bool Sim900::IsPoweredUp()
{
	return GetIMEI() == AtResultType::Success;
}

void Sim900::wait(int ms)
{
	unsigned long start = millis();
	while ((millis() - start) <= (unsigned long)ms)
	{
		if (serial.available())
		{
			parser.FeedChar(serial.read());
		}
	}
}

AtResultType Sim900::ExecuteFunction(FunctionBase &function)
{
	parser.SetCommandType(&function);
	serial.println(function.getCommand());
	
	auto initialResult = PopCommandResult(function.functionTimeout);
	
	if (initialResult == AtResultType::Success)
	{
		return AtResultType::Success;
	}
	if(function.GetInitSequence() == NULL)
		return initialResult;
		
	char *p= (char*)function.GetInitSequence();
	while(pgm_read_byte(p) != 0)
	{
			
	//	ds.print(F("Exec: "));
			
		//printf("Exec: %s\n", p);
		ds.println((__FlashStringHelper*)p);
		delay(100);
		SendAt_P(AT_DEFAULT, (__FlashStringHelper*)p);
		auto r = PopCommandResult(AT_DEFAULT_TIMEOUT);
		if(r != AtResultType::Success)
		{
				
			ds.println(F("Fail"));
			return r;
		}
		ds.println(" __Fin");		

		while(pgm_read_byte(p) != 0)			
			p++;
			
		p++;
	}
	delay(500);
	parser.SetCommandType(&function);
	serial.println(function.getCommand());
	return PopCommandResult(function.functionTimeout);
}

AtResultType Sim900::SendSms(char *number, char *message)
{
	SendAt_P(AT_DEFAULT, F("AT+CMGS=\"%s\""), number);
	
	uint64_t start = millis();
	// wait for >
	while (serial.read() != '>')
		if (millis() - start > 200)
			return AtResultType::Error;
	serial.print(message);
	serial.print('\x1a');
	return PopCommandResult(AT_DEFAULT_TIMEOUT);
}
AtResultType Sim900::SendUssdWaitResponse(char *ussd, char*response, int responseBufferLength)
{
	buffer_ptr = response;
	buffer_size = responseBufferLength;

	SendAt_P(AT_CUSD, F("AT+CUSD=1,\"%s\""), ussd);
	return PopCommandResult(10000);
}
void Sim900::SendAt_P(int commandType, const __FlashStringHelper* command, ...)
{
	parser.SetCommandType(commandType);

	va_list argptr;
	va_start(argptr, command);

	char commandBuffer[200];
	vsnprintf_P(commandBuffer, 200, (PGM_P)command, argptr);
	ds.printf("  send -> '%s'\n", commandBuffer);
	serial.println(commandBuffer);

	va_end(argptr);
}
int Sim900::FindCurrentBaudRate()
{
	if (UpdateBaudeRate == nullptr)
	{
		return 0;
	}
	int i = 0;
	int baudRate = 0;
	do
	{
		baudRate = _defaultBaudRates[i];
		ds.printf("Trying baud rate: %d\n", baudRate);
		yield();
		UpdateBaudeRate(baudRate);
		yield();
		if (At() == AtResultType::Success)
		{
			ds.printf(" Found baud rate: %d\n", baudRate);
			return baudRate;
		}
		i++;
	} 
	while (_defaultBaudRates[i] != 0);

	return 0;
}
int Sim900::UnwriteDataBuffer()
{
	if (dataBufferTail == 0)
		dataBufferTail = DATA_BUFFER_SIZE - 1;
	else
		dataBufferTail--;
	return _dataBuffer[dataBufferTail];
}

void Sim900::WriteDataBuffer(char c)
{
	int tmp = dataBufferTail+1;
	if(tmp == DATA_BUFFER_SIZE)
		tmp = 0;
	if(tmp == dataBufferHead)
	{
		ds.println(F("Buffer overflow"));
		return;
	}
	//ds.print(F("Written ")); this->PrintDataByte(c);
	_dataBuffer[dataBufferTail] = c;
	dataBufferTail = tmp;
}

int Sim900::ReadDataBuffer()
{
	if(dataBufferHead != dataBufferTail)
	{
		int ret= _dataBuffer[dataBufferHead];
		dataBufferHead++;
		if(dataBufferHead==DATA_BUFFER_SIZE)
			dataBufferHead = 0;
		return ret;
	}
	return -1;
}

AtResultType Sim900::Cipshut()
{
	SendAt_P(AT_CIPSHUT, F("AT+CIPSHUT"));
	return PopCommandResult(AT_DEFAULT_TIMEOUT);
}

void Sim900::DataWriteNumber(int c)
{
	serial.print(c);
	lastDataWrite = millis();
}
void Sim900::DataWriteNumber(uint16_t c)
{
	serial.print(c);
	lastDataWrite = millis();
}
//
//void Sim900::data_printf(const __FlashStringHelper *fmt, ...)
//{
//	va_list args;
//	va_start(args, fmt);
//	vfprintf_P(&dataStream, (const char*)fmt, args);
//	va_end(args);
//	lastDataWrite = millis();
//}

AtResultType Sim900::Call(char *number)
{
	SendAt_P(AT_DEFAULT, F("ATD%s;"), number);
	return PopCommandResult(AT_DEFAULT_TIMEOUT);
}

AtResultType Sim900::EnableCallerId()
{
	SendAt_P(AT_DEFAULT, F("AT+CLIP=1"));
	return PopCommandResult(AT_DEFAULT_TIMEOUT);
}

AtResultType Sim900::Shutdown()
{
	SendAt_P(AT_DEFAULT, F("AT+CPOWD=0"));
	return PopCommandResult(AT_DEFAULT_TIMEOUT);
}







































