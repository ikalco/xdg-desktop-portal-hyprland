#pragma once

#include "../includes.hpp"
#include "wayland.hpp"

#include "PortalManager.hpp"

class CCExtOutputImageCaptureSourceManagerV1;
class CCExtForeignToplevelImageCaptureSourceManagerV1;

namespace Wayland {
    class CScreencopyProtocol;
    class CImageCopyCaptureProtocol;
    class CToplevelExportProtocol;
    class CWlrForeignToplevelProtocol;
    class CExtForeignToplevelProtocol;
    class CGlobalShortcutsProtocol;
    class CShmProtocol;
    class CLinuxDmabufProtocol;
    class CWlOutput;

    struct SSupportedProtos {
        bool wl_output : 1 = false;

        bool linux_dmabuf : 1 = false;
        bool wl_shm : 1       = false;

        bool screencopy : 1 = false;

        bool wlr_foreign_toplevel : 1 = false;
        bool toplevel_mapping : 1     = false;
        bool toplevel_export : 1      = false;

        bool ext_foreign_toplevel : 1          = false;
        bool output_image_capture_source : 1   = false;
        bool toplevel_image_capture_source : 1 = false;
        bool image_copy_capture : 1            = false;

        bool global_shortcuts : 1 = false;
    };

    class CWaylandManager {
      public:
        static inline WP<CWaylandManager> mgr() {
            return Portal::mgr()->m_waylandManager;
        }

        CWaylandManager();

        bool init();

        void roundTrip();
        int  displayFD();
        void processEvents();

      private:
        wl_display*      m_display;
        SP<CCWlRegistry> m_registry;

        struct {
            UP<CLinuxDmabufProtocol>        linuxDmabuf;
            UP<CShmProtocol>                wlShm;

            UP<CScreencopyProtocol>         screencopy;

            UP<CToplevelExportProtocol>     toplevelExport;
            UP<CWlrForeignToplevelProtocol> wlrForeignToplevel;
            UP<CExtForeignToplevelProtocol> extForeignToplevel;

            std::vector<UP<CWlOutput>>      outputs;
            struct {
                UP<CCExtOutputImageCaptureSourceManagerV1>          output;
                UP<CCExtForeignToplevelImageCaptureSourceManagerV1> toplevel;
            } imageCaptureSource;
            UP<CImageCopyCaptureProtocol> imageCopyCapture;

            UP<CGlobalShortcutsProtocol>  globalShortcuts;
        } m_protos;

        SSupportedProtos m_supportedProtos;

        // registry funcs
        void onGlobal(uint32_t name, const char* interface, uint32_t version);
        void onGlobalRemoved(uint32_t name);
    };

    inline WP<CWaylandManager> mgr() {
        return CWaylandManager::mgr();
    }
};
