#include "PipewireManager.hpp"

// TODO: impl this

using namespace Pipewire;

inline UP<CPipewireManager> g_pPipewireManager;
inline WP<CPipewireManager> Pipewire::mgr() {
    if (!g_pPipewireManager)
        g_pPipewireManager = makeUnique<CPipewireManager>();
    return g_pPipewireManager;
}

CPipewireManager::CPipewireManager() {
    ;
}

bool CPipewireManager::init() {
    ;
}

void CPipewireManager::processEvents() {
    while (pw_loop_iterate(m_loop, 0) != 0) {
        ;
    }
}
