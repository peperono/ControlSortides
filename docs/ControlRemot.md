# ControlRemot

Active Object (`QP::QActive`) que gestiona el mode i l'estat resultant de cada
sortida del sistema. Tota la lògica de decisió "què surt físicament a la
sortida" viu aquí.

## Estat intern

Cada sortida té una entrada al mapa `m_outputs: int → OutputEntry`:

| Camp        | Significat                                                  |
|-------------|-------------------------------------------------------------|
| `mode`      | `AUTO` o `REMOTE`                                           |
| `physical`  | Últim estat físic conegut (entrada)                         |
| `commanded` | Última comanda `activate`/`deactivate` rebuda               |
| `result`    | Valor resultant publicat (el que surt al món)               |

## Regla de càlcul de `result`

```
mode == AUTO    →  result segueix physical
mode == REMOTE  →  result segueix commanded
```

En `AUTO`, una comanda puntual aplica `result = commanded` immediatament,
però el proper `OUTPUT_STATE_SIG` tornarà a sobreescriure `result` amb
`physical`. El camp `commanded` queda com a "rastre" de l'última ordre.

## Entrades (events)

| Event                  | Signal                        | Origen          | Efecte |
|------------------------|-------------------------------|-----------------|--------|
| `OutputStateEvt`       | `OUTPUT_STATE_SIG`            | `POST /io_state`| Actualitza `physical`; en `AUTO` també `result` |
| `OutputCmdEvt`         | `CTRL_OUTPUT_CMD_SIG`         | `POST /control` | Posa `commanded = result = activate` |
| `OutputModeEvt`        | `CTRL_OUTPUT_MODE_SIG`        | `POST /control` | Canvia `mode`, recalcula `result` |
| `OutputReturnAutoEvt`  | `CTRL_OUTPUT_RETURN_AUTO_SIG` | `POST /control` | Força `mode = AUTO` (`-1` = totes) |
| `OutputDeleteEvt`      | `CTRL_OUTPUT_DELETE_SIG`      | `POST /delete`  | Elimina la sortida del registre |

Tots provenen del thread Mongoose. `HttpServer` parseja el JSON i fa
`POST(ev, this)` directe sobre l'AO — `QActive::POST` és thread-safe.

## Sortides

Després de qualsevol canvi, `publishResult()`:

1. **Publica** `OutputResultEvt` amb signal `OUTPUT_RESULT_SIG` al bus QP
   (per a qualsevol AO subscrit).
2. **Escriu** `SharedState::outputsResult` (mapa complet `id → OutputInfo`)
   sota mutex i marca `se.push_pending = true`.

El thread del `HttpServer` veu el flag i envia l'estat per WebSocket a tots
els clients connectats.

## Format JSON dels endpoints

**POST `/control`** — array de comandes:
```json
[
  {"id": 10, "action": "activate"},
  {"id": 11, "action": "set_remote"},
  {"id": 12, "action": "return_auto"}
]
```

Accions vàlides: `activate`, `deactivate`, `set_remote`, `set_auto`, `return_auto`.

**POST `/io_state`** — injecció d'estat físic:
```json
{"outputs": [{"id": 10, "state": true}, {"id": 11, "state": false}]}
```

**POST `/delete`** — elimina una sortida:
```json
{"id": 10}
```

## Thread-safety

- L'AO viu al thread QV (cooperatiu, un sol fil QP).
- `HttpServer` viu al thread Mongoose.
- Comunicació HTTP→AO: només via `Q_NEW` + `POST` (sense `PUBLISH` des del
  thread Mongoose, que no és segur en aquest port QV).
- Comunicació AO→HTTP: via `SharedState` protegit amb `std::mutex` i el
  flag atòmic `push_pending`.

## Diagrama

Vegeu [ControlRemot.drawio](ControlRemot.drawio).
