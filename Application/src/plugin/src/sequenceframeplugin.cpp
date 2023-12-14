// Copyright 2022-2023 by Rightware. All rights reserved.

#include "sequenceframeplugin.hpp"
#include <string>

#include "filemapping.h"
#include "decompressor.h"
#define LZ4_EXTERNAL_FILE (1)

PropertyType<string> SequenceFramePlugin::PackagePathProperty(
    kzMakeFixedString("SequenceFramePlugin.PackagePath"), "", 0, false,
    KZ_DECLARE_EDITOR_METADATA(
        metadata.tooltip = "Path for the texture package.xyzzz";
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
    , m_texturePackageFile(nullptr)
    , m_texture(nullptr)
    , m_isReversed(false)
    , m_currentTextureIndex(0)
    , m_textureSize(0)
    , m_textureData(nullptr)
    , m_fpsTimeStamp(0)
    , m_fpsCounter(0)
{
	computeSourcePointer = nullptr;
    m_decompressionThread = std::thread(&SequenceFramePlugin::decompressTexture, this);
}

SequenceFramePlugin::~SequenceFramePlugin()
{
    {
        std::lock_guard<kanzi::mutex> lock(m_decompressionThreadLock);
        m_decompressionThreadStatus = DecompressionThreadStatus_Destroy;
    }
    m_decompressionCondition.notify_one();
    m_decompressionThread.join();
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

    removeMessageHandler(m_loadAnimationMessageToken);
    removeMessageHandler(m_playAnimationMessageToken);
    removeMessageHandler(m_stopAnimationMessageToken);
    removeMessageHandler(m_pauseAnimationMessageToken);

    getDomain()->getMainLoopScheduler()->removeTimer(m_playTextureTimerToken);

    resetPluginStatus();
}

bool SequenceFramePlugin::loadAnimationFile()
{
#if LZ4_EXTERNAL_FILE
    m_texturePackageFile = new FileMapping();
    if (nullptr == m_texturePackageFile) {
        kzLogDebug(("SequenceFramePlugin::onLoadAnimation Could not create fileapping."));
        return false;
    }

    string filePath = getProperty(PackagePathProperty);
    int ret = m_texturePackageFile->mapFileIntoMemory(filePath.c_str());
    if (0 != ret) {
        kzLogDebug(("SequenceFramePlugin::onLoadAnimation: failed to map file '{}'", filePath));
        delete m_texturePackageFile;
        m_texturePackageFile = nullptr;
        return false;
    }

    if (0 != getFileInformation()) {
        kzLogDebug(("SequenceFramePlugin::onLoadAnimation Fail to get file info."));
        m_texturePackageFile->closeFileMapping();
        delete m_texturePackageFile;
        m_texturePackageFile = nullptr;
        return false;
    }
#else
	BinaryResourceSharedPtr bResource = nullptr;
	bResource = getDomain()->getResourceManager()->acquireResource<BinaryResource>(getProperty(PackagePathProperty).c_str());
	//kzLogDebug(("SequenceFrameIndexPlugin::onLoadAnimation computeSourcePointer PackagePath: {}  =====", getProperty(PackagePathProperty)));
	if (bResource) {
		computeSourcePointer = bResource->getData();
	}
	//kzLogDebug(("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!`~~~~~~~~~~~~1"));
	if (computeSourcePointer == nullptr) {
		kzLogDebug(("SequenceFrameIndexPlugin::onLoadAnimation computeSourcePointer is nullptr."));
		return false;
	}
    //kzLogDebug(("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!`~~~~~~~~~~~~2"));

	if (0 != getFileInformation()) {
		kzLogDebug(("SequenceFrameIndexPlugin::onLoadAnimation Fail to get file info."));
		bResource = nullptr;
		delete computeSourcePointer;
		computeSourcePointer = nullptr;
        return false;
    }
	//kzLogDebug(("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!`~~~~~~~~~~~~3"));
#endif
    m_isReversed = getProperty(ReverseProperty);
    if (!m_isReversed) {
        m_currentTextureIndex = 0;
    }
    else {
        m_currentTextureIndex = m_texturePackageInfo.textureNumber - 1;
    }

    Texture::CreateInfo2D createInfo(m_texturePackageInfo.textureWidth,
        m_texturePackageInfo.textureHeight,
        m_texturePackageInfo.textureFormat);
    m_texture = Texture::create(getDomain(), createInfo, "Animated Texture");

    // request decompress first texture
    {
        std::lock_guard<kanzi::mutex> lock(m_decompressionThreadLock);
        m_decompressionThreadStatus = DecompressionThreadStatus_RequestWork;
    }
    m_decompressionCondition.notify_one();

    EmptyMessageArguments oLoadingFinishedArgs;
    dispatchMessage(oLoadingFinished, oLoadingFinishedArgs);

    return true;
}

void SequenceFramePlugin::onLoadAnimation(const EmptyMessageArguments&)
{
    {
        std::lock_guard<kanzi::mutex> lock(m_decompressionThreadLock);
        if (m_decompressionThreadStatus != DecompressionThreadStatus_Idle) {
            kzLogDebug(("SequenceFramePlugin::onLoadAnimation err decompression not idle, status: {}.",
                m_decompressionThreadStatus));
            return;
        }
    }

    resetPluginStatus();

    loadAnimationFile();
}

void SequenceFramePlugin::onPlayAnimation(const EmptyMessageArguments&)
{
    if (nullptr == m_texturePackageFile) {
        //kzLogDebug(("SequenceFramePlugin::onPlayAnimation texture package is null"));
        if (!loadAnimationFile())
            return;
    }

    if (!m_playTextureTimerToken.expired()) {
        kzLogDebug(("SequenceFramePlugin::onPlayAnimation It's already play!."));
        return;
    }

    float FPS = getProperty(FPSProperty);
    if (FPS < 1.0) {
        FPS = 1.0;
    }
    m_playTextureTimerToken = getDomain()->getMainLoopScheduler()->appendTimer(UserStage,
        kzMakeFixedString(""),
        MainLoopScheduler::TimerRecurrence::Recurring,
        kanzi::chrono::milliseconds(int64_t(1000.0 / FPS)),
        bind(&SequenceFramePlugin::onTimerShowTexture, this, placeholders::_1, placeholders::_2));
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
    getDomain()->getMainLoopScheduler()->removeTimer(m_playTextureTimerToken);

    /*
    if (!getProperty(KeepLastFrameVisibleProperty)) {
        setProperty(StandardMaterial::TextureProperty, nullptr);
    }
    */
}

void SequenceFramePlugin::onTimerShowTexture(chrono::nanoseconds, unsigned int)
{
    {
        std::lock_guard<kanzi::mutex> lock(m_decompressionThreadLock);
        if (DecompressionThreadStatus_Idle != m_decompressionThreadStatus) {
            // kzLogDebug(("SequenceFramePlugin::onTimerShowTexture current frame is not ready."));
            return;
        }
    }

    if (0 <= m_currentTextureIndex
        && m_currentTextureIndex < m_texturePackageInfo.textureNumber) {

        /*
        Texture::CreateInfo2D createInfo(m_texturePackageInfo.textureWidth,
            m_texturePackageInfo.textureHeight,
            m_texturePackageInfo.textureFormat);
        m_texture_temp = Texture::create(getDomain(), createInfo, "Animated Texture temp");
        m_texture_temp->setData(m_textureData);

        swap(*m_texture, *m_texture_temp);
        */
        m_texture->setData(m_textureData);

        if (0 == m_currentTextureIndex
            || nullptr == getProperty(StandardMaterial::TextureProperty)) {
            setProperty(StandardMaterial::TextureProperty, m_texture);
        } else {
            //setChangeFlag(PropertyTypeChangeFlagRender);
            invalidateRender();
        }

        /*
        unsigned int textureHandle = m_texture_temp->getNativeHandle();

        Renderer * renderer = getDomain()->getRenderer3D()->getCoreRenderer();
        renderer->deleteTexture(textureHandle);
        m_texture_temp.reset();
        */

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
        {
            std::lock_guard<kanzi::mutex> lock(m_decompressionThreadLock);
            m_decompressionThreadStatus = DecompressionThreadStatus_Working;
        }
        m_decompressionCondition.notify_one();
    }
}

void SequenceFramePlugin::decompressTexture()
{
    //kzLogDebug(("SequenceFramePlugin::decompressTexture Thread decompress texture begin!."));

    while (true)
    {
        // Wait for work
        {
            std::unique_lock<kanzi::mutex> lock(m_decompressionThreadLock);
            m_decompressionCondition.wait(lock, [this]() { return m_decompressionThreadStatus != DecompressionThreadStatus_Idle; });
            if (DecompressionThreadStatus_Destroy == m_decompressionThreadStatus) {
                break;
            }
            kzAssert(m_decompressionThreadStatus == DecompressionThreadStatus_RequestWork);
            // Tell the kanzi thread we are decompressing
            m_decompressionThreadStatus = DecompressionThreadStatus_Working;
        }

        size_t size = 0;
        size_t offset = 0;

        // thread status working
        if (0 == m_currentTextureIndex) {
            size = m_texturePointerVector[m_currentTextureIndex] - m_texturePackageInfo.dataOffset;
            offset = m_texturePackageInfo.dataOffset;
        } else {
            size = m_texturePointerVector[m_currentTextureIndex] - m_texturePointerVector[m_currentTextureIndex - 1];
            offset = m_texturePointerVector[m_currentTextureIndex - 1];
        }
#if LZ4_EXTERNAL_FILE
        auto* fileOffset = static_cast<byte*>(m_texturePackageFile->getFileBuffer()) + offset;

        if (CompressionAlgorithm_LZ4 == m_texturePackageInfo.compressionAlgorithm) {
            DecompressBufferLZ4(size, fileOffset, m_textureSize, m_textureData);
        } else if (CompressionAlgorithm_ZLIB == m_texturePackageInfo.compressionAlgorithm) {
            DecompressBufferZLIB(size, fileOffset, m_textureSize, m_textureData);
        }
#else
        if (CompressionAlgorithm_LZ4 == m_texturePackageInfo.compressionAlgorithm) {
            DecompressBufferLZ4(size, 
                 const_cast<byte*>(computeSourcePointer) + offset,
                m_textureSize, m_textureData);
        } else if (CompressionAlgorithm_ZLIB
                   == m_texturePackageInfo.compressionAlgorithm) {
            DecompressBufferZLIB(size, 
				const_cast<byte*>(computeSourcePointer) + offset,
                m_textureSize, m_textureData);
        }
#endif
        //kzLogDebug(("SequenceFramePlugin::decompressTexture Thread decompress texture {}", pluginData->m_currentTextureIndex));
        // current texture decompressed
        {
            std::lock_guard<kanzi::mutex> lock(m_decompressionThreadLock);
            if (m_decompressionThreadStatus == DecompressionThreadStatus_Working)
                m_decompressionThreadStatus = DecompressionThreadStatus_Idle;
        }
    }

    kzLogDebug(("SequenceFramePlugin::decompressTexture(): exit thread."));
}

int SequenceFramePlugin::getFileInformation()
{
#if LZ4_EXTERNAL_FILE
    if (nullptr == m_texturePackageFile->getFileBuffer()) {
        kzLogDebug(("SequenceFramePlugin::getFileInformation Texture package buffer is NULL."));
        return -1;
    }

    auto* bufStart = static_cast<byte*>(m_texturePackageFile->getFileBuffer());
    memcpy(&(m_texturePackageInfo.sizeOffset),
        bufStart,
        sizeof(int32_t));
    memcpy(&(m_texturePackageInfo.dataOffset),
        bufStart + sizeof(int32_t),
        sizeof(int32_t));
    memcpy(&(m_texturePackageInfo.textureNumber),
        bufStart + sizeof(int32_t) * 2,
        sizeof(int32_t));
    memcpy(&(m_texturePackageInfo.textureWidth),
        bufStart + sizeof(int32_t) * 3,
        sizeof(int32_t));
    memcpy(&(m_texturePackageInfo.textureHeight),
        bufStart + sizeof(int32_t) * 4,
        sizeof(int32_t));
    memcpy(&(m_texturePackageInfo.textureFormat),
        bufStart + sizeof(int32_t) * 5,
        sizeof(int32_t));
    memcpy(&(m_texturePackageInfo.compressionAlgorithm),
        bufStart + sizeof(int32_t) * 6,
        sizeof(int32_t));
#else
	if (nullptr == computeSourcePointer) {
		kzLogDebug(("SequenceFramePlugin::getFileInformation Texture package buffer is NULL."));
		return -1;
	}
	memcpy(&(m_texturePackageInfo.sizeOffset),
		computeSourcePointer + sizeof(int32_t) * 0,
		sizeof(int32_t));
	memcpy(&(m_texturePackageInfo.dataOffset),
		computeSourcePointer + sizeof(int32_t) * 1,
		sizeof(int32_t));
	memcpy(&(m_texturePackageInfo.textureNumber),
		computeSourcePointer + sizeof(int32_t) * 2,
		sizeof(int32_t));
	memcpy(&(m_texturePackageInfo.textureWidth),
		computeSourcePointer + sizeof(int32_t) * 3,
		sizeof(int32_t));
	memcpy(&(m_texturePackageInfo.textureHeight),
		computeSourcePointer + sizeof(int32_t) * 4,
		sizeof(int32_t));
	memcpy(&(m_texturePackageInfo.textureFormat),
		computeSourcePointer + sizeof(int32_t) * 5,
		sizeof(int32_t));
	memcpy(&(m_texturePackageInfo.compressionAlgorithm),
		computeSourcePointer + sizeof(int32_t) * 6,
		sizeof(int32_t));
#endif

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
#if LZ4_EXTERNAL_FILE
		memcpy(&textureSize, static_cast<byte*>(m_texturePackageFile->getFileBuffer())
			+ m_texturePackageInfo.sizeOffset + sizeof(int32_t) * i, sizeof(int32_t));
#else
			memcpy(&textureSize, const_cast<byte*>(computeSourcePointer)
				+ m_texturePackageInfo.sizeOffset + sizeof(int32_t) * i, sizeof(int32_t));
#endif

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
    getDomain()->getMainLoopScheduler()->removeTimer(m_playTextureTimerToken);

#if LZ4_EXTERNAL_FILE
    if (m_texturePackageFile != nullptr) {
        m_texturePackageFile->closeFileMapping();
        delete m_texturePackageFile;
        m_texturePackageFile = nullptr;
    }
#endif

    m_currentTextureIndex = 0;
    
    m_texturePointerVector.clear();

    delete[] m_textureData;
    m_textureData = nullptr;
}
