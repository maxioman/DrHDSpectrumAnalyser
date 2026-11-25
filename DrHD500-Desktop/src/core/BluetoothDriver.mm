#include "BluetoothDriver.hpp"
#import <Foundation/Foundation.h>
#import <IOBluetooth/IOBluetooth.h>
#include <iostream>
#include <mutex>
#include <atomic>

namespace DrHD {

struct BluetoothDriver::Impl {
    IOBluetoothDevice* device = nil;
    IOBluetoothRFCOMMChannel* channel = nil;
    id delegate = nil;
    dispatch_queue_t txQueue = nullptr;
    std::atomic<bool> connected{false};
    std::string currentMac = "01:AC:78:00:04:55";
    BluetoothDriver::DataCallback dataCallback;
    BluetoothDriver::StatusCallback statusCallback;
    std::mutex cbMutex;
};

} // namespace DrHD

@interface DrHDRFCOMMDelegate : NSObject <IOBluetoothRFCOMMChannelDelegate>
@property (nonatomic, assign) DrHD::BluetoothDriver::Impl* impl;
@end

@implementation DrHDRFCOMMDelegate

- (void)rfcommChannelOpenComplete:(IOBluetoothRFCOMMChannel*)rfcommChannel status:(IOReturn)error {
    if (error == kIOReturnSuccess) {
        if (_impl) {
            _impl->connected = true;
            std::lock_guard<std::mutex> lock(_impl->cbMutex);
            if (_impl->statusCallback) {
                _impl->statusCallback(true, "Native IOBluetooth RFCOMM Connected");
            }
        }
    } else {
        if (_impl) {
            _impl->connected = false;
            std::lock_guard<std::mutex> lock(_impl->cbMutex);
            if (_impl->statusCallback) {
                _impl->statusCallback(false, "RFCOMM Open Failed: " + std::to_string(error));
            }
        }
    }
}

- (void)rfcommChannelData:(IOBluetoothRFCOMMChannel*)rfcommChannel data:(void *)dataPointer length:(size_t)dataLength {
    if (_impl && _impl->dataCallback && dataLength > 0) {
        std::lock_guard<std::mutex> lock(_impl->cbMutex);
        _impl->dataCallback(reinterpret_cast<const uint8_t*>(dataPointer), dataLength);
    }
}

- (void)rfcommChannelClosed:(IOBluetoothRFCOMMChannel*)rfcommChannel {
    if (_impl) {
        _impl->connected = false;
        _impl->channel = nil;
        std::lock_guard<std::mutex> lock(_impl->cbMutex);
        if (_impl->statusCallback) {
            _impl->statusCallback(false, "RFCOMM Channel Closed");
        }
    }
}

@end

namespace DrHD {

BluetoothDriver& BluetoothDriver::instance() {
    static BluetoothDriver driver;
    return driver;
}

BluetoothDriver::BluetoothDriver() {
    pImpl_ = new Impl();
    pImpl_->txQueue = dispatch_queue_create("com.drhd.bluetooth.tx", DISPATCH_QUEUE_SERIAL);
    pImpl_->delegate = [[DrHDRFCOMMDelegate alloc] init];
    static_cast<DrHDRFCOMMDelegate*>(pImpl_->delegate).impl = pImpl_;
}

BluetoothDriver::~BluetoothDriver() {
    disconnect();
    if (pImpl_->delegate) {
        static_cast<DrHDRFCOMMDelegate*>(pImpl_->delegate).impl = nullptr;
        pImpl_->delegate = nil;
    }
    delete pImpl_;
}

void BluetoothDriver::setDataCallback(DataCallback cb) {
    std::lock_guard<std::mutex> lock(pImpl_->cbMutex);
    pImpl_->dataCallback = cb;
}

void BluetoothDriver::setStatusCallback(StatusCallback cb) {
    std::lock_guard<std::mutex> lock(pImpl_->cbMutex);
    pImpl_->statusCallback = cb;
}

bool BluetoothDriver::isConnected() const {
    return pImpl_->connected;
}

bool BluetoothDriver::connect(const std::string& macAddress) {
    disconnect();

    @autoreleasepool {
        pImpl_->currentMac = macAddress;
        
        // Normalize MAC: IOBluetoothDevice deviceWithAddressString works with "01-ac-78-00-04-55" or "01:ac:78:00:04:55"
        std::string normMac = macAddress;
        for (char& c : normMac) c = std::tolower(c);

        NSString* nsMac = [NSString stringWithUTF8String:normMac.c_str()];
        IOBluetoothDevice* dev = [IOBluetoothDevice deviceWithAddressString:nsMac];
        if (!dev) {
            std::string hyphenMac = normMac;
            std::replace(hyphenMac.begin(), hyphenMac.end(), ':', '-');
            dev = [IOBluetoothDevice deviceWithAddressString:[NSString stringWithUTF8String:hyphenMac.c_str()]];
        }
        if (!dev) {
            std::string colonMac = normMac;
            std::replace(colonMac.begin(), colonMac.end(), '-', ':');
            dev = [IOBluetoothDevice deviceWithAddressString:[NSString stringWithUTF8String:colonMac.c_str()]];
        }
        if (!dev) {
            std::cerr << "[BluetoothDriver] Device not found for MAC: " << macAddress << std::endl;
            return false;
        }

        // CRITICAL ON MACOS: Bring up baseband connection first if not connected!
        // Calling openRFCOMMChannelSync on a disconnected device fails with 0xe00002d6 (kIOReturnNotOpen).
        if (![dev isConnected]) {
            std::cout << "[BluetoothDriver] Connecting baseband to " << macAddress << "..." << std::endl;
            IOReturn baseRet = [dev openConnection];
            if (baseRet != kIOReturnSuccess) {
                std::cerr << "[BluetoothDriver] openConnection notice: 0x" << std::hex << baseRet << std::dec << std::endl;
            }
        }

        pImpl_->device = dev;
        IOBluetoothRFCOMMChannel* chan = nil;
        
        // Open RFCOMM Channel 1 synchronously
        IOReturn ret = [dev openRFCOMMChannelSync:&chan withChannelID:1 delegate:static_cast<DrHDRFCOMMDelegate*>(pImpl_->delegate)];
        if (ret == kIOReturnSuccess && chan != nil) {
            pImpl_->channel = chan;
            pImpl_->connected = true;
            std::cout << "[BluetoothDriver] Successfully opened RFCOMM Channel 1 to " << macAddress << std::endl;
            if (pImpl_->statusCallback) {
                pImpl_->statusCallback(true, "Connected to " + macAddress);
            }
            return true;
        } else {
            std::cerr << "[BluetoothDriver] openRFCOMMChannelSync failed: 0x" << std::hex << ret << std::dec << std::endl;
            pImpl_->connected = false;
            pImpl_->channel = nil;
            if (pImpl_->statusCallback) {
                pImpl_->statusCallback(false, "Connection error: 0x" + std::to_string(ret));
            }
            return false;
        }
    }
}

void BluetoothDriver::disconnect() {
    @autoreleasepool {
        if (pImpl_->channel) {
            [pImpl_->channel closeChannel];
            pImpl_->channel = nil;
        }
        pImpl_->connected = false;
        pImpl_->device = nil;
    }
}

void BluetoothDriver::sendPacketPaced(const std::vector<uint8_t>& packet) {
    if (!pImpl_->connected || !pImpl_->channel || packet.empty()) return;

    // Dispatch to background txQueue so 130ms inter-chunk sleeps don't block the caller
    std::vector<uint8_t> pktCopy = packet;
    dispatch_async(pImpl_->txQueue, ^{
        @autoreleasepool {
            if (!pImpl_->connected || !pImpl_->channel) return;

            for (size_t i = 0; i < pktCopy.size(); i += 2) {
                size_t chunkLen = std::min<size_t>(2, pktCopy.size() - i);
                IOReturn ret = [pImpl_->channel writeSync:const_cast<uint8_t*>(&pktCopy[i]) length:static_cast<UInt16>(chunkLen)];
                if (ret != kIOReturnSuccess) {
                    std::cerr << "[BluetoothDriver] writeSync chunk error: 0x" << std::hex << ret << std::dec << std::endl;
                    break;
                }
                
                // Hardware requires ~130ms spacing between 2-byte chunks
                if (i + 2 < pktCopy.size()) {
                    [NSThread sleepForTimeInterval:0.130];
                }
            }
        }
    });
}

} // namespace DrHD
