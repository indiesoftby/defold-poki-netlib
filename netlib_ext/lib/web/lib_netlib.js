var LibraryNetlibJS = {
    $NetlibJS: {
        instances: {},
        instanceId: 0,
        eraseCallbackRef: null, // set by NetlibJS_Network_New

        /**
         * Adds a new instance to the instances object and returns its ID.
         * @param {Object} instance - The instance to be added.
         * @returns {number} The ID of the newly added instance.
         */
        addInstance: function (instance) {
            this.instanceId++;
            this.instances[this.instanceId] = {
                id: this.instanceId,
                network: instance,
                eventListeners: {},
            };
            return this.instanceId;
        },

        /**
         * Removes an instance from the instances object by its ID.
         * @param {number} id - The ID of the instance to be removed.
         */
        removeInstance: function (id) {
            delete this.instances[id];
        },

        addEventListener: function (instance, event, context, once, callback) {
            if (!instance.eventListeners[event]) {
                instance.eventListeners[event] = [];
            }
            if (once) {
                const toCall = callback;
                callback = (...args) => {
                    toCall(...args);
                    if (once) NetlibJS.removeEventListener(instance, event, context, true);
                };
            }
            instance.eventListeners[event].push({ context, callback });
            if (once) {
                instance.network.once(event, callback);
            } else {
                instance.network.on(event, callback);
            }
        },

        removeEventListener: function (instance, event, context, onlyRemove) {
            if (!instance.eventListeners[event]) return;

            const listener = instance.eventListeners[event].find((listener) => listener.context === context);
            if (!listener) return;

            instance.eventListeners[event].splice(instance.eventListeners[event].indexOf(listener), 1);
            if (onlyRemove) {
                const fn = NetlibJS.eraseCallbackRef;
                {{{ makeDynCall('viii', 'fn') }}}(instance.id, 0, context);
                return;
            }

            instance.network.off(event, listener.callback);
        },

        removeAllEventListeners: function (instance, event) {
            if (!instance.eventListeners[event]) return;

            instance.eventListeners[event] = [];
            instance.network.off(event);
        },

        eventNames: {
            1: "ready",
            2: "lobby",
            3: "leader",
            4: "lobbyUpdated",
            5: "connecting",
            6: "connected",
            7: "reconnecting",
            8: "reconnected",
            9: "disconnected",
            10: "signalingreconnected",
            11: "failed",
            12: "message",
            13: "close",
            14: "rtcerror",
            15: "signalingerror",
        },

        enumToChannelName: function (channel) {
            switch (channel) {
                case 1: return "reliable";
                case 2: return "unreliable";
                default: return "unknown";
            }
        },

        channelNameToEnum: function (channel) {
            switch (channel) {
                case "reliable": return 1;
                case "unreliable": return 2;
                default: return -1;
            }
        },
    },

    /**
     * Creates a new Network instance and returns its ID.
     * @todo Add rtcConfig and signalingURL params.
     * @param {number} cGameId - The game ID for the network (C pointer).
     * @param {number} cGameIdLen - The length of the game ID string.
     * @returns {number} The internal ID of the newly created Network instance.
     */
    NetlibJS_Network_New: function (eraseCallbackRef, cGameId, cGameIdLen) {
        NetlibJS.eraseCallbackRef = eraseCallbackRef;

        const gameId = UTF8ToString(cGameId, cGameIdLen);
        const network = new window.netlib.Network(gameId);
        const instanceId = NetlibJS.addInstance(network);

        return instanceId;
    },

    /**
     * Closes the network instance by its ID.
     * @param {number} instanceId - The internal ID of the Network instance to be closed.
     * @param {number} cReason - The reason for closing the network (C pointer).
     * @param {number} cReasonLen - The length of the reason string.
     */
    NetlibJS_Network_Close: function (instanceId, cReason, cReasonLen) {
        const network = NetlibJS.instances[instanceId].network;
        if (cReason !== 0) {
            network.close(UTF8ToString(cReason, cReasonLen));
        } else {
            network.close();
        }
        NetlibJS.removeInstance(instanceId);
    },

    NetlibJS_Network_List: function (instanceId, cFilter, cFilterLen, callback, context) {
        const network = NetlibJS.instances[instanceId].network;
        const filter = cFilter !== 0 ? JSON.parse(UTF8ToString(cFilter, cFilterLen)) : null;
        network.list(filter).then((lobbies) => {
            {{{ makeDynCall('viii', 'callback') }}}(context, 0, stringToNewUTF8(JSON.stringify(lobbies)));
        }).catch((err) => {
            {{{ makeDynCall('viii', 'callback') }}}(context, stringToUTF8OnStack(err.toString()), 0);
        });
    },

    NetlibJS_Network_Create: function (instanceId, cSettings, cSettingsLen, callback, context) {
        const network = NetlibJS.instances[instanceId].network;
        const settings = cSettings !== 0 ? JSON.parse(UTF8ToString(cSettings, cSettingsLen)) : null;
        network.create(settings).then((lobby) => {
            if (context === 0) return;
            {{{ makeDynCall('viii', 'callback') }}}(context, 0, stringToNewUTF8(lobby));
        }).catch((err) => {
            if (context === 0) return;
            {{{ makeDynCall('viii', 'callback') }}}(context, stringToUTF8OnStack(err.toString()), 0);
        });
    },

    NetlibJS_Network_Join: function (instanceId, cLobby, cLobbyLen, cPassword, cPasswordLen, callback, context) {
        const network = NetlibJS.instances[instanceId].network;
        const lobby = UTF8ToString(cLobby, cLobbyLen);
        const password = cPassword !== 0 ? UTF8ToString(cPassword, cPasswordLen) : undefined;
        network.join(lobby, password).then((lobbyInfo) => {
            if (context === 0) return;
            {{{ makeDynCall('viii', 'callback') }}}(context, 0, stringToNewUTF8(JSON.stringify(lobbyInfo)));
        }).catch((err) => {
            if (context === 0) return;
            {{{ makeDynCall('viii', 'callback') }}}(context, stringToUTF8OnStack(err.toString()), 0);
        });
    },

    NetlibJS_Network_SetLobbySettings: function (instanceId, cSettings, cSettingsLen, callback, context) {
        const network = NetlibJS.instances[instanceId].network;
        const settings = cSettings !== 0 ? JSON.parse(UTF8ToString(cSettings, cSettingsLen)) : null;
        network.setLobbySettings(settings).then((result) => {
            if (context === 0) return;
            if (typeof result === 'boolean') {
                {{{ makeDynCall('viii', 'callback') }}}(context, 0, result);
            } else {
                {{{ makeDynCall('viii', 'callback') }}}(context, stringToUTF8OnStack(result.toString()), false);
            }
        }).catch((err) => {
            if (context === 0) return;
            {{{ makeDynCall('viii', 'callback') }}}(context, stringToUTF8OnStack(err.toString()), false);
        });
    },

    NetlibJS_Network_Send: function (instanceId, channel, cPeerId, cPeerIdLen, cData, cDataLen) {
        const network = NetlibJS.instances[instanceId].network;
        const channelName = NetlibJS.enumToChannelName(channel);
        const peerId = UTF8ToString(cPeerId, cPeerIdLen);
        const data = new Uint8Array(HEAPU8.slice(cData, cDataLen));
        try {
            network.send(channelName, peerId, data);
            return 0;
        } catch (err) {
            return stringToUTF8OnStack(err.toString());
        }
    },

    NetlibJS_Network_Broadcast: function (instanceId, channel, cData, cDataLen) {
        const network = NetlibJS.instances[instanceId].network;
        const channelName = NetlibJS.enumToChannelName(channel);
        const data = new Uint8Array(HEAPU8.slice(cData, cDataLen));
        try {
            network.broadcast(channelName, data);
            return 0;
        } catch (err) {
            return stringToUTF8OnStack(err.toString());
        }
    },

    //
    // Events
    //

    NetlibJS_Network_On_Ready: function (instanceId, callback, context, once) {
        const instance = NetlibJS.instances[instanceId];
        NetlibJS.addEventListener(instance, "ready", context, once, () => {
            {{{ makeDynCall('vii', 'callback') }}}(context, once);
        });
    },

    NetlibJS_Network_On_Lobby: function (instanceId, callback, context, once) {
        const instance = NetlibJS.instances[instanceId];
        NetlibJS.addEventListener(instance, "lobby", context, once, (code, lobbyInfo) => {
            {{{ makeDynCall('viiii', 'callback') }}}(context, once, stringToUTF8OnStack(code), stringToNewUTF8(JSON.stringify(lobbyInfo)));
        });
    },

    NetlibJS_Network_On_Leader: function (instanceId, callback, context, once) {
        const instance = NetlibJS.instances[instanceId];
        NetlibJS.addEventListener(instance, "leader", context, once, (leader) => {
            {{{ makeDynCall('viii', 'callback') }}}(context, once, stringToUTF8OnStack(leader));
        });
    },

    NetlibJS_Network_On_LobbyUpdated: function (instanceId, callback, context, once) {
        const instance = NetlibJS.instances[instanceId];
        NetlibJS.addEventListener(instance, "lobbyUpdated", context, once, (code, settings) => {
            {{{ makeDynCall('viiii', 'callback') }}}(context, once, stringToUTF8OnStack(code), stringToNewUTF8(JSON.stringify(settings)));
        });
    },

    NetlibJS_Network_On_Connecting: function (instanceId, callback, context, once) {
        const instance = NetlibJS.instances[instanceId];
        NetlibJS.addEventListener(instance, "connecting", context, once, (peer) => {
            {{{ makeDynCall('viii', 'callback') }}}(context, once, stringToUTF8OnStack(peer.id));
        });
    },

    NetlibJS_Network_On_Connected: function (instanceId, callback, context, once) {
        const instance = NetlibJS.instances[instanceId];
        NetlibJS.addEventListener(instance, "connected", context, once, (peer) => {
            {{{ makeDynCall('viii', 'callback') }}}(context, once, stringToUTF8OnStack(peer.id));
        });
    },

    NetlibJS_Network_On_Reconnecting: function (instanceId, callback, context, once) {
        const instance = NetlibJS.instances[instanceId];
        NetlibJS.addEventListener(instance, "reconnecting", context, once, (peer) => {
            {{{ makeDynCall('viii', 'callback') }}}(context, once, stringToUTF8OnStack(peer.id));
        });
    },

    NetlibJS_Network_On_Reconnected: function (instanceId, callback, context, once) {
        const instance = NetlibJS.instances[instanceId];
        NetlibJS.addEventListener(instance, "reconnected", context, once, (peer) => {
            {{{ makeDynCall('viii', 'callback') }}}(context, once, stringToUTF8OnStack(peer.id));
        });
    },

    NetlibJS_Network_On_Disconnected: function (instanceId, callback, context, once) {
        const instance = NetlibJS.instances[instanceId];
        NetlibJS.addEventListener(instance, "disconnected", context, once, (peer) => {
            {{{ makeDynCall('viii', 'callback') }}}(context, once, stringToUTF8OnStack(peer.id));
        });
    },

    NetlibJS_Network_On_SignalingReconnected: function (instanceId, callback, context, once) {
        const instance = NetlibJS.instances[instanceId];
        NetlibJS.addEventListener(instance, "signalingreconnected", context, once, () => {
            {{{ makeDynCall('vii', 'callback') }}}(context, once);
        });
    },

    NetlibJS_Network_On_Failed: function (instanceId, callback, context, once) {
        const instance = NetlibJS.instances[instanceId];
        NetlibJS.addEventListener(instance, "failed", context, once, () => {
            {{{ makeDynCall('vii', 'callback') }}}(context, once);
        });
    },

    NetlibJS_Network_On_Message: function (instanceId, callback, context, once) {
        const instance = NetlibJS.instances[instanceId];
        NetlibJS.addEventListener(instance, "message", context, once, (peer, channel, data) => {
            const cPeerId = stringToUTF8OnStack(peer.id);
            const cChannel = NetlibJS.channelNameToEnum(channel);
            if (typeof data === 'string') {
                const bytes = stringToNewUTF8(data);
                const size = lengthBytesUTF8(data) + 1;
                {{{ makeDynCall('viiiiii', 'callback') }}}(context, once, cPeerId, cChannel, bytes, size);
            } else if (data instanceof Blob) {
                data.arrayBuffer().then((buffer) => {
                    const bytes = _malloc(buffer.byteLength);
                    HEAPU8.set(new Uint8Array(buffer), bytes);
                    const size = buffer.byteLength;
                    {{{ makeDynCall('viiiiii', 'callback') }}}(context, once, cPeerId, cChannel, bytes, size);
                }).catch((err) => {
                    throw err;
                });
            } else if (data instanceof ArrayBuffer || ArrayBuffer.isView(data)) {
                // console.log("arraybuffer", Array.apply([], new Uint8Array(data)).join(","));
                const bytes = _malloc(data.byteLength);
                HEAPU8.set(new Uint8Array(data), bytes);
                const size = data.byteLength;
                {{{ makeDynCall('viiiiii', 'callback') }}}(context, once, cPeerId, cChannel, bytes, size);
            } else {
                throw "Unsupported data type";
            }
        });
    },

    NetlibJS_Network_On_Close: function (instanceId, callback, context, once) {
        const instance = NetlibJS.instances[instanceId];
        NetlibJS.addEventListener(instance, "close", context, once, (reason) => {
            {{{ makeDynCall('viii', 'callback') }}}(context, once, typeof reason === 'string' ? stringToUTF8OnStack(reason) : 0);
        });
    },

    NetlibJS_Network_On_RTCError: function (instanceId, callback, context, once) {
        const instance = NetlibJS.instances[instanceId];
        NetlibJS.addEventListener(instance, "rtcerror", context, once, (event) => {
            const message = event.error ? event.error.toString() : event.type;
            {{{ makeDynCall('viii', 'callback') }}}(context, once, stringToUTF8OnStack(message));
        });
    },

    NetlibJS_Network_On_SignalingError: function (instanceId, callback, context, once) {
        const instance = NetlibJS.instances[instanceId];
        NetlibJS.addEventListener(instance, "signalingerror", context, once, (error) => {
            {{{ makeDynCall('viii', 'callback') }}}(context, once, stringToUTF8OnStack(error.message));
        });
    },

    NetlibJS_Network_Off: function (instanceId, event, context) {
        const instance = NetlibJS.instances[instanceId];
        const eventName = NetlibJS.eventNames[event];
        if (context !== 0) {
            NetlibJS.removeEventListener(instance, eventName, context, false);
        } else {
            NetlibJS.removeAllEventListeners(instance, eventName);
        }
    },

    //
    // Getters
    //

    NetlibJS_Network_GetPeers: function (instanceId) {
        const network = NetlibJS.instances[instanceId].network;
        const peers = {};
        network.peers.forEach((peer) => {
            peers[peer.id] = {
                id: peer.id,
                latency: {
                    last: peer.latency.last,
                    average: peer.latency.average,
                    jitter: peer.latency.jitter,
                    max: peer.latency.max,
                    min: peer.latency.min,
                },
            };
        });
        return stringToNewUTF8(JSON.stringify(peers));
    },

    NetlibJS_Network_GetId: function (instanceId) {
        const network = NetlibJS.instances[instanceId].network;
        return stringToUTF8OnStack(network.id);
    },

    NetlibJS_Network_GetClosing: function (instanceId) {
        const network = NetlibJS.instances[instanceId].network;
        return network.closing;
    },

    NetlibJS_Network_GetSize: function (instanceId) {
        const network = NetlibJS.instances[instanceId].network;
        return network.size;
    },

    NetlibJS_Network_GetCurrentLobby: function (instanceId) {
        const network = NetlibJS.instances[instanceId].network;
        const lobby = network.currentLobby;
        return typeof lobby === 'string' ? stringToUTF8OnStack(lobby) : 0;
    },

    NetlibJS_Network_GetCurrentLobbyInfo: function (instanceId) {
        const network = NetlibJS.instances[instanceId].network;
        const lobbyInfo = network.currentLobbyInfo;
        return lobbyInfo ? stringToNewUTF8(JSON.stringify(lobbyInfo)) : 0;
    },

    NetlibJS_Network_GetCurrentLeader: function (instanceId) {
        const network = NetlibJS.instances[instanceId].network;
        const leader = network.currentLeader;
        return typeof leader === 'string' ? stringToUTF8OnStack(leader) : 0;
    },
};

autoAddDeps(LibraryNetlibJS, "$NetlibJS");
mergeInto(LibraryManager.library, LibraryNetlibJS);