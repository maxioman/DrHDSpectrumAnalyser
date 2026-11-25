//
//  CRC8.swift
//  DrHD500
//
//  Reversed from com.drhd.finder500.base.CRC8
//  Maxim / Dallas 1-Wire CRC-8 polynomial (x^8 + x^5 + x^4 + 1)
//

import Foundation

public struct CRC8 {
    private static let table: [UInt8] = {
        var tbl = [UInt8](repeating: 0, count: 256)
        for i in 0..<256 {
            var curr = i
            var crc = 0
            for _ in 0..<8 {
                if ((curr ^ crc) & 1) == 1 {
                    crc = ((crc ^ 24) >> 1) | 128
                } else {
                    crc = crc >> 1
                }
                curr >>= 1
            }
            tbl[i] = UInt8(crc & 0xFF)
        }
        return tbl
    }()

    public static func compute(_ bytes: [UInt8], offset: Int = 0, length: Int? = nil, initial: UInt8 = 0) -> UInt8 {
        let len = length ?? (bytes.count - offset)
        var crc = initial
        for i in 0..<len {
            let idx = Int((crc ^ bytes[offset + i]) & 0xFF)
            crc = table[idx]
        }
        return crc
    }
}
