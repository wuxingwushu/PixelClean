#pragma once
//迭代器模块
template <typename T>
class Iterator {
private:
    T* mPtr;
public:
    Iterator(T* ptr) : mPtr(ptr) {}

    inline Iterator operator++() {
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

    inline Iterator<T> begin() {
        return mBegin;
    }

    inline Iterator<T> end() {
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

        inline AddressIterator operator++() {
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

    inline AddressIterator begin() {
        return mBegin;
    }

    inline AddressIterator end() {
        return mEnd;
    }
};

