#include "MainComponent.h"

//==============================================================================
MainComponent::MainComponent()
{
    // Make sure you set the size of the component after
    // you add any child components.
    setSize (800, 600);

    // Some platforms require permissions to open input channels so request that here
    if (juce::RuntimePermissions::isRequired (juce::RuntimePermissions::recordAudio)
        && ! juce::RuntimePermissions::isGranted (juce::RuntimePermissions::recordAudio))
    {
        juce::RuntimePermissions::request (juce::RuntimePermissions::recordAudio,
                                           [&] (bool granted) { setAudioChannels (granted ? 2 : 0, 2); });
    }
    else
    {
        // Specify the number of input and output channels that we want to open
        setAudioChannels (2, 2);
    }
    plugin = std::make_unique<Plugin>();
}

MainComponent::~MainComponent()
{
    // This shuts down the audio device and clears the audio source.
    shutdownAudio();
}

//==============================================================================
void MainComponent::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    // This function will be called when the audio device is started, or when
    // its settings (i.e. sample rate, block size, etc) are changed.

    // You can use this function to initialise any resources you might need,
    // but be careful - it will be called on the audio thread, not the GUI thread.

    // For more details, see the help for AudioProcessor::prepareToPlay()
    if (plugin != nullptr)
    {
        plugin->position = 0;
    }
}

void MainComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
    if (plugin == nullptr || plugin->fileBuffer == nullptr) 
    {
        bufferToFill.clearActiveBufferRegion();
        return;
    }

    auto* device = deviceManager.getCurrentAudioDevice();
    
    if (device == nullptr)
    {
        bufferToFill.clearActiveBufferRegion();
        return;
    }
    
    auto activeOutputChannels = device->getActiveOutputChannels();
    auto maxOutputChannels = activeOutputChannels.getHighestBit() + 1;
    
    if (maxOutputChannels == 0)
    {
        bufferToFill.clearActiveBufferRegion();
        return;
    }
    
    int fileNumChannels = plugin->fileBuffer->getNumChannels();
    int fileLength = plugin->fileBuffer->getNumSamples();
    int numSamples = bufferToFill.numSamples;
    
    if (fileNumChannels == 0 || fileLength == 0)
    {
        bufferToFill.clearActiveBufferRegion();
        return;
    }
    for (int channel = 0; channel < maxOutputChannels; ++channel)
    {
        if (!activeOutputChannels[channel])
        {
            bufferToFill.buffer->clear(channel, bufferToFill.startSample, numSamples);
            continue;
        }

        auto* outBuffer = bufferToFill.buffer->getWritePointer(channel, bufferToFill.startSample);
        
        int fileChannel = channel % fileNumChannels;
        const auto* fileData = plugin->fileBuffer->getReadPointer(fileChannel);

        for (int sample = 0; sample < numSamples; ++sample)
        {
            if (plugin->position < fileLength)
            {
                outBuffer[sample] = fileData[plugin->position];
            }
            else
            {
                outBuffer[sample] = 0.0f;
            }
            
            if (channel == maxOutputChannels - 1)
            {
                plugin->position++;
                
                if (plugin->position >= fileLength)
                {
                    plugin->position = 0; 
                }
            }
        }
    }
}

void MainComponent::releaseResources()
{
    // This will be called when the audio device stops, or when it is being
    // restarted due to a setting change.

    // For more details, see the help for AudioProcessor::releaseResources()
}

//==============================================================================
void MainComponent::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    // You can add your drawing code here!
}

void MainComponent::resized()
{
    // This is called when the MainContentComponent is resized.
    // If you add any child components, this is where you should
    // update their positions.
}
