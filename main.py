#!/usr/bin/env python3

from aiohttp import web
from aiopg.sa import create_engine
import asyncio
import json
from sqlalchemy.sql import (insert, select, update, exists, join, func,
                            text, and_)
from models import User, Location, Visit


# TODO : REFACTOR!
class APIMixin:
    def get_id(self):
        try:
            # и тут, блин, закат солнца вручную
            return int(self.request.match_info['id'])
        except ValueError as e:
            raise web.HTTPNotFound from e

    async def get_object(self):
        try:
            obj = await self.request.json()
        except json.JSONDecodeError as e:
            raise web.HTTPBadRequest from e
        if None in obj.values():
            raise web.HTTPBadRequest
        return obj


class UsersView(APIMixin, web.View):
    async def get(self):
        id = self.get_id()
        tail = self.request.match_info['tail']
        if not tail:
            return await self.get_user(id)
        if tail == '/visits':
            return await self.get_user_visits(id)
        raise web.HTTPBadRequest

    async def post(self):
        tail = self.request.match_info['tail']
        if tail:
            raise web.HTTPBadRequest
        if self.request.match_info['id'] == 'new':
            return await self.add_user()
        return await self.change_user(self.get_id())

    async def get_user(self, id):
        async with self.request.app['engine'].acquire() as conn:
            row = await conn.execute(select([User]).where(User.id == id))
            user = await row.first()
            if not user:
                raise web.HTTPNotFound
            return web.json_response(dict(zip(user, user.as_tuple())))

    async def change_user(self, id):
        async with self.request.app['engine'].acquire() as conn:
            row = await conn.execute(select([exists().where(User.id == id)]))
            exists_ = await row.scalar()
            if not exists_:
                raise web.HTTPNotFound
            obj = await self.get_object()
            await conn.execute(update(User).where(User.id == id).values(**obj))
            return web.json_response({})

    async def add_user(self):
        obj = await self.get_object()
        async with self.request.app['engine'].acquire() as conn:
            row = await conn.execute(select([exists().where(User.id == obj.get(id, 0))]))
            exists_ = await row.scalar()
            if exists_:
                raise web.HTTPBadRequest

            await conn.execute(insert(User).values(**obj))
            return web.json_response({})

    async def get_user_visits(self, id):
        try:
            from_date = self.request.url.query.get('fromDate')
            if from_date is not None:
                from_date = int(from_date)
            to_date = self.request.url.query.get('toDate')
            if to_date is not None:
                to_date = int(to_date)
            country = self.request.url.query.get('country')
            to_distance = self.request.url.query.get('toDistance')
            if to_distance is not None:
                to_distance = int(to_distance)
        except ValueError as e:
            raise web.HTTPBadRequest from e
        async with self.request.app['engine'].acquire() as conn:
            row = await conn.execute(select([exists().where(User.id == id)]))
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
                query = query.select_from(join(Visit, Location,
                                               Location.country == country))
            if to_distance is not None:
                query = query.where(Location.distance < to_distance)
            rows = await conn.execute(query.order_by(Visit.visited_at))
            visits = await rows.fetchall()
            return web.json_response(
                {'visits':
                 [dict(zip(visit, visit.as_tuple())) for visit in visits]})


class LocationsView(APIMixin, web.View):
    async def get(self):
        id = self.get_id()
        tail = self.request.match_info['tail']
        if not tail:
            return await self.get_location(id)
        if tail == '/avg':
            return await self.get_location_avg(id)
        raise web.HTTPBadRequest

    async def post(self):
        tail = self.request.match_info['tail']
        if tail:
            raise web.HTTPBadRequest
        if self.request.match_info['id'] == 'new':
            return await self.add_location()
        return await self.change_location(self.get_id())

    async def get_location(self, id):
        async with self.request.app['engine'].acquire() as conn:
            row = await conn.execute(select([Location]).where(
                Location.id == id))
            location = await row.first()
            if not location:
                raise web.HTTPNotFound
            return web.json_response(dict(zip(location, location.as_tuple())))

    async def change_location(self, id):
        async with self.request.app['engine'].acquire() as conn:
            row = await conn.execute(select([exists().where(
                Location.id == id)]))
            exists_ = await row.scalar()
            if not exists_:
                raise web.HTTPNotFound
            obj = await self.get_object()
            await conn.execute(update(Location).where(
                Location.id == id).values(**obj))
            return web.json_response({})

    async def add_location(self):
        obj = await self.get_object()
        async with self.request.app['engine'].acquire() as conn:
            row = await conn.execute(select([exists().where(Location.id == obj.get('id', 0))]))
            exists_ = await row.scalar()
            if exists_:
                raise web.HTTPBadRequest

            await conn.execute(insert(Location).values(**obj))
            return web.json_response({})

    async def get_location_avg(self, id):
        try:
            from_date = self.request.url.query.get('fromDate')
            if from_date is not None:
                from_date = int(from_date)
            to_date = self.request.url.query.get('toDate')
            if to_date is not None:
                to_date = int(to_date)
            from_age = self.request.url.query.get('fromAge')
            if from_age is not None:
                from_age = int(from_age)
            to_age = self.request.url.query.get('toAge')
            if to_age is not None:
                to_age = int(to_age)
            gender = self.request.url.query.get('gender')
            if gender is not None and gender not in ['m', 'f']:
                raise web.HTTPBadRequest
        except ValueError as e:
            raise web.HTTPBadRequest from e
        async with self.request.app['engine'].acquire() as conn:
            row = await conn.execute(select([exists().where(Location.id == id)]))
            exists_ = await row.scalar()
            if not exists_:
                raise web.HTTPNotFound

            query = select([func.avg(Visit.mark)]).where(Visit.location == id)
            if from_date is not None:
                query = query.where(Visit.visited_at > from_date)
            if to_date is not None:
                query = query.where(Visit.visited_at < to_date)
            if from_age is not None or to_age is not None or gender is not None:
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
            return web.json_response({'avg': float(round(avg, 5).normalize())})


class VisitsView(APIMixin, web.View):
    async def get(self):
        id = self.get_id()
        tail = self.request.match_info['tail']
        if tail:
            raise web.HTTPBadRequest
        return await self.get_visit(id)

    async def post(self):
        tail = self.request.match_info['tail']
        if tail:
            raise web.HTTPBadRequest
        if self.request.match_info['id'] == 'new':
            return await self.add_visit()
        return await self.change_visit(self.get_id())

    async def get_visit(self, id):
        async with self.request.app['engine'].acquire() as conn:
            row = await conn.execute(select([Visit]).where(Visit.id == id))
            visit = await row.first()
            if not visit:
                raise web.HTTPNotFound
            return web.json_response(dict(zip(visit, visit.as_tuple())))

    async def change_visit(self, id):
        async with self.request.app['engine'].acquire() as conn:
            row = await conn.execute(select([exists().where(Visit.id == id)]))
            exists_ = await row.scalar()
            if not exists_:
                raise web.HTTPNotFound
            obj = await self.get_object()
            await conn.execute(update(Visit).where(Visit.id == id).values(**obj))
            return web.json_response({})

    async def add_visit(self):
        obj = await self.get_object()
        async with self.request.app['engine'].acquire() as conn:
            row = await conn.execute(select([exists().where(Visit.id == obj.get('id', 0))]))
            exists_ = await row.scalar()
            if exists_:
                raise web.HTTPBadRequest

            await conn.execute(insert(Visit).values(**obj))
            return web.json_response({})


loop = asyncio.get_event_loop()
app = web.Application(loop=loop)

app.router.add_route('*', '/users/{id}{tail:.*}', UsersView)
app.router.add_route('*', '/locations/{id}{tail:.*}', LocationsView)
app.router.add_route('*', '/visits/{id}{tail:.*}', VisitsView)

app['engine'] = loop.run_until_complete(
    create_engine('dbname=default user=root', minsize=16, maxsize=16))

web.run_app(app, port=80)
