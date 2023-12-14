/**
****************************************************************
***********************************
* Copyright (C) 2019 Parrot Faurecia Automotive SAS
*
* @brief Sequence Frame process
*
****************************************************************
***********************************
*/
#include "sequenceframeplugin.hpp"
#include <string>

#include "filemapping.h"
#include "decompressor.h"

PropertyType<string> SequenceFramePlugin::PackagePathProperty(
    kzMakeFixedString("SequenceFramePlugin.PackagePath"), "", 0, false,
    KZ_DECLARE_EDITOR_METADATA(
        metadata.tooltip = "Path for the texture package.";
        metadata.editor = "BrowseFileTextEditor";
)
);

PropertyType<float> SequenceFramePlugin::FPSProperty(
    kzMakeFixedString("SequenceFramePlugin.FPS"), 60.0, 0, false,
    KZ_DECLARE_EDITOR_METADATA(
        metadata.tooltip = "Frames per second. The max value is"
              " {500.0}. The default value is {60.0}.";
)
);

PropertyType<bool> SequenceFramePlugin::LoopPlaybackProperty(
    kzMakeFixedString("SequenceFramePlugin.LoopPlayback"), false, 0, false,
    KZ_DECLARE_EDITOR_METADATA(
        metadata.tooltip = "Whether or not to play the animation circularly."
              "The default value is false.";
)
);

PropertyType<bool> SequenceFramePlugin::KeepLastFrameVisibleProperty(
    kzMakeFixedString("SequenceFramePlugin.KeepLastFrameVisible"), false, 0, false,
    KZ_DECLARE_EDITOR_METADATA(
        metadata.tooltip = "Whether or not to keep the last frame visible."
              "The default value is false.";
)
);

PropertyType<bool> SequenceFramePlugin::ReverseProperty(
    kzMakeFixedString("SequenceFramePlugin.ReverseProperty"), false, 0, false,
    KZ_DECLARE_EDITOR_METADATA(
        metadata.tooltip = "Whether or not to reverse."
        "The default value is false.";
)
);

MessageType<SequenceFramePlugin::EmptyMessageArguments> SequenceFramePlugin::LoadAnimation(
    kzMakeFixedString("SequenceFramePlugin.LoadAnimation"), 0);
MessageType<SequenceFramePlugin::EmptyMessageArguments> SequenceFramePlugin::PlayAnimation(
    kzMakeFixedString("SequenceFramePlugin.PlayAnimation"), 0);
MessageType<SequenceFramePlugin::EmptyMessageArguments> SequenceFramePlugin::StopAnimation(
    kzMakeFixedString("SequenceFramePlugin.StopAnimation"), 0);
MessageType<SequenceFramePlugin::EmptyMessageArguments> SequenceFramePlugin::PauseAnimation(
    kzMakeFixedString("SequenceFramePlugin.PauseAnimation"), 0);

MessageType<SequenceFramePlugin::EmptyMessageArguments> SequenceFramePlugin::oLoadingFinished(
    kzMakeFixedString("SequenceFramePlugin.oLoadingFinished"), 0);
MessageType<SequenceFramePlugin::EmptyMessageArguments> SequenceFramePlugin::oPlayingFinished(
    kzMakeFixedString("SequenceFramePlugin.oPlayingFinished"), 0);

SequenceFramePluginSharedPtr SequenceFramePlugin::create(Domain* domain, string_view name)
{
    SequenceFramePluginSharedPtr sequenceFramePlugin(new SequenceFramePlugin(domain, name));

    sequenceFramePlugin->initialize();

    return sequenceFramePlugin;
}

SequenceFramePlugin::SequenceFramePlugin(Domain* domain, string_view name)
    : Node2D(domain, name)
    , m_decompressionThreadStatus(DecompressionThreadStatus_Idle)
    , m_decompressionThread(nullptr)
    , m_texturePackageFile(nullptr)
    , m_texture(nullptr)
    , m_isReversed(false)
    , m_currentTextureIndex(0)
    , m_textureSize(0)
    , m_textureData(nullptr)
    , m_fpsTimeStamp(0)
    , m_fpsCounter(0)
{

    kzsThreadLockCreate(&m_decompressionThreadLock);

    kzsThreadCreate(decompressTexture, this, KZ_TRUE, &m_decompressionThread);
}

SequenceFramePlugin::~SequenceFramePlugin()
{
    if (m_loadAnimationMessageToken.isValid()) {
        removeMessageHandler(m_loadAnimationMessageToken);
    }
    if (m_playAnimationMessageToken.isValid()) {
        removeMessageHandler(m_playAnimationMessageToken);
    }
    if (m_playAnimationMessageToken.isValid()) {
        removeMessageHandler(m_stopAnimationMessageToken);
    }
    if (m_playAnimationMessageToken.isValid()) {
        removeMessageHandler(m_pauseAnimationMessageToken);
    }

    if (nullptr != m_playTextureTimerToken) {
        removeTimerHandler(getMessageDispatcher(), m_playTextureTimerToken);
        m_playTextureTimerToken = nullptr;
    }

    kzsThreadLockAcquire(m_decompressionThreadLock);
    while (DecompressionThreadStatus_Idle != m_decompressionThreadStatus) {
        kzLogDebug(("SequenceFramePlugin::onTimerShowTexture FPS exceeds maximum value"));
        kzsThreadLockWaitAndReset(m_decompressionThreadLock, KZ_FALSE);
    }
    m_decompressionThreadStatus = DecompressionThreadStatus_Destroy;
    kzsThreadLockRelease(m_decompressionThreadLock);

    if (nullptr != m_decompressionThreadLock) {
        kzsThreadLockDelete(m_decompressionThreadLock);
        m_decompressionThreadLock = nullptr;
    }
}

void SequenceFramePlugin::onAttached()
{
    Node2D::onAttached();

    setProperty(StandardMaterial::TextureProperty, m_texture);

    m_loadAnimationMessageToken = addMessageHandler(
        LoadAnimation, bind(&SequenceFramePlugin::onLoadAnimation, this, placeholders::_1));
    m_playAnimationMessageToken = addMessageHandler(
        PlayAnimation, bind(&SequenceFramePlugin::onPlayAnimation, this, placeholders::_1));
    m_stopAnimationMessageToken = addMessageHandler(
        StopAnimation, bind(&SequenceFramePlugin::onStopAnimation, this, placeholders::_1));
    m_pauseAnimationMessageToken = addMessageHandler(
        PauseAnimation, bind(&SequenceFramePlugin::onPauseAnimation, this, placeholders::_1));
}

void SequenceFramePlugin::onDetached()
{
    Node2D::onDetached();

    if (m_loadAnimationMessageToken.isValid()) {
        removeMessageHandler(m_loadAnimationMessageToken);
    }
    if (m_playAnimationMessageToken.isValid()) {
        removeMessageHandler(m_playAnimationMessageToken);
    }
    if (m_playAnimationMessageToken.isValid()) {
        removeMessageHandler(m_stopAnimationMessageToken);
    }
    if (m_playAnimationMessageToken.isValid()) {
        removeMessageHandler(m_pauseAnimationMessageToken);
    }

    if (nullptr != m_playTextureTimerToken) {
        removeTimerHandler(getMessageDispatcher(), m_playTextureTimerToken);
        m_playTextureTimerToken = nullptr;
    }

    kzsThreadLockAcquire(m_decompressionThreadLock);
    while (DecompressionThreadStatus_Idle != m_decompressionThreadStatus) {
        kzLogDebug(("SequenceFramePlugin::onTimerShowTexture FPS exceeds maximum value"));
        kzsThreadLockWaitAndReset(m_decompressionThreadLock, KZ_FALSE);
    }
    kzsThreadLockRelease(m_decompressionThreadLock);

    resetPluginStatus();
}

void SequenceFramePlugin::onLoadAnimation(const EmptyMessageArguments&)
{
    if (m_decompressionThreadStatus != DecompressionThreadStatus_Idle) {
        kzLogDebug(("SequenceFramePlugin::onLoadAnimation err decompression not idle, status: {}.",
            m_decompressionThreadStatus));
        return;
    }

    resetPluginStatus();

    m_texturePackageFile = new FileMapping();
    if (nullptr == m_texturePackageFile) {
        kzLogDebug(("SequenceFramePlugin::onLoadAnimation Could not create fileapping."));
        return;
    }

    int ret = m_texturePackageFile->mapFileIntoMemory(getProperty(PackagePathProperty).c_str());
    if (0 != ret) {
        kzLogDebug(("SequenceFramePlugin::onLoadAnimation Fail to do file mapping by PackagePath"));
        delete m_texturePackageFile;
        m_texturePackageFile = nullptr;
        return;
    }

    if (0 != getFileInformation()) {
        kzLogDebug(("SequenceFramePlugin::onLoadAnimation Fail to get file info."));
        m_texturePackageFile->closeFileMapping();
        delete m_texturePackageFile;
        m_texturePackageFile = nullptr;
        return;
    }

    m_isReversed = getProperty(ReverseProperty);
    if (!m_isReversed) {
        m_currentTextureIndex = 0;
    } else {
        m_currentTextureIndex = m_texturePackageInfo.textureNumber - 1;
    }

    Texture::CreateInfo2D createInfo(m_texturePackageInfo.textureWidth,
        m_texturePackageInfo.textureHeight,
        m_texturePackageInfo.textureFormat);
    m_texture = Texture::create(getDomain(), createInfo, "Animated Texture");

    // request decompress first texture
    kzsThreadLockAcquire(m_decompressionThreadLock);
    m_decompressionThreadStatus = DecompressionThreadStatus_Working;
    kzsThreadLockSet(m_decompressionThreadLock, KZ_TRUE, KZ_FALSE);
    kzsThreadLockRelease(m_decompressionThreadLock);

    EmptyMessageArguments oLoadingFinishedArgs;
    dispatchMessage(oLoadingFinished, oLoadingFinishedArgs);
}

void SequenceFramePlugin::onPlayAnimation(const EmptyMessageArguments&)
{
    if (nullptr == m_texturePackageFile) {
        kzLogDebug(("SequenceFramePlugin::onPlayAnimation texture package is null!."));
        return;
    }

    if (m_playTextureTimerToken != nullptr) {
        kzLogDebug(("SequenceFramePlugin::onPlayAnimation It's already play!."));
        return;
    }

    float FPS = getProperty(FPSProperty);
    if (FPS < 1.0) {
        FPS = 1.0;
    }
    m_playTextureTimerToken = addTimerHandler(
        getMessageDispatcher(),
        kanzi::chrono::milliseconds(int64_t(1000.0 / FPS)),
        KZU_TIMER_MESSAGE_MODE_REPEAT_BATCH,
        bind(&SequenceFramePlugin::onTimerShowTexture, this, placeholders::_1));
}

void SequenceFramePlugin::onStopAnimation(const EmptyMessageArguments&)
{
    while (m_decompressionThreadStatus != DecompressionThreadStatus_Idle) {
        kzLogDebug(("SequenceFramePlugin::onStopAnimation waiting \n"));
    }

    if (!getProperty(KeepLastFrameVisibleProperty)) {
        setProperty(StandardMaterial::TextureProperty, nullptr);
    }

    resetPluginStatus();
}

void SequenceFramePlugin::onPauseAnimation(const EmptyMessageArguments&)
{
    if (m_playTextureTimerToken != nullptr) {
        removeTimerHandler(getMessageDispatcher(), m_playTextureTimerToken);
        m_playTextureTimerToken = nullptr;
    }

    if (!getProperty(KeepLastFrameVisibleProperty)) {
        setProperty(StandardMaterial::TextureProperty, nullptr);
    }
}

void SequenceFramePlugin::onTimerShowTexture(const TimerMessageArguments&)
{
    DecompressionThreadStatus threadStatus = DecompressionThreadStatus_Idle;

    kzsThreadLockAcquire(m_decompressionThreadLock);
    threadStatus = m_decompressionThreadStatus;
    kzsThreadLockRelease(m_decompressionThreadLock);

    if (DecompressionThreadStatus_Working == m_decompressionThreadStatus) {
        // kzLogDebug(("SequenceFramePlugin::onTimerShowTexture current frame is not ready."));
        return;
    }

    if (0 <= m_currentTextureIndex
        && m_currentTextureIndex < m_texturePackageInfo.textureNumber) {

        Texture::CreateInfo2D createInfo(m_texturePackageInfo.textureWidth,
            m_texturePackageInfo.textureHeight,
            m_texturePackageInfo.textureFormat);
        m_texture_temp = Texture::create(getDomain(), createInfo, "Animated Texture temp");
        m_texture_temp->setData(m_textureData);

        swap(*m_texture, *m_texture_temp);

        if (0 == m_currentTextureIndex
            || nullptr == getProperty(StandardMaterial::TextureProperty)) {
            setProperty(StandardMaterial::TextureProperty, m_texture);
        } else {
            setChangeFlag(PropertyTypeChangeFlagRender);
        }

        unsigned int textureHandle = m_texture_temp->getNativeHandle();

        Renderer * renderer = getDomain()->getRenderer3D()->getCoreRenderer();
        renderer->deleteTexture(textureHandle);
        m_texture_temp.reset();

    // fps log
#if 0
        if (0 == m_fpsCounter) {
            m_fpsTimeStamp = kzsTimeGetCurrentTimestamp();
        }

        ++m_fpsCounter;

        if (static_cast<unsigned int>(getProperty(FPSProperty)) == m_fpsCounter - 1) {
            kzLogDebug(("SequenceFramePlugin::onTimerShowTexture FPS of animation: {}\n",
                static_cast<float>(m_fpsCounter - 1) /
                        static_cast<float>(kzsTimeGetCurrentTimestamp() - m_fpsTimeStamp)
                        * 1000.0));
            m_fpsCounter = 0;
        }
#endif
        if (!m_isReversed) {
            ++m_currentTextureIndex;

            // check if it's ending of animation
            if (m_currentTextureIndex >= m_texturePackageInfo.textureNumber) {
                if (getProperty(LoopPlaybackProperty)) {
                    m_currentTextureIndex = 0;
                }
                else {
                    if (!getProperty(KeepLastFrameVisibleProperty)) {
                        setProperty(StandardMaterial::TextureProperty, nullptr);
                    }
                    resetPluginStatus();

                    // sending finished message
                    EmptyMessageArguments oPlayingFinishedArgs;
                    dispatchMessage(oPlayingFinished, oPlayingFinishedArgs);
                    return;
                }
            }
        } else {
            --m_currentTextureIndex;

            // check if it's ending of animation
            if (m_currentTextureIndex < 0
                && m_texturePackageInfo.textureNumber >= 1) {
                if (getProperty(LoopPlaybackProperty)) {
                    m_currentTextureIndex = m_texturePackageInfo.textureNumber - 1;
                }
                else {
                    if (!getProperty(KeepLastFrameVisibleProperty)) {
                        setProperty(StandardMaterial::TextureProperty, nullptr);
                    }
                    resetPluginStatus();

                    // sending finished message
                    EmptyMessageArguments oPlayingFinishedArgs;
                    dispatchMessage(oPlayingFinished, oPlayingFinishedArgs);
                    return;
                }
            }
        }

        // request decompress next texture
        kzsThreadLockAcquire(m_decompressionThreadLock);
        m_decompressionThreadStatus = DecompressionThreadStatus_Working;
        kzsThreadLockSet(m_decompressionThreadLock, KZ_TRUE, KZ_FALSE);
        kzsThreadLockRelease(m_decompressionThreadLock);
    }
}

kzsError SequenceFramePlugin::decompressTexture(void* userData)
{
    //kzLogDebug(("SequenceFramePlugin::decompressTexture Thread decompress texture begin!."));

    SequenceFramePlugin* pluginData = static_cast<SequenceFramePlugin*>(userData);
    DecompressionThreadStatus threadStatus = DecompressionThreadStatus_Idle;
    int32_t size = 0;
    int32_t offset = 0;

    while (1) {
        kzsThreadLockAcquire(pluginData->m_decompressionThreadLock);
        while (DecompressionThreadStatus_Idle == pluginData->m_decompressionThreadStatus) {
            kzsThreadLockWaitAndReset(pluginData->m_decompressionThreadLock, KZ_FALSE);
        }
        threadStatus = pluginData->m_decompressionThreadStatus;
        kzsThreadLockRelease(pluginData->m_decompressionThreadLock);

        if (DecompressionThreadStatus_Destroy == threadStatus) {
            kzLogDebug(("SequenceFramePlugin::decompressTexture"
                        " Decompression thread will be destroyed."));
            break;
        }

        // thread status working
        if (0 == pluginData->m_currentTextureIndex) {
            size = pluginData->m_texturePointerVector[pluginData->m_currentTextureIndex]
                - pluginData->m_texturePackageInfo.dataOffset;
            offset = pluginData->m_texturePackageInfo.dataOffset;
        } else {
            size = pluginData->m_texturePointerVector[pluginData->m_currentTextureIndex]
                - pluginData->m_texturePointerVector[pluginData->m_currentTextureIndex - 1];
            offset = pluginData->m_texturePointerVector[pluginData->m_currentTextureIndex - 1];
        }
        if (CompressionAlgorithm_LZ4 == pluginData->m_texturePackageInfo.compressionAlgorithm) {
            DecompressBufferLZ4(size, 
                static_cast<byte*>(pluginData->m_texturePackageFile->getFileBuffer()) + offset,
                pluginData->m_textureSize, pluginData->m_textureData);
        } else if (CompressionAlgorithm_ZLIB
                   == pluginData->m_texturePackageInfo.compressionAlgorithm) {
            DecompressBufferZLIB(size, 
                static_cast<byte*>(pluginData->m_texturePackageFile->getFileBuffer()) + offset,
                pluginData->m_textureSize, pluginData->m_textureData);
        }
        //kzLogDebug(("SequenceFramePlugin::decompressTexture Thread decompress texture {}", pluginData->m_currentTextureIndex));
        // current texture decompressed
        kzsThreadLockAcquire(pluginData->m_decompressionThreadLock);
        pluginData->m_decompressionThreadStatus = DecompressionThreadStatus_Idle;
        kzsThreadLockRelease(pluginData->m_decompressionThreadLock);
    }

    //kzLogDebug(("SequenceFramePlugin::decompressTexture Thread decompress texture end!."));
    return KZS_SUCCESS;
}

int SequenceFramePlugin::getFileInformation()
{
    if (nullptr == m_texturePackageFile->getFileBuffer()) {
        kzLogDebug(("SequenceFramePlugin::getFileInformation Texture package buffer is NULL."));
        return -1;
    }

    memcpy(&(m_texturePackageInfo.sizeOffset),
        static_cast<byte*>(m_texturePackageFile->getFileBuffer()) + sizeof(int32_t) * 0,
        sizeof(int32_t));
    memcpy(&(m_texturePackageInfo.dataOffset),
        static_cast<byte*>(m_texturePackageFile->getFileBuffer()) + sizeof(int32_t) * 1,
        sizeof(int32_t));
    memcpy(&(m_texturePackageInfo.textureNumber),
        static_cast<byte*>(m_texturePackageFile->getFileBuffer()) + sizeof(int32_t) * 2,
        sizeof(int32_t));
    memcpy(&(m_texturePackageInfo.textureWidth),
        static_cast<byte*>(m_texturePackageFile->getFileBuffer()) + sizeof(int32_t) * 3,
        sizeof(int32_t));
    memcpy(&(m_texturePackageInfo.textureHeight),
        static_cast<byte*>(m_texturePackageFile->getFileBuffer()) + sizeof(int32_t) * 4,
        sizeof(int32_t));
    memcpy(&(m_texturePackageInfo.textureFormat),
        static_cast<byte*>(m_texturePackageFile->getFileBuffer()) + sizeof(int32_t) * 5,
        sizeof(int32_t));
    memcpy(&(m_texturePackageInfo.compressionAlgorithm),
        static_cast<byte*>(m_texturePackageFile->getFileBuffer()) + sizeof(int32_t) * 6,
        sizeof(int32_t));

    if (m_texturePackageInfo.sizeOffset < 0 || m_texturePackageInfo.dataOffset < 0 ||
        m_texturePackageInfo.textureNumber < 0 || m_texturePackageInfo.textureWidth < 0 ||
        m_texturePackageInfo.textureHeight < 0 || m_texturePackageInfo.textureFormat < 1 ||
        m_texturePackageInfo.compressionAlgorithm < 1) {

        kzLogDebug(("SequenceFramePlugin::getFileInformation Bad header information.\n"));
        kzLogDebug(("SequenceFramePlugin::getFileInformation sizeOffset is {}\n",
            m_texturePackageInfo.sizeOffset));
        kzLogDebug(("SequenceFramePlugin::getFileInformation dataOffset is {}\n",
            m_texturePackageInfo.dataOffset));
        kzLogDebug(("SequenceFramePlugin::getFileInformation textureNumber is {}\n",
            m_texturePackageInfo.textureNumber));
        kzLogDebug(("SequenceFramePlugin::getFileInformation textureWidth is {}\n",
            m_texturePackageInfo.textureWidth));
        kzLogDebug(("SequenceFramePlugin::getFileInformation textureHeight is {}\n",
            m_texturePackageInfo.textureHeight));
        kzLogDebug(("SequenceFramePlugin::getFileInformation textureFormat is {}\n",
            m_texturePackageInfo.textureFormat));
        kzLogDebug(("SequenceFramePlugin::getFileInformation compressionAlgorithm is {}\n",
            m_texturePackageInfo.compressionAlgorithm));

        return -1;
    }

    m_textureSize = getDataSizeBytes(m_texturePackageInfo.textureWidth,
        m_texturePackageInfo.textureHeight,
        m_texturePackageInfo.textureFormat);

    m_textureData = new byte[m_textureSize];

    for (int32_t i = 0; i < m_texturePackageInfo.textureNumber; ++i) {
        size_t textureSize = 0;
        memcpy(&textureSize, static_cast<byte*>(m_texturePackageFile->getFileBuffer())
            + m_texturePackageInfo.sizeOffset + sizeof(int32_t) * i, sizeof(int32_t));

        if (textureSize < 0) {
            kzLogDebug(("The {} compressed texture size is less than 0.\n", i));
            m_texturePointerVector.clear();
            return -1;
        }

        m_texturePointerVector.push_back(textureSize);
    }
    return 0;
}

void SequenceFramePlugin::resetPluginStatus()
{
    if (nullptr != m_playTextureTimerToken) {
        removeTimerHandler(getMessageDispatcher(), m_playTextureTimerToken);
        m_playTextureTimerToken = nullptr;
    }

    if (m_texturePackageFile != nullptr) {
        m_texturePackageFile->closeFileMapping();
        delete m_texturePackageFile;
        m_texturePackageFile = nullptr;
    }

    m_currentTextureIndex = 0;
    
    m_texturePointerVector.clear();

    if (m_textureData != nullptr) {
        delete[] m_textureData;
        m_textureData = nullptr;
    }
}
