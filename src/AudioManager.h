#ifndef _AUDIO_MANAGER_INCLUDE
#define _AUDIO_MANAGER_INCLUDE

#include <string>
#include <unordered_map>


class AudioManager
{
public:
    enum class MusicTrack {
        None,
        Menu,
        Game
    };

	enum class HurtProfile {
		Player,
		EnemyLow,
		EnemyHigh
	};

    static AudioManager &instance();

    void init();
    void playSfx(const std::string &eventId, float volume = 1.0f, float pitch = 1.0f);
    void playHurt(HurtProfile profile);
    void playMusic();
    void playMenuMusic(bool restartFromBeginning);
    void playGameMusic(bool restartFromBeginning);
    void pauseMusic();
    void resumeMusic();
    void stopMusic();
    void shutdown();

private:
    AudioManager() = default;
    bool initialized = false;
    int menuMusicPid = -1;
    int gameMusicPid = -1;
    bool gameMusicPaused = false;
    MusicTrack currentTrack = MusicTrack::None;
    std::unordered_map<std::string, std::string> sfxByEvent;
    std::string menuMusicPath;
    std::string gameMusicPath;

    void startMusicPath(const std::string &path, MusicTrack track, bool restartFromBeginning, float volume);
 	int spawnLoopingMusic(const std::string &path, float volume);
 	void stopPid(int &pidRef);
 	void pausePid(int pid);
 	void resumePid(int pid);

    static std::string shellQuote(const std::string &raw);
    static bool fileExists(const std::string &path);
    static void runPlaybackCommand(const std::string &path, float volume, float pitch);
};


#endif // _AUDIO_MANAGER_INCLUDE
