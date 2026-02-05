#include "StrigidEngine.h"

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
    // Stack allocation is fine for the main engine object
    StrigidEngine& engine = StrigidEngine::Get();

    if (engine.Initialize("Strigid v0.1", 1920, 1080))
    {
        engine.Run();
    }

    return 0;
}
