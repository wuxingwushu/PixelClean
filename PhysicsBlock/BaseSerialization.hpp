#pragma once

#include "BaseStruct.hpp"

#if TranslatorLocality
#include "../Tool/json.hpp"
#else
#include "../samples/json.hpp"
#endif

// 名字字符串化宏
// 将宏参数转换为字符串字面量
#define TOSIRING(str) #str

// 序列化功能开关
// 设置为 1 启用序列化功能，设置为 0 禁用
#define PhysicsBlock_Serialization 1

#if PhysicsBlock_Serialization
// 序列化继承宏（用于单继承场景）
// 在类定义中使用，使类继承自 BaseSerialization
#define SerializationInherit_ : public BaseSerialization

// 序列化继承宏（用于多重继承场景）
// 在类定义中使用，添加 BaseSerialization 作为多重继承的一部分
#define SerializationInherit , public BaseSerialization

// 序列化虚函数声明宏
// 在类中声明 JsonSerialization 和 JsonContrarySerialization 两个虚函数
#define SerializationVirtualFunction \
    virtual void JsonSerialization(nlohmann::json_abi_v3_12_0::basic_json<> &data); \
    virtual void JsonContrarySerialization(const nlohmann::json_abi_v3_12_0::basic_json<> &data);

// Vec2 向量序列化宏
// 将 Vec2 类型的向量序列化为 JSON
// @param Json 输出的 JSON 对象
// @param Vec 要序列化的 Vec2 向量
#define SerializationVec2(Json, Vec) {Json[TOSIRING(Vec.x)] = Vec.x;Json[TOSIRING(Vec.y)] = Vec.y;} 

// Vec2 向量反序列化宏
// 从 JSON 中恢复 Vec2 类型的向量
// @param Json 输入的 JSON 对象
// @param Vec 要恢复的 Vec2 向量
#define ContrarySerializationVec2(Json, Vec) {Vec.x = Json[TOSIRING(Vec.x)];Vec.y = Json[TOSIRING(Vec.y)];}
#else
// 禁用序列化时的空宏定义
#define SerializationInherit_
#define SerializationInherit
#define SerializationVirtualFunction
#endif

#if PhysicsBlock_Serialization
namespace PhysicsBlock
{

    /**
     * @brief 序列化基类
     * @details 提供序列化和反序列化的接口基类
     * 所有需要支持序列化的类都应该继承此类
     */
    class BaseSerialization
    {
    public:
        /**
         * @brief 默认构造函数
         */
        BaseSerialization() {};
        
        /**
         * @brief 默认析构函数
         */
        ~BaseSerialization() {};

        /**
         * @brief 将对象序列化为 JSON
         * @param data 输出的 JSON 对象
         * @note 派生类应该重写此方法，将自身数据序列化到 JSON 中
         */
        virtual void JsonSerialization(nlohmann::json_abi_v3_12_0::basic_json<> &data) {};
        
        /**
         * @brief 从 JSON 反序列化对象
         * @param data 输入的 JSON 对象
         * @note 派生类应该重写此方法，从 JSON 中恢复自身数据
         */
        virtual void JsonContrarySerialization(const nlohmann::json_abi_v3_12_0::basic_json<> &data) {};
    };

}
#endif
