FROM python:alpine3.6

COPY . /usr/src/app
WORKDIR /usr/src/app

RUN pip install --no-cache-dir -r /usr/src/app/requirements.txt

EXPOSE 80

CMD python3 main.py
