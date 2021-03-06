//------------------------------------------------------------------------------
// FRONTIÈRES:  An interactive granular sampler.
//------------------------------------------------------------------------------
// More information is available at
//     http::/ccrma.stanford.edu/~carlsonc/256a/Borderlands/index.html
//
//
// Copyright (C) 2011  Christopher Carlson
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 3.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.


//-----------------------------------------------------------------------------
// name: MyRtAudio.cpp
// implementation of wrapper class for RtAudio
// author: Chris Carlson
//   date: fall 2011
//-----------------------------------------------------------------------------

#include "MyRtAudio.h"
#include <string.h>


MyRtAudio::MyRtAudio(unsigned int numOuts)
{
    // configure RtAudio
    // create the object
    audio.reset(new RtAudio);

    // check audio devices
    const RtAudio::DeviceInfo &info =
        audio->getDeviceInfo(audio->getDefaultOutputDevice());

    // io
    if (numOuts <= info.outputChannels)
        numOutputs = numOuts;
    else
        numOutputs = info.outputChannels;

    // store pointer to bufferSize
    myBufferSize = 1024;

    // set sample rate;
    mySRate = info.preferredSampleRate;
}


int MyRtAudio::rtaudioCallback(
    void *outputBuffer, void *, unsigned int numFrames,
    double, RtAudioStreamStatus, void *userData)
{
    MyRtAudio *self = (MyRtAudio *)userData;
    self->audioCallback((float *)outputBuffer, numFrames, self->audioUserData);
    return 0;
}


// set the audio callback and start the audio stream
bool MyRtAudio::openStream(AudioCallback callback, void *userData)
{
    audioCallback = callback;
    audioUserData = userData;

    // create stream options
    RtAudio::StreamOptions options;
    options.streamName = "Frontieres";

    RtAudio::StreamParameters oParams;
    // i/o params
    oParams.deviceId = audio->getDefaultOutputDevice();
    oParams.nChannels = numOutputs;
    oParams.firstChannel = 0;

    // open stream
    hasError = false;
    audio->openStream(&oParams, nullptr, RTAUDIO_FLOAT32, mySRate,
                      &myBufferSize, &rtaudioCallback, this, &options, &errorCallback);

    return !hasError;
}

// report the current sample rate
unsigned int MyRtAudio::getSampleRate()
{
    return mySRate;
}


// report the current buffer size
unsigned int MyRtAudio::getBufferSize()
{
    return myBufferSize;
}


// get the obtained channel count
unsigned int MyRtAudio::getObtainedOutputChannels()
{
    return numOutputs;
}


// start rtaudio stream
bool MyRtAudio::startStream()
{
    // start audio stream
    hasError = false;
    audio->startStream();

    return !hasError;
}

// stop rtaudio stream
bool MyRtAudio::stopStream()
{
    // try to stop audio stream
    hasError = false;
    audio->stopStream();

    return !hasError;
}

// report the stream latency
int MyRtAudio::getStreamLatency()
{
    return audio->getStreamLatency();
}

void MyRtAudio::errorCallback(RtAudioError::Type type, const std::string &errorText)
{
    if (type == RtAudioError::DEBUG_WARNING)
        return;

    hasError = type != RtAudioError::WARNING;
    std::cerr << errorText << "\n";
}

void MyRtAudio::midiInCallback(double timeStamp, std::vector<unsigned char> *message, void *userData)
{
    MyRtAudio *self = (MyRtAudio *)userData;
    self->receiveMidiIn(message->data(), (unsigned)message->size(), self->receiveMidiInUserData);
}

int MyRtAudio::openMidiInput(ReceiveMidi receive, unsigned bufferSize, void *userData)
{
    if (midiIn)
        return -1;

    midiIn.reset(new RtMidiIn(RtMidi::UNSPECIFIED, "Frontieres", bufferSize));

    receiveMidiIn = receive;
    receiveMidiInUserData = userData;

    midiIn->setCallback(&midiInCallback, this);
    midiIn->openVirtualPort();

    return 0;
}

bool MyRtAudio::hasError;

//------------------------------------------------------------------------------
MyRtJack::MyRtJack(unsigned int numOuts)
{
    numOutputs = numOuts;
}

int MyRtJack::jackProcess(jack_nframes_t nframes, void *userData)
{
    MyRtJack *self = (MyRtJack *)userData;

    if (self->midiIn && self->receiveMidiIn) {
        void *buffer = jack_port_get_buffer(self->midiIn, nframes);

        for (uint32_t i = 0, n = jack_midi_get_event_count(buffer); i < n; ++i) {
            jack_midi_event_t event;
            if (jack_midi_event_get(&event, buffer, i) == 0)
                self->receiveMidiIn(event.buffer, event.size, self->receiveMidiInUserData);
        }
    }

    float *temp = self->outputBuffer.get();
    self->audioCallback(temp, nframes, self->audioUserData);

    for (unsigned c = 0, nc = self->numOutputs; c < nc; ++c) {
        float *buffer = (float *)jack_port_get_buffer(self->outputs[c], nframes);
        for (unsigned i = 0; i < nframes; ++i)
            buffer[i] = temp[i * nc + c];
    }

    return 0;
}

bool MyRtJack::openStream(AudioCallback callback, void *userData)
{
    if (client)
        return false;

    client.reset(jack_client_open("Frontieres", JackNoStartServer, nullptr));
    if (!client)
        return false;

    outputBuffer.reset(new float[numOutputs * jack_get_buffer_size(client.get())]);

    audioCallback = callback;
    audioUserData = userData;

    for (unsigned i = 0; i < numOutputs; ++i) {
        char name[64];
        sprintf(name, "Output %u", i + 1);
        jack_port_t *port = jack_port_register(client.get(), name, JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
        if (!port) return false;
        outputs.push_back(port);
    }

    jack_set_process_callback(client.get(), &jackProcess, this);
    return true;
}

void MyRtJack::connectOutputs()
{
    std::unique_ptr<const char *[], void (*)(void *)> ports(
        jack_get_ports(client.get(), nullptr, JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput),
        &free);

    if (!ports)
        return;

    // identify the default device (the first one)
    std::string device;
    if (ports[0]) {
        if (const char *separator = strchr(ports[0], ':'))
            device.assign(ports[0], separator);
    }

    if (device.empty())
        return;

    // connect ports
    unsigned nports = 0;
    unsigned maxports = outputs.size();
    for (const char **p = ports.get(), *port; nports < maxports && (port = *p); ++p) {
        bool is_of_device = strlen(port) > device.size() &&
            port[device.size()] == ':' &&
            !memcmp(port, device.data(), device.size());
        if (is_of_device)
            jack_connect(client.get(), jack_port_name(outputs[nports++]), port);
    }
}

unsigned int MyRtJack::getSampleRate()
{
    return jack_get_sample_rate(client.get());
}

unsigned int MyRtJack::getBufferSize()
{
    return jack_get_buffer_size(client.get());
}

unsigned int MyRtJack::getObtainedOutputChannels()
{
    return (unsigned)outputs.size();
}

bool MyRtJack::startStream()
{
    return jack_activate(client.get()) == 0;
}

bool MyRtJack::stopStream()
{
    return jack_deactivate(client.get()) == 0;
}

int MyRtJack::getStreamLatency()
{
    return 0;
}

int MyRtJack::openMidiInput(ReceiveMidi receive, unsigned bufferSize, void *userData)
{
    if (midiIn)
        return -1;

    receiveMidiInUserData = userData;
    receiveMidiIn = receive;

    midiIn = jack_port_register(client.get(), "MIDI input", JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, bufferSize);
    if (!midiIn)
        return -1;

    return 0;
}
