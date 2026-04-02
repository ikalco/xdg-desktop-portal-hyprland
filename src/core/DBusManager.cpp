#include "DBusManager.hpp"
#include "../helpers/Log.hpp"
#include "../portals/ScreenCast.hpp"
#include "../portals/Screenshot.hpp"
// #include "../portals/GlobalShortcuts.hpp"

using namespace DBus;
using namespace Hyprutils::Memory;

inline UP<CDBusManager> g_pDBusManager;
inline WP<CDBusManager> DBus::mgr() {
    if (!g_pDBusManager)
        g_pDBusManager = makeUnique<CDBusManager>();
    return g_pDBusManager;
}

CDBusManager::CDBusManager() {
    m_screencast = makeUnique<CScreenCastPortal>();
    m_screenshot = makeUnique<CScreenshotPortal>();
    // m_globalShortcuts = makeUnique<CGlobalShortcutsPortal>();
}

bool CDBusManager::init(const Wayland::SSupportedProtos& protos) {
    try {
        m_connection = CUniquePointer<sdbus::IConnection>(sdbus::createSessionBusConnection(sdbus::ServiceName{"org.freedesktop.impl.portal.desktop.hyprland"}).release());
    } catch (std::exception& e) {
        Debug::log(CRIT, "[dbus] Couldn't create the dbus connection ({})", e.what());
        return false;
    }

    if (!m_connection) {
        Debug::log(CRIT, "[dbus] Couldn't connect to dbus");
        return false;
    }

    m_screencast->init(protos);
    m_screenshot->init(protos);
    // m_globalShortcuts->init(protos);

    return true;
}

UP<SDBusSession> CDBusManager::createSession(sdbus::ObjectPath handle) {
    Debug::log(TRACE, "[dbus] Create Session {}", handle.c_str());

    auto session = makeUnique<SDBusSession>();

    session->object = UP<sdbus::IObject>(sdbus::createObject(connection(), handle).release());

    session->object
        ->addVTable(sdbus::registerMethod("Close").implementedAs([s = WP<SDBusSession>(session)]() {
            if (s.expired()) {
                Debug::log(TRACE, "[dbus] Close called for invalid session object");
                return;
            }

            Debug::log(TRACE, "[dbus] Close Session {}", s.get());

            s->onDestroy.emit();
        }))
        .forInterface("org.freedesktop.impl.portal.Session");

    return session;
}

UP<SDBusRequest> CDBusManager::createRequest(sdbus::ObjectPath handle) {
    Debug::log(TRACE, "[internal] Create Request {}", handle.c_str());

    auto request = makeUnique<SDBusRequest>();

    request->object = UP<sdbus::IObject>(sdbus::createObject(connection(), handle).release());

    request->object
        ->addVTable(sdbus::registerMethod("Close").implementedAs([r = WP<SDBusRequest>(request)]() {
            if (r.expired()) {
                Debug::log(TRACE, "[dbus] Close called for invalid request object");
                return;
            }

            Debug::log(TRACE, "[dbus] Close Request {}", r.get());

            r->onDestroy.emit();
        }))
        .forInterface("org.freedesktop.impl.portal.Request");

    return request;
}

sdbus::IConnection& CDBusManager::connection() {
    if (!m_connection) {
        Debug::log(ERR, "[dbus] Connection is gone?");
        exit(1);
    }
    return *m_connection;
}

void CDBusManager::processEvents() {
    while (m_connection->processPendingEvent()) {
        ;
    }
}
