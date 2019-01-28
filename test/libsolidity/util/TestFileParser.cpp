/*
	This file is part of solidity.

	solidity is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	solidity is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with solidity.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <test/libsolidity/util/TestFileParser.h>

#include <test/Options.h>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/optional.hpp>
#include <boost/throw_exception.hpp>
#include <fstream>
#include <memory>
#include <stdexcept>

using namespace dev;
using namespace langutil;
using namespace solidity;
using namespace dev::solidity::test;
using namespace std;

namespace
{
	void expect(string::iterator& _it, string::iterator _end, string::value_type _c)
	{
		if (_it == _end || *_it != _c)
			throw Error(Error::Type::ParserError, string("Invalid test expectation. Expected: \"") + _c + "\".");
		++_it;
	}

	template<typename IteratorType>
	void skipWhitespace(IteratorType& _it, IteratorType _end)
	{
		while (_it != _end && isspace(*_it))
			++_it;
	}

	template<typename IteratorType>
	void skipSlashes(IteratorType& _it, IteratorType _end)
	{
		while (_it != _end && *_it == '/')
			++_it;
	}
}

pair<bytes, ByteFormats> TestFileParser::formattedStringToBytes(string _string)
{
	bytes result;
	vector<ByteFormat> formats;
	auto it = _string.begin();
	while (it != _string.end())
	{
		if (isdigit(*it) || (*it == '-' && (it + 1) != _string.end() && isdigit(*(it + 1))))
		{
			ByteFormat format;
			format.type = ByteFormat::UnsignedDec;

			bool isNegative = (*it == '-');
			if (isNegative)
				format.type = ByteFormat::SignedDec;

			auto valueBegin = it;
			while (it != _string.end() && !isspace(*it) && *it != ',')
				++it;

			bytes newBytes;
			try {
				u256 numberValue{string{valueBegin, it}};
				// TODO: Convert to compact big endian if padded
				if (numberValue == u256{0})
					newBytes = bytes{0};
				else
					newBytes = toBigEndian(numberValue);
				result += newBytes;
				formats.push_back(std::move(format));
			}
			catch (std::exception const&)
			{
				throw Error(Error::Type::ParserError, "Argument encoding invalid.");
			}
		}
		else
			throw Error(Error::Type::ParserError, "Argument encoding invalid.");

		skipWhitespace(it, _string.end());
		if (it != _string.end())
			expect(it, _string.end(), ',');
		skipWhitespace(it, _string.end());
	}
	return make_pair(result, formats);
}

string TestFileParser::bytesToFormattedString(bytes const& _bytes, ByteFormats const& _formats)
{
	// TODO: Convert from compact big endian if padded
	auto it = _bytes.begin();
	stringstream resultStream;
	for (auto const& format: _formats)
	{
		bytes byteRange{it, it + format.size};
		switch(format.type)
		{
			case ByteFormat::SignedDec:
				if (*byteRange.begin() & 0x80)
				{
					for (auto& v: byteRange)
						v ^= 0xFF;
					resultStream << "-" << fromBigEndian<u256>(byteRange) + 1;
				}
				else
					resultStream << fromBigEndian<u256>(byteRange);
				break;
			case ByteFormat::UnsignedDec:
				resultStream << fromBigEndian<u256>(byteRange);
				break;
		}
		it += format.size;
		if (it != _bytes.end())
			resultStream << ",";
	}
	return resultStream.str();
}



vector<dev::solidity::test::FunctionCall> TestFileParser::parseFunctionCalls()
{
	vector<FunctionCall> calls;
	while (advanceLine())
	{
		if (m_scanner.eol())
			continue;

		FunctionCall call;
		call.signature = parseFunctionCallSignature();
		if (auto optionalValue = parseFunctionCallValue())
			call.value = optionalValue.get();
		call.arguments = parseFunctionCallArgument();

		if (!advanceLine())
			throw Error(Error::Type::ParserError, "Expected result missing.");
		call.expectations = parseFunctionCallExpectations();

		if (call.expectations.status)
			call.expectations.output = "-> " + call.expectations.raw;
		else
			call.expectations.output = "REVERT";

		calls.emplace_back(std::move(call));
	}
	return calls;
}

string TestFileParser::parseFunctionCallSignature()
{
	auto signatureBegin = m_scanner.position();
	while (!m_scanner.eol() && m_scanner.current() != ')')
		m_scanner.advance();
	expectCharacter(')');

	return string{signatureBegin, m_scanner.position()};
}

FunctionCallArgs TestFileParser::parseFunctionCallArgument()
{
	skipWhitespaces();

	FunctionCallArgs arguments;
	if (!m_scanner.eol())
	{
		if (m_scanner.current() != '#')
		{
			expectCharacter(':');
			skipWhitespaces();

			auto argumentBegin = m_scanner.position();
			// TODO: allow # in quotes
			while (!m_scanner.eol() && m_scanner.current() != '#')
				m_scanner.advance();
			arguments.raw = string{argumentBegin, m_scanner.position()};
			boost::algorithm::trim(arguments.raw);

			auto bytesFormat = formattedStringToBytes(arguments.raw);
			arguments.rawBytes = bytesFormat.first;
			arguments.formats = bytesFormat.second;
		}

		if (!m_scanner.eol())
		{
			expectCharacter('#');
			skipWhitespaces();
			arguments.comment = string(m_scanner.position(), m_scanner.endPosition());
		}
	}
	return arguments;
}

FunctionCallExpectations TestFileParser::parseFunctionCallExpectations()
{
	FunctionCallExpectations result;
	if (!m_scanner.eol() && m_scanner.current() == '-')
	{
		expectCharacter('-');
		expectCharacter('>');
		skipWhitespaces();

		auto expectedResultBegin = m_scanner.position();
		// TODO: allow # in quotes
		while (!m_scanner.eol() && m_scanner.current() != '#')
			m_scanner.advance();
		result.raw = string{expectedResultBegin, m_scanner.position()};
		boost::algorithm::trim(result.raw);

		auto bytesFormat = formattedStringToBytes(result.raw);
		result.rawBytes = bytesFormat.first;
		result.formats = bytesFormat.second;

		result.status = true;

		if (!m_scanner.eol())
		{
			expectCharacter('#');
			skipWhitespaces();
			result.comment = string(m_scanner.position(), m_scanner.endPosition());
		}
	}
	else
	{
		expectCharacterSequence("REVERT");
		result.status = false;
	}
	return result;
}

boost::optional<u256> TestFileParser::parseFunctionCallValue()
{
	skipWhitespaces();
	if (m_scanner.current() != ',')
		return boost::none;
	m_scanner.advance();

	u256 value;
	auto etherBegin = m_scanner.position();
	while (!m_scanner.eol() && m_scanner.current() != ':')
		m_scanner.advance();
	string etherString(etherBegin, m_scanner.position());
	boost::algorithm::trim(etherString);
	try
	{
		value = u256(etherString);
	}
	catch (exception const&)
	{
		throw Error(Error::Type::ParserError, "Ether value encoding invalid.");
	}
	return std::move(value);
}


bool TestFileParser::advanceLine()
{
	bool success = m_scanner.advanceLine();

	skipWhitespaces();
	skipSlashes(m_scanner.position(), m_scanner.endPosition());
	skipWhitespaces();

	return success;
}

void TestFileParser::expectCharacter(char const _char)
{
	expect(m_scanner.position(), m_scanner.endPosition(), _char);
}

void TestFileParser::expectCharacterSequence(string const& _charSequence)
{
	for (char c: _charSequence)
		expectCharacter(c);
}

void TestFileParser::skipWhitespaces()
{
	skipWhitespace(m_scanner.position(), m_scanner.endPosition());
}
