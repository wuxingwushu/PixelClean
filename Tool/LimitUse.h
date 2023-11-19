#pragma once

template <typename D>
class LimitUse
{
private:
	D mData;
	unsigned int mUseNumber = 0;
public:
	LimitUse(D Data, unsigned int UseNumber):
		mUseNumber(UseNumber),
		mData(Data){}

	[[nodiscard]] inline constexpr D Use() noexcept {
		return mData;
	}

	inline void Limit() noexcept {
		if (mUseNumber != 0) {
			--mUseNumber;
		}
	}

	inline void Delete() {
		if (mUseNumber == 0) {
			delete mData;
		}
	}

	inline bool LimitDelete() {
		Limit();
		if (mUseNumber == 0) {
			delete mData;
			return true;
		}
		return false;
	}

	[[nodiscard]] inline unsigned int GetUseNumber() {
		return mUseNumber;
	}

	~LimitUse(){}
};