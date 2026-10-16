#pragma once

#include "BaseStruct3D.hpp"

#if TranslatorLocality3D
#include "../Tool/json.hpp"
#else
#include "../samples/json.hpp"
#endif

// 名字字符串化宏（与 2D 版同名会冲突，故加 3D 后缀）
#define TOSIRING3D(str) #str

// 序列化功能开关（3D 独立开关，便于单独关闭）
#ifndef PhysicsBlock_Serialization3D
#define PhysicsBlock_Serialization3D 1
#endif

#if PhysicsBlock_Serialization3D
// 序列化继承宏（单继承）
#define SerializationInherit3D_ : public BaseSerialization3D

// 序列化继承宏（多重继承）
#define SerializationInherit3D , public BaseSerialization3D

// 序列化虚函数声明宏
#define SerializationVirtualFunction3D                                        \
    virtual void JsonSerialization(nlohmann::json_abi_v3_12_0::basic_json<> &data); \
    virtual void JsonContrarySerialization(const nlohmann::json_abi_v3_12_0::basic_json<> &data);

// Vec3 向量序列化宏（2D 的 SerializationVec2 对应物）
// @warning 本宏用 `TOSIRING3D(Vec.x)` 把**整个实参的拼写**变成键名前缀，因此
//          只对"裸标识符"成立（如 `pos`、`Wind`、`GravityAcceleration`）。
//          若传入 member 表达式（如 `OutlineSet[i].LocalPos`），序列化侧拼出的键
//          是 "OutlineSet[i].LocalPos.x"，而反序列化侧若写成 `p.LocalPos` 就会去读
//          "p.LocalPos.x"，两侧不一致 → 反序列化抛 `type_error.302`。
//          遇到 member 表达式请手写 `Json["x"] = Vec.x;` 这类字面键。
#define SerializationVec3(Json, Vec)        \
    {                                       \
        Json[TOSIRING3D(Vec.x)] = Vec.x;    \
        Json[TOSIRING3D(Vec.y)] = Vec.y;    \
        Json[TOSIRING3D(Vec.z)] = Vec.z;    \
    }

// Vec3 向量反序列化宏
#define ContrarySerializationVec3(Json, Vec) \
    {                                        \
        Vec.x = Json[TOSIRING3D(Vec.x)];     \
        Vec.y = Json[TOSIRING3D(Vec.y)];     \
        Vec.z = Json[TOSIRING3D(Vec.z)];     \
    }

// ivec3 序列化宏
#define SerializationIVec3(Json, Vec)       \
    {                                       \
        Json[TOSIRING3D(Vec.x)] = Vec.x;    \
        Json[TOSIRING3D(Vec.y)] = Vec.y;    \
        Json[TOSIRING3D(Vec.z)] = Vec.z;    \
    }

#define ContrarySerializationIVec3(Json, Vec) \
    {                                         \
        Vec.x = Json[TOSIRING3D(Vec.x)];      \
        Vec.y = Json[TOSIRING3D(Vec.y)];      \
        Vec.z = Json[TOSIRING3D(Vec.z)];      \
    }

// 四元数序列化宏（3D 旋转用 quat 而非 2D 的标量 angle）
#define SerializationQuat(Json, Q)          \
    {                                       \
        Json[TOSIRING3D(Q.w)] = Q.w;        \
        Json[TOSIRING3D(Q.x)] = Q.x;        \
        Json[TOSIRING3D(Q.y)] = Q.y;        \
        Json[TOSIRING3D(Q.z)] = Q.z;        \
    }

#define ContrarySerializationQuat(Json, Q) \
    {                                      \
        Q.w = Json[TOSIRING3D(Q.w)];       \
        Q.x = Json[TOSIRING3D(Q.x)];       \
        Q.y = Json[TOSIRING3D(Q.y)];       \
        Q.z = Json[TOSIRING3D(Q.z)];       \
    }

// 3×3 矩阵序列化宏（惯量张量 / 有效质量矩阵）
#define SerializationMat3(Json, M)                    \
    {                                                 \
        for (int _r = 0; _r < 3; ++_r)                \
            for (int _c = 0; _c < 3; ++_c)            \
                Json["m" + std::to_string(_r) +       \
                     std::to_string(_c)] = M[_r][_c]; \
    }

#define ContrarySerializationMat3(Json, M)                        \
    {                                                             \
        for (int _r = 0; _r < 3; ++_r)                            \
            for (int _c = 0; _c < 3; ++_c)                        \
                M[_r][_c] = Json["m" + std::to_string(_r) +       \
                                     std::to_string(_c)];         \
    }
#else
#define SerializationInherit3D_
#define SerializationInherit3D
#define SerializationVirtualFunction3D
#define SerializationVec3(Json, Vec)
#define ContrarySerializationVec3(Json, Vec)
#define SerializationIVec3(Json, Vec)
#define ContrarySerializationIVec3(Json, Vec)
#define SerializationQuat(Json, Q)
#define ContrarySerializationQuat(Json, Q)
#define SerializationMat3(Json, M)
#define ContrarySerializationMat3(Json, M)
#endif

#if PhysicsBlock_Serialization3D
namespace PhysicsBlock3D
{

    /**
     * @brief 序列化基类（3D）
     * @details 与 2D BaseSerialization 结构一致，仅改名以避免 ODR 冲突。 */
    class BaseSerialization3D
    {
    public:
        BaseSerialization3D() {};
        ~BaseSerialization3D() {};

        virtual void JsonSerialization(nlohmann::json_abi_v3_12_0::basic_json<> &data) {};
        virtual void JsonContrarySerialization(const nlohmann::json_abi_v3_12_0::basic_json<> &data) {};
    };

}
#endif
