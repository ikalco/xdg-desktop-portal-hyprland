#pragma once

#include "../core/DBusManager.hpp"
#include "../core/WaylandManager.hpp"

class CCZwlrForeignToplevelHandleV1;
class CCExtForeignToplevelHandleV1;

namespace DBus {
    enum eCursorModes : uint32_t {
        HIDDEN   = 1,
        EMBEDDED = 2,
        METADATA = 4,
    };

    enum eSourceTypes : uint32_t {
        MONITOR = 1,
        WINDOW  = 2,
        VIRTUAL = 4,
    };

    enum ePersistModes : uint32_t {
        NO_PERSIST    = 0,
        WHILE_RUNNING = 1,
        UNTIL_REVOKED = 2,
    };

    enum eSelectionType {
        TYPE_INVALID = -1,
        TYPE_OUTPUT  = 0,
        TYPE_TOPLEVEL,
        TYPE_REGION,
        TYPE_WORKSPACE,
    };

    struct SSelectionData {
        eSelectionType type          = TYPE_INVALID;
        bool           allowToken    = false;
        bool           overlayCursor = true;

        // output + region
        std::string output;
        uint32_t    x = 0, y = 0, w = 0, h = 0;

        // toplevel
        SP<CCZwlrForeignToplevelHandleV1> toplevelHandleWlr = nullptr;
        SP<CCExtForeignToplevelHandleV1>  toplevelHandleExt = nullptr;
        std::string                       windowClass;

        // TODO: workspace in the future
    };

    struct SScreenCastSession {
        WP<SScreenCastSession> m_self;

        // dbus session and request objects
        UP<SDBusSession>    m_session;
        CHyprSignalListener onDestroySession;
        UP<SDBusRequest>    m_request;
        CHyprSignalListener onDestroyRequest;

        // requested options
        uint32_t              m_sourceTypes = eSourceTypes::MONITOR;
        bool                  m_multiple    = false;
        eCursorModes          m_cursorMode  = eCursorModes::HIDDEN;
        ePersistModes         m_persistMode = ePersistModes::NO_PERSIST;

        SSelectionData        selectionData;

        const SSelectionData& getSelectData();
        std::string           promptSharePicker();

        // restore stuff
        sdbus::Struct<std::string, uint32_t, sdbus::Variant> getRestoreData();
        bool                                                 selectionDataRestored = false;
    };

    class CScreenCastPortal {
      public:
        CScreenCastPortal();

        bool   init(const Wayland::SSupportedProtos& protos);

        dbUasv onCreateSession(sdbus::ObjectPath requestHandle, sdbus::ObjectPath sessionHandle, std::string appID, std::unordered_map<std::string, sdbus::Variant> opts);
        dbUasv onSelectSources(sdbus::ObjectPath requestHandle, sdbus::ObjectPath sessionHandle, std::string appID, std::unordered_map<std::string, sdbus::Variant> opts);
        dbUasv onStart(sdbus::ObjectPath requestHandle, sdbus::ObjectPath sessionHandle, std::string appID, std::string parentWindow,
                       std::unordered_map<std::string, sdbus::Variant> opts);

      private:
        UP<sdbus::IObject> m_object;

        uint32_t           m_sourceTypes = 0;
        uint32_t           m_cursorModes = 0;

        struct SObjectPathHasher {
            std::size_t operator()(const sdbus::ObjectPath& p) const {
                return std::hash<std::string>{}(static_cast<const std::string&>(p));
            }
        };
        std::unordered_map<sdbus::ObjectPath, UP<SScreenCastSession>, SObjectPathHasher> m_sessions;

        //
        const sdbus::InterfaceName INTERFACE_NAME = sdbus::InterfaceName{"org.freedesktop.impl.portal.ScreenCast"};
    };
}
