#ifndef NETLIB_EXTENSION_HPP
#define NETLIB_EXTENSION_HPP

#include <dmsdk/sdk.h> // Lua headers
#include <dmsdk/dlib/hashtable.h>

namespace NetlibExt
{
    typedef int InstancePtr;
    typedef int LuaRef;
    struct CallbackWithEvent
    {
        dmScript::LuaCallbackInfo* m_Callback;
        int m_Event;
    };
    typedef dmHashTable<LuaRef, CallbackWithEvent> LuaCallbacksMap;
    struct InstanceContext
    {
        InstancePtr m_Ptr; // Just for debugging, no real use.
        LuaCallbacksMap* m_Map;
    };
    typedef dmHashTable<InstancePtr, InstanceContext> InstanceContexts;

    enum Event
    {
        EVENT_FIRST                 = 1,
        EVENT_READY                 = 1,
        EVENT_LOBBY                 = 2,
        EVENT_LEADER                = 3,
        EVENT_LOBBY_UPDATED         = 4,
        EVENT_CONNECTING            = 5,
        EVENT_CONNECTED             = 6,
        EVENT_RECONNECTING          = 7,
        EVENT_RECONNECTED           = 8,
        EVENT_DISCONNECTED          = 9,
        EVENT_SIGNALING_RECONNECTED = 10,
        EVENT_FAILED                = 11,
        EVENT_MESSAGE               = 12,
        EVENT_CLOSE                 = 13,
        EVENT_RTC_ERROR             = 14,
        EVENT_SIGNALING_ERROR       = 15,
        EVENT_LAST                  = 15,
    };

    enum DataChannel
    {
        CHANNEL_RELIABLE   = 1,
        CHANNEL_UNRELIABLE = 2,
    };

    struct SetupLuaCallback
    {
        dmScript::LuaCallbackInfo* m_Callback;
        bool m_Once;

        SetupLuaCallback(dmScript::LuaCallbackInfo* callback, bool once)
        {
            if (!dmScript::IsCallbackValid(callback))
                return;

            if (!dmScript::SetupCallback(callback))
            {
                dmLogError("Failed to setup callback (has the calling script been destroyed?)");
                dmScript::DestroyCallback(callback);
                return;
            }

            m_Callback = callback;
            m_Once     = once;
        }

        bool Valid()
        {
            return m_Callback != NULL;
        }

        lua_State* GetLuaContext()
        {
            return dmScript::GetCallbackLuaContext(m_Callback);
        }

        void Call(lua_State* L, int nargs)
        {
            if (m_Callback)
            {
                dmScript::PCall(L, nargs + 1, 0);
            }
            else
            {
                lua_pop(L, nargs);
            }
        }

        ~SetupLuaCallback()
        {
            if (m_Callback)
            {
                dmScript::TeardownCallback(m_Callback);
                if (m_Once)
                {
                    dmScript::DestroyCallback(m_Callback);
                }
            }
        }
    };
} // namespace NetlibExt

#endif // NETLIB_EXTENSION_HPP
