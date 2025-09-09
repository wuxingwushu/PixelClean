#pragma once

#include "BaseStruct.hpp"

#if TranslatorLocality
#include "../Tool/json.hpp"
#else
#include "../samples/json.hpp"
#endif

// 名字字符串化
#define TOSIRING(str) #str

#define PhysicsBlock_Serialization 1

#if PhysicsBlock_Serialization
#define SerializationInherit_ : public BaseSerialization
#define SerializationInherit , public BaseSerialization
#define SerializationVirtualFunction virtual void JsonSerialization(nlohmann::json_abi_v3_12_0::basic_json<> &data);virtual void JsonContrarySerialization(const nlohmann::json_abi_v3_12_0::basic_json<> &data);
#define SerializationVec2(Json, Vec) {Json[TOSIRING(Vec.x)] = Vec.x;Json[TOSIRING(Vec.y)] = Vec.y;} 
#define ContrarySerializationVec2(Json, Vec) {Vec.x = Json[TOSIRING(Vec.x)];Vec.y = Json[TOSIRING(Vec.y)];}
#else
#define SerializationInherit
#define SerializationVirtualFunction
#endif

#if PhysicsBlock_Serialization
namespace PhysicsBlock
{

    class BaseSerialization
    {
    public:
        BaseSerialization() {};
        ~BaseSerialization() {};

        virtual void JsonSerialization(nlohmann::json_abi_v3_12_0::basic_json<> &data) {};
        virtual void JsonContrarySerialization(const nlohmann::json_abi_v3_12_0::basic_json<> &data) {};
    };

}
#endif