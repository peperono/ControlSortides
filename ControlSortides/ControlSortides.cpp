#include "ControlSortides.h"
#include "../SharedState.h"
#include <mutex>

// ── Constructor ───────────────────────────────────────────────────────────────

ControlSortides::ControlSortides() noexcept
    : QP::QActive{Q_STATE_CAST(&ControlSortides::initial)},
      m_resultEvt{}
{}

// ── State: initial ────────────────────────────────────────────────────────────

Q_STATE_DEF(ControlSortides, initial) {
    Q_UNUSED_PAR(e);
    subscribe(IO_STATE_CHANGED_SIG);
    // CTRL_OUTPUT_*_SIG arriben per POST directe des de ControlRemot,
    // no cal subscribe.
    return tran(&ControlSortides::operating);
}

// ── State: operating ──────────────────────────────────────────────────────────

Q_STATE_DEF(ControlSortides, operating) {
    QP::QState status;

    switch (e->sig) {

        case Q_ENTRY_SIG: {
            status = Q_HANDLED();
            break;
        }

        // Entrada: estat físic actual de les sortides
        case IO_STATE_CHANGED_SIG: {
            auto const* ev = Q_EVT_CAST(IOStateEvt);
            for (auto const& [id, state] : ev->outputs) {
                m_outputs[id].physical = state;
            }
            publishResult();
            status = Q_HANDLED();
            break;
        }

        // [1] Comanda activar/desactivar
        // Mode automàtic: aplica puntualment i continua en automàtic.
        // Mode remot:     aplica i persisteix fins a nova comanda o canvi de mode.
        case CTRL_OUTPUT_CMD_SIG: {
            auto const* ev        = Q_EVT_CAST(OutputCmdEvt);
            auto&       entry     = m_outputs[ev->output_id];
            entry.commanded       = ev->activate;
            publishResult();
            status = Q_HANDLED();
            break;
        }

        // [2] Canvi de mode auto/remot
        case CTRL_OUTPUT_MODE_SIG: {
            auto const* ev  = Q_EVT_CAST(OutputModeEvt);
            auto&       entry = m_outputs[ev->output_id];
            entry.mode = ev->remote ? OutputEntry::Mode::REMOTE
                                    : OutputEntry::Mode::AUTO;
            publishResult();
            status = Q_HANDLED();
            break;
        }

        // [3] Retorn a automàtic (-1 = totes les sortides)
        case CTRL_OUTPUT_RETURN_AUTO_SIG: {
            auto const* ev = Q_EVT_CAST(OutputReturnAutoEvt);
            if (ev->output_id == -1) {
                for (auto& [id, entry] : m_outputs)
                    entry.mode = OutputEntry::Mode::AUTO;
            } else {
                m_outputs[ev->output_id].mode = OutputEntry::Mode::AUTO;
            }
            publishResult();
            status = Q_HANDLED();
            break;
        }

        default: {
            status = super(&ControlSortides::top);
            break;
        }
    }
    return status;
}

// ── publishResult ─────────────────────────────────────────────────────────────
// Calcula l'estat resultant de cada sortida i el publica via OUTPUT_RESULT_SIG.
// En mode AUTO:  resultant = estat físic
// En mode REMOT: resultant = última comanda
// En AUTO + comanda puntual (CTRL_OUTPUT_CMD_SIG): la comanda acaba d'escriure
//   commanded, però el mode segueix AUTO → el pròxim IO_STATE_CHANGED_SIG
//   tornarà a reflectir l'estat físic (comportament transparent restaurat).

void ControlSortides::publishResult() {
    m_resultEvt.outputs.clear();
    for (auto const& [id, entry] : m_outputs) {
        bool result;
        if (entry.mode == OutputEntry::Mode::AUTO) {
            result = entry.physical;
        } else {
            result = entry.commanded;
        }
        m_resultEvt.outputs[id] = result;
    }

    // Actualitza SharedState perquè el WebSocket pugui fer push
    {
        std::lock_guard<std::mutex> lk(se.mtx);
        se.outputs = m_resultEvt.outputs;
    }
    se.push_pending.store(true);

    PUBLISH(&m_resultEvt, this);
}
