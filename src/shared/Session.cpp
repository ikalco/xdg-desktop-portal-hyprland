#include "Session.hpp"
#include "../core/PortalManager.hpp"
#include "../helpers/Log.hpp"

static int onCloseRequest(SDBusRequest* req) {
    Debug::log(TRACE, "[internal] Close Request {}", (void*)req);

    if (!req)
        return 0;

    req->onDestroy();
    req->object.release();

    return 0;
}

static int onCloseSession(SDBusSession* sess) {
    Debug::log(TRACE, "[internal] Close Session {}", (void*)sess);

    if (!sess)
        return 0;

    sess->onDestroy();
    sess->object.release();

    return 0;
}




