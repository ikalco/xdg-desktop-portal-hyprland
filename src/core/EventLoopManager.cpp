#include "EventLoopManager.hpp"
#include "../helpers/Log.hpp"
#include "DBusManager.hpp"
#include "WaylandManager.hpp"
#include "PipewireManager.hpp"

using namespace EventLoop;

inline UP<CEventLoopManager> g_pEventLoopManager;
inline WP<CEventLoopManager> EventLoop::mgr() {
    if (!g_pEventLoopManager)
        g_pEventLoopManager = makeUnique<CEventLoopManager>();
    return g_pEventLoopManager;
}

CEventLoopManager::CEventLoopManager() {
    ;
}

bool CEventLoopManager::startPollThread() {
    // init pollfds and check they're valid
    pollfd pollfds[3]{{
                          .fd     = DBus::mgr()->getFD(),
                          .events = POLLIN,
                      },
                      {
                          .fd     = Wayland::mgr()->getFD(),
                          .events = POLLIN,
                      },
                      {
                          .fd     = Pipewire::mgr()->getFD(),
                          .events = POLLIN,
                      }};
    for (auto fd : pollfds) {
        if (fd.fd < 0)
            return false;
    }

    try {
        m_pollThread = std::jthread([this, &pollfds](std::stop_token st) {
            // copy pollfds cause its stack allocated
            pollfd fds[3];
            std::copy(fds, fds + 3, pollfds);

            auto terminate_token = m_terminate.get_token();
            while (!st.stop_requested() && !terminate_token.stop_requested()) {
                int ret = poll(pollfds, 3, 5000 /* 5 seconds, reasonable. It's because we might need to terminate */);
                if (ret < 0) {
                    Debug::log(CRIT, "[core] Polling fds failed with {}", strerror(errno));
                    m_terminate.request_stop();
                    break;
                }

                for (size_t i = 0; i < 3; ++i) {
                    if (pollfds[i].revents & POLLHUP) {
                        Debug::log(CRIT, "[core] Disconnected from pollfd id {}", i);
                        m_terminate.request_stop();
                        break;
                    }
                }

                if (ret != 0) {
                    Debug::log(TRACE, "[core] got poll event");
                    m_notify.mutex.lock();
                    m_notify.ready = true;
                    // avoid race condition with revents, just copy it
                    std::copy(m_notify.fds_copy, m_notify.fds_copy + 3, fds);
                    m_notify.mutex.unlock();
                    m_notify.signal.notify_all();
                }
            }
        });
    } catch (const std::system_error& e) {
        Debug::log(CRIT, "[core] Failed to start poll thread!!\n{}\n{}", e.code(), e.what());
        return false;
    }

    return true;
}

bool CEventLoopManager::startTimerThread() {
    try {
        m_timerThread = std::jthread([this](std::stop_token st) {
            auto terminate_token = m_terminate.get_token();
            while (!st.stop_requested() && !terminate_token.stop_requested()) {
                // find nearest timer ms
                m_timer.timersMutex.lock();
                float nearest = 60000; /* reasonable timeout */
                for (auto& t : m_timer.timers) {
                    float until = t->duration() - t->passedMs();
                    if (until < nearest)
                        nearest = until;
                }
                m_timer.timersMutex.unlock();

                // sleep until woken by terminate, timeout of nearest, or timer added
                std::unique_lock lk(m_timer.timerAdded);
                m_timer.timerAddedSignal.wait_for(lk, terminate_token, std::chrono::milliseconds((int)nearest),
                                                  [this, &terminate_token] { return m_timer.timerAdded || terminate_token.stop_requested(); });
                m_timer.timerAdded = false;
                lk.unlock();

                if (terminate_token.stop_requested())
                    break;

                // awakened. Check if any timers passed
                m_timer.timersMutex.lock();
                bool notify = false;
                for (auto& t : m_timer.timers) {
                    if (t->passed()) {
                        Debug::log(TRACE, "[core] got timer event");
                        notify = true;
                        break;
                    }
                }
                m_timer.timersMutex.unlock();

                if (notify) {
                    m_notify.mutex.lock();
                    m_notify.ready = true;
                    m_notify.mutex.unlock();
                    m_notify.signal.notify_all();
                }
            }
        });
    } catch (const std::system_error& e) {
        Debug::log(CRIT, "[core] Failed to start timer thread!!\n{}\n{}", e.code(), e.what());
        return false;
    }

    return true;
}

void CEventLoopManager::processTimers() {
    // process timers, essentially just frame pacing timer for now
    std::vector<CTimer*> toRemove;
    m_timer.timersMutex.lock();
    for (auto& t : m_timer.timers) {
        if (t->passed()) {
            t->m_fnCallback();
            toRemove.emplace_back(t.get());
            Debug::log(TRACE, "[core] calling timer {}", (void*)t.get());
        }
    }

    if (!toRemove.empty())
        std::erase_if(m_timer.timers,
                      [&](const auto& t) { return std::find_if(toRemove.begin(), toRemove.end(), [&](const auto& other) { return other == t.get(); }) != toRemove.end(); });

    m_timer.timersMutex.unlock();
}

void CEventLoopManager::start() {
    pollfd pollfds[3];

    auto   st = m_terminate.get_token();
    while (!st.stop_requested()) {
        // its possible that by the time we finish the loop, ready gets set again
        if (!m_notify.ready) {
            // wait for events
            std::unique_lock lk(m_notify.mutex);
            m_notify.signal.wait_for(lk, st, std::chrono::seconds(5), [this, &st] { return m_notify.ready || st.stop_requested(); });

            if (st.stop_requested())
                break;

            // condition variable automatically locks the mutex
            m_notify.ready = false;
            std::copy(pollfds, pollfds + 3, m_notify.fds_copy);
            lk.unlock();
        } else {
            // if ready was set again, recopy fds revents
            m_notify.mutex.lock();
            m_notify.ready = false;
            std::copy(pollfds, pollfds + 3, m_notify.fds_copy);
            m_notify.mutex.unlock();
        }

        if (pollfds[0].revents & POLLIN)
            DBus::mgr()->processEvents();

        if (pollfds[1].revents & POLLIN)
            Wayland::mgr()->readEvents();

        if (pollfds[2].revents & POLLIN)
            Pipewire::mgr()->processEvents();

        processTimers();

        Wayland::mgr()->processEvents();
    }

    // make sure jthreads get stopped if main stopped
    m_terminate.request_stop();
}
