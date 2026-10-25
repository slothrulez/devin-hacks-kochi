#ifndef OPEN_FIGHT_SDL_AUDIO_BACKEND_H
#define OPEN_FIGHT_SDL_AUDIO_BACKEND_H

#include "audioBackend.h"
#include <SDL_mixer.h>
#include <map>
#include <string>

class SdlAudioBackend : public AudioBackend {
public:
    SdlAudioBackend();
    ~SdlAudioBackend() override;
    bool isOpen() const;
    void playSound(const std::string &name) override;
    void playMusic(const std::string &name) override;
    void stopMusic() override;

private:
    bool open;
    std::map<std::string, Mix_Chunk *> chunks;
    Mix_Music *music;
    Mix_Chunk *load(const std::string &name);
};

#endif
