// Copyright 2021 Chia Network Inc

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//    http://www.apache.org/licenses/LICENSE-2.0

// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef SRC_SERIALIZE_HPP_
#define SRC_SERIALIZE_HPP_

#include <stdexcept>
#include <string>
#include <vector>

template<typename Type> class Serializable{
public:
    static void SerializeImpl(const Type& in, std::vector<uint8_t>& out)
    {
        size_t nSize = sizeof(in);
        out.reserve(out.size() + nSize);
        for (size_t i = 0; i < nSize; ++i) {
            out.push_back(*((uint8_t*)&in + i));
        }
    }
    static size_t DeserializeImpl(std::vector<uint8_t>& in, Type& out, size_t nPosition)
    {
        size_t nSize = sizeof(out);
        if (nPosition + nSize > in.size()) {
            throw std::invalid_argument("DeserializeImpl: Trying to exceed bounds.");
        }
        for (size_t i = 0; i < nSize; ++i) {
            *((uint8_t*)&out + i) = in[nPosition + i];
        }
        return nSize;
    }
};

template<typename TypeIn, typename TypeOut>
void Serialize(const TypeIn& in, TypeOut& out)
{
    Serializable<TypeIn>::SerializeImpl(in, out);
}

template<typename TypeIn, typename TypeOut>
size_t Deserialize(TypeIn& in, TypeOut& out, const size_t nPosition)
{
    return Serializable<TypeOut>::DeserializeImpl(in, out, nPosition);
}

template<typename TypeIn, typename TypeOut>
void SerializeContainer(const TypeIn& in, TypeOut& out)
{
    Serialize(in.size(), out);
    for (auto& entry : in) {
        Serialize(entry, out);
    }
}

template<typename TypeIn, typename TypeOut>
size_t DeserializeContainer(TypeIn& in, TypeOut& out, const size_t nPosition)
{
    size_t nCount;
    size_t nSerialized = Deserialize(in, nCount, nPosition);
    if (nCount == 0) {
        return nSerialized;
    }
    out.clear();
    out.resize(nCount);
    for (size_t i = 0; i < nCount; ++i) {
        nSerialized += Deserialize(in, out[i], nPosition + nSerialized);
    }
    return nSerialized;
}

template<typename Type>
class Serializable<std::vector<Type>>{
public:
    static void SerializeImpl(const std::vector<Type>& in, std::vector<uint8_t>& out)
    {
        SerializeContainer(in, out);
    }
    static size_t DeserializeImpl(std::vector<uint8_t>& in, std::vector<Type>& out, const size_t nPosition)
    {
        return DeserializeContainer(in, out, nPosition);
    }
};

template<>
class Serializable<std::string>{
public:
    static void SerializeImpl(const std::string& in, std::vector<uint8_t>& out)
    {
        SerializeContainer(in, out);
    }
    static size_t DeserializeImpl(std::vector<uint8_t>& in, std::string& out, const size_t nPosition)
    {
        return DeserializeContainer(in, out, nPosition);
    }
};

class Serializer
{
    std::vector<uint8_t> target;
public:
    template<typename AddType>
    void Append(AddType value)
    {
        Serialize(value, target);
    }
    std::vector<uint8_t>& Data()
    {
        return target;
    }
    void Reset()
    {
        target.clear();
    }
};


class Deserializer
{
    size_t nPosition{0};
    std::vector<uint8_t>& source;
public:
    explicit Deserializer(std::vector<uint8_t>& value) : source(value) {}
    void Reset()
    {
        nPosition = 0;
    }
    template<typename TargetType>
    TargetType Get()
    {
        TargetType target;
        nPosition += Deserialize(source, target, nPosition);
        return target;
    }
    bool End()
    {
        return nPosition == source.size();
    }
};

#endif  // SRC_SERIALIZE_HPP_
