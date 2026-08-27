#ifndef JSON_READER_H
#define JSON_READER_H

#include <charconv>
#include <cctype>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>

namespace Json
{
	inline bool LoadTextFile(const char* file_path, std::string& output)
	{
		std::ifstream input(file_path, std::ios::binary);
		if (!input) return false;

		output.assign(
			std::istreambuf_iterator<char>(input),
			std::istreambuf_iterator<char>{});
		return true;
	}

	class Reader
	{
	public:
		explicit Reader(std::string_view source) : m_Source(source) {}

		bool Consume(char expected)
		{
			SkipWhitespace();
			if (m_Position >= m_Source.size() ||
				m_Source[m_Position] != expected)
			{
				return false;
			}
			++m_Position;
			return true;
		}

		bool ParseString(std::string& output)
		{
			SkipWhitespace();
			if (m_Position >= m_Source.size() || m_Source[m_Position++] != '"')
			{
				return false;
			}

			output.clear();
			while (m_Position < m_Source.size())
			{
				const char character = m_Source[m_Position++];
				if (character == '"') return true;
				if (character != '\\')
				{
					output.push_back(character);
					continue;
				}

				if (m_Position >= m_Source.size()) return false;
				switch (m_Source[m_Position++])
				{
				case 'b': output.push_back('\b'); break;
				case 'f': output.push_back('\f'); break;
				case 'n': output.push_back('\n'); break;
				case 'r': output.push_back('\r'); break;
				case 't': output.push_back('\t'); break;
				case '\\': output.push_back('\\'); break;
				case '/': output.push_back('/'); break;
				case '"': output.push_back('"'); break;
				default: return false;
				}
			}
			return false;
		}

		bool ParseFloat(float& output)
		{
			SkipWhitespace();
			const std::size_t start = m_Position;
			while (m_Position < m_Source.size())
			{
				const char value = m_Source[m_Position];
				if (!(std::isdigit(static_cast<unsigned char>(value)) ||
					value == '-' || value == '+' || value == '.' ||
					value == 'e' || value == 'E'))
				{
					break;
				}
				++m_Position;
			}

			if (start == m_Position) return false;
			const char* begin = m_Source.data() + start;
			const char* end = m_Source.data() + m_Position;
			const auto result = std::from_chars(begin, end, output);
			return result.ec == std::errc{} && result.ptr == end;
		}

		bool ParseInt(int& output)
		{
			SkipWhitespace();
			const char* begin = m_Source.data() + m_Position;
			const char* end = m_Source.data() + m_Source.size();
			const auto result = std::from_chars(begin, end, output);
			if (result.ec != std::errc{} || result.ptr == begin) return false;
			m_Position = static_cast<std::size_t>(result.ptr - m_Source.data());
			return true;
		}

		bool ParseBool(bool& output)
		{
			SkipWhitespace();
			if (m_Source.substr(m_Position, 4) == "true")
			{
				m_Position += 4;
				output = true;
				return true;
			}
			if (m_Source.substr(m_Position, 5) == "false")
			{
				m_Position += 5;
				output = false;
				return true;
			}
			return false;
		}

		bool SkipValue()
		{
			SkipWhitespace();
			if (m_Position >= m_Source.size()) return false;

			if (m_Source[m_Position] == '"')
			{
				std::string ignored;
				return ParseString(ignored);
			}
			if (m_Source[m_Position] == '{')
			{
				if (!Consume('{')) return false;
				if (Consume('}')) return true;
				while (true)
				{
					std::string key;
					if (!ParseString(key) || !Consume(':') || !SkipValue())
					{
						return false;
					}
					if (Consume('}')) return true;
					if (!Consume(',')) return false;
				}
			}
			if (m_Source[m_Position] == '[')
			{
				if (!Consume('[')) return false;
				if (Consume(']')) return true;
				while (true)
				{
					if (!SkipValue()) return false;
					if (Consume(']')) return true;
					if (!Consume(',')) return false;
				}
			}

			bool ignored_bool = false;
			if (ParseBool(ignored_bool)) return true;
			SkipWhitespace();
			if (m_Source.substr(m_Position, 4) == "null")
			{
				m_Position += 4;
				return true;
			}

			float ignored_number = 0.0f;
			return ParseFloat(ignored_number);
		}

		bool IsAtEnd()
		{
			SkipWhitespace();
			return m_Position == m_Source.size();
		}

	private:
		void SkipWhitespace()
		{
			while (m_Position < m_Source.size() &&
				std::isspace(static_cast<unsigned char>(m_Source[m_Position])))
			{
				++m_Position;
			}
		}

		std::string_view m_Source;
		std::size_t m_Position{ 0 };
	};
}

#endif // JSON_READER_H
