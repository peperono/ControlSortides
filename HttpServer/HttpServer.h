#pragma once
#include "../qpcpp/include/qpcpp.hpp"
#include <cstdint>

// ── HttpServer ────────────────────────────────────────────────────────────────
// Servidor HTTP/WebSocket en un thread Mongoose dedicat.
//
//   POST /control    → [{"id":<int>,"action":"<activate|deactivate|set_remote|
//                        set_auto|return_auto>"},...]
//                      Crida controlRemot->handleJson().
//
//   GET  /           → Pàgina HTML de monitorització
//   WS   /ws         → Push JSON {"outputs":{...}} quan push_pending és true

namespace HttpServer {
    void start(uint16_t port, QP::QActive* controlRemot);
    void stop();
}
