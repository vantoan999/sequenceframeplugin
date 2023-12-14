// Use kanzi.hpp only when you are learning to develop Kanzi applications. 
// To improve compilation time in production projects, include only the header files of the Kanzi functionality you are using.
#include <kanzi/kanzi.hpp>

#if !defined(ANDROID) && !defined(KANZI_CORE_API_IMPORT)
#include <sequenceframeplugin_module.hpp>

// [CodeBehind libs inclusion]. Do not remove this identifier.

#if defined(SEQUENCEFRAMEPLUGIN_CODE_BEHIND_API)
#include <SequenceFramePlugin_code_behind_module.hpp>
#endif

#endif

using namespace kanzi;

// Application class.
// Implements application logic.
class SequenceFramePluginApplication : public ExampleApplication
{
protected:

    // Configures application.
    void onConfigure(ApplicationProperties& configuration) override
    {
        configuration.binaryName = "sequenceframeplugin.kzb.cfg";
    }

    // Initializes application after project has been loaded.
    void onProjectLoaded() override
    {
        // Code to run after the .KZB has been loaded.
    }

    void registerMetadataOverride(ObjectFactory& factory) override
    {
        ExampleApplication::registerMetadataOverride(factory);

#if !defined(ANDROID) && !defined(KANZI_CORE_API_IMPORT)
        Domain* domain = getDomain();
        SequenceFramePluginModule::registerModule(domain);

#if defined(SEQUENCEFRAMEPLUGIN_CODE_BEHIND_API)
        SequenceFramePluginCodeBehindModule::registerModule(domain);
#endif

        // [CodeBehind module inclusion]. Do not remove this identifier.
#endif
    }
};

// Creates application instance. Called by framework.
Application* createApplication()
{
    return new SequenceFramePluginApplication();
}
