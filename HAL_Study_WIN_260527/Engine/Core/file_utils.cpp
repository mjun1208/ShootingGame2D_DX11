#include "file_utils.h"

#include <cstddef>
#include <fstream>
#include <iterator>

bool ReadBinaryFile(const char* path, std::vector<unsigned char>& bytes)
{
	std::ifstream stream(path, std::ios::binary | std::ios::ate);
	if (!stream)
	{
		return false;
	}

	const std::streamsize size = stream.tellg();
	if (size <= 0)
	{
		return false;
	}

	bytes.resize(static_cast<std::size_t>(size));
	stream.seekg(0, std::ios::beg);
	return static_cast<bool>(stream.read(reinterpret_cast<char*>(bytes.data()), size));
}

bool ReadTextFile(const char* path, std::string& text)
{
	std::ifstream stream(path, std::ios::binary);
	if (!stream)
	{
		return false;
	}

	text.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>{});
	return true;
}
