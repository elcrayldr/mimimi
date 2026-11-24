// =============================================================================
// CRAY LDR URBAN INSTRUMENTS - Unit Test Runner (TestMain.cpp)
// =============================================================================
// Entry point for the JUCE unit test runner.
// All test classes register themselves at static-init time.
// =============================================================================

#include <JuceHeader.h>

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
            juce::Logger::writeToLog("[FAIL] " + r->unitTestName + " (" + juce::String(r->failures) + " failure(s))");
        }
        else
        {
            juce::Logger::writeToLog("[PASS] " + r->unitTestName);
        }
    }

    if (failures == 0)
        juce::Logger::writeToLog("\n==  All tests passed  ==");
    else
        juce::Logger::writeToLog("\n== " + juce::String(failures) + " test suite(s) FAILED ==");

    return failures > 0 ? 1 : 0;
}
