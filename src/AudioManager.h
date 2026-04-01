#ifndef _AUDIO_MANAGER_INCLUDE
#define _AUDIO_MANAGER_INCLUDE

#include <string>
#include <unordered_map>


class AudioManager
{
public:
    static AudioManager &instance();

    void init();
    void playSfx(const std::string &eventId);
    void playMusic();

private:
    AudioManager() = default;
    bool initialized = false;
    std::unordered_map<std::string, std::string> sfxByEvent;
    std::string musicPath;

    static std::string shellQuote(const std::string &raw);
    static bool fileExists(const std::string &path);
    static void runPlaybackCommand(const std::string &path);
};


#endif // _AUDIO_MANAGER_INCLUDE
