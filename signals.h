#pragma once
#include "qpcpp/include/qpcpp.hpp"
#include <unordered_map>

// ── Signals ───────────────────────────────────────────────────────────────────

enum Signals : QP::QSignal {
    IO_STATE_CHANGED_SIG = QP::Q_USER_SIG, // publicat per IOStateMonitor
    IO_STATE_HTTP_SIG,                      // postat per HttpServer (thread Mongoose)
    CTRL_OUTPUT_CMD_SIG,                    // intern: handleJson → ControlRemot
    CTRL_OUTPUT_MODE_SIG,                   // intern: handleJson → ControlRemot
    CTRL_OUTPUT_RETURN_AUTO_SIG,            // intern: handleJson → ControlRemot
    CTRL_OUTPUT_DELETE_SIG,                 // postat per HttpServer (thread Mongoose)
    OUTPUT_RESULT_SIG,                      // publicat per ControlRemot
    HORARI_TICK_SIG,                        // intern: QTimeEvt de ControlHorari
    MAX_SIG
};

// ── Events ────────────────────────────────────────────────────────────────────

// Estat físic de les sortides (de IOStateMonitor).
// Semàntica estàtica (poolNum_=0): unordered_map no és compatible amb pools QP.
// Segur en QV (cooperatiu, un sol fil QP).
struct IOStateEvt : public QP::QEvt {
    std::unordered_map<int, bool> outputs;
    explicit IOStateEvt() noexcept : QP::QEvt{IO_STATE_CHANGED_SIG} {}
};

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

// Estat físic de les sortides injectat via HTTP (POST /io_state).
// Pool event amb arrays de mida fixa: compatible amb QP memory pools.
struct IoStateHttpEvt : public QP::QEvt {
    static constexpr int MAX_OUTPUTS = 32;
    struct Entry { int id; bool state; };
    Entry outputs[MAX_OUTPUTS];
    int   n_outputs = 0;
};

// Estat resultant de les sortides (publicat per ControlRemot).
// Semàntica estàtica: mateixa raó que IOStateEvt.
struct OutputResultEvt : public QP::QEvt {
    std::unordered_map<int, bool> outputs;
    explicit OutputResultEvt() noexcept : QP::QEvt{OUTPUT_RESULT_SIG} {}
};
