#pragma once

struct EngineConfig {
    // 1. Visuals (Variable) - e.g., 144Hz Monitor
    int TargetFPS = 0; // 0 = Uncapped

    // 2. Physics/Simulation (Fixed High) - e.g., 60Hz or 128Hz
    int FixedUpdateHz = 60;

    // 3. Networking (Fixed Low/Med) - e.g., 20Hz or 30Hz
    // This is your "Tick Rate". Lower = Less Bandwidth, Higher = More Precision.
    int NetworkUpdateHz = 30;

    // --- Helpers ---
    double GetTargetFrameTime() const { 
        return (TargetFPS > 0) ? 1.0 / TargetFPS : 0.0; 
    }
    double GetFixedStepTime() const { 
        return 1.0 / FixedUpdateHz; 
    }
    double GetNetworkStepTime() const {
        return (NetworkUpdateHz > 0) ? 1.0 / NetworkUpdateHz : 0.0;
    }
};