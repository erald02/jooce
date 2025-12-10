#include "PluginProcessor.h"
#include "juce_audio_formats/juce_audio_formats.h"
#include <JuceHeader.h>

Plugin::Plugin(){
    juce::File file("/home/ep/Projects/NewProject/audio/Junk shop - Helvetia.wav");
    
    if (!file.existsAsFile())
    {
        DBG("Warning: Audio file not found: " + file.getFullPathName());
        return;
    }
    
    formatManager.registerBasicFormats();
    loadFile(file);
    
    if (fileBuffer == nullptr)
    {
        DBG("Warning: Failed to load audio file: " + file.getFullPathName());
    }
}

void Plugin::loadFile(juce::File& file){
    auto* reader = formatManager.createReaderFor(file);

    if (reader != nullptr){
        fileBuffer.reset(new juce::AudioBuffer<float>(reader->numChannels, (int)reader->lengthInSamples));

        reader->read(fileBuffer.get(), 0, (int)reader->lengthInSamples, 0, true, true);

        delete reader;
    }
}
