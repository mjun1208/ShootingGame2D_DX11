#ifndef INDEXED_SLOT_POOL_H
#define INDEXED_SLOT_POOL_H

#include <array>
#include <cstddef>

template <std::size_t Capacity, int InvalidID = -1> class IndexedSlotPool final
{
	static_assert(Capacity > 0);
	static_assert(InvalidID < 0 || InvalidID >= static_cast<int>(Capacity));

  public:
	IndexedSlotPool()
	{
		Reset();
	}

	void Reset()
	{
		for (int index = 0; index < static_cast<int>(Capacity); ++index)
		{
			m_ActiveIDs[index] = InvalidID;
			m_ActiveIndices[index] = InvalidID;
			m_FreeIDs[index] = static_cast<int>(Capacity) - 1 - index;
		}
		m_ActiveCount = 0;
		m_FreeCount = static_cast<int>(Capacity);
	}

	int Acquire()
	{
		if (m_FreeCount <= 0)
		{
			return InvalidID;
		}

		const int id = m_FreeIDs[--m_FreeCount];
		m_ActiveIndices[id] = m_ActiveCount;
		m_ActiveIDs[m_ActiveCount++] = id;
		return id;
	}

	bool Release(int id)
	{
		if (!IsActive(id))
		{
			return false;
		}

		const int active_index = m_ActiveIndices[id];
		const int last_active_index = --m_ActiveCount;
		const int last_id = m_ActiveIDs[last_active_index];
		m_ActiveIDs[active_index] = last_id;
		m_ActiveIndices[last_id] = active_index;
		m_ActiveIDs[last_active_index] = InvalidID;
		m_ActiveIndices[id] = InvalidID;
		m_FreeIDs[m_FreeCount++] = id;
		return true;
	}

	bool IsActive(int id) const
	{
		return id >= 0 && id < static_cast<int>(Capacity) && m_ActiveIndices[id] != InvalidID;
	}

	bool HasFreeSlot() const
	{
		return m_FreeCount > 0;
	}

	int GetActiveID(int active_index) const
	{
		return active_index >= 0 && active_index < m_ActiveCount ? m_ActiveIDs[active_index] : InvalidID;
	}

	int GetActiveCount() const
	{
		return m_ActiveCount;
	}

	static constexpr int GetCapacity()
	{
		return static_cast<int>(Capacity);
	}

  private:
	std::array<int, Capacity> m_ActiveIDs{};
	std::array<int, Capacity> m_ActiveIndices{};
	std::array<int, Capacity> m_FreeIDs{};
	int m_ActiveCount{ 0 };
	int m_FreeCount{ 0 };
};

#endif // INDEXED_SLOT_POOL_H
