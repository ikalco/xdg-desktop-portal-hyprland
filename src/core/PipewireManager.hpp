#pragma once

#include "../includes.hpp"

// TODO: impl this

namespace Pipewire {

    class CPipewireManager {
      public:
        CPipewireManager();

        bool init();
        void processEvents();

        int  getFD();

      private:
        //
    };

    WP<CPipewireManager> mgr();
};
