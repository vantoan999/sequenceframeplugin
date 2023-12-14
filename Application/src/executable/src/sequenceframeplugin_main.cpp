/**
****************************************************************
***********************************
* Copyright (C) 2019 Parrot Faurecia Automotive SAS
*
* @brief Use kanzi.hpp only when you are learning to develop Kanzi applications.
*        To improve compilation time in production projects, include only the header files of
*        the Kanzi functionality you are using.
*
****************************************************************
***********************************
*/
#include <kanzi/kanzi.hpp>

#if defined(ANDROID) || !defined(KANZI_API_IMPORT)
#include <sequenceframeplugin_module.hpp>
#endif

using namespace kanzi;

// Application class.
// Implements application logic.
class SequenceFramePluginApplication : public ExampleApplication
{
protected:

    // Configures application.
    virtual void onConfigure(ApplicationProperties& configuration) KZ_OVERRIDE
    {
        configuration.binaryName = "sequenceframeplugin.kzb.cfg";
    }

    // Initializes application after project has been loaded.
    virtual void onProjectLoaded() KZ_OVERRIDE
    {
        // Code to run after the .KZB has been loaded.
    }

    virtual void registerMetadataOverride(ObjectFactory& /*factory*/) KZ_OVERRIDE
    {
        Domain* domain = getDomain();
        KanziComponentsModule::registerModule(domain);
#if defined(ANDROID) || !defined(KANZI_API_IMPORT)
        SequenceFramePluginModule::registerModule(domain);
#endif
    }
};

// Creates application instance. Called by framework.
Application* createApplication()
{
    return new SequenceFramePluginApplication();
}
