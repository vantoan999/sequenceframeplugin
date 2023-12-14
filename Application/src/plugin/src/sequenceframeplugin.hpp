// Copyright 2022-2023 by Rightware. All rights reserved.

#ifndef SEQUENCEFRAMEPLUGIN_HPP
#define SEQUENCEFRAMEPLUGIN_HPP

// Use kanzi.hpp only when you are learning to develop Kanzi applications.
// To improve compilation time in production projects, include only the header files of the Kanzi functionality you are using.
#include <kanzi/kanzi.hpp>

using namespace kanzi;

class SequenceFramePlugin;
class FileMapping;
typedef kanzi::shared_ptr<SequenceFramePlugin> SequenceFramePluginSharedPtr;

// The template component.
class SEQUENCEFRAMEPLUGIN_API SequenceFramePlugin : public Node2D
{
public:
    class EmptyMessageArguments : public MessageArguments {
    public:
        KZ_MESSAGE_ARGUMENTS_METACLASS_BEGIN(EmptyMessageArguments, MessageArguments, "Empty Message Arguments");
        KZ_METACLASS_END()
    };

    static PropertyType<string> PackagePathProperty;
    static PropertyType<float> FPSProperty;
    static PropertyType<bool> LoopPlaybackProperty;
    static PropertyType<bool> KeepLastFrameVisibleProperty;
    static PropertyType<bool> ReverseProperty;

    static MessageType<EmptyMessageArguments> LoadAnimation;
    static MessageType<EmptyMessageArguments> PlayAnimation;
    static MessageType<EmptyMessageArguments> StopAnimation;
    static MessageType<EmptyMessageArguments> PauseAnimation;

    static MessageType<EmptyMessageArguments> oLoadingFinished;
    static MessageType<EmptyMessageArguments> oPlayingFinished;

    KZ_METACLASS_BEGIN(SequenceFramePlugin, Node2D, "SequenceFramePlugin")
        KZ_METACLASS_PROPERTY_TYPE(PackagePathProperty);
        KZ_METACLASS_PROPERTY_TYPE(FPSProperty);
        KZ_METACLASS_PROPERTY_TYPE(LoopPlaybackProperty);
        KZ_METACLASS_PROPERTY_TYPE(KeepLastFrameVisibleProperty);
        KZ_METACLASS_PROPERTY_TYPE(ReverseProperty);
        KZ_METACLASS_MESSAGE_TYPE(LoadAnimation);
        KZ_METACLASS_MESSAGE_TYPE(PlayAnimation);
        KZ_METACLASS_MESSAGE_TYPE(StopAnimation);
        KZ_METACLASS_MESSAGE_TYPE(PauseAnimation);
        KZ_METACLASS_MESSAGE_TYPE(oLoadingFinished);
        KZ_METACLASS_MESSAGE_TYPE(oPlayingFinished);
    KZ_METACLASS_END()


    // Creates a SequenceFramePlugin.
    static SequenceFramePluginSharedPtr create(Domain* domain, string_view name);

    virtual ~SequenceFramePlugin();

protected:

    // Constructor.
    explicit SequenceFramePlugin(Domain* domain, string_view name);

    virtual void onAttached() KZ_OVERRIDE;
    virtual void onDetached() KZ_OVERRIDE;

private:
    enum DecompressionThreadStatus {
        DecompressionThreadStatus_Idle,         // thread is idle (set by worker thread)
        DecompressionThreadStatus_RequestWork,  // kanzi thread requested the worker thread to work (set by kanzi thread)
        DecompressionThreadStatus_Working,      // thread is working (set by worker thread)
        DecompressionThreadStatus_Destroy       // thread should quit (set by kanzi thread)
    };

    enum CompressionAlgorithm {
        CompressionAlgorithm_None = 0,
        CompressionAlgorithm_LZ4 = 1,
        CompressionAlgorithm_ZLIB = 2
    };

    struct TexturePackageInfo {
        int32_t sizeOffset;
        int32_t dataOffset;
        int32_t textureNumber;
        int32_t textureWidth;
        int32_t textureHeight;
        GraphicsFormat textureFormat;
        CompressionAlgorithm compressionAlgorithm;
    };


    /**
     * @brief loading animation and notify decompression first frame
     */
    void onLoadAnimation(const EmptyMessageArguments&);
    
    /**
     * @brief notification animation playing
     */
    void onPlayAnimation(const EmptyMessageArguments&);
    
    /**
     * @brief stop animation
     */
    void onStopAnimation(const EmptyMessageArguments&);
    
    /**
     * @brief pause animation
     */
    void onPauseAnimation(const EmptyMessageArguments&);

    /**
     * @brief show every frame when timer out
     */
    void onTimerShowTexture(kanzi::chrono::nanoseconds, unsigned int);

    /**
     * @brief decompress texture on the other thread
     */
    void decompressTexture();

    /**
     * @brief get the common information of comression file
     */
    int getFileInformation();

    /**
     * @brief reset status of this plugin
     */
    void resetPluginStatus();

    bool loadAnimationFile();

    DecompressionThreadStatus m_decompressionThreadStatus;
    kanzi::thread m_decompressionThread;
    kanzi::mutex m_decompressionThreadLock;
    kanzi::condition_variable m_decompressionCondition;

    FileMapping* m_texturePackageFile;
    TexturePackageInfo m_texturePackageInfo;
    TextureSharedPtr m_texture;
    TextureSharedPtr m_texture_temp;

    bool m_isReversed;
    int32_t m_currentTextureIndex;
    vector<size_t> m_texturePointerVector;
    size_t m_textureSize;
    byte* m_textureData;
	const byte* computeSourcePointer = NULL;
    MessageSubscriptionToken m_loadAnimationMessageToken;
    MessageSubscriptionToken m_playAnimationMessageToken;
    MessageSubscriptionToken m_stopAnimationMessageToken;
    MessageSubscriptionToken m_pauseAnimationMessageToken;

    kanzi::MainLoopTimerToken m_playTextureTimerToken;

    unsigned int m_fpsTimeStamp;
    unsigned int m_fpsCounter;
};

#endif
