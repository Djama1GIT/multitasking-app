.PHONY: build-docker-compose run-docker-compose run-multitasking-client build-multitasking-client build run-servers run-client rs rc

build-docker-compose:
	docker-compose build

run-docker-compose:
	docker-compose up

build-multitasking-client:
	cd client && docker build -t multitasking-client .

run-multitasking-client:
	docker run -it -e term=xterm-256color --network=multitasking-app_multitasking-app multitasking-client

build: build-docker-compose build-multitasking-client

run-servers: run-docker-compose

run-client: run-multitasking-client

rs: run-servers

rc: run-client