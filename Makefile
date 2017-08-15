
all: build

build:
	docker build -t highload .

.PHONY: all
