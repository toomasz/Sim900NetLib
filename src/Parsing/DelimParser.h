#ifndef __DELIMPARSER_H__
#define __DELIMPARSER_H__

#include <inttypes.h>
#include <string.h>
#include "FixedString.h"
#include <WString.h>

enum class LineParserState
{
	Initial,
	Expression,
	StartQuote,
	QuotedExpression,
	EndQuote,
	Delimiter,
	Error,
	END
};

class DelimParser
{
	FixedStringBase &_line;
	uint8_t _position;
	LineParserState _currentState;
	uint8_t _tokenStart;
	LineParserState GetNextState(char c, LineParserState state);
	int hexDigitToInt(char c);
	char _separator;
public:
	bool StartsWith(const __FlashStringHelper* commandStart);
	DelimParser(FixedStringBase &line, char separator = ',');
	bool NextToken();
	FixedString150 CurrentToken();
	bool Skip(int tokenCount);
	bool NextString(FixedStringBase& targetString);
	bool NextNum(uint8_t & dst, bool allowNull = false, int base = 10);
	bool NextNum(int16_t& dst, bool allowNull = false, int base = 10);
	bool NextNum(uint16_t& dst, bool allowNull = false, int base = 10);

	static const  __FlashStringHelper* StateToStr(LineParserState state);
};

#endif //__DELIMPARSER_H__
