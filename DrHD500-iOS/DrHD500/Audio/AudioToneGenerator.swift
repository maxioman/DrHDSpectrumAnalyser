//
//  AudioToneGenerator.swift
//  DrHD500
//
//  Generates pitch-varying satellite alignment tone matching com.drhd.finder500.base.Beeper
//

import Foundation
import AVFoundation

public final class AudioToneGenerator: ObservableObject {
    public static let shared = AudioToneGenerator()
    
    @Published public var isMuted: Bool = false
    
    private var audioEngine: AVAudioEngine?
    private var playerNode: AVAudioPlayerNode?
    private var pitchNode: AVAudioUnitTimePitch?
    private var audioBuffer: AVAudioPCMBuffer?
    
    public init() {
        setupAudio()
    }
    
    private func setupAudio() {
        let engine = AVAudioEngine()
        let player = AVAudioPlayerNode()
        let pitch = AVAudioUnitTimePitch()
        
        engine.attach(player)
        engine.attach(pitch)
        
        let format = AVAudioFormat(standardFormatWithSampleRate: 44100, channels: 1)!
        engine.connect(player, to: pitch, format: format)
        engine.connect(pitch, to: engine.mainMixerNode, format: format)
        
        // Generate continuous 880Hz sine wave buffer
        let sampleRate: Double = 44100
        let frameCount = AVAudioFrameCount(sampleRate * 0.25)
        guard let pcm = AVAudioPCMBuffer(pcmFormat: format, frameCapacity: frameCount) else { return }
        pcm.frameLength = frameCount
        
        let channels = pcm.floatChannelData!
        let channel = channels[0]
        let frequency: Double = 880.0
        for i in 0..<Int(frameCount) {
            let t = Double(i) / sampleRate
            channel[i] = Float(sin(2.0 * .pi * frequency * t) * 0.35)
        }
        
        self.audioBuffer = pcm
        self.audioEngine = engine
        self.playerNode = player
        self.pitchNode = pitch
        
        do {
            #if os(iOS)
            try AVAudioSession.sharedInstance().setCategory(.playback, mode: .default, options: [.mixWithOthers])
            try AVAudioSession.sharedInstance().setActive(true)
            #endif
            try engine.start()
        } catch {
            print("Audio setup failed: \(error)")
        }
    }
    
    public func updateSignalTone(snrDb: Float, isLocked: Bool) {
        guard !isMuted, isLocked, snrDb > 2.0 else {
            stopTone()
            return
        }
        
        // Pitch shift formula: semitones proportional to SNR
        // Match formula from Beeper.java: (45 - SNR * 2.143)
        let pitchCents = Float(min(2400, max(-1200, (snrDb - 8.0) * 150.0)))
        pitchNode?.pitch = pitchCents
        
        if !(playerNode?.isPlaying ?? false) {
            playTone()
        }
    }
    
    private func playTone() {
        guard let player = playerNode, let buf = audioBuffer else { return }
        player.scheduleBuffer(buf, at: nil, options: .loops, completionHandler: nil)
        player.play()
    }
    
    public func stopTone() {
        playerNode?.stop()
    }
}
