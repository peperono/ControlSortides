#pragma once
#include "../qpcpp/include/qpcpp.hpp"
#include "../signals.h"
#include <array>
#include <vector>
#include <cstdint>
#include <cstddef>

// ── ControlHorari ─────────────────────────────────────────────────────────────
// Active Object que manté un calendari setmanal de maniobres i, en rebre
// RELLOTGE_TICK_SIG, publica les maniobres que coincideixen amb l'hora actual
// (IoStateEvt amb IO_STATE_SIG) perquè ControlRemot les apliqui.
//
// Format del JSON:
//   { "dilluns":[{"id":10,"act":"on","time":"07:30"}, ...], "dimarts":[...], ... }

class ControlHorari : public QP::QActive {
public:
    explicit ControlHorari() noexcept;

    // Carrega el calendari des d'un buffer JSON. Thread-safe només des del
    // thread QV.
    void loadJson(const char* buf, std::size_t len);

private:
    struct Maniobra {
        int     id;
        bool    on;      // true = activate, false = deactivate
        uint8_t hour;    // 0-23
        uint8_t minute;  // 0-59
    };

    std::array<std::vector<Maniobra>, 7> m_schedule; // 0=dilluns..6=diumenge

    Q_STATE_DECL(initial);
    Q_STATE_DECL(operating);
};
