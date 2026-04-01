#include <fstream>
#include <cstdlib>
#include <sstream>
#include <random>
#include <algorithm>

#ifndef _WIN32
#include <csignal>
#include <unistd.h>
#endif

#include "AudioManager.h"

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#endif

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
    sfxByEvent["pickup_key"] = "sounds/key_pickup.wav";
    sfxByEvent["pickup_sword"] = "sounds/sword_pickup.wav";
    sfxByEvent["pickup_heal"] = "sounds/heal_pickup.wav";
    sfxByEvent["pickup_shield"] = "sounds/shield_pickup.wav";
    sfxByEvent["teleport"] = "sounds/teleport.wav";
    sfxByEvent["door"] = "sounds/door.wav";
    sfxByEvent["jump"] = "sounds/spring.wav";
    sfxByEvent["spring"] = "sounds/spring.wav";
    sfxByEvent["hurt"] = "sounds/hurt.wav";
    sfxByEvent["walk"] = "sounds/walk.wav";
    sfxByEvent["parry"] = "sounds/parry.wav";
    sfxByEvent["explosion"] = "sounds/explosion.wav";
    sfxByEvent["sword_attack"] = "sounds/sword_attack.wav";
    sfxByEvent["menu_music"] = "sounds/menu_music.mp3";
    sfxByEvent["game_music"] = "sounds/game_music.mp3";

    menuMusicPath = "sounds/menu_music.mp3";
    if (!fileExists(menuMusicPath))
        menuMusicPath.clear();

    gameMusicPath = "sounds/game_music.mp3";
    if (!fileExists(gameMusicPath))
    {
        gameMusicPath = "sounds/music_bg.mp3";
        if (!fileExists(gameMusicPath))
            gameMusicPath.clear();
    }
    initialized = true;
}

void AudioManager::playSfx(const std::string &eventId, float volume, float pitch)
{
    if (!initialized)
        init();

    auto it = sfxByEvent.find(eventId);
    if (it == sfxByEvent.end())
        return;
    if (!fileExists(it->second))
        return;

    runPlaybackCommand(it->second, volume, pitch);
}

void AudioManager::playHurt(HurtProfile profile)
{
    if (!initialized)
        init();

    auto it = sfxByEvent.find("hurt");
    if (it == sfxByEvent.end() || !fileExists(it->second))
        return;

    static std::mt19937 rng(std::random_device{}());
    float basePitch = 1.0f;
    float jitter = 0.04f;
    switch (profile)
    {
    case HurtProfile::Player:
        basePitch = 1.0f;
        jitter = 0.05f;
        break;
    case HurtProfile::EnemyLow:
        basePitch = 0.75f;
        jitter = 0.04f;
        break;
    case HurtProfile::EnemyHigh:
        basePitch = 1.8f;
        jitter = 0.05f;
        break;
    }
    std::uniform_real_distribution<float> dist(-jitter, jitter);
    float pitch = std::max(0.5f, std::min(1.8f, basePitch + dist(rng)));
    playSfx("hurt", 1.0f, pitch);
}

void AudioManager::playMusic()
{
    playGameMusic(true);
}

int AudioManager::spawnLoopingMusic(const std::string &path, float volume)
{
    if (path.empty() || !fileExists(path))
        return -1;

#ifdef _WIN32
    // WINDOWS: Native MP3 Playback
    // Give it a unique alias so we can control it later
    std::string alias = (path.find("menu") != std::string::npos) ? "bgm_menu" : "bgm_game";
    
    std::string cmdClose = "close " + alias;
    mciSendStringA(cmdClose.c_str(), NULL, 0, NULL);

    std::string cmdOpen = "open \"" + path + "\" type mpegvideo alias " + alias;
    mciSendStringA(cmdOpen.c_str(), NULL, 0, NULL);

    std::string cmdVol = "setaudio " + alias + " volume to " + std::to_string(int(volume * 500.0f));    // de 0 a 1000 según la documentación de MCI
    mciSendStringA(cmdVol.c_str(), NULL, 0, NULL);
    
    std::string cmdPlay = "play " + alias + " repeat";
    mciSendStringA(cmdPlay.c_str(), NULL, 0, NULL);
    
    return (alias == "bgm_menu") ? 100 : 200; // Return a fake PID for Windows
#else
    // LINUX: Your coworker's original code
    const std::string q = shellQuote(path);
    std::ostringstream volSs;
    volSs.setf(std::ios::fixed);
    volSs.precision(3);
    volSs << std::max(0.0f, std::min(1.5f, volume));
    int pulseVol = int(std::max(0.0f, std::min(1.0f, volume)) * 65536.0f);
    std::ostringstream pulseVolSs;
    pulseVolSs << pulseVol;

    std::string pidFile = std::string("/tmp/bbc_music_pid_") + std::to_string(getpid()) + ".txt";
    std::string pidQ = shellQuote(pidFile);
    std::string ffplayLoop = "ffplay -nodisp -loglevel quiet -nostats -probesize 32 -analyzeduration 0 -stream_loop -1 -autoexit -af \"volume=" + volSs.str() + "\" " + q;
    std::string fallbackLoop = "while true; do paplay --volume=" + pulseVolSs.str() + " " + q + " || aplay " + q + " || play -q " + q + "; done";
    std::string cmd = "sh -c 'setsid sh -c \"(" + ffplayLoop + " || " + fallbackLoop + ") >/dev/null 2>&1\" & echo $! > " + pidQ + "'";
    std::system(cmd.c_str());

    std::ifstream pidIn(pidFile.c_str());
    int pid = -1;
    if (pidIn.good())
        pidIn >> pid;
    pidIn.close();
    std::remove(pidFile.c_str());
    return pid;
#endif
}

void AudioManager::stopPid(int &pidRef)
{
    if (pidRef > 0)
    {
#ifdef _WIN32
        if (pidRef == 100) mciSendStringA("close bgm_menu", NULL, 0, NULL);
        if (pidRef == 200) mciSendStringA("close bgm_game", NULL, 0, NULL);
#else
        kill(-pidRef, SIGTERM);
        kill(pidRef, SIGTERM);
        usleep(120000);
        kill(-pidRef, SIGKILL);
        kill(pidRef, SIGKILL);
#endif
        pidRef = -1;
    }
}

void AudioManager::pausePid(int pid)
{
    if (pid > 0) {
#ifdef _WIN32
        if (pid == 100) mciSendStringA("pause bgm_menu", NULL, 0, NULL);
        if (pid == 200) mciSendStringA("pause bgm_game", NULL, 0, NULL);
#else
        kill(-pid, SIGSTOP);
#endif
    }
}

void AudioManager::resumePid(int pid)
{
    if (pid > 0) {
#ifdef _WIN32
        if (pid == 100) mciSendStringA("resume bgm_menu", NULL, 0, NULL);
        if (pid == 200) mciSendStringA("resume bgm_game", NULL, 0, NULL);
#else
        kill(-pid, SIGCONT);
#endif
    }
}

void AudioManager::pausePid(int pid)
{
    if (pid > 0)
        kill(-pid, SIGSTOP);
}

void AudioManager::resumePid(int pid)
{
    if (pid > 0)
        kill(-pid, SIGCONT);
}

void AudioManager::startMusicPath(const std::string &path, MusicTrack track, bool restartFromBeginning, float volume)
{
    if (!initialized)
        init();
    if (path.empty() || !fileExists(path))
        return;

    if (track == MusicTrack::Menu)
    {
        if (gameMusicPid > 0 && !gameMusicPaused)
        {
            pausePid(gameMusicPid);
            gameMusicPaused = true;
        }
        if (restartFromBeginning || menuMusicPid <= 0)
        {
            stopPid(menuMusicPid);
            menuMusicPid = spawnLoopingMusic(path, volume);
        }
        currentTrack = MusicTrack::Menu;
        return;
    }

    if (track == MusicTrack::Game)
    {
        stopPid(menuMusicPid);
        if (restartFromBeginning)
        {
            stopPid(gameMusicPid);
            gameMusicPid = spawnLoopingMusic(path, volume);
            gameMusicPaused = false;
        }
        else
        {
            if (gameMusicPid > 0 && gameMusicPaused)
            {
                resumePid(gameMusicPid);
                gameMusicPaused = false;
            }
            else if (gameMusicPid <= 0)
            {
                gameMusicPid = spawnLoopingMusic(path, volume);
                gameMusicPaused = false;
            }
        }
        currentTrack = MusicTrack::Game;
    }

}

void AudioManager::playMenuMusic(bool restartFromBeginning)
{
    startMusicPath(menuMusicPath, MusicTrack::Menu, restartFromBeginning, 0.50f);
}

void AudioManager::playGameMusic(bool restartFromBeginning)
{
    startMusicPath(gameMusicPath, MusicTrack::Game, restartFromBeginning, 0.60f);
}

void AudioManager::pauseMusic()
{
    if (gameMusicPid > 0 && !gameMusicPaused)
    {
        pausePid(gameMusicPid);
        gameMusicPaused = true;
    }
}

void AudioManager::resumeMusic()
{
    if (gameMusicPid > 0 && gameMusicPaused)
    {
        resumePid(gameMusicPid);
        gameMusicPaused = false;
    }
}

void AudioManager::stopMusic()
{
    stopPid(menuMusicPid);
    stopPid(gameMusicPid);
    gameMusicPaused = false;
    currentTrack = MusicTrack::None;
}

void AudioManager::shutdown()
{
    stopMusic();
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

void AudioManager::runPlaybackCommand(const std::string &path, float volume, float pitch)
{
#ifdef _WIN32
    // WINDOWS: Native WAV Playback
    // (Windows PlaySound doesn't support pitch shifting easily, but it works instantly and won't crash!)
    PlaySoundA(path.c_str(), NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
#else
    // LINUX: Your coworker's original code
    const float sfxMaster = 0.45f;
    const std::string q = shellQuote(path);
    float safeVol = std::max(0.0f, std::min(3.0f, volume * sfxMaster));
    float safePitch = std::max(0.5f, std::min(1.8f, pitch));

    std::ostringstream volSs;
    volSs.setf(std::ios::fixed);
    volSs.precision(3);
    volSs << safeVol;
    std::ostringstream pitchSs;
    pitchSs.setf(std::ios::fixed);
    pitchSs.precision(3);
    pitchSs << safePitch;
    int pulseVol = int(std::max(0.0f, std::min(1.0f, safeVol)) * 65536.0f);
    std::ostringstream pulseVolSs;
    pulseVolSs << pulseVol;

    bool neutralFx = std::abs(safePitch - 1.0f) < 0.01f && std::abs(safeVol - 1.0f) < 0.01f;
    std::string ffplayCmd = "ffplay -nodisp -autoexit -loglevel quiet -nostats -probesize 32 -analyzeduration 0 -af \"asetrate=44100*" + pitchSs.str() + ",aresample=44100,volume=" + volSs.str() + "\" " + q;
    std::string paplayCmd = "paplay --volume=" + pulseVolSs.str() + " " + q;
    std::string aplayCmd = "aplay " + q;
    std::string soxCmd = "play -q " + q + " vol " + volSs.str() + " speed " + pitchSs.str();
    std::string cmd;
    if (neutralFx)
        cmd = "sh -c '(" + paplayCmd + " || " + aplayCmd + " || " + soxCmd + " || " + ffplayCmd + ") >/dev/null 2>&1 &'";
    else
        cmd = "sh -c '(" + ffplayCmd + " || " + paplayCmd + " || " + aplayCmd + " || " + soxCmd + ") >/dev/null 2>&1 &'";
    std::system(cmd.c_str());
#endif
}