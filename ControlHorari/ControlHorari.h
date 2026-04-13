#pragma once
#include "../qpcpp/include/qpcpp.hpp"
#include "../signals.h"
#include <array>
#include <vector>
#include <cstdint>
#include <cstddef>

// ── ControlHorari ─────────────────────────────────────────────────────────────
// Active Object que cada minut comprova el calendari i publica les maniobres
// programades (OutputCmdEvt amb CTRL_OUTPUT_CMD_SIG) perquè ControlRemot les
// apliqui.
//
// El calendari es carrega amb loadJson() des d'un JSON amb aquest format:
//   { "dilluns":[{"id":10,"act":"on","time":"07:30"}, ...], "dimarts":[...], ... }

class ControlHorari : public QP::QActive {
public:
    explicit ControlHorari() noexcept;

    // Carrega el calendari des d'un buffer JSON. Es pot tornar a cridar per
    // recarregar en calent. Thread-safe només des del thread QV.
    void loadJson(const char* buf, std::size_t len);

private:
    struct Maniobra {
        int     id;
        bool    on;      // true = activate, false = deactivate
        uint8_t hour;    // 0-23
        uint8_t minute;  // 0-59
    };

    QP::QTimeEvt                         m_tick;
    std::array<std::vector<Maniobra>, 7> m_schedule; // 0=dilluns..6=diumenge

    // Rellotge simulat per a proves: cada tick avança un minut
    int m_wday   = 0;
    int m_hour   = 0;
    int m_minute = 0;

    Q_STATE_DECL(initial);
    Q_STATE_DECL(operating);
};
