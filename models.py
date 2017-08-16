#!/usr/bin/env python3

from sqlalchemy import ForeignKey, Column, Integer, String
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
