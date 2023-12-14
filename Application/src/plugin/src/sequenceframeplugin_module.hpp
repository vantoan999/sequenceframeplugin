/**
****************************************************************
***********************************
* Copyright (C) 2019 Parrot Faurecia Automotive SAS
*
* @brief Sequence Frame module header file
*
****************************************************************
***********************************
*/
#ifndef SEQUENCEFRAMEPLUGIN_MODULE_HPP
#define SEQUENCEFRAMEPLUGIN_MODULE_HPP

#include <kanzi/core/module/plugin.hpp>


class SequenceFramePluginModule : public kanzi::Plugin
{
public:

    SEQUENCEFRAMEPLUGIN_API static void registerModule(kanzi::Domain* domain);

protected:

    virtual MetaclassContainer getMetaclassesOverride() KZ_OVERRIDE;
};

#endif
