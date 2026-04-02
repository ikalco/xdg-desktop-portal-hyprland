#pragma once

#include <sdbus-c++/sdbus-c++.h>
#include <hyprutils/signal/Signal.hpp>
#include "../includes.hpp"

#include "WaylandManager.hpp"

using namespace Hyprutils::Signal;

namespace DBus {
    typedef std::tuple<uint32_t, std::unordered_map<std::string, sdbus::Variant>> dbUasv;

    class CScreenCastPortal;
    class CScreenshotPortal;
    class CGlobalShortcutsPortal;

    static sdbus::ObjectPath OBJECT_PATH = sdbus::ObjectPath{"/org/freedesktop/portal/desktop"};

    struct SDBusSession {
        UP<sdbus::IObject> object;
        sdbus::ObjectPath  handle;
        CSignalT<>         onDestroy;
    };

    struct SDBusRequest {
        UP<sdbus::IObject> object;
        sdbus::ObjectPath  handle;
        CSignalT<>         onDestroy;
    };

    class CDBusManager {
      public:
        CDBusManager();

        bool                init(const Wayland::SSupportedProtos& protos);
        void                processEvents();
        sdbus::IConnection& connection();

        // caller owns this and should hook into onDestroy
        UP<SDBusSession> createSession(sdbus::ObjectPath handle);
        UP<SDBusRequest> createRequest(sdbus::ObjectPath handle);

        int              getFD();

      private:
        UP<CScreenCastPortal>      m_screencast;
        UP<CScreenshotPortal>      m_screenshot;
        UP<CGlobalShortcutsPortal> m_globalShortcuts;

        UP<sdbus::IConnection>     m_connection;
    };

    WP<CDBusManager> mgr();
};
