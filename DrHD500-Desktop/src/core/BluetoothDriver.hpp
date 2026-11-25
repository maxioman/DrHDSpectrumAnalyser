#pragma once

#include <string>
#include <vector>
#include <functional>
#include <cstdint>

namespace DrHD {

class BluetoothDriver {
public:
    using DataCallback = std::function<void(const uint8_t* data, size_t length)>;
    using StatusCallback = std::function<void(bool connected, const std::string& info)>;

    static BluetoothDriver& instance();

    bool connect(const std::string& macAddress = "01:AC:78:00:04:55");
    void disconnect();
    bool isConnected() const;

    void sendPacketPaced(const std::vector<uint8_t>& packet);

    void setDataCallback(DataCallback cb);
    void setStatusCallback(StatusCallback cb);

    struct Impl;

private:
    BluetoothDriver();
    ~BluetoothDriver();

    Impl* pImpl_ = nullptr;
};

} // namespace DrHD
