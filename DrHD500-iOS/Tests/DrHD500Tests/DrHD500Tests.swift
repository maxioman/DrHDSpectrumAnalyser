//
//  DrHD500Tests.swift
//  DrHD500Tests
//

import XCTest
@testable import DrHD500Core

final class DrHD500Tests: XCTestCase {
    
    func testCRC8Calculation() {
        let data: [UInt8] = [61, 8, 64, UInt8((10750 >> 8) & 0xFF), UInt8(10750 & 0xFF), UInt8((27500 >> 8) & 0xFF), UInt8(27500 & 0xFF)]
        let crc = CRC8.compute(data)
        XCTAssertEqual(crc, 0x41)
    }
    
    func testMeasuresSatPacketBuilder() {
        let packet = DrHDPacketBuilder.makeMeasuresSatPacket(
            intermediateFreq: 975,
            symbolRate: 27500,
            isS2: true,
            powerMode: 2
        )
        XCTAssertEqual(packet.count, 8)
        XCTAssertEqual(packet[0], DrHDConstants.packetHeaderByte)
        XCTAssertEqual(packet[1], 0x08)
        XCTAssertEqual(packet[2], (2 << 5) | 0x10) // 13V + S2
        XCTAssertEqual(packet[3], UInt8((975 >> 8) & 0xFF))
        XCTAssertEqual(packet[4], UInt8(975 & 0xFF))
        
        // Verify CRC matches
        let computedCrc = CRC8.compute(packet, offset: 0, length: 7)
        XCTAssertEqual(packet[7], computedCrc)
    }
    
    func testDiseqcCommandBuilder() {
        let cmd = "[E0 10 38 F0]"
        let packet = DrHDPacketBuilder.decodeDiseqcCommand(cmd)
        XCTAssertTrue(packet.count >= 6)
        XCTAssertEqual(packet[0], DrHDConstants.packetHeaderByte)
        XCTAssertEqual(packet[2], 0xE0)
        XCTAssertEqual(packet[3], 0x10)
        XCTAssertEqual(packet[4], 0x38)
        XCTAssertEqual(packet[5], 0xF0)
    }
    
    func testHardwareTelemetryParsing() {
        var packet = [UInt8](repeating: 0, count: 104)
        packet[0] = DrHDConstants.incomingHeaderByte // 0xFE
        packet[1] = 13 // Status packet
        
        let battMV = 7400
        packet[2] = UInt8((battMV >> 8) & 0xFF)
        packet[3] = UInt8(battMV & 0xFF)
        
        let lnb10xV = 135
        packet[4] = UInt8((lnb10xV >> 8) & 0xFF)
        packet[5] = UInt8(lnb10xV & 0xFF)
        
        let lnbA = 160
        packet[8] = UInt8((lnbA >> 8) & 0xFF)
        packet[9] = UInt8(lnbA & 0xFF)
        
        packet[11] = 134 // HW 134
        packet[13] = 49  // SW 49
        
        let tel = HardwareTelemetry.parse(packet: packet)
        XCTAssertNotNil(tel)
        XCTAssertEqual(tel?.batteryMilliVolts, 7400)
        XCTAssertEqual(tel?.lnbVolts, 13.5)
        XCTAssertEqual(tel?.lnbCurrentMilliAmps, 160)
        XCTAssertEqual(tel?.hardwareVersion, 134)
        XCTAssertEqual(tel?.softwareVersion, 49)
    }
    
    func testFramerChecksumValidation() {
        let framer = DrHDFramer()
        var received = false
        framer.onPacketReceived = { pkt in
            received = true
            XCTAssertEqual(pkt.count, 104)
            XCTAssertEqual(pkt[0], 0xFE)
        }
        
        var packet = [UInt8](repeating: 0x10, count: 104)
        packet[0] = DrHDConstants.incomingHeaderByte
        
        var sum = 0
        for i in 0..<102 {
            sum += Int(packet[i]) & 0xFF
        }
        packet[102] = UInt8((sum >> 8) & 0xFF)
        packet[103] = UInt8(sum & 0xFF)
        
        framer.appendBytes([0xAA, 0xBB]) // Garbage prefix
        framer.appendBytes(packet)
        
        XCTAssertTrue(received)
    }
    
    func testSpectrumIngest() {
        var model = SpectrumModel()
        
        for chunk in 0..<3 {
            var pkt = [UInt8](repeating: 0, count: 104)
            pkt[0] = DrHDConstants.incomingHeaderByte
            pkt[1] = UInt8(chunk)
            for i in 0..<100 {
                pkt[i + 2] = UInt8((chunk * 50) + i)
            }
            _ = model.ingestPacket(pkt)
        }
        
        XCTAssertEqual(model.levels.count, 300)
    }
    
    func testMeasuresTerPacketBuilder() {
        let packet = DrHDPacketBuilder.makeMeasuresTerPacket(
            mode: DrHDConstants.packetModeTerSNR,
            frequencyMHz: 474,
            isT2: true,
            bandwidthCode: 0,
            powerMode: 0
        )
        XCTAssertEqual(packet.count, 6)
        XCTAssertEqual(packet[0], DrHDConstants.packetHeaderByte) // 0x3D
        XCTAssertEqual(packet[1], 0x14) // mode 1 << 4 | 4 = 0x14
        XCTAssertEqual(packet[2], 0x10) // DVB-T2 bit (0x10)
        XCTAssertEqual(packet[3], UInt8((474 >> 8) & 0xFF))
        XCTAssertEqual(packet[4], UInt8(474 & 0xFF))
        
        let computedCrc = CRC8.compute(packet, offset: 0, length: 5)
        XCTAssertEqual(packet[5], computedCrc)
    }
    
    func testTerrestrialDatabaseUHFChannels() {
        let uhf = TerrestrialDatabase.shared.uhfChannels
        XCTAssertGreaterThanOrEqual(uhf.count, 49)
        XCTAssertEqual(uhf.first?.channel, 21)
        XCTAssertEqual(uhf.first?.frequencyMHz, 474.0)
    }
    
    func testConstellationModulations() {
        var model = ConstellationModel()
        
        model.modulation = .qpsk
        XCTAssertEqual(model.idealSymbols.count, 4)
        
        model.modulation = .psk8
        XCTAssertEqual(model.idealSymbols.count, 8)
        
        model.modulation = .qam16
        XCTAssertEqual(model.idealSymbols.count, 16)
        
        model.modulation = .qam64
        XCTAssertEqual(model.idealSymbols.count, 64)
        
        model.modulation = .qam256
        XCTAssertEqual(model.idealSymbols.count, 256)
    }
    
    func testSpectrumMarker() {
        var model = SpectrumModel()
        model.centerFrequencyMHz = 670.0
        model.spanMHz = 400
        
        model.updateMarker(atIndex: 150)
        XCTAssertNotNil(model.markerFrequencyMHz)
        XCTAssertEqual(model.markerFrequencyMHz!, 670.0, accuracy: 1.0)
    }
}
