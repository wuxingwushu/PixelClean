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

	D Use() {
		return mData;
	}

	void Limit() {
		if (mUseNumber != 0) {
			--mUseNumber;
		}
	}

	void Delete() {
		if (mUseNumber == 0) {
			delete mData;
		}
	}

	bool LimitDelete() {
		Limit();
		if (mUseNumber == 0) {
			delete mData;
			return true;
		}
		return false;
	}

	unsigned int GetUseNumber() {
		return mUseNumber;
	}

	~LimitUse(){}
};