#pragma once

namespace cray
{
    namespace ProductInfo
    {
        inline constexpr const char* kProductName       = "CRAY LDR URBAN INSTRUMENTS";
        inline constexpr const char* kShortName         = "CRAY LDR";
        inline constexpr const char* kManufacturer      = "CRAY LDR";
        inline constexpr const char* kVersionString     = "1.0.0";
        inline constexpr int         kStateSchemaVersion = 1;
        inline constexpr int         kMaxPolyphony      = 128;
        inline constexpr int         kNumOscillators    = 4;
        inline constexpr int         kMaxUnisonVoices   = 8;
        inline constexpr int         kNumLfos           = 2;
        inline constexpr int         kNumModSlots       = 8;
        inline constexpr int         kInitialSampleLayers = 4;

        inline constexpr const char* kPresetExtension     = ".craypreset";
        inline constexpr const char* kInstrumentExtension = ".crayinst";
        inline constexpr const char* kPackExtension       = ".craypack";
    }
}
