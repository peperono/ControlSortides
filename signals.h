#pragma once
#include "qpcpp/include/qpcpp.hpp"
#include <unordered_map>

// ── Signals ───────────────────────────────────────────────────────────────────

enum Signals : QP::QSignal {
    OUTPUT_STATE_SIG = QP::Q_USER_SIG,       // estat físic de sortides
    CTRL_OUTPUT_CMD_SIG,                    // intern: handleJson → ControlRemot
    CTRL_OUTPUT_MODE_SIG,                   // intern: handleJson → ControlRemot
    CTRL_OUTPUT_RETURN_AUTO_SIG,            // intern: handleJson → ControlRemot
    CTRL_OUTPUT_DELETE_SIG,                 // postat per HttpServer (thread Mongoose)
    OUTPUT_RESULT_SIG,                      // publicat per ControlRemot
    RELLOTGE_TICK_INTERNAL_SIG,             // intern: QTimeEvt de Rellotge
    RELLOTGE_TICK_SIG,                      // publicat per Rellotge cada minut simulat
    MAX_SIG
};

// Tick del rellotge simulat. Publicat per Rellotge.
struct RellotgeTickEvt : public QP::QEvt {
    int hour;    // 0-23
    int minute;  // 0-59
    int wday;    // 0=dilluns..6=diumenge
};

// ── Events ────────────────────────────────────────────────────────────────────

// Comanda activar/desactivar una sortida concreta.
struct OutputCmdEvt : public QP::QEvt {
    int  output_id;
    bool activate;
};

// Canvi de mode auto/remot per a una sortida concreta.
struct OutputModeEvt : public QP::QEvt {
    int  output_id;
    bool remote;
};

// Retorn a mode automàtic. output_id==-1 → totes les sortides.
struct OutputReturnAutoEvt : public QP::QEvt {
    int output_id;
};

// Elimina una sortida del registre de ControlRemot.
struct OutputDeleteEvt : public QP::QEvt {
    int output_id;
};

// Estat físic de les sortides. Pool event amb arrays de mida fixa:
// compatible amb QP memory pools. Publicat per Rellotge→ControlHorari i
// injectat via HttpServer (POST /io_state).
struct OutputStateEvt : public QP::QEvt {
    static constexpr int MAX_OUTPUTS = 32;
    struct Entry { int id; bool state; };
    Entry outputs[MAX_OUTPUTS];
    int   n_outputs = 0;
};

// Estat resultant de les sortides (publicat per ControlRemot).
struct OutputResultEvt : public QP::QEvt {
    std::unordered_map<int, bool> outputs;
    explicit OutputResultEvt() noexcept : QP::QEvt{OUTPUT_RESULT_SIG} {}
};
