#ifndef RANDOM_UTILS_H
#define RANDOM_UTILS_H

#include <random>

std::mt19937& RandomEngine();
int RandomInt(int minimum, int maximum);
float RandomFloat(float minimum, float maximum);
float Random01();
float RandomSigned();

#endif // RANDOM_UTILS_H
