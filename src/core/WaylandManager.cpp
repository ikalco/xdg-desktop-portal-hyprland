#include "WaylandManager.hpp"
#include "PipewireManager.hpp"
#include "../helpers/Log.hpp"

#include "wayland.hpp"
#include "hyprland-global-shortcuts-v1.hpp"
#include "linux-dmabuf-v1.hpp"
#include "wlr-screencopy-unstable-v1.hpp"
#include "hyprland-toplevel-export-v1.hpp"
#include "hyprland-toplevel-mapping-v1.hpp"
#include "wlr-foreign-toplevel-management-unstable-v1.hpp"
#include "ext-foreign-toplevel-list-v1.hpp"
#include "ext-image-capture-source-v1.hpp"
#include "ext-image-copy-capture-v1.hpp"

using namespace Wayland;

inline UP<CWaylandManager> g_pWaylandManager;
inline WP<CWaylandManager> Wayland::mgr() {
    if (!g_pWaylandManager)
        g_pWaylandManager = makeUnique<CWaylandManager>();
    return g_pWaylandManager;
}

CWaylandManager::CWaylandManager() {
    ;
}

bool CWaylandManager::init() {
    m_display = wl_display_connect(nullptr);

    if (!m_display) {
        Debug::log(CRIT, "[wayland] Couldn't connect to the compositor");
        return false;
    }

    m_registry = makeShared<CCWlRegistry>((wl_proxy*)wl_display_get_registry(m_display));
    m_registry->setGlobal([this](CCWlRegistry* r, uint32_t name, const char* iface, uint32_t ver) { onGlobal(name, iface, ver); });
    m_registry->setGlobalRemove([this](CCWlRegistry* r, uint32_t name) { onGlobalRemoved(name); });

    Debug::log(LOG, "[wayland] Gathering supported wayland protocols");

    wl_display_roundtrip(m_display);

    return true;
}

void CWaylandManager::readEvents() {
    wl_display_flush(m_display);
    if (wl_display_prepare_read(m_display) == 0) {
        wl_display_read_events(m_display);
        wl_display_dispatch_pending(m_display);
    } else {
        wl_display_dispatch(m_display);
    }
}

void CWaylandManager::processEvents() {
    // get any immediate events out of the way
    int ret = 0;
    do {
        ret = wl_display_dispatch_pending(m_display);
        wl_display_flush(m_display);
    } while (ret > 0);
}

void CWaylandManager::onGlobal(uint32_t name, const char* interface_, uint32_t version) {
    const std::string interface = interface_;

    Debug::log(LOG, " | Got interface: {} (ver {})", interface, version);

    // only bind screensharing protos if we have pipewire
    if (Pipewire::mgr()) {
        if (interface == zwlr_screencopy_manager_v1_interface.name) {
            m_supportedProtos.screencopy = true;
            m_protos.screencopy          = makeUnique<CScreencopyProtocol>(m_registry->resource(), name, version);
        } else if (interface == zwlr_foreign_toplevel_manager_v1_interface.name) {
            m_supportedProtos.wlr_foreign_toplevel = true;
            m_protos.wlrForeignToplevel            = makeUnique<CWlrForeignToplevelProtocol>(m_registry->resource(), name, version);
        } else if (interface == hyprland_toplevel_mapping_manager_v1_interface.name) {
            m_supportedProtos.toplevel_mapping = true;
            m_protos.wlrForeignToplevel        = makeUnique<CToplevelMappingProtocol>(m_registry->resource(), name, version);
        } else if (interface == hyprland_toplevel_export_manager_v1_interface.name) {
            m_supportedProtos.toplevel_export = true;
            m_protos.toplevelExport           = makeUnique<CToplevelExportProtocol>(m_registry->resource(), name, version);
        } else if (interface == ext_foreign_toplevel_list_v1_interface.name) {
            m_supportedProtos.ext_foreign_toplevel = true;
            m_protos.extForeignToplevel            = makeUnique<CExtForeignToplevelProtocol>(m_registry->resource(), name, version);
        } else if (interface == ext_output_image_capture_source_manager_v1_interface.name) {
            m_supportedProtos.output_image_capture_source = true;
            m_protos.imageCaptureSource.output            = makeUnique<CCExtOutputImageCaptureSourceManagerV1>(
                (wl_proxy*)wl_registry_bind((wl_registry*)m_registry->resource(), name, &ext_output_image_capture_source_manager_v1_interface, version));
        } else if (interface == ext_foreign_toplevel_image_capture_source_manager_v1_interface.name) {
            m_supportedProtos.toplevel_image_capture_source = true;
            m_protos.imageCaptureSource.toplevel            = makeUnique<CCExtForeignToplevelImageCaptureSourceManagerV1>(
                (wl_proxy*)wl_registry_bind((wl_registry*)m_registry->resource(), name, &ext_foreign_toplevel_image_capture_source_manager_v1_interface, version));
        } else if (interface == ext_image_copy_capture_manager_v1_interface.name) {
            m_supportedProtos.image_copy_capture = true;
            m_protos.imageCopyCapture            = makeUnique<CImageCopyCaptureProtocol>(m_registry->resource(), name, version);
        }
    }

    if (interface == hyprland_global_shortcuts_manager_v1_interface.name) {
        m_supportedProtos.global_shortcuts = true;
        m_protos.globalShortcuts           = makeUnique<CGlobalShortcutsProtocol>(m_registry->resource(), name, version);
    }

    if (interface == wl_shm_interface.name) {
        m_supportedProtos.wl_shm = true;
        m_protos.wlShm           = makeUnique<CShmProtocol>(m_registry->resource(), name, version);
    }

    if (interface == zwp_linux_dmabuf_v1_interface.name) {
        m_supportedProtos.linux_dmabuf = true;
        m_protos.linuxDmabuf           = makeUnique<CLinuxDmabufProtocol>(m_registry->resource(), name, version);
    }

    if (interface == wl_output_interface.name) {
        m_supportedProtos.wl_output = true;
        m_protos.outputs.emplace_back(makeUnique<CWlOutput>(m_registry->resource(), name, version));
    }
}

void CWaylandManager::onGlobalRemoved(uint32_t name) {
    std::erase_if(m_protos.outputs, [&](const auto& other) { return other->id == name; });
}
