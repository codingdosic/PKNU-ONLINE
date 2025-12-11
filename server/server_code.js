

const WebSocket = require('ws');
const { v4: uuidv4 } = require('uuid');

const PORT = 8080;
const wss = new WebSocket.Server({ port: PORT });

console.log(`WebSocket 서버 시작: ws://localhost:${PORT}`);

let initialized = false;
const worldObjects = new Map();
const playerCharacters = new Map();
const clients = new Map();


wss.on('connection', (ws) => {
  const connectionId = uuidv4();
  clients.set(ws, { connectionId, playerID: null });
  console.log(`클라이언트 접속: connectionId=${connectionId}`);

  // 초기 전체 상태 전송
  send(ws, {
    type: 'state_sync',
    initialized,
    worldObjects: Array.from(worldObjects.entries()).map(([objectID, state]) => ({ objectID, state })),
    playerCharacters: Array.from(playerCharacters.entries()).map(([playerID, state]) => ({ playerID, state }))
  });

  ws.on('message', (raw) => {
    let msg;
    try { msg = JSON.parse(raw.toString()); }
    catch (err) { console.warn("JSON 파싱 실패:", err.message); return; }

    handleMessage(ws, msg);
  });

  ws.on('close', () => {
    const meta = clients.get(ws) || {};
    const playerID = meta.playerID;

    clients.delete(ws);
    console.log(`클라이언트 연결 종료: connectionId=${connectionId}`);
    if (playerID && playerCharacters.has(playerID)) {
      playerCharacters.delete(playerID);

      broadcast({
        type: 'render_update',
        action: 'remove_character',
        playerID
      });
    }
  });
});

function handleMessage(ws, msg) {
  switch (msg.type) {

    case 'register_batch': {
      if (initialized) return;

      const objects = Array.isArray(msg.objects) ? msg.objects : [];
      objects.forEach(o => {
        if (!o.objectID) return;
        worldObjects.set(o.objectID, {
          position: o.position,
          rotation: o.rotation
        });
      });

      initialized = true;

      broadcast({
        type: 'render_update',
        action: 'add_batch',
        objects: Array.from(worldObjects.entries()).map(([objectID, state]) => ({ objectID, ...state }))
      });

      break;
    }

    case 'register_character': {
      const playerID = msg.playerID || uuidv4();

      // 메타 저장
      const meta = clients.get(ws) || {};
      meta.playerID = playerID;
      clients.set(ws, meta);

      send(ws, { type: 'id', id: playerID });

      const initialState = {
        position: msg.position,
        rotation: msg.rotation,
        speed: 0,
        isFalling: false,
        meta: msg.meta || {}
      };

      if (msg.meta && msg.meta.playerName) {
        initialState.meta.playerName = msg.meta.playerName;
      }

      playerCharacters.set(playerID, initialState);

      broadcast({
        type: 'render_update',
        action: 'add_character',
        playerID,
        state: initialState
      });

      break;
    }

    case 'update': {
        const { entityType, id, state, isObject } = msg;

        if (entityType === 'world' && isObject) {

            if (!worldObjects.has(id)) {
                worldObjects.set(id, state);

                console.log(`[WORLD OBJECT ADDED] id=${id}, pos=(${state.position.x},${state.position.y},${state.position.z}), rot=(${state.rotation.pitch},${state.rotation.yaw},${state.rotation.roll})`);

                // 새 오브젝트 추가 (자신 제외)
                broadcastToOthers(ws, {
                    type: 'render_update',
                    action: 'add_object',
                    id,
                    state,
                    isObject: true
                });
            } 
            // 이미 등록된 오브젝트 위치/회전 갱신
            else {
                const obj = worldObjects.get(id);
                obj.position = state.position;
                obj.rotation = state.rotation;

                console.log(`[WORLD OBJECT UPDATED] id=${id}, pos=(${state.position.x},${state.position.y},${state.position.z}), rot=(${state.rotation.pitch},${state.rotation.yaw},${state.rotation.roll})`);

                // (자신 제외)
                broadcastToOthers(ws, {
                    type: 'render_update',
                    action: 'update',
                    entityType: 'world',
                    id,
                    state: obj,
                    isObject: true
                });
            }
            return; // update 처리 완료
        }

        // 기존 player 처리 
        if (entityType === 'player') {
            if (!playerCharacters.has(id)) return;

            const p = playerCharacters.get(id);
            p.position = state.position;
            p.rotation = state.rotation;
            if (state.speed !== undefined) p.speed = state.speed;
            if (state.isFalling !== undefined) p.isFalling = state.isFalling;
            playerCharacters.set(id, p);

            broadcastToOthers(ws, {
                type: 'render_update',
                action: 'update',
                entityType: 'player',
                id,
                state: p
            });
        }

        break;
    }


    case 'transform': {
        const { id, x, y, z, pitch, yaw, roll, speed, isFalling } = msg;

        // 플레이어인지 월드 오브젝트인지 체크
        if (playerCharacters.has(id)) {
            const p = playerCharacters.get(id);
            p.position = { x, y, z };
            p.rotation = { pitch, yaw, roll };
            if (speed !== undefined) p.speed = speed;
            if (isFalling !== undefined) p.isFalling = isFalling;

            playerCharacters.set(id, p);

            // console.log(`[PLAYER TRANSFORM] id=${id}, pos=(${x},${y},${z}), rot=(${pitch},${yaw},${roll}), speed=${p.speed}, isFalling=${p.isFalling}`);

            broadcastToOthers(ws, {
                type: 'render_update',
                action: 'transform',
                playerID: id,
                state: p
            });
        } 
        else if (worldObjects.has(id)) {
            const obj = worldObjects.get(id);
            obj.position = { x, y, z };
            obj.rotation = { pitch, yaw, roll };

            console.log(`[WORLD OBJECT TRANSFORM] id=${id}, pos=(${x},${y},${z}), rot=(${pitch},${yaw},${roll})`);

            broadcastToOthers(ws, {
                type: 'render_update',
                action: 'update',
                entityType: 'world',
                id,
                state: obj
            });
        } 
        else {
            console.warn(`[TRANSFORM] Unknown ID received: ${id}`);
        }

        break;
    }


    case 'chat': {
      const playerID = msg.playerID;
      const playerState = playerCharacters.get(playerID);
      const playerName = (playerState && playerState.meta && playerState.meta.playerName) ? playerState.meta.playerName : playerID;

      broadcast({
        type: 'render_update',
        action: 'new_chat',
        playerID: playerName, 
        message: msg.message
      });
      break;
    }

    case 'request_state': {
      send(ws, {
        type: 'state_sync',
        initialized,
        worldObjects: Array.from(worldObjects.entries()).map(([objectID, state]) => ({ objectID, state })),
        playerCharacters: Array.from(playerCharacters.entries()).map(([playerID, state]) => ({ playerID, state }))
      });
      break;
    }
  }
}

function send(ws, obj) {
  try {
    if (ws.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify(obj));
    }
  } catch {}
}

function broadcast(obj) {
  const data = JSON.stringify(obj);
  wss.clients.forEach(client => {
    if (client.readyState === WebSocket.OPEN) {
      setImmediate(() => client.send(data));
    }
  });
}

function broadcastToOthers(sender, obj) {
    const data = JSON.stringify(obj);
    wss.clients.forEach(client => {
        if (client !== sender && client.readyState === WebSocket.OPEN) {
            setImmediate(() => client.send(data));
        }
    });
}
