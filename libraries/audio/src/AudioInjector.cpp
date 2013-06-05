//
//  AudioInjector.cpp
//  hifi
//
//  Created by Stephen Birarda on 4/23/13.
//  Copyright (c) 2012 High Fidelity, Inc. All rights reserved.
//

#include <fstream>
#include <cstring>

#include <SharedUtil.h>
#include <PacketHeaders.h>
#include <UDPSocket.h>

#include "AudioInjector.h"

const int MAX_INJECTOR_VOLUME = 0xFF;

AudioInjector::AudioInjector(const char* filename) :
    _position(0.0f, 0.0f, 0.0f),
    _orientation(0.0f, 0.0f, 0.0f, 0.0f),
    _radius(0.0f),
    _volume(MAX_INJECTOR_VOLUME),
    _indexOfNextSlot(0),
    _isInjectingAudio(false)
{
    loadRandomIdentifier(_streamIdentifier, STREAM_IDENTIFIER_NUM_BYTES);
    
    std::fstream sourceFile;
    
    sourceFile.open(filename, std::ios::in | std::ios::binary);
    sourceFile.seekg(0, std::ios::end);
    
    int totalBytes = sourceFile.tellg();
    if (totalBytes == -1) {
        printf("Error reading audio data from file %s\n", filename);
        _audioSampleArray = NULL;
    } else {
        printf("Read %d bytes from audio file\n", totalBytes);
        sourceFile.seekg(0, std::ios::beg);
        _numTotalSamples = totalBytes / 2;
        _audioSampleArray = new int16_t[_numTotalSamples];
        
        sourceFile.read((char *)_audioSampleArray, totalBytes);
    }
}

AudioInjector::AudioInjector(int maxNumSamples) :
    _numTotalSamples(maxNumSamples),
    _position(0.0f, 0.0f, 0.0f),
    _orientation(0.0f, 0.0f, 0.0f, 0.0f),
    _radius(0.0f),
    _volume(MAX_INJECTOR_VOLUME),
    _indexOfNextSlot(0),
    _isInjectingAudio(false)
{
    loadRandomIdentifier(_streamIdentifier, STREAM_IDENTIFIER_NUM_BYTES);
    
    _audioSampleArray = new int16_t[maxNumSamples];
    memset(_audioSampleArray, 0, _numTotalSamples * sizeof(int16_t));
}

AudioInjector::~AudioInjector() {
    delete[] _audioSampleArray;
}

void AudioInjector::injectAudio(UDPSocket* injectorSocket, sockaddr* destinationSocket) {
    if (_audioSampleArray) {
        _isInjectingAudio = true;
        
        timeval startTime;
        
        // calculate the number of bytes required for additional data
        int leadingBytes = sizeof(PACKET_HEADER) + sizeof(INJECT_AUDIO_AT_POINT_COMMAND) + sizeof(_streamIdentifier)
            + sizeof(_position) + sizeof(_orientation) + sizeof(_volume);
        
        if (_radius > 0) {
            // we'll need 4 extra bytes if the cube side length is being sent as well
            leadingBytes += sizeof(_radius);
        }
        
        unsigned char dataPacket[BUFFER_LENGTH_BYTES + leadingBytes];
        
        dataPacket[0] = PACKET_HEADER_INJECT_AUDIO;
        // add the correct command for point source or cube of sound
        dataPacket[1] = (_radius > 0) ? INJECT_AUDIO_AT_CUBE_COMMAND : INJECT_AUDIO_AT_POINT_COMMAND;
        unsigned char *currentPacketPtr = dataPacket + sizeof(PACKET_HEADER) + sizeof(INJECT_AUDIO_AT_POINT_COMMAND);
        
        // copy the identifier for this injector
        memcpy(currentPacketPtr, &_streamIdentifier, sizeof(_streamIdentifier));
        currentPacketPtr += sizeof(_streamIdentifier);
        
        memcpy(currentPacketPtr, &_position, sizeof(_position));
        currentPacketPtr += sizeof(_position);
        
        if (_radius > 0) {
            // if we have a cube half height we need to send it here
            // this tells the mixer how much volume the injected audio will occupy
            memcpy(currentPacketPtr, &_radius, sizeof(_radius));
            currentPacketPtr += sizeof(_radius);
        }
        
        *currentPacketPtr = _volume;
        currentPacketPtr++;
        
        memcpy(currentPacketPtr, &_orientation, sizeof(_orientation));
        currentPacketPtr += sizeof(_orientation);
        
        gettimeofday(&startTime, NULL);
        int nextFrame = 0;
        
        for (int i = 0; i < _numTotalSamples; i += BUFFER_LENGTH_SAMPLES) {
            int numSamplesToCopy = BUFFER_LENGTH_SAMPLES;
            
            if (_numTotalSamples - i < BUFFER_LENGTH_SAMPLES) {
                numSamplesToCopy = _numTotalSamples - i;
                memset(currentPacketPtr + numSamplesToCopy, 0, BUFFER_LENGTH_BYTES - (numSamplesToCopy * sizeof(int16_t)));
            }
            
            memcpy(currentPacketPtr, _audioSampleArray + i, numSamplesToCopy * sizeof(int16_t));
            
            injectorSocket->send(destinationSocket, dataPacket, sizeof(dataPacket));
            
            double usecToSleep = usecTimestamp(&startTime) + (++nextFrame * BUFFER_SEND_INTERVAL_USECS) - usecTimestampNow();
            if (usecToSleep > 0) {
                usleep(usecToSleep);
            }
        }
        
        _isInjectingAudio = false;
    }
}

void AudioInjector::addSample(const int16_t sample) {
    if (_indexOfNextSlot != _numTotalSamples) {
        // only add this sample if we actually have space for it
        _audioSampleArray[_indexOfNextSlot++] = sample;
    }
}

void AudioInjector::addSamples(int16_t* sampleBuffer, int numSamples) {
    if (_audioSampleArray + _indexOfNextSlot + numSamples <= _audioSampleArray + (_numTotalSamples / sizeof(int16_t))) {
        // only copy the audio from the sample buffer if there's space
        memcpy(_audioSampleArray + _indexOfNextSlot, sampleBuffer, numSamples * sizeof(int16_t));
        _indexOfNextSlot += numSamples;
    }
}
