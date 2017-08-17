#!/usr/bin/env python3

from aiohttp import web
from aiopg.sa import create_engine
import asyncio
import json
from sqlalchemy.sql import select

from models import User


class UsersView(web.View):
    async def get(self):
        tail = self.request.match_info['tail']
        if not tail:
            return await self.get_user()
        if tail == '/visits':
            return await self.get_user_visits()
        raise web.HTTPBadRequest

    async def post(self):
        tail = self.request.match_info['tail']
        if tail:
            raise web.HTTPBadRequest()
        id = self.request.match_info['id']
        if id == 'new':
            return await self.add_user()
        return await self.change_user(id)

    async def get_user(self):
        id_ = self.request.match_info['id']  # и тут, блин, закат солнца вручную
        return web.Response(text='get_user')

    async def change_user(self, id):
        return web.Response(text='change_user')

    async def add_user(self):
        return web.Response(text='add_user')

    async def get_user_visits(self):
        return web.Response(text='get_user_visits')


loop = asyncio.get_event_loop()
app = web.Application(loop=loop)

app.router.add_route('*', '/users/{id}{tail:.*}', UsersView)

app['engine'] = loop.run_until_complete(
    create_engine('dbname=default user=root'))

web.run_app(app, port=80)
