#pragma once
#include "../qpcpp/include/qpcpp.hpp"
#include "../signals.h"

// ── Rellotge ──────────────────────────────────────────────────────────────────
// Active Object que manté un rellotge simulat (dia/hora/minut) i publica
// RELLOTGE_TICK_SIG cada minut simulat. Els subscriptors reben l'hora actual
// dins l'event (RellotgeTickEvt).

class Rellotge : public QP::QActive {
public:
    explicit Rellotge() noexcept;

private:
    QP::QTimeEvt m_tick;
    int          m_wday   = 0;
    int          m_hour   = 0;
    int          m_minute = 0;

    Q_STATE_DECL(initial);
    Q_STATE_DECL(operating);
};
