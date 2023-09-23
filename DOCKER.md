## Docker

A Docker image is available at https://hub.docker.com/r/pvtom/s10m

### Configuration

Create a `.config` file as described in the [Readme](README.md).

### Start the docker container

```
docker run --rm -v /path/to/your/.config:/app/.config pvtom/s10m:latest
```

Thanks to [felix1507](https://github.com/felix1507/s10m-docker) for developing the Dockerfile!
