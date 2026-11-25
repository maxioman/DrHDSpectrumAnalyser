//
//  SatFinderTransport.swift
//  DrHD500
//
//  Transport abstraction protocol for BLE, Wi-Fi/TCP, and Simulator
//

import Foundation
import Combine

public enum TransportConnectionState: Equatable {
    case disconnected
    case scanning
    case connecting(String)
    case connected(String)
    case failed(String)
}

public protocol SatFinderTransport: AnyObject {
    var connectionStatePublisher: AnyPublisher<TransportConnectionState, Never> { get }
    var currentState: TransportConnectionState { get }
    var onBytesReceived: (([UInt8]) -> Void)? { get set }
    
    func connect()
    func disconnect()
    func sendBytes(_ bytes: [UInt8])
}
