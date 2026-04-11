#pragma once
#include "../qpcpp/include/qpcpp.hpp"
#include "../signals.h"
#include <unordered_map>

// ── ControlSortides ───────────────────────────────────────────────────────────
// Active Object que gestiona l'estat resultant de cada sortida física.
//
// Entrades:
//   - IO_STATE_CHANGED_SIG      → estat físic actual de les sortides
//   - CTRL_OUTPUT_CMD_SIG       → [1] comanda activar/desactivar (de ControlRemot)
//   - CTRL_OUTPUT_MODE_SIG      → [2] canvi de mode auto/remot   (de ControlRemot)
//   - CTRL_OUTPUT_RETURN_AUTO_SIG → [3] retorn a automàtic       (de ControlRemot)
//
// Sortida:
//   - OUTPUT_RESULT_SIG (publicat) → estat sortida resultant
//
// Comportament per sortida:
//   Mode automàtic : estat resultant = estat físic (transparent)
//   Mode remot     : estat resultant = última comanda rebuda (ignora físic)
//   Auto + comanda : aplica la comanda puntualment i continua en automàtic
class ControlSortides : public QP::QActive {
public:
    explicit ControlSortides() noexcept;

private:
    struct OutputEntry {
        enum class Mode : uint8_t { AUTO, REMOTE } mode = Mode::AUTO;
        bool physical  = false; // últim estat físic conegut
        bool commanded = false; // últim valor ordenat (usat en mode remot)
    };

    std::unordered_map<int, OutputEntry> m_outputs;
    OutputResultEvt                      m_resultEvt; // static event (poolNum_=0)

    void publishResult();

    Q_STATE_DECL(initial);
    Q_STATE_DECL(operating);
};
