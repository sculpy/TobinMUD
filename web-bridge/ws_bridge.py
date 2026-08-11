#!/usr/bin/env python3
"""TobinMUD web play-client bridge: accepts WebSocket connections (from
play.html, proxied through nginx as wss://tobinmud.com/play/ws) and pipes
their bytes to/from the live MUD on 127.0.0.1:4000. Browsers can't open raw
TCP/telnet sockets, so this is the shim that lets the in-browser terminal
talk to the same server the Windows client and telnet users connect to.

Bytes are passed through untouched in BOTH directions -- telnet IAC
negotiation, ANSI colour, GMCP/MSP markers and all -- the browser frontend
is responsible for telnet-stripping and ANSI rendering, exactly as a real
telnet client would be. One TCP connection per WebSocket; either side
closing tears down the other."""
import asyncio
import websockets

MUD_HOST = "127.0.0.1"
MUD_PORT = 4000
LISTEN_HOST = "127.0.0.1"   # nginx proxies to us; never exposed directly
LISTEN_PORT = 4001


async def handle(ws):
    try:
        reader, writer = await asyncio.open_connection(MUD_HOST, MUD_PORT)
    except Exception:
        await ws.close(code=1011, reason="MUD unreachable")
        return

    async def mud_to_ws():
        try:
            while True:
                data = await reader.read(4096)
                if not data:
                    break
                await ws.send(data)  # bytes -> binary frame
        except Exception:
            pass
        finally:
            try:
                await ws.close()
            except Exception:
                pass

    async def ws_to_mud():
        try:
            async for msg in ws:
                if isinstance(msg, str):
                    msg = msg.encode("utf-8", "replace")
                writer.write(msg)
                await writer.drain()
        except Exception:
            pass
        finally:
            try:
                writer.close()
            except Exception:
                pass

    await asyncio.gather(mud_to_ws(), ws_to_mud())


async def main():
    async with websockets.serve(handle, LISTEN_HOST, LISTEN_PORT,
                                max_size=None, ping_interval=30):
        await asyncio.Future()  # run forever


if __name__ == "__main__":
    asyncio.run(main())
