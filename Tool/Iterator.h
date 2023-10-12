#pragma once
//迭代器模块
template <typename T>
class Iterator {
private:
    T* mPtr;
public:
    Iterator(T* ptr) : mPtr(ptr) {}

    Iterator operator++() {
        ++mPtr;
        return *this;
    }

    bool operator!=(const Iterator& other) const {
        return mPtr != other.mPtr;
    }

    T& operator*() {
        return *mPtr;
    }
};

template <typename T>
class DataIterator {
private:
    Iterator<T> mBegin;
    Iterator<T> mEnd;
public:
    DataIterator(T* Begin, unsigned int size): mBegin(Begin), mEnd(Begin + size){}

    Iterator<T> begin() {
        return mBegin;
    }

    Iterator<T> end() {
        return mEnd;
    }
};

template <typename T>
class PointerIterator {
private:
    class AddressIterator {
    private:
        T* mPtr;
    public:
        AddressIterator(T* ptr) : mPtr(ptr) {}

        AddressIterator operator++() {
            ++mPtr;
            return *this;
        }

        bool operator!=(const AddressIterator& other) const {
            return mPtr != other.mPtr;
        }

        T* operator*() {
            return mPtr;
        }
    };

    AddressIterator mBegin;
    AddressIterator mEnd;
public:
    PointerIterator(T* Begin, unsigned int size) : mBegin(Begin), mEnd(Begin + size) {}

    AddressIterator begin() {
        return mBegin;
    }

    AddressIterator end() {
        return mEnd;
    }
};

