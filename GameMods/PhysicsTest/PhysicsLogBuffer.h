#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <deque>

namespace PhysicsBlock
{
	class PhysicsLogBuffer
	{
	public:
		static PhysicsLogBuffer& GetInstance();

		void AddLog(const std::string& log);
		void AddLog(const char* format, ...);

		void Clear();
		size_t GetLogCount() const;
		const std::string& GetLog(size_t index) const;
		const std::deque<std::string>& GetAllLogs() const;

		void SetMaxLogCount(size_t maxCount);

	private:
		PhysicsLogBuffer();
		~PhysicsLogBuffer() = default;

		PhysicsLogBuffer(const PhysicsLogBuffer&) = delete;
		PhysicsLogBuffer& operator=(const PhysicsLogBuffer&) = delete;

		std::deque<std::string> mLogs;
		mutable std::mutex mMutex;
		size_t mMaxLogCount;
	};
}

#define PhysicsLog(...) PhysicsBlock::PhysicsLogBuffer::GetInstance().AddLog(__VA_ARGS__)