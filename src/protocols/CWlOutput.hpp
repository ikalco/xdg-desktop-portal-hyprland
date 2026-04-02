#pragma once

#include "wayland.hpp"

#include "../core/WaylandManager.hpp"

namespace Wayland {
    class CWlOutput {
      public:
        CWlOutput(wl_registry* registry, uint32_t name, uint32_t version);

      private:
        SP<CCWlOutput> m_resource;

        // gemoetry
        uint32_t x, y;
        uint32_t physical_w, physical_h;
    };
};
