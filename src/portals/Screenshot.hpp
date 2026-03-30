#pragma once

#include <sdbus-c++/sdbus-c++.h>

#include "../includes.hpp"
#include "../core/DBusManager.hpp"
#include "../core/WaylandManager.hpp"

namespace DBus {
    class CScreenshotPortal {
      public:
        CScreenshotPortal();

        bool   init(const Wayland::SSupportedProtos& protos);

        dbUasv onScreenshot(sdbus::ObjectPath requestHandle, std::string appID, std::string parentWindow, std::unordered_map<std::string, sdbus::Variant> options);
        dbUasv onPickColor(sdbus::ObjectPath requestHandle, std::string appID, std::string parentWindow, std::unordered_map<std::string, sdbus::Variant> options);

      private:
        UP<sdbus::IObject>         m_object;
        std::string                m_lastScreenshotFile = "";

        const sdbus::InterfaceName INTERFACE_NAME = sdbus::InterfaceName{"org.freedesktop.impl.portal.Screenshot"};
    };
};
