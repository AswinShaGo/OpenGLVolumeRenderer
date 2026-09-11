#include "Application.h"
#include <iostream>
#include <string>

static void printUsage() {
    std::cout << "Usage: VolumeRenderer [options]\n"
              << "  --ini  <path>         Load volume from .ini sidecar (auto-finds .raw)\n"
              << "  --raw  <path>         Load raw binary file\n"
              << "  --dim  <X> <Y> <Z>    Volume dimensions (required with --raw)\n"
              << "  --bits <8|16>         Bits per voxel (default: 8)\n"
              << "  --help                Show this help\n\n"
              << "If no arguments given, loads data/VisMale.raw.ini or generates a procedural sphere.\n";
}

int main(int argc, char** argv) {
    Application app;

    // Parse CLI args
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") { printUsage(); return 0; }
        else if (arg == "--ini"  && i + 1 < argc) app.iniPath = argv[++i];
        else if (arg == "--raw"  && i + 1 < argc) app.rawPath = argv[++i];
        else if (arg == "--dim"  && i + 3 < argc) {
            app.rawDimX = std::stoi(argv[++i]);
            app.rawDimY = std::stoi(argv[++i]);
            app.rawDimZ = std::stoi(argv[++i]);
        }
        else if (arg == "--bits" && i + 1 < argc) app.rawBits = std::stoi(argv[++i]);
        else { std::cerr << "Unknown arg: " << arg << "\n"; printUsage(); return 1; }
    }

    if (!app.init(1600, 900, "Volume Renderer")) {
        std::cerr << "Failed to initialize\n";
        return 1;
    }

    app.run();
    app.shutdown();
    return 0;
}
