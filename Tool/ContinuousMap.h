#pragma once
#include <unordered_map>
#include <time.h>
#include <iostream>
#include "Iterator.h"

typedef const int ContinuousMapFlags;
enum ContinuousMapFlags_
{
    ContinuousMap_None       = 0,
    ContinuousMap_New        = 1 << 0, // 开启  没有键就自动生成
    ContinuousMap_Debug      = 1 << 1, // 开启  事件打印
    ContinuousMap_Timeout    = 1 << 2, // 开启  超时检测
    ContinuousMap_Pointer    = 1 << 3, // 开启  指针更新（在移动数据时更新对应引用）
    ContinuousMap_Expansion  = 1 << 4  // 开启  自动扩容
};


template <typename TKey, typename TData>
class ContinuousMap
{
private:

    typedef void (*_DeleteCallback)(TData data, void* ptr);//销毁 回调函数类型
    _DeleteCallback mDeleteCallback = nullptr;//用户自定义销毁 回调函数
    void* mDeleteData = nullptr;

    typedef bool (*_TimeoutCallback)(TData data, void* ptr);//超时 回调函数类型（返回值 表示释放要调用 销毁 函数）
    _TimeoutCallback mTimeoutCallback = nullptr;//用户自定义超时 回调函数
    void* mTimeoutData = nullptr;

    typedef void (*_PointerCallback)(TData* data, void* ptr);//更新引用指针对象回调函数
    _PointerCallback mPointerCallback = nullptr;//用户自定义更新指针 回调函数
    void** mPointerData = nullptr;//用户自定义更新指针 回调函数

    TKey* KeyS = nullptr;//键
    TData* DataS = nullptr;//数据
    ContinuousMapFlags mFlags = ContinuousMap_None;//配置信息
    clock_t* TimeS = nullptr;//键对应的更新时间（Get算更新）
    clock_t TimeoutTime = 3000;//默认三秒
    unsigned int Max = 0;//最多容量
    unsigned int Number = 0;//当前容量
    std::unordered_map<TKey, unsigned int> Dictionary;//索引对应数据
public:

    /**
     * @brief 构造函数
     * @param siez 初始数据大小
     * @param flags 配置信息 */
    constexpr ContinuousMap(const unsigned int siez, ContinuousMapFlags flags = ContinuousMap_None) :
        mFlags(flags),
        Max(siez)
    {
        KeyS = new TKey[Max];
        DataS = new TData[Max];
        if (mFlags & ContinuousMap_Timeout) {
            TimeS = new clock_t[Max];
        }
        if (mFlags & ContinuousMap_Pointer){
            mPointerData = new void* [Max];
        }
    }

    ~ContinuousMap(){
        delete[] KeyS;
        delete[] DataS;
        if (mFlags & ContinuousMap_Timeout) {
            delete[] TimeS;
        }
        if (mFlags & ContinuousMap_Pointer) {
            delete[] mPointerData;
        }
    }
    
    /**
     * @brief 添加新 Map&
     * @param key 键值对的键
     * @return 返回分配数据空间的指针 */
    [[nodiscard]] inline TData* New(TKey key){
        if (Max == Number)//判断是否达到上线
        {
            if (mFlags & ContinuousMap_Expansion) {
                Expansion();
            }
            else {
                if (mFlags & ContinuousMap_Debug) {
                    std::cerr << "创建 " << key << " 失败：达到上线！" << std::endl;
                }
                return nullptr;
            }
        }
        if (Dictionary.find(key) != Dictionary.end())//判断键是否存在
        {
            if (mFlags & ContinuousMap_Debug) {
                std::cerr << "存在 " << key << " 创建失败！" << std::endl;
            }
            return &DataS[Number++];
        }
        KeyS[Number] = key;//获取最前面的空位储存键
        if (mFlags & ContinuousMap_Timeout){
            TimeS[Number] = clock();//获取时间戳
        }
        Dictionary.insert(std::make_pair(key, Number));//添加到Map
        if (mFlags & ContinuousMap_Debug) {
            std::cerr << "创建 " << key << " 成功！" << std::endl;
        }
        return &DataS[Number++];//返回数据指针
    };
    
    /**
     * @brief 根据 key& 获取对应映射
     * @param key 键值对的键
     * @return 返回分配数据空间的指针 */
    [[nodiscard]] inline TData* Get(TKey key){
        if (Dictionary.find(key) == Dictionary.end())//判断是否存在键
        {
            if (mFlags & ContinuousMap_Debug) {
                std::cerr << "没有 " << key << " !" << std::endl;
            }
            if (mFlags & ContinuousMap_New) {
                return New(key);
            }
            return nullptr;
        }
        if (mFlags & ContinuousMap_Timeout) {
            unsigned int dictionary = Dictionary[key];//获取键
            TimeS[dictionary] = clock();//更新键的时间搓
            return &DataS[dictionary];//返回数据指针
        }
        return &DataS[Dictionary[key]];//返回数据指针
    }

    /**
     * @brief 绑定销毁回调函数
     * @param DeleteCallback 销毁函数指针
     * @param Data 传入数据指针 */
    void inline SetDeleteCallback(_DeleteCallback DeleteCallback, void* Data) noexcept {
        mDeleteCallback = DeleteCallback;//设置销毁回调函数
        mDeleteData = Data;
    }

    /**
     * @brief 根据 key& 销毁对应数据
     * @param key 键值对的键 */
    void Delete(TKey key){
        if (Dictionary.find(key) == Dictionary.end())//判断是否存在键
        {
            if (mFlags & ContinuousMap_Debug) {
                std::cerr << "释放 " << key << " 失败： 没有找到！" << std::endl;
            }
            return;
        }
        if (mDeleteCallback != nullptr) {//判断是否有销毁回调函数
            mDeleteCallback(*Get(key), mDeleteData);//调用对应的回调函数
        }
        --Number;
        unsigned int keyData = Dictionary[key];//获取要销毁的键
        if(keyData != Number){//判断不是在最后面
            //和最后一个的键交换位置
            DataS[keyData] = std::move(DataS[Number]);
            KeyS[keyData] = std::move(KeyS[Number]);
            if (mFlags & ContinuousMap_Timeout) {
                TimeS[keyData] = std::move(TimeS[Number]);
            }
            Dictionary[KeyS[Number]] = keyData;
            if (mFlags & ContinuousMap_Pointer) {
                assert(mPointerCallback != nullptr && "[Error]: mPointerCallback = nullptr !");
                mPointerData[keyData] = std::move(mPointerData[Number]);
                mPointerCallback(&DataS[keyData], mPointerData[keyData]);
            }
        }
        Dictionary.erase(key);//Map销毁
        if (mFlags & ContinuousMap_Debug) {
            std::cerr << "销毁 " << key << " 成功！" << std::endl;
        }
    }

    /**
     * @brief 超时检测 */
    void inline TimeoutDetection() {
        assert(mFlags & ContinuousMap_Timeout && "Not Turned On ContinuousMap_Timeout");
        clock_t time = clock();
        for (size_t i = 0; i < Number; ++i)
        {
            if ((time - TimeS[i]) > TimeoutTime) {//判断是否超时
                if (mFlags & ContinuousMap_Debug) {
                    std::cerr << KeyS[i] << " 超时： 销毁！" << std::endl;
                }
                if (mTimeoutCallback(*Get(KeyS[i]), mTimeoutData)) {
                    Delete(KeyS[i]);//销毁超时的键
                }
            }
        }
    }

    /**
     * @brief 设置超时回调函数
     * @param TimeoutCallback 超时函数指针
     * @param Data 传入数据 */
    void inline SetTimeoutCallback(_TimeoutCallback TimeoutCallback, void* Data) noexcept {
        assert(mFlags & ContinuousMap_Timeout && "Not Turned On ContinuousMap_Timeout");
        mTimeoutCallback = TimeoutCallback;
        mTimeoutData = Data;
    }

    /**
     * @brief 设置超时时间
     * @param Time 超时时间 */
    void inline SetTimeoutTime(clock_t Time) noexcept {
        assert(mFlags & ContinuousMap_Timeout && "Not Turned On ContinuousMap_Timeout");
        TimeoutTime = Time;
    }

    /**
     * @brief 更新所有时间 
     * @note 清空所有数据的计时*/
    void inline UpDataWholeTime() noexcept {
        assert(mFlags & ContinuousMap_Timeout && "Not Turned On ContinuousMap_Timeout");
        clock_t time = clock();
        for (size_t i = 0; i < Number; ++i)
        {
            TimeS[i] = time;
        }
    }

    /**
     * @brief 数据对齐时更新引用对象绑定的数据指针
     * @param PointerCallback 更新引用回调函数指针 */
    void inline SetPointerCallback(_PointerCallback PointerCallback) noexcept {
        assert(mFlags & ContinuousMap_Pointer && "Not Turned On ContinuousMap_Pointer");
        mPointerCallback = PointerCallback;
    }

    /**
     * @brief 绑定数据引用对象
     * @param key 键值对的键
     * @param Data 更新引用的对象指针 */
    void inline SetPointerData(TKey key, void* Data) {
        assert(mFlags & ContinuousMap_Pointer && "Not Turned On ContinuousMap_Pointer");
        mPointerData[Dictionary[key]] = Data;
    }

    /**
     * @brief 获取除 Key& 以外的所有 Data （且将 Key 移动最后面）
     * @param key 键值对的键
     * @return 返回数据指针 */
    [[nodiscard]] TData* GetKeyData(TKey key) {
        if (Dictionary.find(key) == Dictionary.end()) {//判断是否存在键
            return DataS;
        }
        unsigned int keyData = Dictionary[key];//获取键
        if (keyData != (Number - 1)) {//判断键是否是最后一个
            //和最后一个键交换位置
            Dictionary[key] = Number - 1;
            Dictionary[KeyS[Number - 1]] = keyData;
            std::swap<TKey>(KeyS[keyData], KeyS[Number - 1]);
            TData LSData = std::move(DataS[keyData]);
            DataS[keyData] = std::move(DataS[Number - 1]);
            DataS[Number - 1] = std::move(LSData);
            LSData = TData{};//清空，防止函数结束释放时调用 ~析构（），导致资源被释放; 
            //std::swap<TData>(DataS[keyData], DataS[Number - 1]);
            if (mFlags & ContinuousMap_Timeout) {
                std::swap<clock_t>(TimeS[keyData], TimeS[Number - 1]);
            }
            if (mFlags & ContinuousMap_Pointer) {
                assert(mPointerCallback != nullptr && "[Error]: mPointerCallback = nullptr !");
                void* LPointer = std::move(mPointerData[keyData]);
                mPointerData[keyData] = std::move(mPointerData[Number - 1]);
                mPointerData[Number - 1] = std::move(LPointer);

                mPointerCallback(&DataS[keyData], mPointerData[keyData]);
                mPointerCallback(&DataS[Number - 1], mPointerData[Number - 1]);
            }
        }
        return DataS;
    }

    /**
     * @brief 获取对应键值
     * @param i 第几个数据
     * @return 返回键值 */
    [[nodiscard]] inline TKey GetIndexKey(unsigned int i) noexcept {
        return KeyS[i];
    }

    /**
     * @brief 迭代器头
     * @return 迭代器 */
    Iterator<TData> inline begin() {
        return Iterator<TData>(DataS);
    }

    /**
     * @brief 迭代器尾
     * @return 迭代器 */
    Iterator<TData> inline end() {
        return Iterator<TData>(DataS + Number);
    }

    /**
     * @brief 获取 Data 数量 - 1
     * @return Data 数量 - 1 */
    [[nodiscard]] inline unsigned int GetKeyNumber() noexcept {
        return (Number - 1) > Max ? 0 : (Number - 1);
    }

    //获取除 Key 以外 Data 数量的数据一共多少字节

    /**
     * @brief 获取（Data数据 - 1）一共多少字节
     * @return 字节数 */
    [[nodiscard]] inline unsigned int GetKeyDataSize() noexcept {
        return GetKeyNumber() * sizeof(TData);
    }

    /**
     * @brief 获取 Data 指针
     * @return Data 指针 */
    [[nodiscard]] inline constexpr TData* GetData() noexcept {
        return DataS;
    }

    /**
     * @brief 获取 Data 数量
     * @return Data 数量 */
    [[nodiscard]] inline unsigned int GetNumber() noexcept {
        return Number;
    }

    /**
     * @brief 获取 Data数据 一共多少字节
     * @return 字节数 */
    [[nodiscard]] inline unsigned int GetDataSize() noexcept {
        return Number * sizeof(TData);
    }

    /**
     * @brief 扩容函数 */
    void inline Expansion() {
        TKey* PKeyS = KeyS;
        TKey* LKeyS = new TKey[Max * 2];
        TKey* WKeyS = LKeyS;
        TData* PDataS = DataS;
        TData* LDataS = new TData[Max * 2];
        TData* WDataS = LDataS;
        for (size_t i = 0; i < Max; i++)
        {
            *LKeyS = *PKeyS;
            ++PKeyS;
            ++LKeyS;
            *LDataS = *PDataS;
            ++PDataS;
            ++LDataS;
        }
        delete KeyS;
        KeyS = WKeyS;
        delete DataS;
        DataS = WDataS;

        
        if (mFlags & ContinuousMap_Timeout) {
            clock_t* PTimeS = TimeS;
            clock_t* LTimeS = new clock_t[Max * 2];
            clock_t* WTimeS = LTimeS;
            for (size_t i = 0; i < Max; i++)
            {
                *LTimeS = *PTimeS;
                ++PTimeS;
                ++LTimeS;
            }
            delete TimeS;
            TimeS = WTimeS;
        }
        if (mFlags & ContinuousMap_Pointer) {
            void** PPointerData = mPointerData;
            void** LPointerData = new void* [Max * 2];
            void** WPointerData = LPointerData;
            for (size_t i = 0; i < Max; i++)
            {
                *LPointerData = *PPointerData;
                ++PPointerData;
                ++LPointerData;
            }
            delete mPointerData;
            mPointerData = WPointerData;
        }

        Max *= 2;
    }
};
