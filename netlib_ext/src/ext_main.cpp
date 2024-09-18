
#include "ext.hpp"

#include <cstring>

#if defined(DM_PLATFORM_HTML5)

extern "C"
{
    int NetlibJS_Network_New(void (*eraseCallback)(NetlibExt::InstancePtr instanceId, NetlibExt::LuaRef fnRef, dmScript::LuaCallbackInfo* callback), const char* cGameId, const int cGameIdLen);
    void NetlibJS_EngineStep(const double minDelta);
    // Instance methods
    void NetlibJS_Network_Close(const int instanceId, const char* cReason, const int cReasonLen);
    void NetlibJS_Network_List(const int instanceId, const char* cFilter, const int cFilterLen, void (*callback)(void* context, const char* error, const char* lobbies), const void* context);
    void NetlibJS_Network_Create(const int instanceId, const char* cLobbySettings, const int cLobbySettingsLen, void (*callback)(void* context, const char* error, const char* lobby), const void* context);
    void NetlibJS_Network_Join(const int instanceId, const char* cLobby, const int cLobbyLen, const char* cPassword, const int cPasswordLen, void (*callback)(void* context, const char* error, const char* lobbyListEntry), const void* context);
    void NetlibJS_Network_SetLobbySettings(const int instanceId, const char* cLobbySettings, const int cLobbySettingsLen, void (*callback)(void* context, const char* error, const bool success), const void* context);
    const char* NetlibJS_Network_Send(const int instanceId, const int channel, const char* cPeerId, const int cPeerIdLen, const char* cData, const int cDataLen);
    const char* NetlibJS_Network_Broadcast(const int instanceId, const int channel, const char* cData, const int cDataLen);
    // Events
    void NetlibJS_Network_On_Ready(const int instanceId, void (*callback)(void* context, bool once), const void* context, const bool once);
    void NetlibJS_Network_On_Lobby(const int instanceId, void (*callback)(void* context, bool once, const char* code, const char* lobbyInfo), const void* context, const bool once);
    void NetlibJS_Network_On_Leader(const int instanceId, void (*callback)(void* context, bool once, const char* leader), const void* context, const bool once);
    void NetlibJS_Network_On_LobbyUpdated(const int instanceId, void (*callback)(void* context, bool once, const char* code, const char* settings), const void* context, const bool once);
    void NetlibJS_Network_On_Connecting(const int instanceId, void (*callback)(void* context, bool once, const char* peer), const void* context, const bool once);
    void NetlibJS_Network_On_Connected(const int instanceId, void (*callback)(void* context, bool once, const char* peer), const void* context, const bool once);
    void NetlibJS_Network_On_Reconnecting(const int instanceId, void (*callback)(void* context, bool once, const char* peer), const void* context, const bool once);
    void NetlibJS_Network_On_Reconnected(const int instanceId, void (*callback)(void* context, bool once, const char* peer), const void* context, const bool once);
    void NetlibJS_Network_On_Disconnected(const int instanceId, void (*callback)(void* context, bool once, const char* peer), const void* context, const bool once);
    void NetlibJS_Network_On_SignalingReconnected(const int instanceId, void (*callback)(void* context, bool once), const void* context, const bool once);
    void NetlibJS_Network_On_Failed(const int instanceId, void (*callback)(void* context, bool once), const void* context, const bool once);
    void NetlibJS_Network_On_Message(const int instanceId, void (*callback)(void* context, bool once, const char* peer, const int channel, const char* data, const int dataLen), const void* context, const bool once);
    void NetlibJS_Network_On_Close(const int instanceId, void (*callback)(void* context, bool once, const char* reason), const void* context, const bool once);
    void NetlibJS_Network_On_RTCError(const int instanceId, void (*callback)(void* context, bool once, const char* event), const void* context, const bool once);
    void NetlibJS_Network_On_SignalingError(const int instanceId, void (*callback)(void* context, bool once, const char* error), const void* context, const bool once);
    void NetlibJS_Network_Off(const int instanceId, const int event, const void* context);
    // Custom getters
    const char* NetlibJS_Network_GetPeers(const int instanceId);
    // Getters
    const char* NetlibJS_Network_GetId(const int instanceId);
    const bool NetlibJS_Network_GetClosing(const int instanceId);
    const int NetlibJS_Network_GetSize(const int instanceId);
    const char* NetlibJS_Network_GetCurrentLobby(const int instanceId);
    const char* NetlibJS_Network_GetCurrentLobbyInfo(const int instanceId);
    const char* NetlibJS_Network_GetCurrentLeader(const int instanceId);
}

/**
 * Helpers
 */

static void LuaToJson(lua_State* L, int index, char** json, size_t* json_len)
{
    lua_pushvalue(L, index);
    lua_insert(L, 1);
    if (dmScript::LuaToJson(L, json, json_len) < 0)
    {
        luaL_error(L, "Failed to convert the table to JSON string.");
    }
    lua_remove(L, 1);
}

static int LuaError(lua_State* L, const char* error)
{
    lua_pushstring(L, error);
    lua_error(L);
    return 0;
}

/**
 * To be able to subscribe to an event, subscribe once and unsubscribe from it, then we have to store
 * subscription data in both C++ extension and JS. `InstanceContext` and `LuaCallbacksMap` are here
 * exclusively for this purpose.
 */

static NetlibExt::InstanceContexts g_InstanceContexts;

static NetlibExt::InstancePtr GetInstanceFromLua(lua_State* L)
{
    if (!lua_istable(L, 1))
    {
        luaL_error(L, "Instance is not found. Call the function like: `netlib.network_xxxxx(instance, arg1, arg2, ...)`.");
        return NULL;
    }
    lua_getfield(L, 1, "__id");
    if (!lua_islightuserdata(L, -1))
    {
        lua_pop(L, 1);
        luaL_error(L, "Instance is not found. Call the function like: `network.network_xxxxx(instance, arg1, arg2, ...)`.");
        return NULL;
    }
    NetlibExt::InstancePtr instanceId = (NetlibExt::InstancePtr)lua_touserdata(L, -1);
    lua_pop(L, 1);
    if (g_InstanceContexts.Get(instanceId) == NULL)
    {
        luaL_error(L, "Instance is not found. Have you called `netlib.network_new` or already closed it?");
        return NULL;
    }
    return instanceId;
}

static bool StoreCallbackRef(NetlibExt::InstancePtr instanceId, const int event, NetlibExt::LuaRef fnRef, dmScript::LuaCallbackInfo* callback)
{
    NetlibExt::InstanceContext* context = g_InstanceContexts.Get(instanceId);
    if (context->m_Map->Full())
    {
        context->m_Map->SetCapacity(context->m_Map->Capacity() * 2, context->m_Map->Capacity() * 2);
    }
    NetlibExt::CallbackWithEvent callbackWithEvent;
    callbackWithEvent.m_Callback = callback;
    callbackWithEvent.m_Event    = event;
    context->m_Map->Put(fnRef, callbackWithEvent);
    return true;
}

/**
 * If callback is not NULL, then we have to iterate over the map and erase that entry that match the callback.
 * Otherwise we erase the entry that match the fnRef.
 */
static void EraseCallbackRef(NetlibExt::InstancePtr instanceId, NetlibExt::LuaRef fnRef, dmScript::LuaCallbackInfo* callback)
{
    NetlibExt::InstanceContext* context = g_InstanceContexts.Get(instanceId);
    if (callback != 0)
    {
        NetlibExt::LuaCallbacksMap::Iterator it = context->m_Map->GetIterator();
        while (it.Next())
        {
            if (it.GetValue().m_Callback == callback)
            {
                NetlibExt::LuaRef callbackFnRef = it.GetKey();
                lua_State* L                    = dmScript::GetCallbackLuaContext(callback);
                luaL_unref(L, LUA_REGISTRYINDEX, callbackFnRef);
                dmScript::DestroyCallback(callback);
                context->m_Map->Erase(callbackFnRef);
                break;
            }
        }
    }
    else
    {
        NetlibExt::CallbackWithEvent* callbackWithEvent = context->m_Map->Get(fnRef);
        if (callbackWithEvent != NULL)
        {
            dmScript::LuaCallbackInfo* callback = callbackWithEvent->m_Callback;
            lua_State* L                        = dmScript::GetCallbackLuaContext(callback);
            luaL_unref(L, LUA_REGISTRYINDEX, fnRef);
            dmScript::DestroyCallback(callback);
            context->m_Map->Erase(fnRef);
        }
    }
}

static void InitContexts()
{
    g_InstanceContexts.SetCapacity(2, 2);
}

static void DestroyContexts()
{
    NetlibExt::InstanceContexts::Iterator it = g_InstanceContexts.GetIterator();
    while (it.Next())
    {
        NetlibExt::InstanceContext instance = it.GetValue();
        NetlibJS_Network_Close(instance.m_Ptr, 0, 0);
        NetlibExt::LuaCallbacksMap::Iterator it2 = instance.m_Map->GetIterator();
        while (it2.Next())
        {
            NetlibExt::LuaRef fnRef             = it2.GetKey();
            dmScript::LuaCallbackInfo* callback = it2.GetValue().m_Callback;
            lua_State* L                        = dmScript::GetCallbackLuaContext(callback);
            luaL_unref(L, LUA_REGISTRYINDEX, fnRef);
            dmScript::DestroyCallback(callback);
        }
        delete instance.m_Map;
    }
    g_InstanceContexts.Clear();
}

/**
 * Instance methods
 */

// close (reason?: string)
static int Network_Close(lua_State* L)
{
    const NetlibExt::InstancePtr instanceId = GetInstanceFromLua(L);
    if (lua_isstring(L, 2))
    {
        const char* reason  = luaL_checkstring(L, 2);
        const int reasonLen = strlen(reason);
        NetlibJS_Network_Close(instanceId, reason, reasonLen);
    }
    else
    {
        NetlibJS_Network_Close(instanceId, 0, 0);
    }
    NetlibExt::LuaCallbacksMap::Iterator it = g_InstanceContexts.Get(instanceId)->m_Map->GetIterator();
    while (it.Next())
    {
        luaL_unref(L, LUA_REGISTRYINDEX, it.GetKey());
        dmScript::DestroyCallback(it.GetValue().m_Callback);
    }
    delete g_InstanceContexts.Get(instanceId)->m_Map;
    g_InstanceContexts.Erase(instanceId);
    return 0;
}

/**
 * list (filter?: object, callback?: (self, err, lobbies: LobbyListEntry[]) => void)
 */

static void ListCallback(void* context, const char* error, const char* lobbies)
{
    NetlibExt::SetupLuaCallback lc((dmScript::LuaCallbackInfo*)context, true);
    if (!lc.Valid())
    {
        if (lobbies != 0)
            free((void*)lobbies);
        return;
    }

    lua_State* L = lc.GetLuaContext();

    if (error != 0)
    {
        lua_pushstring(L, error);
        lua_pushnil(L);
    }
    else
    {
        lua_pushnil(L);
        dmScript::JsonToLua(L, lobbies, strlen(lobbies));
        free((void*)lobbies);
    }

    lc.Call(L, 2);
}

static int Network_List(lua_State* L)
{
    const NetlibExt::InstancePtr instanceId = GetInstanceFromLua(L);
    size_t filterLen                        = 0;
    char* filter                            = 0;
    if (lua_istable(L, 2))
    {
        LuaToJson(L, 2, &filter, &filterLen);
    }
    dmScript::LuaCallbackInfo* luaCallback = dmScript::CreateCallback(L, 3);

    NetlibJS_Network_List(instanceId, filter, filterLen, ListCallback, luaCallback);
    if (filter != 0)
        free((void*)filter);
    return 0;
}

/**
 * create (settings?: LobbySettings, callback?: (lobby: string) => void)
 */

static void CreateCallback(void* context, const char* error, const char* lobby)
{
    NetlibExt::SetupLuaCallback lc((dmScript::LuaCallbackInfo*)context, true);
    if (!lc.Valid())
    {
        if (lobby != 0)
            free((void*)lobby);
        return;
    }

    lua_State* L = lc.GetLuaContext();

    if (error != 0)
    {
        lua_pushstring(L, error);
        lua_pushnil(L);
    }
    else
    {
        lua_pushnil(L);
        lua_pushstring(L, lobby);
        free((void*)lobby);
    }

    lc.Call(L, 2);
}

static int Network_Create(lua_State* L)
{
    const NetlibExt::InstancePtr instanceId = GetInstanceFromLua(L);
    size_t settingsLen                      = 0;
    char* settings                          = 0;
    if (lua_istable(L, 2))
    {
        LuaToJson(L, 2, &settings, &settingsLen);
    }
    dmScript::LuaCallbackInfo* luaCallback = 0;
    if (lua_isfunction(L, 3))
    {
        luaCallback = dmScript::CreateCallback(L, 3);
    }

    NetlibJS_Network_Create(instanceId, settings, settingsLen, CreateCallback, luaCallback);
    if (settings != 0)
        free((void*)settings);
    return 0;
}

/**
 * join (lobby: string, password?: string, callback?: (lobby: LobbyListEntry | undefined) => void)
 */

static void JoinCallback(void* context, const char* error, const char* lobbyListEntry)
{
    NetlibExt::SetupLuaCallback lc((dmScript::LuaCallbackInfo*)context, true);
    if (!lc.Valid())
    {
        if (lobbyListEntry != 0)
            free((void*)lobbyListEntry);
        return;
    }

    lua_State* L = lc.GetLuaContext();

    if (error != 0)
    {
        lua_pushstring(L, error);
        lua_pushnil(L);
    }
    else
    {
        lua_pushnil(L);
        dmScript::JsonToLua(L, lobbyListEntry, strlen(lobbyListEntry));
        free((void*)lobbyListEntry);
    }

    lc.Call(L, 2);
}

static int Network_Join(lua_State* L)
{
    const int instanceId                   = GetInstanceFromLua(L);
    const char* lobby                      = luaL_checkstring(L, 2);
    const char* password                   = lua_isstring(L, 3) ? lua_tostring(L, 3) : 0;
    dmScript::LuaCallbackInfo* luaCallback = 0;
    if (lua_isfunction(L, 4))
    {
        luaCallback = dmScript::CreateCallback(L, 4);
    }

    NetlibJS_Network_Join(instanceId, lobby, strlen(lobby), password, password ? strlen(password) : 0, JoinCallback, luaCallback);
    return 0;
}

/**
 * setLobbySettings (settings: LobbySettings, callback?: (success: boolean, error?: string) => void)
 */

static void SetLobbySettingsCallback(void* context, const char* error, bool success)
{
    NetlibExt::SetupLuaCallback lc((dmScript::LuaCallbackInfo*)context, true);
    if (!lc.Valid())
    {
        return;
    }

    lua_State* L = lc.GetLuaContext();

    if (error != 0)
    {
        lua_pushstring(L, error);
        lua_pushboolean(L, false);
    }
    else
    {
        lua_pushnil(L);
        lua_pushboolean(L, success);
    }

    lc.Call(L, 2);
}

static int Network_SetLobbySettings(lua_State* L)
{
    const NetlibExt::InstancePtr instanceId = GetInstanceFromLua(L);
    size_t settingsLen                      = 0;
    char* settings                          = 0;
    if (lua_istable(L, 2))
    {
        LuaToJson(L, 2, &settings, &settingsLen);
    }
    dmScript::LuaCallbackInfo* luaCallback = 0;
    if (lua_isfunction(L, 3))
    {
        luaCallback = dmScript::CreateCallback(L, 3);
    }

    NetlibJS_Network_SetLobbySettings(instanceId, settings, settingsLen, SetLobbySettingsCallback, luaCallback);
    if (settings != 0)
        free((void*)settings);
    return 0;
}

/**
 * send (channel: string, peerID: string, data: string | Blob | ArrayBuffer | ArrayBufferView)
 */

static int Network_Send(lua_State* L)
{
    const NetlibExt::InstancePtr instanceId = GetInstanceFromLua(L);

    size_t peerIdLen   = 0;
    size_t dataLen     = 0;
    const int channel  = luaL_checkinteger(L, 2);
    const char* peerId = luaL_checklstring(L, 3, &peerIdLen);
    const char* data   = luaL_checklstring(L, 4, &dataLen);

    if (channel != NetlibExt::CHANNEL_RELIABLE && channel != NetlibExt::CHANNEL_UNRELIABLE)
    {
        return LuaError(L, "Invalid channel");
    }

    const char* error = NetlibJS_Network_Send(instanceId, channel, peerId, peerIdLen, data, dataLen);
    if (error != 0)
    {
        return LuaError(L, error);
    }
    return 0;
}

/**
 * broadcast (channel: string, data: string | Blob | ArrayBuffer | ArrayBufferView)
 */

static int Network_Broadcast(lua_State* L)
{
    const NetlibExt::InstancePtr instanceId = GetInstanceFromLua(L);

    size_t dataLen    = 0;
    const int channel = luaL_checkinteger(L, 2);
    const char* data  = luaL_checklstring(L, 3, &dataLen);

    if (channel != NetlibExt::CHANNEL_RELIABLE && channel != NetlibExt::CHANNEL_UNRELIABLE)
    {
        return LuaError(L, "Invalid channel");
    }

    const char* error = NetlibJS_Network_Broadcast(instanceId, channel, data, dataLen);
    if (error != 0)
    {
        return LuaError(L, error);
    }
    return 0;
}

/**
 * Events
 */

static void OnReadyCallback(void* context, bool once)
{
    NetlibExt::SetupLuaCallback lc((dmScript::LuaCallbackInfo*)context, false);
    if (!lc.Valid())
    {
        return;
    }

    lua_State* L = lc.GetLuaContext();
    lc.Call(L, 0);
}

static void OnLobbyCallback(void* context, bool once, const char* code, const char* lobbyInfo)
{
    NetlibExt::SetupLuaCallback lc((dmScript::LuaCallbackInfo*)context, false);
    if (!lc.Valid())
    {
        free((void*)lobbyInfo);
        return;
    }

    lua_State* L = lc.GetLuaContext();

    lua_pushstring(L, code);
    dmScript::JsonToLua(L, lobbyInfo, strlen(lobbyInfo));
    free((void*)lobbyInfo);

    lc.Call(L, 2);
}

static void OnLeaderCallback(void* context, bool once, const char* leader)
{
    NetlibExt::SetupLuaCallback lc((dmScript::LuaCallbackInfo*)context, false);
    if (!lc.Valid())
    {
        return;
    }

    lua_State* L = lc.GetLuaContext();

    lua_pushstring(L, leader);

    lc.Call(L, 1);
}

static void OnLobbyUpdatedCallback(void* context, bool once, const char* code, const char* settings)
{
    NetlibExt::SetupLuaCallback lc((dmScript::LuaCallbackInfo*)context, false);
    if (!lc.Valid())
    {
        free((void*)settings);
        return;
    }

    lua_State* L = lc.GetLuaContext();

    lua_pushstring(L, code);
    dmScript::JsonToLua(L, settings, strlen(settings));
    free((void*)settings);

    lc.Call(L, 2);
}

static void OnConnectingCallback(void* context, bool once, const char* peer)
{
    NetlibExt::SetupLuaCallback lc((dmScript::LuaCallbackInfo*)context, false);
    if (!lc.Valid())
    {
        return;
    }

    lua_State* L = lc.GetLuaContext();

    lua_pushstring(L, peer);

    lc.Call(L, 1);
}

static void OnConnectedCallback(void* context, bool once, const char* peer)
{
    NetlibExt::SetupLuaCallback lc((dmScript::LuaCallbackInfo*)context, false);
    if (!lc.Valid())
    {
        return;
    }

    lua_State* L = lc.GetLuaContext();

    lua_pushstring(L, peer);

    lc.Call(L, 1);
}

static void OnReconnectingCallback(void* context, bool once, const char* peer)
{
    NetlibExt::SetupLuaCallback lc((dmScript::LuaCallbackInfo*)context, false);
    if (!lc.Valid())
    {
        return;
    }

    lua_State* L = lc.GetLuaContext();

    lua_pushstring(L, peer);

    lc.Call(L, 1);
}

static void OnReconnectedCallback(void* context, bool once, const char* peer)
{
    NetlibExt::SetupLuaCallback lc((dmScript::LuaCallbackInfo*)context, false);
    if (!lc.Valid())
    {
        return;
    }

    lua_State* L = lc.GetLuaContext();

    lua_pushstring(L, peer);

    lc.Call(L, 1);
}

static void OnDisconnectedCallback(void* context, bool once, const char* peer)
{
    NetlibExt::SetupLuaCallback lc((dmScript::LuaCallbackInfo*)context, false);
    if (!lc.Valid())
    {
        return;
    }

    lua_State* L = lc.GetLuaContext();

    lua_pushstring(L, peer);

    lc.Call(L, 1);
}

static void OnSignalingReconnectedCallback(void* context, bool once)
{
    NetlibExt::SetupLuaCallback lc((dmScript::LuaCallbackInfo*)context, false);
    if (!lc.Valid())
    {
        return;
    }

    lua_State* L = lc.GetLuaContext();

    lc.Call(L, 0);
}

static void OnFailedCallback(void* context, bool once)
{
    NetlibExt::SetupLuaCallback lc((dmScript::LuaCallbackInfo*)context, false);
    if (!lc.Valid())
    {
        return;
    }

    lua_State* L = lc.GetLuaContext();

    lc.Call(L, 0);
}

static void OnMessageCallback(void* context, bool once, const char* peer, const int channel, const char* data, const int dataLen)
{
    NetlibExt::SetupLuaCallback lc((dmScript::LuaCallbackInfo*)context, false);
    if (!lc.Valid())
    {
        free((void*)data);
        return;
    }

    lua_State* L = lc.GetLuaContext();

    lua_pushstring(L, peer);
    lua_pushnumber(L, channel);
    lua_pushlstring(L, data, dataLen);
    free((void*)data);

    lc.Call(L, 3);
}

static void OnCloseCallback(void* context, bool once, const char* reason)
{
    NetlibExt::SetupLuaCallback lc((dmScript::LuaCallbackInfo*)context, false);
    if (!lc.Valid())
    {
        return;
    }

    lua_State* L = lc.GetLuaContext();

    lua_pushstring(L, reason);

    lc.Call(L, 1);
}

static void OnRTCErrorCallback(void* context, bool once, const char* error)
{
    NetlibExt::SetupLuaCallback lc((dmScript::LuaCallbackInfo*)context, false);
    if (!lc.Valid())
    {
        return;
    }

    lua_State* L = lc.GetLuaContext();

    lua_pushstring(L, error);

    lc.Call(L, 1);
}

static void OnSignalingErrorCallback(void* context, bool once, const char* error)
{
    NetlibExt::SetupLuaCallback lc((dmScript::LuaCallbackInfo*)context, false);
    if (!lc.Valid())
    {
        return;
    }

    lua_State* L = lc.GetLuaContext();

    lua_pushstring(L, error);

    lc.Call(L, 1);
}

static void DoAddListener(lua_State* L, bool once)
{
    const int instanceId                   = GetInstanceFromLua(L);
    const int event                        = luaL_checkinteger(L, 2);
    dmScript::LuaCallbackInfo* luaCallback = dmScript::CreateCallback(L, 3);

    lua_pushvalue(L, 3);
    NetlibExt::LuaRef fnRef = luaL_ref(L, LUA_REGISTRYINDEX);

    switch (event)
    {
        case NetlibExt::EVENT_READY:
            NetlibJS_Network_On_Ready(instanceId, OnReadyCallback, luaCallback, once);
            break;

        case NetlibExt::EVENT_LOBBY:
            NetlibJS_Network_On_Lobby(instanceId, OnLobbyCallback, luaCallback, once);
            break;

        case NetlibExt::EVENT_LEADER:
            NetlibJS_Network_On_Leader(instanceId, OnLeaderCallback, luaCallback, once);
            break;

        case NetlibExt::EVENT_LOBBY_UPDATED:
            NetlibJS_Network_On_LobbyUpdated(instanceId, OnLobbyUpdatedCallback, luaCallback, once);
            break;

        case NetlibExt::EVENT_CONNECTING:
            NetlibJS_Network_On_Connecting(instanceId, OnConnectingCallback, luaCallback, once);
            break;

        case NetlibExt::EVENT_CONNECTED:
            NetlibJS_Network_On_Connected(instanceId, OnConnectedCallback, luaCallback, once);
            break;

        case NetlibExt::EVENT_RECONNECTING:
            NetlibJS_Network_On_Reconnecting(instanceId, OnReconnectingCallback, luaCallback, once);
            break;

        case NetlibExt::EVENT_RECONNECTED:
            NetlibJS_Network_On_Reconnected(instanceId, OnReconnectedCallback, luaCallback, once);
            break;

        case NetlibExt::EVENT_DISCONNECTED:
            NetlibJS_Network_On_Disconnected(instanceId, OnDisconnectedCallback, luaCallback, once);
            break;

        case NetlibExt::EVENT_SIGNALING_RECONNECTED:
            NetlibJS_Network_On_SignalingReconnected(instanceId, OnSignalingReconnectedCallback, luaCallback, once);
            break;

        case NetlibExt::EVENT_FAILED:
            NetlibJS_Network_On_Failed(instanceId, OnFailedCallback, luaCallback, once);
            break;

        case NetlibExt::EVENT_MESSAGE:
            NetlibJS_Network_On_Message(instanceId, OnMessageCallback, luaCallback, once);
            break;

        case NetlibExt::EVENT_CLOSE:
            NetlibJS_Network_On_Close(instanceId, OnCloseCallback, luaCallback, once);
            break;

        case NetlibExt::EVENT_RTC_ERROR:
            NetlibJS_Network_On_RTCError(instanceId, OnRTCErrorCallback, luaCallback, once);
            break;

        case NetlibExt::EVENT_SIGNALING_ERROR:
            NetlibJS_Network_On_SignalingError(instanceId, OnSignalingErrorCallback, luaCallback, once);
            break;

        default:
            luaL_error(L, "Invalid event: %d", event);
            return;
    }

    StoreCallbackRef(instanceId, event, fnRef, luaCallback);
}

/**
 * on (event: string, callback: (...args: any[]) => void)
 */
static int Network_On(lua_State* L)
{
    DoAddListener(L, false);
    return 0;
}

/**
 * off (event: string, callback: (...args: any[]) => void)
 */
static int Network_Off(lua_State* L)
{
    const NetlibExt::InstancePtr instanceId = GetInstanceFromLua(L);
    const int event                         = luaL_checkinteger(L, 2);

    if (event < NetlibExt::EVENT_FIRST || event > NetlibExt::EVENT_LAST)
    {
        return luaL_error(L, "Invalid event: %d", event);
    }

    NetlibExt::InstanceContext* instanceContext = g_InstanceContexts.Get(instanceId);
    if (!lua_isnoneornil(L, 3))
    {
        const int fnRef = luaL_checkinteger(L, 3);

        NetlibExt::CallbackWithEvent* callbackWithEvent = instanceContext->m_Map->Get(fnRef);
        if (callbackWithEvent == NULL)
        {
            LuaError(L, "Callback not found");
            return 0;
        }
        dmScript::LuaCallbackInfo* luaCallback = callbackWithEvent->m_Callback;
        NetlibJS_Network_Off(instanceId, event, luaCallback);
        EraseCallbackRef(instanceId, fnRef, NULL);
    }
    else
    {
        NetlibJS_Network_Off(instanceId, event, NULL);
        NetlibExt::LuaCallbacksMap::Iterator it = instanceContext->m_Map->GetIterator();
        while (it.Next())
        {
            if (it.GetValue().m_Event == event)
            {
                NetlibExt::LuaRef callbackFnRef = it.GetKey();
                luaL_unref(L, LUA_REGISTRYINDEX, callbackFnRef);
                dmScript::DestroyCallback(it.GetValue().m_Callback);
                instanceContext->m_Map->Erase(callbackFnRef);
            }
        }
    }

    return 0;
}

/**
 * once (event: string, callback: (...args: any[]) => void)
 */
static int Network_Once(lua_State* L)
{
    DoAddListener(L, true);
    return 0;
}

static int Network_GetPeers(lua_State* L)
{
    const NetlibExt::InstancePtr instanceId = GetInstanceFromLua(L);
    const char* peers                       = NetlibJS_Network_GetPeers(instanceId);
    dmScript::JsonToLua(L, peers, strlen(peers));
    free((void*)peers);
    return 1;
}

/**
 * Getters
 */

static int Network_GetId(lua_State* L)
{
    const NetlibExt::InstancePtr instanceId = GetInstanceFromLua(L);
    const char* id                          = NetlibJS_Network_GetId(instanceId);
    lua_pushstring(L, id);
    return 1;
}

static int Network_GetClosing(lua_State* L)
{
    const NetlibExt::InstancePtr instanceId = GetInstanceFromLua(L);
    const bool closing                      = NetlibJS_Network_GetClosing(instanceId);
    lua_pushboolean(L, closing);
    return 1;
}

static int Network_GetSize(lua_State* L)
{
    const NetlibExt::InstancePtr instanceId = GetInstanceFromLua(L);
    const int size                          = NetlibJS_Network_GetSize(instanceId);
    lua_pushinteger(L, size);
    return 1;
}

static int Network_GetCurrentLobby(lua_State* L)
{
    const NetlibExt::InstancePtr instanceId = GetInstanceFromLua(L);
    const char* lobby                       = NetlibJS_Network_GetCurrentLobby(instanceId);
    if (lobby != 0)
    {
        lua_pushstring(L, lobby);
    }
    else
    {
        lua_pushnil(L);
    }
    return 1;
}

static int Network_GetCurrentLobbyInfo(lua_State* L)
{
    const int instanceId  = GetInstanceFromLua(L);
    const char* lobbyInfo = NetlibJS_Network_GetCurrentLobbyInfo(instanceId);
    if (lobbyInfo != 0)
    {
        dmScript::JsonToLua(L, lobbyInfo, strlen(lobbyInfo));
        free((void*)lobbyInfo);
    }
    else
    {
        lua_pushnil(L);
    }
    return 1;
}

static int Network_GetCurrentLeader(lua_State* L)
{
    const NetlibExt::InstancePtr instanceId = GetInstanceFromLua(L);
    const char* leader                      = NetlibJS_Network_GetCurrentLeader(instanceId);
    if (leader != 0)
    {
        lua_pushstring(L, leader);
    }
    else
    {
        lua_pushnil(L);
    }
    return 1;
}

/**
 * Functions
 */

static int Network_New(lua_State* L)
{
    size_t gameIdLen;
    const char* gameId = luaL_checklstring(L, 1, &gameIdLen);

    const NetlibExt::InstancePtr instanceId = NetlibJS_Network_New(EraseCallbackRef, gameId, gameIdLen);

    lua_newtable(L);
    lua_pushlightuserdata(L, (void*)instanceId);
    lua_setfield(L, -2, "__id");

    if (g_InstanceContexts.Full())
    {
        g_InstanceContexts.SetCapacity(g_InstanceContexts.Capacity() * 2, g_InstanceContexts.Capacity() * 2);
    }
    g_InstanceContexts.Put(instanceId, NetlibExt::InstanceContext());
    NetlibExt::InstanceContext* instanceContext = g_InstanceContexts.Get(instanceId);
    instanceContext->m_Ptr                      = instanceId;
    instanceContext->m_Map                      = new NetlibExt::LuaCallbacksMap();
    instanceContext->m_Map->SetCapacity(16, 16);

    return 1;
}

static int EngineStep(lua_State* L)
{
    const double minDelta = lua_isnumber(L, 1) ? lua_tonumber(L, 1) : 0;
    NetlibJS_EngineStep(minDelta);
    return 0;
}

// Functions exposed to Lua
static const luaL_reg Ext_methods[] = {
    { "new", Network_New },
    { "engine_step", EngineStep },
    // Instance methods
    { "close", Network_Close },
    { "list", Network_List },
    { "create", Network_Create },
    { "join", Network_Join },
    { "set_lobby_settings", Network_SetLobbySettings },
    { "send", Network_Send },
    { "broadcast", Network_Broadcast },
    // Event system
    { "on", Network_On },
    { "off", Network_Off },
    { "once", Network_Once },
    // Properties
    { "peers", Network_GetPeers },
    { "id", Network_GetId },
    { "closing", Network_GetClosing },
    { "size", Network_GetSize },
    { "current_lobby", Network_GetCurrentLobby },
    { "current_lobby_info", Network_GetCurrentLobbyInfo },
    { "current_leader", Network_GetCurrentLeader },

    /* Sentinel: */
    { NULL, NULL }
};

static void LuaInit(lua_State* L)
{
    int top = lua_gettop(L);

    // Register lua names
    luaL_register(L, "netlib", Ext_methods);

#define SETCONSTANT(name, val) \
    lua_pushnumber(L, (lua_Number)val); \
    lua_setfield(L, -2, #name);

    SETCONSTANT(EVENT_READY, NetlibExt::EVENT_READY);
    SETCONSTANT(EVENT_LOBBY, NetlibExt::EVENT_LOBBY);
    SETCONSTANT(EVENT_LEADER, NetlibExt::EVENT_LEADER);
    SETCONSTANT(EVENT_LOBBY_UPDATED, NetlibExt::EVENT_LOBBY_UPDATED);
    SETCONSTANT(EVENT_CONNECTING, NetlibExt::EVENT_CONNECTING);
    SETCONSTANT(EVENT_CONNECTED, NetlibExt::EVENT_CONNECTED);
    SETCONSTANT(EVENT_RECONNECTING, NetlibExt::EVENT_RECONNECTING);
    SETCONSTANT(EVENT_RECONNECTED, NetlibExt::EVENT_RECONNECTED);
    SETCONSTANT(EVENT_DISCONNECTED, NetlibExt::EVENT_DISCONNECTED);
    SETCONSTANT(EVENT_SIGNALING_RECONNECTED, NetlibExt::EVENT_SIGNALING_RECONNECTED);
    SETCONSTANT(EVENT_FAILED, NetlibExt::EVENT_FAILED);
    SETCONSTANT(EVENT_MESSAGE, NetlibExt::EVENT_MESSAGE);
    SETCONSTANT(EVENT_CLOSE, NetlibExt::EVENT_CLOSE);
    SETCONSTANT(EVENT_RTC_ERROR, NetlibExt::EVENT_RTC_ERROR);
    SETCONSTANT(EVENT_SIGNALING_ERROR, NetlibExt::EVENT_SIGNALING_ERROR);

    SETCONSTANT(CHANNEL_RELIABLE, NetlibExt::CHANNEL_RELIABLE);
    SETCONSTANT(CHANNEL_UNRELIABLE, NetlibExt::CHANNEL_UNRELIABLE);

    lua_pop(L, 1);
    assert(top == lua_gettop(L));
}

#endif

static dmExtension::Result InitializeExt(dmExtension::Params* params)
{
#if defined(DM_PLATFORM_HTML5)
    LuaInit(params->m_L);
    InitContexts();
#endif

    return dmExtension::RESULT_OK;
}

static dmExtension::Result AppInitializeExt(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

static dmExtension::Result FinalizeExt(dmExtension::Params* params)
{
#if defined(DM_PLATFORM_HTML5)
    DestroyContexts();
#endif
    return dmExtension::RESULT_OK;
}

static dmExtension::Result AppFinalizeExt(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(netlib, "netlib", AppInitializeExt, AppFinalizeExt, InitializeExt, 0, 0, FinalizeExt)
