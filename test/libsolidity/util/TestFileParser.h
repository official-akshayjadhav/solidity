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

#pragma once

#include <libdevcore/CommonData.h>
#include <libsolidity/ast/Types.h>
#include <liblangutil/Exceptions.h>

#include <iosfwd>
#include <stdexcept>
#include <string>
#include <vector>
#include <utility>

namespace dev
{
namespace solidity
{
namespace test
{

/**
 * Format information used for the conversion of human-readable
 * function arguments and return values to `bytes`. Defaults to
 * a 32-byte representation.
 */
struct ByteFormat
{
	enum Type {
		UnsignedDec,
		SignedDec
	};
	Type type;
	long size = 32;
};

using ByteFormats = std::vector<ByteFormat>;

/**
 * Represents the expected result of a function call after it has been executed. This may be a single
 * return value or a comma-separated list of return values. It also contains the detected input
 * formats used to convert the values to `bytes` needed for the comparison with the actual result
 * of a call. In addition to that, it also stores the expected transaction status.
 * An optional comment can be assigned.
 */
struct FunctionCallExpectations
{
	std::string raw;
	bytes rawBytes;
	std::vector<ByteFormat> formats;
	bool status = true;
	std::string output;
	std::string comment;
};

/**
 * Represents the arguments passed to a function call. This can be a single
 * argument or a comma-separated list of arguments. It also contains the detected input
 * formats used to convert the arguments to `bytes` needed for the call.
 * An optional comment can be assigned.
 */
struct FunctionCallArgs
{
	std::string raw;
	bytes rawBytes;
	std::vector<ByteFormat> formats;
	std::string comment;
};

/**
 * Represents a function call read from an input stream. It contains the signature, the
 * arguments, an optional ether value and an expected execution result.
 */
struct FunctionCall
{
	std::string signature;
	FunctionCallArgs arguments;
	FunctionCallExpectations expectations;
	u256 value;
};

/**
 * Class that is able to parse an additional and well-formed comment section in a Solidity
 * source file used by the file-based unit test environment. For now, it parses function
 * calls and their expected result after the call was made.
 *
 * - Function calls defined in blocks:
 * // f(uint256, uint256): 1, 1 # Signature and comma-separated list of arguments
 * // -> 1, 1                   # Expected result value
 * // g(), 2 ether              # (Optional) Ether to be send with the call
 * // -> 2, 3
 * // h(uint256), 1 ether: 42
 * // REVERT
 * ...
 */
class TestFileParser
{
public:

	/// Tries to convert the formatted \param _string to it's byte representation and
	/// preserves the chosen byte formats. Supported types:
	/// - unsigned and signed decimal number literals
	/// Throws an InvalidArgumentEncoding exception if data is encoded incorrectly or
	/// if data type is not supported.
	static std::pair<bytes, ByteFormats> formattedStringToBytes(std::string _string);

	/// Formats \param _bytes given the byte formats \param _formats. Supported formats:
	/// - unsigned and signed decimal number literals
	static std::string bytesToFormattedString(bytes const& _bytes, ByteFormats const& _formats);

	/// Constructor that takes an input stream \param _stream to operate on
	/// and creates the internal scanner.
	TestFileParser(std::istream& _stream): m_scanner(_stream) {}

	/// Parses function calls blockwise and returns a list of function calls found.
	/// Throws an exception if a function call cannot be parsed because of its
	/// incorrect structure, an invalid or unsupported encoding
	/// of its arguments or expected results.
	std::vector<FunctionCall> parseFunctionCalls();

private:
	/**
	 * Simple scanner that is used internally to abstract away character traversal.
	 */
	class Scanner
	{
	public:
		/// Constructor that takes an input stream \param _stream to operate on.
		Scanner(std::istream& _stream): m_stream(_stream) {}

		/// Returns the current character.
		char current() const { return *m_char; }
		/// Returns true if the end of a line is reached, false otherwise.
		bool eol() const { return m_char == m_line.end(); }
		/// Returns an iterator of the current position in the input stream.
		std::string::iterator& position() { return m_char; }
		/// Returns an iterator to the end position of the input stream.
		std::string::iterator endPosition() { return m_line.end(); }
		/// Advances current position in the input stream.
		void advance() { ++m_char; }
		/// Advances stream by one line. Returns true if stream contains a new line,
		/// false if no new line could be read.
		bool advanceLine()
		{
			std::istream& line = std::getline(m_stream, m_line);
			m_char = m_line.begin();
			return line ? true : false;
		}

	private:
		std::string m_line;
		std::string::iterator m_char;
		std::istream& m_stream;
	};

	/// Parses a function call signature in the form of f(uint256, ...).
	std::string parseFunctionCallSignature();

	/// Parses a comma-separated list of arguments passed with a function call.
	/// Does not check for a potential mismatch between the signature and the number
	/// or types of arguments.
	FunctionCallArgs parseFunctionCallArguments();

	/// Parses the expected result of a function call execution.
	FunctionCallExpectations parseFunctionCallExpectations();

	/// Parses the optional ether value that can be passed alongside the
	/// function call arguments. Throws an InvalidEtherValueEncoding exception
	/// if given value cannot be converted to `u256`.
	boost::optional<u256> parseFunctionCallValue();

	bool advanceLine();
	void expectCharacter(char const _char);
	void expectCharacterSequence(std::string const& _charSequence);
	std::string parseUntilCharacter(char const _char, bool const _expect = false);
	void skipWhitespaces();

	Scanner m_scanner;
};

}
}
}
