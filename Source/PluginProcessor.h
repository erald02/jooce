#include <JuceHeader.h>
#include <cstdlib>
#include <string_view>

class Plugin
{
public:
    Plugin();
    std::unique_ptr<juce::AudioBuffer<float>> fileBuffer; 
    int position = 0; 
private:
    juce::AudioFormatManager formatManager; 
    
    void loadFile(juce::File& file_name);

};