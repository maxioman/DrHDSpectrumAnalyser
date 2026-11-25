//
//  TCPTransport.swift
//  DrHD500
//
//  Wi-Fi / TCP Socket implementation via Network.framework NWConnection
//

import Foundation
import Network
import Combine

public final class TCPTransport: SatFinderTransport {
    private var connection: NWConnection?
    private let queue = DispatchQueue(label: "com.drhd.tcptransport")
    
    public var host: String
    public var port: UInt16
    
    private let stateSubject = CurrentValueSubject<TransportConnectionState, Never>(.disconnected)
    public var connectionStatePublisher: AnyPublisher<TransportConnectionState, Never> {
        stateSubject.eraseToAnyPublisher()
    }
    public var currentState: TransportConnectionState { stateSubject.value }
    public var onBytesReceived: (([UInt8]) -> Void)?
    
    public init(host: String = "192.168.4.1", port: UInt16 = 8080) {
        self.host = host
        self.port = port
    }
    
    public func connect() {
        disconnect()
        
        let nwHost = NWEndpoint.Host(host)
        let nwPort = NWEndpoint.Port(rawValue: port)!
        let params = NWParameters.tcp
        
        stateSubject.send(.connecting("\(host):\(port)"))
        let conn = NWConnection(host: nwHost, port: nwPort, using: params)
        self.connection = conn
        
        conn.stateUpdateHandler = { [weak self] state in
            guard let self = self else { return }
            DispatchQueue.main.async {
                switch state {
                case .ready:
                    self.stateSubject.send(.connected("\(self.host):\(self.port)"))
                    self.startReceive()
                case .failed(let err):
                    self.stateSubject.send(.failed(err.localizedDescription))
                case .cancelled:
                    self.stateSubject.send(.disconnected)
                default:
                    break
                }
            }
        }
        
        conn.start(queue: queue)
    }
    
    public func disconnect() {
        connection?.cancel()
        connection = nil
        DispatchQueue.main.async {
            self.stateSubject.send(.disconnected)
        }
    }
    
    public func sendBytes(_ bytes: [UInt8]) {
        guard let conn = connection, stateSubject.value == .connected("\(host):\(port)") else { return }
        let data = Data(bytes)
        conn.send(content: data, completion: .contentProcessed { _ in })
    }
    
    private func startReceive() {
        guard let conn = connection else { return }
        conn.receive(minimumIncompleteLength: 1, maximumLength: 1024) { [weak self] data, _, isComplete, error in
            guard let self = self else { return }
            if let data = data, !data.isEmpty {
                let bytes = [UInt8](data)
                DispatchQueue.main.async {
                    self.onBytesReceived?(bytes)
                }
            }
            if isComplete || error != nil {
                self.disconnect()
            } else {
                self.startReceive()
            }
        }
    }
}
