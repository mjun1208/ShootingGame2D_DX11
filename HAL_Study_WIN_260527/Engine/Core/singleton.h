#ifndef SINGLETON_H
#define SINGLETON_H

template <typename T> class cSingleton
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

	// 사용 금지.
	cSingleton(const cSingleton&) = delete;
	cSingleton& operator=(const cSingleton&) = delete;
	cSingleton(cSingleton&&) = delete;
	cSingleton& operator=(cSingleton&&) = delete;

  protected:
	cSingleton() = default;
	~cSingleton() = default;
};

#endif // !SINGLETON_H
