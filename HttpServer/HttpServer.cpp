#include "HttpServer.h"
#include "../ControlRemot/ControlRemot.h"
#include "../SharedState.h"
#include "../signals.h"
#include "../mongoose/mongoose.h"
#include <atomic>
#include <thread>
#include <string>
#include <cstdio>
#include <mutex>
#include "index_html.h"

// ── Server thread ─────────────────────────────────────────────────────────────

static std::atomic<bool>  s_running{false};
static std::thread        s_thread;
static QP::QActive*       s_controlRemot = nullptr;

// ── Helpers JSON ──────────────────────────────────────────────────────────────

static std::string outputs_to_json(
        const std::unordered_map<int, OutputInfo>& m) {
    std::string s = "{\"outputs\":{";
    bool first = true;
    for (auto const& [k, v] : m) {
        if (!first) s += ",";
        s += "\"" + std::to_string(k) + "\":{"
             "\"physical\":" + (v.physical ? "true" : "false") + ","
             "\"commanded\":" + (v.commanded ? "true" : "false") + ","
             "\"result\":" + (v.result ? "true" : "false") + ","
             "\"mode\":\"" + (v.remote ? "REMOTE" : "AUTO") + "\""
             "}";
        first = false;
    }
    s += "}}";
    return s;
}

// ── WebSocket push ────────────────────────────────────────────────────────────

static void push_if_pending(struct mg_mgr* mgr) {
    if (!se.push_pending.load()) return;
    se.push_pending.store(false);

    std::unordered_map<int, OutputInfo> outputs;
    {
        std::lock_guard<std::mutex> lk(se.mtx);
        outputs = se.outputsFull;
    }

    std::string msg = outputs_to_json(outputs);
    for (struct mg_connection* c = mgr->conns; c != nullptr; c = c->next) {
        if (c->is_websocket) {
            mg_ws_send(c, msg.c_str(), msg.size(), WEBSOCKET_OP_TEXT);
        }
    }
}

// ── Mongoose event handler ────────────────────────────────────────────────────

static void http_fn(struct mg_connection* c, int ev, void* ev_data) {
    if (ev == MG_EV_HTTP_MSG) {
        auto* hm = static_cast<struct mg_http_message*>(ev_data);

        // ── GET / ────────────────────────────────────────────────────────────
        if (mg_match(hm->uri, mg_str("/"), NULL)
            && mg_match(hm->method, mg_str("GET"), NULL))
        {
            mg_http_reply(c, 200, "Content-Type: text/html\r\n",
                          "%.*s", (int)sizeof(INDEX_HTML) - 1, INDEX_HTML);
        }

        // ── POST /control ─────────────────────────────────────────────────────
        else if (mg_match(hm->uri, mg_str("/control"), NULL)
                 && mg_match(hm->method, mg_str("POST"), NULL))
        {
            if (s_controlRemot && hm->body.len > 0) {
                static_cast<ControlRemot*>(s_controlRemot)
                    ->handleJson(hm->body.buf, hm->body.len);
            }
            mg_http_reply(c, 200, "Content-Type: application/json\r\n", "{}");
        }

        // ── POST /io_state ────────────────────────────────────────────────────
        else if (mg_match(hm->uri, mg_str("/io_state"), NULL)
                 && mg_match(hm->method, mg_str("POST"), NULL))
        {
            if (!s_controlRemot || hm->body.len == 0) {
                mg_http_reply(c, 400, "", "empty body\n");
                return;
            }

            auto* ev = Q_NEW(IoStateHttpEvt, IO_STATE_HTTP_SIG);
            ev->n_outputs = 0;

            struct mg_str json = hm->body;
            for (int i = 0; i < IoStateHttpEvt::MAX_OUTPUTS; ++i) {
                char path[32];
                std::snprintf(path, sizeof(path), "$.outputs[%d]", i);
                int elen = 0;
                int eoff = mg_json_get(json, path, &elen);
                if (eoff < 0) break;
                struct mg_str elem = { json.buf + eoff, (std::size_t)elen };

                long id = mg_json_get_long(elem, "$.id", -1);
                if (id < 0) continue;
                bool state = false;
                mg_json_get_bool(elem, "$.state", &state);

                ev->outputs[ev->n_outputs++] = { (int)id, state };
            }

            s_controlRemot->POST(ev, nullptr);
            mg_http_reply(c, 200, "Content-Type: application/json\r\n", "{}");
        }

        // ── POST /delete ──────────────────────────────────────────────────────
        else if (mg_match(hm->uri, mg_str("/delete"), NULL)
                 && mg_match(hm->method, mg_str("POST"), NULL))
        {
            if (s_controlRemot && hm->body.len > 0) {
                long id = mg_json_get_long(hm->body, "$.id", -1);
                if (id >= 0) {
                    auto* ev = Q_NEW(OutputDeleteEvt, CTRL_OUTPUT_DELETE_SIG);
                    ev->output_id = (int)id;
                    s_controlRemot->POST(ev, nullptr);
                }
            }
            mg_http_reply(c, 200, "Content-Type: application/json\r\n", "{}");
        }

        // ── GET /ws (WebSocket upgrade) ──────────────────────────────────
        else if (mg_match(hm->uri, mg_str("/ws"), NULL)) {
            mg_ws_upgrade(c, hm, NULL);
        }

        else {
            mg_http_reply(c, 404, "", "Not found\n");
        }

    } else if (ev == MG_EV_WS_MSG) {
        // Missatges WebSocket entrants ignorats per ara
        (void)ev_data;

    } else if (ev == MG_EV_WS_OPEN) {
        // Envia l'estat actual al client que acaba de connectar
        std::unordered_map<int, OutputInfo> outputs;
        {
            std::lock_guard<std::mutex> lk(se.mtx);
            outputs = se.outputsFull;
        }
        std::string msg = outputs_to_json(outputs);
        mg_ws_send(c, msg.c_str(), msg.size(), WEBSOCKET_OP_TEXT);
    }
}

// ── Server loop ───────────────────────────────────────────────────────────────

static void server_loop(uint16_t port) {
    char addr[32];
    std::snprintf(addr, sizeof(addr), "http://0.0.0.0:%u", port);

    struct mg_mgr mgr;
    mg_mgr_init(&mgr);
    mg_http_listen(&mgr, addr, http_fn, NULL);
    std::printf("[HttpServer] escoltant a %s\n", addr);

    while (s_running.load()) {
        mg_mgr_poll(&mgr, 100);
        push_if_pending(&mgr);
    }

    mg_mgr_free(&mgr);
}

// ── API pública ───────────────────────────────────────────────────────────────

void HttpServer::start(uint16_t port, QP::QActive* controlRemot) {
    s_controlRemot = controlRemot;
    s_running      = true;
    s_thread       = std::thread(server_loop, port);
}

void HttpServer::stop() {
    s_running = false;
    if (s_thread.joinable()) s_thread.join();
}
