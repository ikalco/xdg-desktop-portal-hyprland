#include <sdbus-c++/sdbus-c++.h>

#include "helpers/Log.hpp"
#include "core/WaylandManager.hpp"
#include "core/PipewireManager.hpp"
#include "core/DBusManager.hpp"
#include "core/EventLoopManager.hpp"

void printHelp() {
    std::cout << R"#(┃ xdg-desktop-portal-hyprland
┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
┃ -v (--verbose)    → enable trace logging
┃ -q (--quiet)      → disable logging
┃ -h (--help)       → print this menu
┃ -V (--version)    → print xdph's version
)#";
}

int main(int argc, char** argv, char** envp) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--verbose" || arg == "-v")
            Debug::verbose = true;

        else if (arg == "--quiet" || arg == "-q")
            Debug::quiet = true;

        else if (arg == "--help" || arg == "-h") {
            printHelp();
            return 0;
        } else if (arg == "--version" || arg == "-V") {
            std::cout << "xdg-desktop-portal-hyprland v" << XDPH_VERSION << "\n";
            return 0;
        } else {
            printHelp();
            return 1;
        }
    }

    Debug::log(LOG, "Initializing Wayland Manager");
    if (!Wayland::mgr() || !Wayland::mgr()->init()) {
        Debug::log(CRIT, "Failed Initializing Wayland Manager!!");
        return 1;
    }

    Debug::log(LOG, "Initializing Pipewire Manager");
    if (!Pipewire::mgr() || !Pipewire::mgr()->init()) {
        Debug::log(CRIT, "Failed Initializing Pipewire Manager!!");
        return 1;
    }

    Debug::log(LOG, "Initializing DBus Manager");
    if (!DBus::mgr() || !DBus::mgr()->init(Wayland::mgr()->protos())) {
        Debug::log(CRIT, "Failed Initializing DBus Manager!!");
        return 1;
    }

    if (!EventLoop::mgr()) {
        Debug::log(CRIT, "Failed Creating Event Loop!!");
        return 1;
    }

    Debug::log(LOG, "Starting the Event Loop");
    EventLoop::mgr()->start();
    Debug::log(ERR, "[core] Terminated");

    EventLoop::mgr().reset(); // jthreads are auto joined on destroy
    DBus::mgr().reset();
    Pipewire::mgr().reset();
    Wayland::mgr().reset();

    return 0;
}
