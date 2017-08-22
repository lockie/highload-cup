#!/usr/bin/env python3

from sqlalchemy import ForeignKey, Column, String, Integer, SmallInteger
from sqlalchemy.ext.declarative import declarative_base
import sqlalchemy.types as types


Base = declarative_base()


class User(Base):
    __tablename__ = 'users'
    # я уже и забыл, какой всё-таки это закат солнца вручную, эта ваша алхимия
    id = Column(Integer, primary_key=True)
    email = Column(String(100))
    first_name = Column(String(50))
    last_name = Column(String(50))
    gender = Column(types.CHAR(1))
    birth_date = Column(Integer)


class Location(Base):
    __tablename__ = 'locations'
    id = Column(Integer, primary_key=True)
    place = Column(String)
    country = Column(String(50))
    city = Column(String(50))
    distance = Column(Integer)


class Visit(Base):
    __tablename__ = 'visits'
    id = Column(Integer, primary_key=True)
    location = Column(Integer, ForeignKey('locations.id'))
    user = Column(Integer, ForeignKey('users.id'))
    visited_at = Column(Integer)
    mark = Column(SmallInteger)
