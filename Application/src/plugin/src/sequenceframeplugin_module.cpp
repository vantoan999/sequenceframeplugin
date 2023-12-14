// Copyright 2022-2023 by Rightware. All rights reserved.

#include "sequenceframeplugin_module.hpp"
#include "sequenceframeplugin.hpp"

using namespace kanzi;

void SequenceFramePluginModule::registerModule(Domain* domain)
{
    domain->registerModule<SequenceFramePluginModule>("SequenceFramePlugin");
}

SequenceFramePluginModule::MetaclassContainer SequenceFramePluginModule::getMetaclassesOverride()
{
    MetaclassContainer metaclasses;
    metaclasses.push_back(SequenceFramePlugin::getStaticMetaclass());
    return metaclasses;
}

#if defined(SEQUENCEFRAMEPLUGIN_API_EXPORT) || defined(ANDROID)

extern "C"
{

SEQUENCEFRAMEPLUGIN_API Module* createModule(uint32_t /*kanziVersionMajor*/,
                                             uint32_t /*kanziVersionMinor*/)
{
    return new SequenceFramePluginModule;
}

}

#endif
