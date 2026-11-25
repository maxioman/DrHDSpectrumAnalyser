#import <Foundation/Foundation.h>
#import <IOBluetooth/IOBluetooth.h>
#include <iostream>
#include <vector>
#include <cmath>

@interface TestDelegate : NSObject <IOBluetoothRFCOMMChannelDelegate>
@end

@implementation TestDelegate
- (void)rfcommChannelData:(IOBluetoothRFCOMMChannel*)rfcommChannel data:(void *)dataPointer length:(size_t)dataLength {
    const uint8_t* p = (const uint8_t*)dataPointer;
    if (dataLength < 104 || p[0] != 0xFE) return;

    int pktType = p[1];
    if (pktType == 3) {
        int i2 = ((int)p[2] << 8) | (int)p[3];
        int flag = ((int)p[4] & 64) << 8;
        float snr = 0.0f;
        if (i2 > 0) {
            if (flag == 0) snr = log10(65536.0 / (double)i2) * 10.0 + 2.0;
            else snr = log10(16384.0 / (double)i2) * 10.0 - 6.0;
        }
        int rawPwr = (((int)p[5] & 0xFF) << 5) | ((int)p[6] & 0x1F);
        float pwr = 20.0f + ((float)rawPwr / 4096.0f) * 50.0f;
        int iMer = ((int)p[7] << 8) | (int)p[8];
        float mer = (iMer > 0) ? log10(65536.0 / (double)iMer) * 10.0 : 0.0f;
        bool lock = (p[21] & 1) != 0;

        std::cout << ">>> LIVE MEASURES Packet 3: Lock=" << (lock ? "YES" : "NO")
                  << " | C/N=" << snr << " dB"
                  << " | MER=" << mer << " dB"
                  << " | RSSI=" << pwr << " dBuV"
                  << " | PLP=" << (int)p[11]
                  << " | CellID=0x" << std::hex << (((int)p[13] << 8) | (int)p[14])
                  << " | NetID=0x" << (((int)p[15] << 8) | (int)p[16]) << std::dec
                  << std::endl;
    } else if (pktType == 4) {
        std::cout << ">>> LIVE SDT/NIT Packet 4 received!" << std::endl;
        size_t off = 14;
        while (off + 2 < 104) {
            uint8_t desc = p[off];
            if (desc == '@') {
                size_t len = p[off + 1];
                off += 2;
                if (off + len <= 102 && len > 0) {
                    std::string netName((const char*)&p[off], len);
                    std::cout << "    Network Name: " << netName << std::endl;
                }
                off += len;
            } else if (desc == 'H') {
                size_t sdtLen = (size_t)p[off + 1] + 3;
                off += 2;
                if (off + sdtLen <= 102 && sdtLen >= 4) {
                    size_t pLen = p[off + 1];
                    size_t sOff = off + 2 + pLen;
                    if (sOff < off + sdtLen) {
                        size_t sLen = p[sOff];
                        if (sOff + 1 + sLen <= off + sdtLen) {
                            std::string svcName((const char*)&p[sOff + 1], sLen);
                            std::cout << "    Live Service Name: " << svcName << std::endl;
                        }
                    }
                }
                off += sdtLen;
            } else {
                break;
            }
        }
    } else if (pktType == 13) {
        int batt = (((int)p[2] << 8) | (int)p[3]) * 10;
        std::cout << ">>> LIVE TELEMETRY Packet 13: Battery=" << batt << " mV | HW=" << (int)p[11] << " | FW=v" << (int)p[13] << std::endl;
    }
}
@end

uint8_t computeCRC8(const uint8_t* data, size_t length) {
    uint8_t crc = 0;
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (int j = 0; j < 8; ++j) {
            if (crc & 1) crc = (crc >> 1) ^ 0x8C;
            else crc >>= 1;
        }
    }
    return crc;
}

int main() {
    @autoreleasepool {
        NSString* mac = @"01-ac-78-00-04-55";
        IOBluetoothDevice* dev = [IOBluetoothDevice deviceWithAddressString:mac];
        if (!dev) return 1;

        if (![dev isConnected]) {
            [dev openConnection];
        }

        TestDelegate* delegate = [[TestDelegate alloc] init];
        IOBluetoothRFCOMMChannel* chan = nil;
        IOReturn ret = [dev openRFCOMMChannelSync:&chan withChannelID:1 delegate:delegate];
        if (!chan) return 1;

        // Tune to 474 MHz
        std::vector<uint8_t> ter = {0x3D, 0x14, 0x10, 0x01, 0xDA};
        ter.push_back(computeCRC8(ter.data(), ter.size()));

        for (size_t i = 0; i < ter.size(); i += 2) {
            size_t chunk = std::min<size_t>(2, ter.size() - i);
            [chan writeSync:&ter[i] length:chunk];
            [NSThread sleepForTimeInterval:0.130];
        }

        // Run loop for 3 seconds to print live measures
        [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:3.0]];

        [chan closeChannel];
        [dev closeConnection];
    }
    return 0;
}
