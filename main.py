#!/usr/bin/env python3

from aiohttp import web
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
            try:
                obj = await self.request.json()
            except json.JSONDecodeError as e:
                raise web.HTTPBadRequest from e
            await conn.execute(update(User).where(User.id == id).values(**obj))
            return web.json_response({})

    async def add_user(self):
        try:
            obj = await self.request.json()
        except json.JSONDecodeError as e:
            raise web.HTTPBadRequest from e
        async with self.request.app['engine'].acquire() as conn:
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
            try:
                obj = await self.request.json()
            except json.JSONDecodeError as e:
                raise web.HTTPBadRequest from e
            await conn.execute(update(Location).where(
                Location.id == id).values(**obj))
            return web.json_response({})

    async def add_location(self):
        try:
            obj = await self.request.json()
        except json.JSONDecodeError as e:
            raise web.HTTPBadRequest from e
        async with self.request.app['engine'].acquire() as conn:
            await conn.execute(insert(Location).values(**obj))
            return web.json_response({})

    async def get_location_avg(self, id):
        raise web.HTTPNotImplemented
        try:
            from_date = self.request.url.query.get('fromDate')
            if from_date:
                from_date = int(from_date)
            to_date = self.request.url.query.get('toDate')
            if to_date:
                to_date = int(to_date)
            from_age = self.request.url.query.get('fromAge')
            if from_age:
                from_age = int(from_age)
            to_age = self.request.url.query.get('toAge')
            if to_age:
                to_age = int(to_age)
            gender = self.request.url.query.get('gender')
        except ValueError as e:
            raise web.HTTPBadRequest from e
        async with self.request.app['engine'].acquire() as conn:
            query = select([Visit.mark]).where(Visit.location == id)
            if from_date:
                query = query.where(Visit.visited_at > from_date)
            if to_date:
                query = query.where(Visit.visited_at < to_date)
            if from_age or to_age or gender:
                expr = None
                if from_age:
                    # TODO : no idea of how to properly do it.
                    #  text() is not working, "can't adapt type 'TextClause'"
                    age = func.sum(func.current_timestamp() - User.birth_date)
                    expr = and_(expr, age > from_age) \
                        if expr else age > from_age
                if to_age:
                    age = func.sum(func.current_timestamp() - User.birth_date)
                    expr = and_(expr, age < to_age) \
                        if expr else age < to_age
                if gender:
                    expr = and_(expr, User.gender == gender)  # XXX doc unclear
                query = query.select_from(join(Visit, User, expr))
            print(query)
            await conn.execute(query)
            return web.json_response({'avg': 0})


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
            try:
                obj = await self.request.json()
            except json.JSONDecodeError as e:
                raise web.HTTPBadRequest from e
            await conn.execute(update(Visit).where(Visit.id == id).values(**obj))
            return web.json_response({})

    async def add_visit(self):
        try:
            obj = await self.request.json()
        except json.JSONDecodeError as e:
            raise web.HTTPBadRequest from e
        async with self.request.app['engine'].acquire() as conn:
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
