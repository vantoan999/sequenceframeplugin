// Copyright 2022-2023 by Rightware. All rights reserved.

#ifndef SEQUENCEFRAMEPLUGIN_MODULE_HPP
#define SEQUENCEFRAMEPLUGIN_MODULE_HPP

#include <kanzi/core/module/plugin.hpp>


class SEQUENCEFRAMEPLUGIN_API SequenceFramePluginModule : public kanzi::Plugin
{
public:

    static void registerModule(kanzi::Domain* domain);

protected:

    virtual MetaclassContainer getMetaclassesOverride() KZ_OVERRIDE;
};

#endif
