#!/bin/bash
docker run -it --rm \
    -v $PWD:/flycast-dojo \
    -v /mnt/c/Users/glisi/Documents/Fightcade/emulator/flycast:/flycast:ro \
    -v /tmp/.X11-unix:/tmp/.X11-unix \
    -v /mnt/wslg:/mnt/wslg \
    -e DISPLAY \
    -e WAYLAND_DISPLAY \
    -e XDG_RUNTIME_DIR \
    -e PULSE_SERVER \
    --user 1000:1000 \
    flycast-dojo-build /bin/bash
