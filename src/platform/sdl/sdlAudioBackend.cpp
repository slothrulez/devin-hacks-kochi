#include "sdlAudioBackend.h"
#include <SDL.h>

SdlAudioBackend::SdlAudioBackend() : open(false), music(NULL) {
    if (Mix_OpenAudio(22050, MIX_DEFAULT_FORMAT, 2, 1024) != 0) {
        SDL_Log("Mix_OpenAudio failed: %s", Mix_GetError());
        return;
    }
    Mix_AllocateChannels(16);
    Mix_VolumeMusic(MIX_MAX_VOLUME / 3);
    open = true;
}

SdlAudioBackend::~SdlAudioBackend() {
    if (!open)
        return;
    Mix_HaltMusic();
    if (music != NULL)
        Mix_FreeMusic(music);
    for (auto &entry : chunks)
        Mix_FreeChunk(entry.second);
    Mix_CloseAudio();
}

bool SdlAudioBackend::isOpen() const {
    return open;
}

Mix_Chunk *SdlAudioBackend::load(const std::string &name) {
    auto it = chunks.find(name);
    if (it != chunks.end())
        return it->second;
    Mix_Chunk *chunk = Mix_LoadWAV(("data/sfx/" + name + ".wav").c_str());
    if (chunk == NULL)
        SDL_Log("Mix_LoadWAV(%s) failed: %s", name.c_str(), Mix_GetError());
    chunks[name] = chunk;
    return chunk;
}

void SdlAudioBackend::playSound(const std::string &name) {
    if (!open)
        return;
    Mix_Chunk *chunk = load(name);
    if (chunk != NULL)
        Mix_PlayChannel(-1, chunk, 0);
}

void SdlAudioBackend::playMusic(const std::string &name) {
    if (!open)
        return;
    stopMusic();
    music = Mix_LoadMUS(("data/sfx/" + name + ".wav").c_str());
    if (music == NULL) {
        SDL_Log("Mix_LoadMUS(%s) failed: %s", name.c_str(), Mix_GetError());
        return;
    }
    Mix_PlayMusic(music, -1);
}

void SdlAudioBackend::stopMusic() {
    if (!open || music == NULL)
        return;
    Mix_HaltMusic();
    Mix_FreeMusic(music);
    music = NULL;
}
