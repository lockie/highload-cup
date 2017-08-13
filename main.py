#!/usr/bin/env python3

from aiohttp import web

async def handle(request):
    return web.Response(text='O hai')

app = web.Application()
app.router.add_get('/', handle)

web.run_app(app, port=80)
