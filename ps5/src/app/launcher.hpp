// Boxshelf PS5 - starts a game from the shelf. SPDX-License-Identifier: GPL-3.0-or-later
//
// The route is Estante's (main.cpp start_launch / direct_launch_job), run on a thread of its own:
//  * PS5 / PS4 titles and apps: Elf Arsenal's /launch?titleId=<id>.
//  * Porpoise, PS5-ECAM, SwanStationPS5, ProsperoEden: straight into the game. The home-screen launcher that Porpoise
//    ships (/data/homebrew/PPSA99764/assets/forwarder/home-launcher.elf) is patched with the frontend's title id and
//    the game file and sent to the ELF loader on port 9021. A PS1 game folder resolves to its .cue (.chd, .m3u...),
//    a Switch game folder to its biggest .nsp/.xci.
//  * PS5SX2: the image path goes into /data/PCSX2/logs/disc-launch.txt (Elf Arsenal's FTP on 2121), then the
//    frontend is opened.
//  * When the direct route fails, the frontend opens on its own library.
// Before anything, /api/state says whether another game is running; afterwards it confirms the title came up.
#pragma once

#include <pthread.h>

#include <string>

namespace boxshelf::app
{
struct LaunchRequest
{
    std::string title;  // for the messages
    std::string app;    // the title id to open (the game's own, or its emulator's)
    std::string target; // emulator games: the game file or folder on the console
    bool ps1_folder = false, switch_folder = false;
};

class Launcher
{
  public:
    Launcher();
    ~Launcher();
    Launcher(const Launcher &) = delete;
    Launcher &operator=(const Launcher &) = delete;

    // false while another launch is still running
    bool start(LaunchRequest request);
    bool busy() const;
    // The latest message for the toast; `working` while the launch is still under way, `done` once it is over.
    // `serial` changes with every new message.
    struct Status
    {
        std::string message;
        bool working = false, ok = false;
        unsigned serial = 0;
    };
    Status status() const;

  private:
    static void *thread_main(void *self);
    void run();
    void say(const std::string &message, bool working, bool ok = false);

    mutable pthread_mutex_t lock_;
    pthread_t thread_{};
    bool joinable_ = false, busy_ = false;
    LaunchRequest request_;
    Status status_;
};

// Play time: while a started game is open, asks the console every 15 s (/api/state) whether it still runs; done()
// turns true once it does not. The app itself may be asleep in the background meanwhile: the first answer after it
// wakes up ends the session then.
class PlayClock
{
  public:
    PlayClock();
    ~PlayClock();
    PlayClock(const PlayClock &) = delete;
    PlayClock &operator=(const PlayClock &) = delete;

    void watch(const std::string &app); // stops watching the previous one
    void stop();
    bool watching() const;
    bool done() const; // the game closed (once true, watch() again or stop())

  private:
    static void *thread_main(void *self);
    void run();

    mutable pthread_mutex_t lock_;
    pthread_t thread_{};
    bool joinable_ = false, quit_ = false, done_ = false;
    std::string app_;
};
} // namespace boxshelf::app
