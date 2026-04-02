#pragma once

#include "../includes.hpp"

namespace DBus {
    class CDBusManager;
};

namespace Wayland {
    class CWaylandManager;
};

namespace Portal {
    class CPortalManager {
      public:
        CPortalManager();

        std::string hyprDir();
        // const auto RUNTIME_DIR = getenv("XDG_RUNTIME_DIR");
        // const std::string                               HYPR_DIR             = RUNTIME_DIR ? std::string{RUNTIME_DIR} + "/hypr/" : "/tmp/hypr/";

      private:
        UP<DBus::CDBusManager>       m_dbusManager;
        UP<Wayland::CWaylandManager> m_waylandManager;

        friend class DBus::CDBusManager;
        friend class Wayland::CWaylandManager;
    };

    inline WP<CPortalManager> mgr() {
        static UP<CPortalManager> manager = nullptr;
        if (!manager)
            manager = makeUnique<CPortalManager>();
        return manager;
    }
};
