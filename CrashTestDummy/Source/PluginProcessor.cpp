// CrashTestDummy - a deliberately misbehaving JUCE plugin for validating
// host plugin-sandboxing / crash isolation. Do NOT leave this in a real
// plugin scan folder.
//
// Behavior is selected at runtime, no recompile needed:
//   ENV:   CRASHTEST_KIND   = none | segfault | throw | abort | terminate | stackoverflow | hang | oom
//          CRASHTEST_STAGE  = ctor | prepare | process | editor | getstate | setstate
//   File:  ~/.crashtest_mode   (single line: "<kind> <stage>", env wins if set)
//
// Defaults: KIND=none (loads cleanly -> use as control), STAGE=ctor.
// Note: "ctor" == the crash that happens during the host's plugin SCAN,
// because scanning instantiates the plugin.

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include <thread>
#include <chrono>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <cstdio>

namespace ctv
{
    enum class Kind  { None, Segfault, Throw, Abort, Terminate, StackOverflow, Hang, Oom };
    enum class Stage { None, Ctor, Prepare, Process, Editor, GetState, SetState };

    struct Config { Kind kind = Kind::None; Stage stage = Stage::Ctor; };

    static Kind parseKind (const juce::String& s)
    {
        const auto t = s.trim().toLowerCase();
        if (t == "segfault")      return Kind::Segfault;
        if (t == "throw")         return Kind::Throw;
        if (t == "abort")         return Kind::Abort;
        if (t == "terminate")     return Kind::Terminate;
        if (t == "stackoverflow") return Kind::StackOverflow;
        if (t == "hang")          return Kind::Hang;
        if (t == "oom")           return Kind::Oom;
        return Kind::None;
    }

    static Stage parseStage (const juce::String& s)
    {
        const auto t = s.trim().toLowerCase();
        if (t == "prepare")  return Stage::Prepare;
        if (t == "process")  return Stage::Process;
        if (t == "editor")   return Stage::Editor;
        if (t == "getstate") return Stage::GetState;
        if (t == "setstate") return Stage::SetState;
        if (t == "ctor")     return Stage::Ctor;
        return Stage::Ctor; // default target = startup/scan
    }

    static Config readConfig()
    {
        Config c;

        juce::String kindStr, stageStr;

        if (auto* k = std::getenv ("CRASHTEST_KIND"))  kindStr  = k;
        if (auto* s = std::getenv ("CRASHTEST_STAGE")) stageStr = s;

        // Fallback: ~/.crashtest_mode  ->  "<kind> <stage>"
        if (kindStr.isEmpty())
        {
            auto f = juce::File::getSpecialLocation (juce::File::userHomeDirectory)
                         .getChildFile (".crashtest_mode");
            if (f.existsAsFile())
            {
                auto tokens = juce::StringArray::fromTokens (f.loadFileAsString().trim(), " \t", "");
                tokens.removeEmptyStrings();
                if (tokens.size() >= 1) kindStr  = tokens[0];
                if (tokens.size() >= 2) stageStr = tokens[1];
            }
        }

        c.kind  = parseKind (kindStr);
        c.stage = stageStr.isNotEmpty() ? parseStage (stageStr) : Stage::Ctor;
        return c;
    }

    static const char* kindName (Kind k)
    {
        switch (k) {
            case Kind::None:          return "none";
            case Kind::Segfault:      return "segfault";
            case Kind::Throw:         return "throw";
            case Kind::Abort:         return "abort";
            case Kind::Terminate:     return "terminate";
            case Kind::StackOverflow: return "stackoverflow";
            case Kind::Hang:          return "hang";
            case Kind::Oom:           return "oom";
        }
        return "?";
    }

    static const char* stageName (Stage s)
    {
        switch (s) {
            case Stage::None:     return "none";
            case Stage::Ctor:     return "ctor";
            case Stage::Prepare:  return "prepare";
            case Stage::Process:  return "process";
            case Stage::Editor:   return "editor";
            case Stage::GetState: return "getstate";
            case Stage::SetState: return "setstate";
        }
        return "?";
    }

    // Defeats tail-call optimization via a growing local frame + volatile use.
    static void infiniteRecursion (volatile int depth)
    {
        volatile char frame[512];
        frame[0] = (char) depth;
        infiniteRecursion (depth + (int) frame[0] + 1);
        juce::ignoreUnused (frame);
    }

    static void doMisbehave (Kind k)
    {
        std::fprintf (stderr, "[CrashTestDummy] FIRING kind=%s\n", kindName (k));
        std::fflush (stderr);

        switch (k)
        {
            case Kind::None:
                return;

            case Kind::Segfault:
            {
                volatile int* p = nullptr;
                *p = 0xDEAD;            // SIGSEGV
                break;
            }

            case Kind::Throw:
                throw std::runtime_error ("CrashTestDummy: intentional uncaught exception");

            case Kind::Abort:
                std::abort();           // SIGABRT

            case Kind::Terminate:
                std::terminate();

            case Kind::StackOverflow:
                infiniteRecursion (1);  // grows the stack until it faults
                break;

            case Kind::Hang:
                std::fprintf (stderr, "[CrashTestDummy] hanging forever (this is the watchdog test)\n");
                std::fflush (stderr);
                for (;;)
                    std::this_thread::sleep_for (std::chrono::seconds (3600));

            case Kind::Oom:
            {
                std::vector<std::unique_ptr<char[]>> blocks;
                const size_t chunk = 256ull * 1024 * 1024; // 256 MB
                for (;;)
                {
                    auto b = std::make_unique<char[]> (chunk);
                    std::memset (b.get(), 0xAB, chunk); // touch pages so they're really committed
                    blocks.push_back (std::move (b));
                }
            }
        }
    }
}

class CrashTestDummyProcessor : public juce::AudioProcessor
{
public:
    CrashTestDummyProcessor()
        : AudioProcessor (BusesProperties()
              .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
              .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
    {
        config = ctv::readConfig();
        std::fprintf (stderr, "[CrashTestDummy] loaded. kind=%s stage=%s\n",
                      ctv::kindName (config.kind), ctv::stageName (config.stage));
        std::fflush (stderr);

        maybeMisbehave (ctv::Stage::Ctor);  // == scan-time crash
    }

    void prepareToPlay (double, int) override        { maybeMisbehave (ctv::Stage::Prepare); }
    void releaseResources() override                 {}

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        if (firstBlock)
        {
            firstBlock = false;
            maybeMisbehave (ctv::Stage::Process);
        }
        buffer.clear();
    }

    juce::AudioProcessorEditor* createEditor() override
    {
        maybeMisbehave (ctv::Stage::Editor);
        return new juce::GenericAudioProcessorEditor (*this);
    }
    bool hasEditor() const override                  { return true; }

    void getStateInformation (juce::MemoryBlock&) override   { maybeMisbehave (ctv::Stage::GetState); }
    void setStateInformation (const void*, int) override     { maybeMisbehave (ctv::Stage::SetState); }

    const juce::String getName() const override      { return "CrashTestDummy"; }
    bool acceptsMidi() const override                { return false; }
    bool producesMidi() const override               { return false; }
    double getTailLengthSeconds() const override     { return 0.0; }

    int getNumPrograms() override                    { return 1; }
    int getCurrentProgram() override                 { return 0; }
    void setCurrentProgram (int) override            {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override
    {
        const auto& out = layouts.getMainOutputChannelSet();
        return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
    }

private:
    void maybeMisbehave (ctv::Stage stage)
    {
        if (config.kind != ctv::Kind::None && config.stage == stage)
            ctv::doMisbehave (config.kind);
    }

    ctv::Config config;
    bool firstBlock = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CrashTestDummyProcessor)
};

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CrashTestDummyProcessor();
}
