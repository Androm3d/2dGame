#include <fstream>
#include <cstdlib>
#include "AudioManager.h"


AudioManager &AudioManager::instance()
{
    static AudioManager mgr;
    return mgr;
}

void AudioManager::init()
{
    if (initialized)
        return;

    // These files are optional. Missing files are simply skipped at runtime.
    sfxByEvent["pickup_key"] = "../sounds/key_pickup.wav";
    sfxByEvent["pickup_sword"] = "../sounds/sword_pickup.wav";
    sfxByEvent["pickup_heal"] = "../sounds/heal_pickup.wav";
    sfxByEvent["pickup_shield"] = "../sounds/shield_pickup.wav";
    sfxByEvent["teleport"] = "../sounds/teleport.wav";
    sfxByEvent["jump"] = "../sounds/jump.wav";
    sfxByEvent["hurt"] = "../sounds/hurt.wav";
    sfxByEvent["sword_attack"] = "../sounds/sword_attack.wav";

    musicPath = "../sounds/music_bg.mp3";
    initialized = true;

    playMusic();
}

void AudioManager::playSfx(const std::string &eventId)
{
    if (!initialized)
        init();

    auto it = sfxByEvent.find(eventId);
    if (it == sfxByEvent.end())
        return;
    if (!fileExists(it->second))
        return;

    runPlaybackCommand(it->second);
}

void AudioManager::playMusic()
{
    if (!initialized)
        init();
    if (musicPath.empty() || !fileExists(musicPath))
        return;

    runPlaybackCommand(musicPath);
}

std::string AudioManager::shellQuote(const std::string &raw)
{
    std::string out;
    out.reserve(raw.size() + 2);
    out.push_back('\'');
    for (char c : raw)
    {
        if (c == '\'')
            out += "'\"'\"'";
        else
            out.push_back(c);
    }
    out.push_back('\'');
    return out;
}

bool AudioManager::fileExists(const std::string &path)
{
    std::ifstream f(path.c_str(), std::ios::binary);
    return f.good();
}

void AudioManager::runPlaybackCommand(const std::string &path)
{
    const std::string q = shellQuote(path);
    // Try common Linux players in order. Command runs in background.
    std::string cmd = "sh -c '(paplay " + q + " || aplay " + q + " || ffplay -nodisp -autoexit -loglevel quiet " + q + " || play -q " + q + ") >/dev/null 2>&1 &'";
    std::system(cmd.c_str());
}
