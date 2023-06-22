# httpdocker
Serve containers via a http


# Usage
install [docker](https://docs.docker.com/get-docker/) and run:
```
docker compose up
```
or build local (this example is on alpine linux):
```
./alpine-linux-install.sh
make build && ./httpdocker -d www
```
open the browser on http://localhost:8000

# .httpd
all files with the .httpd extension will be picked up by the webserver and executed accordingly.
example the file http://localhost:8000/others/hello-podman.htmld contains:
```
{
    "Image": "quay.io/podman/hello:latest"
}
```
(open it in your browser and see the outcome!)

# API
if you like you can activate the API by starting:
```
./httpdocker -a yes
```
after that you can use curl to test some calls: 
```
curl -d '{"Image": "rodezee/hello-world:0.0.1"}' http://localhost:8000
```
or a little more advanced:
```
curl -d '{"Image": "rodezee/print-env:0.0.1", "Env": ["FOO=1", "BAR=2"]}' http://localhost:8000
```
see the [docker SDK container create](https://docs.docker.com/engine/api/v1.43/#tag/Container/operation/ContainerCreate) for more API information.
