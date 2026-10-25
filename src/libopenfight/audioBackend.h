#ifndef OPEN_FIGHT_AUDIO_BACKEND_H
#define OPEN_FIGHT_AUDIO_BACKEND_H

#include <string>

// Sound names map to data/sfx/<name>.wav. The default implementation is silent so
// platforms without audio support (or tests) keep working unchanged.
class AudioBackend {
public:
    virtual ~AudioBackend() {
    }
    virtual void playSound(const std::string &name) {
        (void)name;
    }
    virtual void playMusic(const std::string &name) {
        (void)name;
    }
    virtual void stopMusic() {
    }
};

extern AudioBackend *audio;

#endif
