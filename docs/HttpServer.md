# HttpServer

Servidor HTTP/WebSocket en un thread Mongoose dedicat (port 8080).

## REST API

### `GET /`

Serveix la pàgina HTML de monitorització (`index.html` compilat com a `INDEX_HTML`).

---

### `POST /control`

Comandes de control manual sobre les sortides.

**Body:**
```json
[
  {"id": 10, "action": "activate"},
  {"id": 11, "action": "set_remote"}
]
```

Accions vàlides:

| Acció | Efecte |
|---|---|
| `activate` | Activa la sortida |
| `deactivate` | Desactiva la sortida |
| `set_remote` | Posa la sortida en mode REMOTE |
| `set_auto` | Posa la sortida en mode AUTO |
| `return_auto` | Retorna a AUTO (`id: -1` → totes) |

**Acció interna:** crida `ControlRemot::handleJson()`, que posta `OutputCmdEvt`, `OutputModeEvt` o `OutputReturnAutoEvt` a l'AO.

**Resposta:** `{}`

---

### `GET /horari`

Retorna el calendari setmanal actiu.

**Resposta:** JSON del calendari llegit de `ch_state.programacioHoraria` (sota mutex).

---

### `POST /horari`

Carrega un nou calendari setmanal.

**Body:** JSON del calendari (mateix format que el GET).

**Acció interna:** escriu a `ch_state.programacioHoraria` i activa `load_pending` perquè `ControlHorari` el recarregui al proper tick de `Rellotge`.

**Resposta:** `{}`

---

### `POST /delete`

Elimina una sortida del registre de `ControlRemot`.

**Body:**
```json
{"id": 10}
```

**Acció interna:** posta `OutputDeleteEvt` (`CTRL_OUTPUT_DELETE_SIG`) a `ControlRemot`.

**Resposta:** `{}`

---

## WebSocket `/ws`

### Connexió nova

En connectar un client nou (`MG_EV_WS_OPEN`), s'envia immediatament l'estat actual complet.

### Push periòdic

El loop de Mongoose comprova cada **100 ms** si hi ha dades noves (`push_if_pending`).
S'activa quan `cr_state.push_pending` **o** `rellotge_state.push_pending` és `true`, cosa que passa quan:
- `ControlRemot` publica un resultat nou (canvi d'estat o comanda rebuda).
- `Rellotge` avança el minut simulat.

**Missatge enviat a tots els clients connectats:**
```json
{
  "time": "07:30",
  "day": "dilluns",
  "outputs": {
    "10": {"physical": true,  "commanded": true,  "result": true,  "mode": "AUTO"},
    "11": {"physical": false, "commanded": false, "result": false, "mode": "REMOTE"}
  }
}
```

### Missatges entrants del client

Ignorats per ara (`MG_EV_WS_MSG` no fa res).
