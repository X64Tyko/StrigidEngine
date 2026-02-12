#include "Registry.h"
#include <iostream>

#include "StrigidEngine.h"
#include "TestEntity.h"

void TestRegistry()
{
    std::cout << "=== Registry Test ===" << std::endl;

    Registry& Reg = Registry::Get();

    // Test 1: Create entities
    std::cout << "Creating 100 test entities..." << std::endl;
    std::vector<EntityID> Entities;
    for (int i = 0; i < 100; ++i)
    {
        EntityID Id = Reg.Create<TestEntity>();
        Entities.push_back(Id);
    }
    std::cout << "Created " << Entities.size() << " entities" << std::endl;

    // Test 2: Verify IDs are valid
    int ValidCount = 0;
    for (EntityID Id : Entities)
    {
        if (Id.IsValid())
            ValidCount++;
    }
    std::cout << "Valid entities: " << ValidCount << "/" << Entities.size() << std::endl;

    // Test 3: Destroy some entities
    std::cout << "Destroying first 10 entities..." << std::endl;
    for (int i = 0; i < 10; ++i)
    {
        Reg.Destroy(Entities[i]);
    }
    Reg.ProcessDeferredDestructions();
    std::cout << "Destruction complete" << std::endl;

    // Test 4: Create new entities (should reuse freed indices)
    std::cout << "Creating 10 new entities (should reuse indices)..." << std::endl;
    for (int i = 0; i < 10; ++i)
    {
        EntityID Id = Reg.Create<TestEntity>();
        std::cout << "  New entity: Index=" << Id.GetIndex() 
                  << " Gen=" << Id.GetGeneration() << std::endl;
    }

    std::cout << "=== Test Complete ===" << std::endl << std::endl;
}

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
    // Run registry test
    TestRegistry();

    // Stack allocation is fine for the main engine object
    StrigidEngine& Engine = StrigidEngine::Get();

    if (Engine.Initialize("Strigid v0.1", 1920, 1080))
    {
        Engine.Run();
    }

    return 0;
}
