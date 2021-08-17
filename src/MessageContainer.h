#ifndef MESSAGECONTAINER_H
#define MESSAGECONTAINER_H

#include <steam/isteamnetworkingutils.h>
#include "flatbuffers/flatbuffers.h"

/**
 * This class holds a pointer to the data and retains ownership of the buffer containing that data.
 * @tparam T MessageType
 */
template<typename T>
class MessageContainer {
private:
    ISteamNetworkingMessage *buffer = nullptr;
    const T *data = nullptr;

public:
    MessageContainer()
            : buffer(nullptr), data(nullptr)
    {}

    MessageContainer(ISteamNetworkingMessage *buffer, const T *data)
            : buffer(buffer), data(data)
    {}

    MessageContainer(MessageContainer &&other) noexcept
            : MessageContainer(other.buffer, other.data)
    {
        other.buffer = nullptr;
        other.data = nullptr;
    }

    ~MessageContainer() {
        FreeBuffer();
    }

    constexpr MessageContainer<T> &operator=(MessageContainer<T> &&other) noexcept
    {
        FreeBuffer();
        buffer = other.buffer;
        data = other.data;
        other.buffer = nullptr;
        other.data = nullptr;
        return *this;
    }

    const T &Data() const
    {
        return *data;
    }

private:
    void FreeBuffer()
    {
        if (buffer != nullptr)
        {
            buffer->Release();
        }
    }

};

#endif // MESSAGECONTAINER_H
