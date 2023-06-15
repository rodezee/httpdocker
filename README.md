# httpdocker
Serve containers via a http


# Usage
install [docker](https://docs.docker.com/get-docker/) and run:
```
docker compose up
```
open the browser on http://localhost:8000 or use curl:
```
curl -d '{"Image": "rodezee/hello-world:0.0.1"}' http://localhost:8000
```
or a little more advanced:
```
curl -d '{"Image": "rodezee/print-env:0.0.1", "Env": ["FOO=1", "BAR=2"]}' http://localhost:8000
```
see the [docker SDK container create](https://docs.docker.com/engine/api/v1.43/#tag/Container/operation/ContainerCreate) for more API information.
