# httpdocker
Serve containers via a httpd


# Usage
install docker and run:
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
