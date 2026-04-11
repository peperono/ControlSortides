#pragma once
#include "../qpcpp/include/qpcpp.hpp"
#include "../signals.h"
#include <cstddef>

// ── ControlRemot ──────────────────────────────────────────────────────────────
// Active Object que genera events de control de sortides a partir d'un JSON
// rebut des del thread Mongoose (HttpServer).
//
// Interfície externa (thread Mongoose):
//   handleJson(buf, len) — analitza el JSON i posta els events corresponents
//                          a ControlSortides de forma thread-safe (Q_NEW + POST).
//
// Format JSON esperat:
//   [{"id":<int>,"action":"<activate|deactivate|set_remote|set_auto|return_auto>"},...]
//
// Senyals generats cap a ControlSortides:
//   [1] CTRL_OUTPUT_CMD_SIG          → OutputCmdEvt
//   [2] CTRL_OUTPUT_MODE_SIG         → OutputModeEvt
//   [3] CTRL_OUTPUT_RETURN_AUTO_SIG  → OutputReturnAutoEvt
class ControlRemot : public QP::QActive {
public:
    explicit ControlRemot(QP::QActive* sortides) noexcept;

    // Cridat des del thread Mongoose. Thread-safe: utilitza Q_NEW i POST.
    void handleJson(const char* buf, std::size_t len);

private:
    QP::QActive* const m_sortides;

    Q_STATE_DECL(initial);
    Q_STATE_DECL(operating);
};
