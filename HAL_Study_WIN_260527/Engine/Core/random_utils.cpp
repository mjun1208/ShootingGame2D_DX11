#include "random_utils.h"

std::mt19937& RandomEngine()
{
	static std::mt19937 engine{ std::random_device{}() };
	return engine;
}

int RandomInt(int minimum, int maximum)
{
	return std::uniform_int_distribution<int>{ minimum, maximum }(RandomEngine());
}

float RandomFloat(float minimum, float maximum)
{
	return std::uniform_real_distribution<float>{ minimum, maximum }(RandomEngine());
}

float Random01()
{
	return RandomFloat(0.0f, 1.0f);
}

float RandomSigned()
{
	return RandomFloat(-1.0f, 1.0f);
}
