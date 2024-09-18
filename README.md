# The Poki Networking Library for Defold - ALPHA

[The Poki Networking Library](https://github.com/poki/netlib) is a peer-to-peer library for web games, using WebRTC datachannels to provide UDP connections between players directly. Like the Steam Networking Library, but for web. Netlib tries to make WebRTC as simple to use as the WebSocket interface (for game development).

This is a native extension for [the Defold game engine](https://defold.com/) that allows you to use Netlib in your Defold HTML5 game.

> [!WARNING]
> This extension is still under heavy development and considered in alpha. The API can change without warning. It closely follows the original Netlib, which is in alpha too.

## Setup

First add this extension as a dependency to your `game.project`:

    https://github.com/indiesoftby/defold-poki-netlib/archive/main.zip

Then you can create a Network instance in your script:

```lua
function init(self)
    -- Any random UUID is a valid game-id, but if your game is hosted at Poki you should use Poki's game-id.
    local instance = netlib.new("33c4c5a6-ee70-4726-aa1f-ced8a9578254")
    -- We'll use this reference later to close the network
    self.network = instance

    netlib.on(instance, netlib.EVENT_READY, function(self)
        print("Netlib is ready!")
        -- Need to create a lobby (e.g the player clicks on 'create game')
        netlib.create(instance)

        -- And the other client can join the lobby
        -- network.join(code)

        -- Make sure to call create() or join() only when the Network is 'ready'
    end)
    netlib.on(instance, netlib.EVENT_LOBBY, function(self, code, lobby_info)
        pprint("Lobby is created", code, lobby_info)
    end)
    netlib.on(instance, netlib.EVENT_MESSAGE, function(self, peer_id, channel, data)
        print("Got", #data, "bytes from", peer_id, "via", channel, "channel")

        -- Schedule a step of the Defold engine (read the explanation below)
        netlib.engine_step(0.05)
    end)
    netlib.on(instance, netlib.EVENT_CONNECTING, function(self, peer_id)
        -- Schedule a step of the Defold engine (read the explanation below)
        netlib.engine_step(0.05)
    end)
    netlib.on(instance, netlib.EVENT_CONNECTED, function(self, peer_id)
        print("New peer connected", peer_id)
        -- Send a message to the peer
        netlib.send(instance, netlib.CHANNEL_RELIABLE, peer_id, "Welcome!")
    end)
    netlib.on(instance, netlib.EVENT_DISCONNECTED, function(self, peer_id)
        print("Peer disconnected", peer_id)
    end)
end

function final(self)
    netlib.close(self.network)
end
```

## Tips & Tricks

### Force a step of the engine

Web game engines, including Defold, use the browser's `requestAnimationFrame()` function to organize the game loop. When the page loses focus (e.g. the player is distracted by something - for example, sending a link to a friend), browsers throttle the rAF function to 0 frames per second to save PC resources. Therefore, the game loop stops working on inactive browser tabs until the focus is restored. If this happens in a P2P networked game on the "server" side, other players will stop receiving updates to the game world state. While an inactive web page continues to receive messages from the network, which can be used to update the game world state.

So, to bypass this limitation, call the `netlib.engine_step(min_delta)` function when receiving messages from the network to schedule a game loop step. `min_delta` is the minimum time between frames, in seconds. In our projects, we use a value of 0.05 seconds.

## Roadmap

- [ ] Add configuration for `rtcConfig` and `signalingURL`, as in the original `netlib`.
- [ ] Include `netlib.js` in the final game build, rather than as a separate file.
- [ ] Add a test collection to verify `netlib` functionality.
- [ ] Write an example of how `netlib` can be used in a game.
- [ ] Add auto-documentation based on the Script API.
- [ ] Implement mock of `netlib` to test game logic locally.

## License

This project is licensed under the ISC License. See the LICENSE file for details.
