#pragma once
#include <cstring>
#include <cmath>

template<typename T>
class ReplicatedProperty {
public:
	ReplicatedProperty() = default;

	ReplicatedProperty(const ReplicatedProperty&) = delete;
	ReplicatedProperty& operator=(const ReplicatedProperty&) = delete;
	ReplicatedProperty(ReplicatedProperty&&) = default;
	ReplicatedProperty& operator=(ReplicatedProperty&&) = default;

	void Set(const T& value) {
		if (mValue != value) {
			mValue = value;
			mDirty = true;
		}
	}

	const T& Get() const { return mValue; }

	void ForceDirty() { mDirty = true; }

	bool IsDirty() const { return mDirty; }
	void ClearDirty() { mDirty = false; }

	operator const T&() const { return mValue; }

private:
	T mValue{};
	bool mDirty = false;
};

template<>
inline void ReplicatedProperty<float>::Set(const float& value) {
	if (std::abs(mValue - value) > 0.001f) {
		mValue = value;
		mDirty = true;
	}
};