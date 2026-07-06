#ifndef SINGLETON_H
#define SINGLETON_H

template <typename T>
class cSingleton
{
public:
	static T& GetInstance()
	{
		static T instance;
		return instance;
	}

	static T* GetInstancePtr()
	{
		return &GetInstance();
	}

	// Don't Use!!!!
	cSingleton(const cSingleton&) = delete;
	cSingleton& operator=(const cSingleton&) = delete;
	cSingleton(cSingleton&&) = delete;
	cSingleton& operator=(cSingleton&&) = delete;

protected:
	cSingleton() = default;
	~cSingleton() = default;
};

#endif // !SINGLETON_H
