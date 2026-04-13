#include "ControlRemot.h"
#include "../SharedState.h"
#include "../mongoose/mongoose.h"
#include <cstdio>
#include <mutex>

// ── Constructor ───────────────────────────────────────────────────────────────

ControlRemot::ControlRemot() noexcept
    : QP::QActive{Q_STATE_CAST(&ControlRemot::initial)},
      m_resultEvt{}
{}

// ── State: initial ────────────────────────────────────────────────────────────

Q_STATE_DEF(ControlRemot, initial) {
    Q_UNUSED_PAR(e);
    subscribe(IO_STATE_HTTP_SIG); // ControlHorari publica estat físic
    return tran(&ControlRemot::operating);
}

// ── State: operating ──────────────────────────────────────────────────────────

Q_STATE_DEF(ControlRemot, operating) {
    QP::QState status;

    switch (e->sig) {

        case Q_ENTRY_SIG: {
            status = Q_HANDLED();
            break;
        }

        // Entrada: estat físic injectat via HTTP (POST /io_state)
        case IO_STATE_HTTP_SIG: {
            auto const* ev = Q_EVT_CAST(IoStateHttpEvt);
            for (int i = 0; i < ev->n_outputs; ++i) {
                auto& out    = m_outputs[ev->outputs[i].id];
                out.physical = ev->outputs[i].state;
                if (out.mode == OutputEntry::Mode::AUTO) {
                    out.result = ev->outputs[i].state;
                }
            }
            publishResult();
            status = Q_HANDLED();
            break;
        }

        // Comanda activar/desactivar
        // Ambdós modes: aplica de seguida. En AUTO el mode no canvia; el pròxim
        // IO_STATE_CHANGED_SIG tornarà a reflectir l'estat físic.
        case CTRL_OUTPUT_CMD_SIG: {
            auto const* ev = Q_EVT_CAST(OutputCmdEvt);
            auto& out      = m_outputs[ev->output_id];
            out.commanded  = ev->activate;
            out.result     = ev->activate;
            publishResult();
            status = Q_HANDLED();
            break;
        }

        // Canvi de mode auto/remot
        case CTRL_OUTPUT_MODE_SIG: {
            auto const* ev = Q_EVT_CAST(OutputModeEvt);
            auto& out      = m_outputs[ev->output_id];
            out.mode = ev->remote ? OutputEntry::Mode::REMOTE
                                  : OutputEntry::Mode::AUTO;
            out.result = (out.mode == OutputEntry::Mode::REMOTE)
                       ? out.commanded
                       : out.physical;
            publishResult();
            status = Q_HANDLED();
            break;
        }

        // Elimina una sortida del registre.
        case CTRL_OUTPUT_DELETE_SIG: {
            auto const* ev = Q_EVT_CAST(OutputDeleteEvt);
            m_outputs.erase(ev->output_id);
            publishResult();
            status = Q_HANDLED();
            break;
        }

        // Retorn a automàtic. output_id==-1 → totes les sortides
        case CTRL_OUTPUT_RETURN_AUTO_SIG: {
            auto const* ev = Q_EVT_CAST(OutputReturnAutoEvt);
            if (ev->output_id == -1) {
                for (auto& [id, out] : m_outputs) {
                    out.mode   = OutputEntry::Mode::AUTO;
                    out.result = out.physical;
                }
            } else {
                auto& out  = m_outputs[ev->output_id];
                out.mode   = OutputEntry::Mode::AUTO;
                out.result = out.physical;
            }
            publishResult();
            status = Q_HANDLED();
            break;
        }

        default: {
            status = super(&ControlRemot::top);
            break;
        }
    }
    return status;
}

// ── publishResult ─────────────────────────────────────────────────────────────
// Publica l'estat resultant de totes les sortides via OUTPUT_RESULT_SIG.
// Actualitza SharedState perquè el WebSocket pugui fer push.

void ControlRemot::publishResult() {
    m_resultEvt.outputs.clear();
    for (auto const& [id, out] : m_outputs) {
        m_resultEvt.outputs[id] = out.result;
    }

    {
        std::lock_guard<std::mutex> lk(se.mtx);
        se.outputs = m_resultEvt.outputs;
        se.outputsFull.clear();
        for (auto const& [id, out] : m_outputs) {
            se.outputsFull[id] = {out.physical, out.commanded, out.result,
                                  out.mode == OutputEntry::Mode::REMOTE};
        }
    }
    se.push_pending.store(true);

    PUBLISH(&m_resultEvt, this);
}

// ── handleJson ────────────────────────────────────────────────────────────────
// Analitza un array JSON i posta els events de control a this.
//
// Format: [{"id":10,"action":"activate"},{"id":11,"action":"set_remote"},...]
//
// Usa el parser de Mongoose (mg_json_get / mg_json_get_long).
// Thread-safe: Q_NEW i QActive::POST són segurs des de qualsevol thread.

void ControlRemot::handleJson(const char* buf, std::size_t len) {
    struct mg_str json = mg_str_n(buf, len);

    for (int i = 0; ; ++i) {
        char path[32];

        // Obté l'element i[n] de l'array
        int elen = 0;
        std::snprintf(path, sizeof(path), "$[%d]", i);
        int eoff = mg_json_get(json, path, &elen);
        if (eoff < 0) break;
        struct mg_str elem = { json.buf + eoff, (std::size_t)elen };

        long id = mg_json_get_long(elem, "$.id", -1);
        if (id < 0) continue;

        // Obté "action" com a token JSON (inclou les cometes)
        int alen = 0;
        int aoff = mg_json_get(elem, "$.action", &alen);
        if (aoff < 0) continue;
        struct mg_str action = { elem.buf + aoff, (std::size_t)alen };

        if (mg_strcmp(action, mg_str("\"activate\"")) == 0 ||
            mg_strcmp(action, mg_str("\"deactivate\"")) == 0)
        {
            auto* ev      = Q_NEW(OutputCmdEvt, CTRL_OUTPUT_CMD_SIG);
            ev->output_id = (int)id;
            ev->activate  = (mg_strcmp(action, mg_str("\"activate\"")) == 0);
            POST(ev, this);
        }
        else if (mg_strcmp(action, mg_str("\"set_remote\"")) == 0 ||
                 mg_strcmp(action, mg_str("\"set_auto\"")) == 0)
        {
            auto* ev      = Q_NEW(OutputModeEvt, CTRL_OUTPUT_MODE_SIG);
            ev->output_id = (int)id;
            ev->remote    = (mg_strcmp(action, mg_str("\"set_remote\"")) == 0);
            POST(ev, this);
        }
        else if (mg_strcmp(action, mg_str("\"return_auto\"")) == 0)
        {
            auto* ev      = Q_NEW(OutputReturnAutoEvt, CTRL_OUTPUT_RETURN_AUTO_SIG);
            ev->output_id = (int)id;
            POST(ev, this);
        }
    }
}
