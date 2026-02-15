#pragma once

struct EngineConfig
{
    // Variadic Update, let the Logic thread run uncapped or limit its updates
    int TargetFPS = 0; // 0 = Uncapped

    // Physics/Simulation (Fixed High) - e.g., 60Hz or 128Hz
    int FixedUpdateHz = 60;

    // Networking (Fixed Low/Med) - e.g., 20Hz or 30Hz
    // This is your "Tick Rate". Lower = Less Bandwidth, Higher = More Precision.
    int NetworkUpdateHz = 30;
    
    // Input (and window management)
    // This controls how fast your main thread goes, higher = better input latency
    int InputPollHz = 1000;
    
    // --- Helpers ---
    double GetTargetFrameTime() const
    {
        return (TargetFPS > 0) ? 1.0 / TargetFPS : 0.0;
    }

    double GetFixedStepTime() const
    {
        return 1.0 / FixedUpdateHz;
    }

    double GetNetworkStepTime() const
    {
        return (NetworkUpdateHz > 0) ? 1.0 / NetworkUpdateHz : 0.0;
    }
};
