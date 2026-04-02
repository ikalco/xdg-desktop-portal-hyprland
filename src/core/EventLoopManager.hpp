#pragma once

#include <sys/poll.h>
#include <condition_variable>
#include <thread>
#include "../includes.hpp"
#include "../helpers/Timer.hpp"

namespace EventLoop {
    class CEventLoopManager {
      public:
        CEventLoopManager();

        void start();

      private:
        struct {
            std::mutex                  mutex;
            std::condition_variable_any signal;
            pollfd                      fds_copy[3] = {};
            bool                        ready       = false;
        } m_notify;

        bool startPollThread();

        struct {
            UP<std::jthread>            thread;

            std::condition_variable_any timerAddedSignal;
            std::mutex                  timerAddedMutex;
            bool                        timerAdded = false;

            std::mutex                  timersMutex;
            std::vector<UP<CTimer>>     timers;
        } m_timer;
        bool             startTimerThread();
        void             processTimers();

        std::stop_source m_terminate;
        std::jthread     m_pollThread;
        std::jthread     m_timerThread;
    };

    WP<CEventLoopManager> mgr();
};
