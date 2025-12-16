#include <JuceHeader.h>
#include <cstdlib>
#include <string_view>

class Plugin
{
public:
    Plugin();
    std::unique_ptr<juce::AudioBuffer<float>> fileBuffer; 
    float position = 0.0F; 
    
    float playbackSpeed = 1.0F;

    void getNextAudioBlock(const juce::AudioSourceChannelInfo &bufferToFill, const juce::AudioDeviceManager &deviceManager);

private:
    juce::AudioFormatManager formatManager; 
    
    void loadFile(juce::File& file_name);

};

struct Grain{
    bool active = false;     
    float position = 0.0F;    
    float increment = 1.0F;  
    int age = 0;              
    int totalLength = 0;

    void start(){
        
    }

};