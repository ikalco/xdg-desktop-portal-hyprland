#include "ScreenCast.hpp"
#include "../core/WaylandManager.hpp"
#include "../helpers/Log.hpp"

#include <hyprutils/os/Process.hpp>
#include <hyprlang.hpp>

using namespace DBus;
using namespace Hyprutils::OS;

CScreenCastPortal::CScreenCastPortal() {
    ;
}

bool CScreenCastPortal::init(const Wayland::SSupportedProtos& protos) {
    if (!DBus::mgr()) {
        Debug::log(ERR, "[screencast] failed, dbus is not initialized");
        return false;
    }

    if (protos.wlr_screencopy) {
        m_sourceTypes |= eSourceTypes::MONITOR;
        m_sourceTypes |= eSourceTypes::VIRTUAL;
    }

    if (protos.toplevel_export && (protos.hl_toplevel_mapping || protos.wlr_foreign_toplevel))
        m_sourceTypes |= eSourceTypes::WINDOW;

    if (protos.image_capture_source && protos.image_copy_capture) {
        m_sourceTypes |= eSourceTypes::MONITOR;
        if (protos.foreign_toplevel)
            m_sourceTypes |= eSourceTypes::WINDOW;

        m_cursorModes |= eCursorModes::METADATA;
    }

    if (m_sourceTypes == 0) {
        Debug::log(ERR, "[screencast] failed, no supported wayland protocols");
        return false;
    }

    // all protos support these
    m_cursorModes |= eCursorModes::HIDDEN | eCursorModes::EMBEDDED;

    m_object = CUniquePointer<sdbus::IObject>(sdbus::createObject(DBus::mgr()->connection(), DBus::OBJECT_PATH).release());

    m_object
        ->addVTable(sdbus::registerMethod("CreateSession")
                        .implementedAs([this](sdbus::ObjectPath o1, sdbus::ObjectPath o2, std::string s1, std::unordered_map<std::string, sdbus::Variant> m1) {
                            return onCreateSession(o1, o2, s1, m1);
                        }),
                    sdbus::registerMethod("SelectSources")
                        .implementedAs([this](sdbus::ObjectPath o1, sdbus::ObjectPath o2, std::string s1, std::unordered_map<std::string, sdbus::Variant> m1) {
                            return onSelectSources(o1, o2, s1, m1);
                        }),
                    sdbus::registerMethod("Start").implementedAs([this](sdbus::ObjectPath o1, sdbus::ObjectPath o2, std::string s1, std::string s2,
                                                                        std::unordered_map<std::string, sdbus::Variant> m1) { return onStart(o1, o2, s1, s2, m1); }),
                    sdbus::registerProperty("AvailableSourceTypes").withGetter([this]() { return m_sourceTypes; }),
                    sdbus::registerProperty("AvailableCursorModes").withGetter([this]() { return m_cursorModes; }),
                    sdbus::registerProperty("version").withGetter([]() { return uint32_t{5}; }))
        .forInterface(INTERFACE_NAME);

    return true;
}

dbUasv CScreenCastPortal::onCreateSession(sdbus::ObjectPath requestHandle, sdbus::ObjectPath sessionHandle, std::string appID,
                                          std::unordered_map<std::string, sdbus::Variant> opts) {
    Debug::log(LOG, "[screencopy] New session:");
    Debug::log(LOG, "[screencopy]  | {}", requestHandle.c_str());
    Debug::log(LOG, "[screencopy]  | {}", sessionHandle.c_str());
    Debug::log(LOG, "[screencopy]  | appid: {}", appID);

    const auto& pair    = m_sessions.emplace(sessionHandle, makeUnique<SScreenCastSession>());
    auto&       session = pair.first->second;
    session->m_self     = session;

    session->m_session = DBus::mgr()->createSession(sessionHandle);
    session->m_session->onDestroy.listen([&session] {
        // if (session->sharingData.active) {
        //     m_pPipewire->destroyStream(session.get());
        //     Debug::log(LOG, "[screencopy] Stream destroyed");
        // }
        //
        // // deactivate toplevel so it doesn't listen and waste battery
        // g_pPortalManager->m_sHelpers.toplevel->deactivate();

        Debug::log(LOG, "[screencast] Session destroyed {}");
        session->m_session.reset();
    });

    session->m_request = DBus::mgr()->createRequest(requestHandle);
    session->m_request->onDestroy.listen([&session] { session->m_request.reset(); });

    // TODO: The session id. A string representing the created screen cast session.
    return {0, {{"session_id", sdbus::Variant{""}}}};
}

dbUasv CScreenCastPortal::onSelectSources(sdbus::ObjectPath requestHandle, sdbus::ObjectPath sessionHandle, std::string appID,
                                          std::unordered_map<std::string, sdbus::Variant> options) {
    Debug::log(LOG, "[screencast] SelectSources:");
    Debug::log(LOG, "[screencast]  | {}", requestHandle.c_str());
    Debug::log(LOG, "[screencast]  | {}", sessionHandle.c_str());
    Debug::log(LOG, "[screencast]  | appid: {}", appID);

    auto& session = m_sessions.at(sessionHandle);

    if (!session) {
        Debug::log(ERR, "[screencast] SelectSources: no session found??");
        throw sdbus::Error{sdbus::Error::Name{"NOSESSION"}, "No session found"};
        return {1, {}};
    }

    for (auto& [key, val] : options) {
        if (key == "types") {
            session->m_sourceTypes = val.get<uint32_t>();
        } else if (key == "multiple") {
            // we basically ignore this since our picker can't select multiple
            session->m_multiple = val.get<bool>();
        } else if (key == "cursor_mode") {
            session->m_cursorMode = val.get<eCursorModes>();
        } else if (key == "persist_mode") {
            session->m_persistMode = val.get<ePersistModes>();
        } else if (key == "restore_data") {
            // TODO
        } else {
            Debug::log(LOG, "[screencast] unused option {}", key);
        }
    }

    Debug::log(LOG, "[screencast] option types = {}", session->m_sourceTypes);
    Debug::log(LOG, "[screencast] option multiple = {}", session->m_multiple);
    Debug::log(LOG, "[screencast] option cursor_mode = {}", (uint32_t)session->m_cursorMode);
    Debug::log(LOG, "[screencast] option persist_mode = {}", (uint32_t)session->m_persistMode);
    // Debug::log(LOG, "[screencast] option restore_data = {}", (uint32_t)session->m_persistMode);

    // if none of the requested source types are supported, fail
    if ((m_sourceTypes & session->m_sourceTypes) == 0) {
        Debug::log(ERR, "[screencast] SelectSources: unsupported source type");
        return {1, {}};
    }

    return {0, {}};
}

dbUasv CScreenCastPortal::onStart(sdbus::ObjectPath requestHandle, sdbus::ObjectPath sessionHandle, std::string appID, std::string parentWindow,
                                  std::unordered_map<std::string, sdbus::Variant> opts) {
    typedef std::unordered_map<std::string, sdbus::Variant> dbSv;

    Debug::log(LOG, "[screencast] Start:");
    Debug::log(LOG, "[screencast]  | {}", requestHandle.c_str());
    Debug::log(LOG, "[screencast]  | {}", sessionHandle.c_str());
    Debug::log(LOG, "[screencast]  | appid: {}", appID);
    // note, this should be an xdg-foreign handle, but not guaranteed and hl doesn't support it.. so dont use it
    Debug::log(LOG, "[screencast]  | parent_window: {}", parentWindow);

    auto& session = m_sessions.at(sessionHandle);

    if (!session) {
        Debug::log(ERR, "[screencast] Start: no session found??");
        throw sdbus::Error{sdbus::Error::Name{"NOSESSION"}, "No session found"};
        return {1, {}};
    }

    const auto& selectionData = session->getSelectData();
    dbSv        options;

    if (selectionData.allowToken) {
        // give them a token :)
        options["restore_data"] = sdbus::Variant{session->getRestoreData()};
        options["persist_mode"] = sdbus::Variant{uint32_t{2}};

        Debug::log(LOG, "[screencopy] Sending restore token to {}", sessionHandle);
    }

    uint32_t nodeId = 0;
    dbSv     streamData;
    switch (selectionData.type) {
        case TYPE_OUTPUT:
            streamData["source_type"] = sdbus::Variant{(uint32_t)eSourceTypes::MONITOR};

            SP<SOutput> mon        = Wayland::mgr()->getOutputFromName(selectionData.output);
            streamData["position"] = sdbus::Variant{sdbus::Struct<int32_t, int32_t>{mon.x, mon.y}};
            streamData["size"]     = sdbus::Variant{sdbus::Struct<int32_t, int32_t>{mon.w, mon.h}};
            // nodeId = // something pipewire manager
            // TODO: start pipewire stream
            break;
        case TYPE_TOPLEVEL:
            streamData["source_type"] = sdbus::Variant{(uint32_t)eSourceTypes::WINDOW};

            // streamData["size"]     = sdbus::Variant{sdbus::Struct<int32_t, int32_t>{mon.w, mon.h}};
            // TODO: start pipewire stream
            break;
        case TYPE_WORKSPACE:
        case TYPE_REGION:
            streamData["source_type"] = sdbus::Variant{(uint32_t)eSourceTypes::VIRTUAL};
            // streamData["size"]     = sdbus::Variant{sdbus::Struct<int32_t, int32_t>{mon.w, mon.h}};
            // TODO: start pipewire stream
            break;
        case TYPE_INVALID:
        default:;
    }

    // we only support one stream so
    // 1 element array of struct { pipewire nodeID; dbSv streamData };
    options["streams"] = sdbus::Variant{std::vector<sdbus::Struct<uint32_t, dbSv>>{{nodeId, streamData}}};

    return {0, options};
}

const SSelectionData& SScreenCastSession::getSelectData() {
    if (selectionDataRestored && selectionData.type != eSelectionType::TYPE_INVALID)
        return selectionData;

    // reset
    selectionDataRestored = false;
    selectionData         = {};

    std::string RETVAL = promptSharePicker();

    if (RETVAL == "")
        return selectionData;

    // parse the share picker stdout
    const auto SELECTION = RETVAL.substr(RETVAL.find("[SELECTION]") + 11);

    Debug::log(LOG, "[sc] Selection: {}", SELECTION);

    const auto FLAGS = SELECTION.substr(0, SELECTION.find_first_of('/'));
    const auto SEL   = SELECTION.substr(SELECTION.find_first_of('/') + 1);

    for (auto& flag : FLAGS) {
        if (flag == 'r')
            selectionData.allowToken = true;
        else if (flag == 'c')
            selectionData.overlayCursor = true;
        else
            Debug::log(LOG, "[screencopy] unknown flag from share-picker: {}", flag);
    }

    if (SEL.find("screen:") == 0) {
        selectionData.type   = TYPE_OUTPUT;
        selectionData.output = SEL.substr(7);
        selectionData.output.pop_back();
    } else if (SEL.find("window:") == 0) {
        selectionData.type              = TYPE_TOPLEVEL;
        uint32_t handleLo               = std::stoull(SEL.substr(7));
        selectionData.toplevelHandleWlr = nullptr;

        // TODO: improve
        const auto HANDLE = Wayland::mgr()->handleFromHandleLower(handleLo);
        if (HANDLE) {
            selectionData.toplevelHandleWlr = HANDLE->handle;
            selectionData.windowClass       = HANDLE->windowClass;
        }
    } else if (SEL.find("region:") == 0) {
        std::string running  = SEL;
        running              = running.substr(7);
        selectionData.type   = TYPE_REGION;
        selectionData.output = running.substr(0, running.find_first_of('@'));
        running              = running.substr(running.find_first_of('@') + 1);

        selectionData.x = std::stoi(running.substr(0, running.find_first_of(',')));
        running         = running.substr(running.find_first_of(',') + 1);
        selectionData.y = std::stoi(running.substr(0, running.find_first_of(',')));
        running         = running.substr(running.find_first_of(',') + 1);
        selectionData.w = std::stoi(running.substr(0, running.find_first_of(',')));
        running         = running.substr(running.find_first_of(',') + 1);
        selectionData.h = std::stoi(running);
    }

    return selectionData;
}

std::string SScreenCastSession::promptSharePicker() {
    const char*              WAYLAND_DISPLAY             = getenv("WAYLAND_DISPLAY");
    const char*              XCURSOR_SIZE                = getenv("XCURSOR_SIZE");
    const char*              HYPRLAND_INSTANCE_SIGNATURE = getenv("HYPRLAND_INSTANCE_SIGNATURE");

    static auto* const*      PALLOWTOKENBYDEFAULT = (Hyprlang::INT* const*)Config::config()->getConfigValuePtr("screencopy:allow_token_by_default")->getDataStaticPtr();
    static auto* const*      PCUSTOMPICKER        = (Hyprlang::STRING* const)Config::config()->getConfigValuePtr("screencopy:custom_picker_binary")->getDataStaticPtr();

    std::vector<std::string> args;
    if (**PALLOWTOKENBYDEFAULT)
        args.emplace_back("--allow-token");

    uint32_t disableSources = ~m_sourceTypes;
    if (disableSources | eSourceTypes::MONITOR)
        args.emplace_back("--disable-screen");
    if (disableSources | eSourceTypes::WINDOW)
        args.emplace_back("--disable-window");
    if (disableSources | eSourceTypes::VIRTUAL)
        args.emplace_back("--disable-region");

    CProcess proc(std::string{*PCUSTOMPICKER}.empty() ? "hyprland-share-picker" : *PCUSTOMPICKER, args);
    proc.addEnv("WAYLAND_DISPLAY", WAYLAND_DISPLAY ? WAYLAND_DISPLAY : "");
    proc.addEnv("QT_QPA_PLATFORM", "wayland");
    proc.addEnv("XCURSOR_SIZE", XCURSOR_SIZE ? XCURSOR_SIZE : "24");
    proc.addEnv("HYPRLAND_INSTANCE_SIGNATURE", HYPRLAND_INSTANCE_SIGNATURE ? HYPRLAND_INSTANCE_SIGNATURE : "0");
    proc.addEnv("XDPH_WINDOW_SHARING_LIST",
                Wayland::mgr()->buildWindowList()); // buildWindowList will sanitize any shell stuff in case the picker (qt) does something funky? It shouldn't.

    if (!proc.runSync())
        return "";

    const auto RETVAL    = proc.stdOut();
    const auto RETVALERR = proc.stdErr();

    if (!RETVAL.contains("[SELECTION]")) {
        // failed
        constexpr const char* QPA_ERR = "qt.qpa.plugin: Could not find the Qt platform plugin";

        if (RETVAL.contains(QPA_ERR) || RETVALERR.contains(QPA_ERR)) {
            // prompt the user to install qt5-wayland and qt6-wayland
            addHyprlandNotification("3", 7000, "0", "[xdph] Could not open the picker: qt5-wayland or qt6-wayland doesn't seem to be installed.");
        }

        return "";
    }

    return RETVAL;
}

sdbus::Struct<std::string, uint32_t, sdbus::Variant> SScreenCastSession::getRestoreData() {
    std::unordered_map<std::string, sdbus::Variant> data;

    data["selectionData"] = sdbus::Variant{selectionData};
    data["timeIssued"]    = sdbus::Variant{uint64_t(time(nullptr))};

    // TODO: use random tokens and keep track
    data["token"] = sdbus::Variant{std::string("todo")};

    return sdbus::Struct<std::string, uint32_t, sdbus::Variant>{"hyprland", 4, sdbus::Variant{data}};
}
