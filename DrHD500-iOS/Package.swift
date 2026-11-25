// swift-tools-version: 5.9
import PackageDescription

let package = Package(
    name: "DrHD500",
    platforms: [
        .iOS(.v16),
        .macOS(.v13)
    ],
    products: [
        .library(
            name: "DrHD500Core",
            targets: ["DrHD500Core"]
        )
    ],
    targets: [
        .target(
            name: "DrHD500Core",
            path: "DrHD500",
            exclude: ["App/DrHD500App.swift", "App/Info.plist"],
            resources: [
                .process("Resources")
            ]
        ),
        .testTarget(
            name: "DrHD500Tests",
            dependencies: ["DrHD500Core"],
            path: "Tests"
        )
    ]
)
