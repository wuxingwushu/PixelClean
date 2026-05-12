#include "PhysicsLogBuffer.h"
#include "../../DebugLog.h"
#include <cstdarg>
#include <cstdio>

namespace PhysicsBlock
{
	PhysicsLogBuffer::PhysicsLogBuffer() : mMaxLogCount(1000)
	{
		LOGD("PhysicsLogBuffer::PhysicsLogBuffer constructor");
	}

	PhysicsLogBuffer& PhysicsLogBuffer::GetInstance()
	{
		static PhysicsLogBuffer instance;
		return instance;
	}

	void PhysicsLogBuffer::AddLog(const std::string& log)
	{
		std::lock_guard<std::mutex> lock(mMutex);
		mLogs.push_back(log);
		while (mLogs.size() > mMaxLogCount)
		{
			mLogs.pop_front();
		}
	}

	void PhysicsLogBuffer::AddLog(const char* format, ...)
	{
		va_list args;
		va_start(args, format);
		
		char buffer[4096];
		vsnprintf(buffer, sizeof(buffer), format, args);
		
		va_end(args);
		
		AddLog(std::string(buffer));
	}

	void PhysicsLogBuffer::Clear()
	{
		std::lock_guard<std::mutex> lock(mMutex);
		mLogs.clear();
	}

	size_t PhysicsLogBuffer::GetLogCount() const
	{
		std::lock_guard<std::mutex> lock(mMutex);
		return mLogs.size();
	}

	const std::string& PhysicsLogBuffer::GetLog(size_t index) const
	{
		std::lock_guard<std::mutex> lock(mMutex);
		if (index >= mLogs.size())
		{
			LOGE("PhysicsLogBuffer::GetLog index %zu out of range (size %zu)", index, mLogs.size());
			static const std::string empty;
			return empty;
		}
		return mLogs[index];
	}

	const std::deque<std::string>& PhysicsLogBuffer::GetAllLogs() const
	{
		return mLogs;
	}

	void PhysicsLogBuffer::SetMaxLogCount(size_t maxCount)
	{
		std::lock_guard<std::mutex> lock(mMutex);
		mMaxLogCount = maxCount;
		while (mLogs.size() > mMaxLogCount)
		{
			mLogs.pop_front();
		}
	}
}