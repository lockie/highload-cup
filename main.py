#!/usr/bin/env python3

from aiohttp import web
import aiomcache
from aiopg.sa import create_engine
import asyncio
import json
from sqlalchemy.sql import (insert, select, update, exists, join, func,
                            text, and_)
from models import User, Location, Visit


class APIMixin:
    def get_id(self):
        try:
            # и тут, блин, закат солнца вручную
            return int(self.request.match_info['id'])
        except ValueError as e:
            raise web.HTTPNotFound from e

    def cache_key(self, id):
        return (self.model.__name__ + str(id)).encode('utf8')

    def deserialize(self, obj):
        return dict(zip(obj, obj.as_tuple()))

    def get_int_param(self, name):
        param = self.request.url.query.get(name)
        if param is not None:
            try:
                param = int(param)
            except ValueError as e:
                raise web.HTTPBadRequest from e
        return param

    async def get_object(self):
        try:
            obj = await self.request.json()
        except json.JSONDecodeError as e:
            raise web.HTTPBadRequest from e
        if None in obj.values():
            raise web.HTTPBadRequest
        return obj

    async def get_instance(self, id):
        cached = await self.request.app['memcache'].get(self.cache_key(id))
        if cached:
            return web.Response(body=cached,
                                content_type='application/json',
                                charset='utf-8')
        else:
            async with self.request.app['engine'].acquire() as conn:
                row = await conn.execute(
                    select([self.model]).where(self.model.id == id))
                obj = await row.first()
                if not obj:
                    raise web.HTTPNotFound
                response = web.json_response(self.deserialize(obj))
                await self.request.app['memcache'].set(self.cache_key(id),
                                                       response.body)
                return response

    async def get_method(self, id, method):
        raise web.HTTPBadRequest

    async def add_instance(self):
        obj = await self.get_object()
        async with self.request.app['engine'].acquire() as conn:
            row = await conn.execute(
                select([exists().where(self.model.id == obj.get(id, 0))]))
            exists_ = await row.scalar()
            if exists_:
                raise web.HTTPBadRequest
            row = await conn.execute(insert(
                self.model, returning=self.model.__table__.c).values(**obj))
            new = await row.first()
            await self.request.app['memcache'].set(
                self.cache_key(new['id']),
                json.dumps(self.deserialize(new)).encode('utf8'))
            return web.json_response({})

    async def change_instance(self, id):
        async with self.request.app['engine'].acquire() as conn:
            row = await conn.execute(
                select([exists().where(self.model.id == id)]))
            exists_ = await row.scalar()
            if not exists_:
                raise web.HTTPNotFound
            obj = await self.get_object()
            row = await conn.execute(update(
                    self.model, returning=self.model.__table__.c
                ).where(self.model.id == id).values(**obj))
            changed = await row.first()
            await self.request.app['memcache'].set(
                self.cache_key(id),
                json.dumps(self.deserialize(changed)).encode('utf8'))
            return web.json_response({})

    async def get(self):
        id = self.get_id()
        tail = self.request.match_info['tail']
        if not tail:
            return await self.get_instance(id)
        result = await self.get_method(id, tail[1:])
        if result is None:
            raise web.HTTPBadRequest
        return result

    async def post(self):
        tail = self.request.match_info['tail']
        if tail:
            raise web.HTTPBadRequest
        if self.request.match_info['id'] == 'new':
            return await self.add_instance()
        return await self.change_instance(self.get_id())


class UsersView(APIMixin, web.View):
    model = User

    async def get_method(self, id, method):
        if method == 'visits':
            from_date = self.get_int_param('fromDate')
            to_date = self.get_int_param('toDate')
            country = self.request.url.query.get('country')
            to_distance = self.get_int_param('toDistance')
            async with self.request.app['engine'].acquire() as conn:
                row = await conn.execute(
                    select([exists().where(User.id == id)]))
                exists_ = await row.scalar()
                if not exists_:
                    raise web.HTTPNotFound
                query = select([Visit.mark, Visit.visited_at, Location.place]
                              ).where(and_(
                                  Visit.user == id,
                                  Visit.location == Location.id))
                if from_date is not None:
                    query = query.where(Visit.visited_at > from_date)
                if to_date is not None:
                    query = query.where(Visit.visited_at < to_date)
                if country is not None:
                    # I have no idea what I'm doing.jpg
                    query = query.select_from(
                        join(Visit, Location, Location.country == country))
                if to_distance is not None:
                    query = query.where(Location.distance < to_distance)
                rows = await conn.execute(query.order_by(Visit.visited_at))
                visits = await rows.fetchall()
                return web.json_response(
                    {'visits':
                     [self.deserialize(visit) for visit in visits]})


class LocationsView(APIMixin, web.View):
    model = Location

    async def get_method(self, id, method):
        if method == 'avg':
            from_date = self.get_int_param('fromDate')
            to_date = self.get_int_param('toDate')
            from_age = self.get_int_param('fromAge')
            to_age = self.get_int_param('toAge')
            gender = self.request.url.query.get('gender')
            if gender is not None and gender not in ['m', 'f']:
                raise web.HTTPBadRequest
            async with self.request.app['engine'].acquire() as conn:
                row = await conn.execute(
                    select([exists().where(Location.id == id)]))
                exists_ = await row.scalar()
                if not exists_:
                    raise web.HTTPNotFound

                query = select([func.avg(Visit.mark)]
                               ).where(Visit.location == id)
                if from_date is not None:
                    query = query.where(Visit.visited_at > from_date)
                if to_date is not None:
                    query = query.where(Visit.visited_at < to_date)
                if from_age is not None or to_age is not None or \
                   gender is not None:
                    query = query.select_from(join(
                        Visit, User, Visit.user == User.id))
                    # see https://stackoverflow.com/a/10258706/1336774
                    if from_age is not None:
                        query = query.where(
                            func.to_timestamp(User.birth_date) < func.now() -
                            text("'%d years'::interval" % from_age))
                    if to_age is not None:
                        query = query.where(
                            func.to_timestamp(User.birth_date) > func.now() -
                            text("'%d years'::interval" % to_age))
                    if gender is not None:
                        query = query.where(User.gender == gender)
                row = await conn.execute(query)
                avg = await row.scalar()
                if avg is None:
                    return web.json_response({'avg': 0.0})
                return web.json_response({'avg':
                                          float(round(avg, 5).normalize())})


class VisitsView(APIMixin, web.View):
    model = Visit


loop = asyncio.get_event_loop()
app = web.Application(loop=loop)

app.router.add_route('*', '/users/{id}{tail:.*}', UsersView)
app.router.add_route('*', '/locations/{id}{tail:.*}', LocationsView)
app.router.add_route('*', '/visits/{id}{tail:.*}', VisitsView)

app['engine'] = loop.run_until_complete(
    create_engine('dbname=default user=root', minsize=63, maxsize=63))
app['memcache'] = aiomcache.Client('127.0.0.1', pool_size=990, loop=loop)

web.run_app(app, port=80)
