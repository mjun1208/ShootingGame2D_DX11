#ifndef FILE_UTILS_H
#define FILE_UTILS_H

#include <string>
#include <vector>

bool ReadBinaryFile(const char* path, std::vector<unsigned char>& bytes);
bool ReadTextFile(const char* path, std::string& text);

#endif // FILE_UTILS_H
