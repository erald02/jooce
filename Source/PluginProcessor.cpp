#include "PluginProcessor.h"
#include "juce_audio_formats/juce_audio_formats.h"
#include <JuceHeader.h>

Plugin::Plugin() {
  juce::File file(
      "/home/ep/Projects/NewProject/audio/Junk shop - Helvetia.wav");

  if (!file.existsAsFile()) {
    DBG("Warning: Audio file not found: " + file.getFullPathName());
    return;
  }

  formatManager.registerBasicFormats();
  loadFile(file);

  if (fileBuffer == nullptr) {
    DBG("Warning: Failed to load audio file: " + file.getFullPathName());
  }
}

void Plugin::loadFile(juce::File &file) {
  auto *reader = formatManager.createReaderFor(file);

  if (reader != nullptr) {
    fileBuffer.reset(new juce::AudioBuffer<float>(
        reader->numChannels, (int)reader->lengthInSamples));

    reader->read(fileBuffer.get(), 0, (int)reader->lengthInSamples, 0, true,
                 true);

    delete reader;
  }
}

void Plugin::getNextAudioBlock(const juce::AudioSourceChannelInfo &bufferToFill,
                               const juce::AudioDeviceManager &deviceManager) {
  if (fileBuffer == nullptr) {
    bufferToFill.clearActiveBufferRegion();
    return;
  }

  int pointerA{};
  int pointerB{};

  auto *device = deviceManager.getCurrentAudioDevice();

  if (device == nullptr) {
    bufferToFill.clearActiveBufferRegion();
    return;
  }

  auto activeOutputChannels = device->getActiveOutputChannels();
  auto maxOutputChannels = activeOutputChannels.getHighestBit() + 1;

  if (maxOutputChannels == 0) {
    bufferToFill.clearActiveBufferRegion();
    return;
  }

  int fileNumChannels = fileBuffer->getNumChannels();
  int fileLength = fileBuffer->getNumSamples();
  int numSamples = bufferToFill.numSamples;

  if (fileNumChannels == 0 || fileLength == 0) {
    bufferToFill.clearActiveBufferRegion();
    return;
  }

  //   int grainLength = 44100;

  for (int sample = 0; sample < numSamples; ++sample) {
    pointerA = (int)position;
    pointerB = pointerA + 1;
    if (pointerB >= fileLength) {
      pointerB = 0;
    }
    // float grainPhase = position / (float)grainLength;
    float window = 1.0F;// 1 was grain pahse

    float fraction = position - (float)pointerA;

    for (int channel = 0; channel < maxOutputChannels; ++channel) {
      if (!activeOutputChannels[channel]) {
        bufferToFill.buffer->clear(channel, bufferToFill.startSample,
                                   numSamples);
        continue;
      }

      auto *outBuffer = bufferToFill.buffer->getWritePointer(
          channel, bufferToFill.startSample);

      int fileChannel = channel % fileNumChannels;
      const auto *fileData = fileBuffer->getReadPointer(fileChannel);

      if (position < fileLength) { // was grain length
        outBuffer[sample] =
            (fileData[pointerA] + ((fileData[pointerB] - fileData[pointerA]) *
                                   (position - (float)pointerA))) *
            window;
      } else {
        outBuffer[sample] = 0.0F;
      }

      if (channel == maxOutputChannels - 1) {
        position = position + playbackSpeed;

        if (position >= fileLength) { // was grain length
          position = 0;
        }
      }
    }
  }
}
