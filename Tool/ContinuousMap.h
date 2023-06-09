#pragma once
#include <map>

//#define ContinuousMap_New             // 开启  没有键就自动生成
//#define ContinuousMap_Debug             // 开启  事件打印
#define ContinuousMap_Timeout			// 开启  超时检测

#ifdef ContinuousMap_Debug// 开启  事件打印
#include <iostream>
#endif

#ifdef ContinuousMap_Timeout// 开启  超时检测
#include <time.h>
#endif


//适用于储存数据（不适应 Class）



template <typename TKey, typename TData>
class ContinuousMap
{
private:
    typedef void (*_DeleteCallback)(TData data);//销毁 回调函数类型
    typedef bool (*_TimeoutCallback)(TData data);//销毁 回调函数类型（返回值 表示释放要调用 销毁 函数）

    _DeleteCallback DeleteCallback = nullptr;//用户自定义销毁 回调函数
    _TimeoutCallback TimeoutCallback = nullptr;//用户自定义超时 回调函数
    TKey* KeyS;//键
    TData* DataS;//数据
#ifdef ContinuousMap_Timeout
    clock_t* TimeS;//键对应的更新时间（Get算更新）
    clock_t TimeoutTime = 3000;//默认三秒
#endif
    unsigned int Max;//最多容量
    unsigned int Number = 0;//当前容量
    std::map<TKey, unsigned int> Dictionary;//索引对应数据
public:
    ContinuousMap(unsigned int siez){
        Max = siez;
        KeyS = new TKey[Max];
        DataS = new TData[Max];
#ifdef ContinuousMap_Timeout
        TimeS = new clock_t[Max];
#endif
    }

    ~ContinuousMap(){
        delete KeyS;
        delete DataS;
#ifdef ContinuousMap_Timeout
        delete TimeS;
#endif
    }

    //添加新 Map
    TData* New(TKey key){
        if (Max == Number)//判断是否达到上线
        {
#ifdef ContinuousMap_Debug
            std::cerr << "创建 " << key << " 失败：达到上线！" << std::endl;
#endif
            return nullptr;
        }
        if (Dictionary.find(key) != Dictionary.end())//判断键是否存在
        {
#ifdef ContinuousMap_Debug
            std::cerr << "存在 " << key << " 创建失败！" << std::endl;
#endif
        }
        KeyS[Number] = key;//获取最前面的空位储存键
#ifdef ContinuousMap_Timeout
        TimeS[Number] = clock();//获取时间戳
#endif
        Dictionary.insert(std::make_pair(key, Number));//添加到Map
#ifdef ContinuousMap_Debug
        std::cerr << "创建 " << key << " 成功！" << std::endl;
#endif
        return &DataS[Number++];//返回数据指针
    };

    //更加 key 获取对应映射
    TData* Get(TKey key){
        if (Dictionary.find(key) == Dictionary.end())//判断是否存在键
        {
#ifdef ContinuousMap_Debug
            std::cerr << "没有 " << key << " !" << std::endl;
#endif
#ifdef ContinuousMap_New
            return New(key);
#else
            return nullptr;
#endif
        }
#ifdef ContinuousMap_Timeout
        unsigned int dictionary = Dictionary[key];//获取键
        TimeS[dictionary] = clock();//更新键的时间搓
        return &DataS[dictionary];//返回数据指针
#else
        return &DataS[Dictionary[key]];//返回数据指针
#endif
    }

    void SetDeleteCallback(_DeleteCallback mDeleteCallback) {
        DeleteCallback = mDeleteCallback;//设置销毁回调函数
    }

    //销毁 TKey 对应的映射
    void Delete(TKey key){
        if (Dictionary.find(key) == Dictionary.end())//判断是否存在键
        {
#ifdef ContinuousMap_Debug
            std::cerr << "释放 " << key << " 失败： 没有找到！" << std::endl;
#endif
            return;
        }
        if (DeleteCallback != nullptr) {//判断是否有销毁回调函数
            DeleteCallback(*Get(key));//调用对应的回调函数
        }
        Number--;
        unsigned int keyData = Dictionary[key];//获取要销毁的键
        if(keyData != Number){//判断不是在最后面
            //和最后一个的键交换位置
            DataS[keyData] = DataS[Number];
            KeyS[keyData] = KeyS[Number];
#ifdef ContinuousMap_Timeout
            TimeS[keyData] = TimeS[Number];
#endif
            Dictionary[KeyS[Number]] = keyData;
        }
        Dictionary.erase(key);//Map销毁
#ifdef ContinuousMap_Debug
        std::cerr << "销毁 " << key << " 成功！" << std::endl;
#endif
    }

#ifdef ContinuousMap_Timeout
    //超时检测
    void TimeoutDetection() {
        clock_t time = clock();
        for (size_t i = 0; i < Number; i++)
        {
            if ((time - TimeS[i]) > TimeoutTime) {//判断是否超时
#ifdef ContinuousMap_Debug
                std::cerr << KeyS[i] << " 超时： 销毁！" << std::endl;
#endif
                if (TimeoutCallback != nullptr) {
                    if (TimeoutCallback(*Get(KeyS[i]))) {
                        Delete(KeyS[i]);//销毁超时的键
                    }
                }
                else {
                    Delete(KeyS[i]);//销毁超时的键
                }
            }
        }
    }

    //设置超时回调函数
    void SetTimeoutCallback(_TimeoutCallback mTimeoutCallback) {
        TimeoutCallback = mTimeoutCallback;
    }

    //设置超时时间
    void SetTimeoutTime(clock_t Time) {
        TimeoutTime = Time;
    }
#endif

    //获取除 Key 以外的所有 Data （且将 Key 移动最后面）
    TData* GetKeyData(TKey key) {
        if (Dictionary.find(key) == Dictionary.end()) {//判断是否存在键
            return DataS;
        }
        unsigned int keyData = Dictionary[key];//获取键
        if (keyData != (Number - 1)) {//判断键是否是最后一个
            //和最后一个键交换位置
            Dictionary[key] = Number - 1;
            Dictionary[KeyS[Number - 1]] = keyData;
            //std::swap<unsigned int>(, Dictionary[Number-1]);
            std::swap<TKey>(KeyS[keyData], KeyS[Number - 1]);
            std::swap<TData>(DataS[keyData], DataS[Number - 1]);
#ifdef ContinuousMap_Timeout
            std::swap<clock_t>(TimeS[keyData], TimeS[Number - 1]);
            
#endif
        }
        return DataS;
    }

    TKey GetIndexKey(unsigned i) {
        return KeyS[i];
    }

    //获取除 Key 以外 Data 数量
    unsigned int GetKeyNumber() {
        return (Number - 1) > Max ? 0 : (Number - 1);
    }

    //获取除 Key 以外 Data 数量的数据一共多少字节
    unsigned int GetKeyDataSize() {
        return GetKeyNumber() * sizeof(TData);
    }

    //获取所有 Data
    TData* GetData(){
        return DataS;
    }

    //获取所有 Data 数量
    unsigned int GetNumber(){
        return Number;
    }

    //获取 Data 数量的数据一共多少字节
    unsigned int GetDataSize(){
        return Number * sizeof(TData);
    }
};
