// =============================================================================
// CRAY LDR URBAN INSTRUMENTS - Unit Test Runner (TestMain.cpp)
// =============================================================================
// Entry point for the JUCE unit test runner.
// All test classes register themselves at static-init time.
// =============================================================================

#include <JuceHeader.h>
#include <iostream>

// Test translation units are explicitly listed in CMakeLists.txt.
// Each compiled test file registers its JUCE UnitTest at static initialization.

int main([[maybe_unused]] int argc, [[maybe_unused]] char** argv)
{
    juce::UnitTestRunner runner;
    runner.setAssertOnFailure(false);

    // Run every registered UnitTest in the process.
    runner.runAllTests();

    int failures = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
    {
        const auto* r = runner.getResult(i);
        if (r->failures > 0)
        {
            ++failures;
            std::cout << "[FAIL] " << r->unitTestName << " (" << r->failures << " failure(s))" << std::endl;
            for (const auto& message : r->messages)
                std::cout << "  " << message << std::endl;
        }
        else
        {
            std::cout << "[PASS] " << r->unitTestName << std::endl;
        }
    }

    if (failures == 0)
        std::cout << "\n==  All tests passed  ==" << std::endl;
    else
        std::cout << "\n== " << failures << " test suite(s) FAILED ==" << std::endl;

    return failures > 0 ? 1 : 0;
}
