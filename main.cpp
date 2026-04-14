#include "qp_config.hpp"
#include "qpcpp/include/qpcpp.hpp"
#include "signals.h"
#include "ControlRemot/ControlRemot.h"
#include "ControlHorari/ControlHorari.h"
#include "ControlHorari/ControlHorariState.h"
#include "ControlHorari/json_horari.h"
#include "Rellotge/Rellotge.h"
#include "HttpServer/HttpServer.h"
#include <cstdio>
#include <cstdlib>

// ── Instàncies globals ────────────────────────────────────────────────────────

static ControlRemot   s_controlRemot;
static ControlHorari  s_controlHorari;
static Rellotge       s_rellotge;

// ── Cues d'events ─────────────────────────────────────────────────────────────

static QP::QEvtPtr s_controlRemotQSto[64];
static QP::QEvtPtr s_controlHorariQSto[32];
static QP::QEvtPtr s_rellotgeQSto[16];

// ── Pool d'events dinàmics ────────────────────────────────────────────────────
// Un sol pool dimensionat per al tipus més gran (OutputStateEvt).

static QF_MPOOL_EL(OutputStateEvt) s_poolSto[8];

// ── Llista de subscripcions publish-subscribe ─────────────────────────────────

static QP::QSubscrList s_subscrSto[MAX_SIG];

// ── main ──────────────────────────────────────────────────────────────────────

int main() {
    std::printf("ControlSortides arrencant...\n");

    QP::QF::init();

    // Taula publish-subscribe
    QP::QActive::psInit(s_subscrSto, Q_DIM(s_subscrSto));

    // Pool per als events dinàmics (OutputCmdEvt, OutputModeEvt, OutputStateEvt…)
    QP::QF::poolInit(s_poolSto, sizeof(s_poolSto), sizeof(s_poolSto[0]));

    s_controlRemot.start(2U,
        s_controlRemotQSto, Q_DIM(s_controlRemotQSto),
        nullptr, 0U);

    ch_state.horariJson.assign(JSON_HORARI, sizeof(JSON_HORARI) - 1);
    s_controlHorari.loadJson(JSON_HORARI, sizeof(JSON_HORARI) - 1);
    s_controlHorari.start(1U,
        s_controlHorariQSto, Q_DIM(s_controlHorariQSto),
        nullptr, 0U);

    s_rellotge.start(3U,
        s_rellotgeQSto, Q_DIM(s_rellotgeQSto),
        nullptr, 0U);

    // Servidor HTTP en el seu thread (Mongoose)
    HttpServer::start(8080U, &s_controlRemot);
    std::printf("HTTP escoltant al port 8080\n");

    // Cedeix el control al kernel QV; torna quan es crida QF::stop()
    return QP::QF::run();
}

// ── Callbacks obligatoris del port QF ─────────────────────────────────────────

namespace QP {

// Cridat per QF::run() just abans d'entrar al bucle d'events.
void QF::onStartup() {
    // 100 ticks/s → 1 tick = 10 ms. Prioritat del thread ticker = 30.
    QF::setTickRate(100U, 30);
}

// Cridat en sortir del bucle d'events (Ctrl-C o QF::stop()).
void QF::onCleanup() {
    HttpServer::stop();
    std::printf("ControlSortides aturat.\n");
}

// Cridat pel thread ticker a cada tick del sistema.
// Ha de cridar QTimeEvt::TICK_X per a cada tick rate usat.
void QF::onClockTick() {
    QP::QTimeEvt::TICK_X(0U, nullptr);
}

} // namespace QP

// ── Callback d'error QP (Q_onError) ──────────────────────────────────────────
// Cridat per les assertions internes de QP quan detecten un estat invàlid.

Q_NORETURN Q_onError(char const * const module, int_t const id) {
    std::fprintf(stderr, "Q_onError: %s:%d\n", module, id);
    std::abort();
}
