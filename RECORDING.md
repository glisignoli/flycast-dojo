# Flycast recording
This is my attempt to get flycast to record to a external video file.

  * I modified the linux Makefile to enable TEST_AUTOMATION 
  * I added a audiobackend called rawfile to write out the audio
  * I modified gles.cpp to write out rendered video frames

# Goal
The goal of this is to produce a build of flycast that can record flycast replays to a video file without the need for screen capture software. This would enable automated recording of replays that can be then uploaded to fightcadevids.com.

Ideally, this wouldn't need to run in real time. Eg: Even if recording a 5 minute replay takes 10 minute, all audio and video frames would still be captured and synced correctly.

# Status
Currently this fork is able to write raw auto out to a file, and RENDERED raw video frames.

Unfortunatly, even though it will write out all rendered frames, it has no concept of how long a frame needs to be repeated and saved to the video file.

For example:
  * MVC2 Game starts up
  * Naiomi logo is rendered
  * The 'NOW LOADING' frame is rendered once, but displayed until loading is finished
    * So in the video file, there will be one 'NOW LOADING' frame, but there should be multiple

# Building
Included is a dockerfile that can build the flycast elf

## Build the docker image:
Build the docker image with:

`docker build -t flycast-docker-build:latest .`

## Build the elf
Run `./docker-make.sh`

Then inside the container run `./build.sh`. This will build the elf. Then you can exit the container.

# Trying the build
You can try the build by running `./test-video.sh`

# Combining the video
You can combine the audio and video using ffmpeg:
```ffmpeg -framerate 59.4 -f rawvideo -pixel_format rgb24 -video_size 640x480 -i ./video.raw -i ./audio.raw -vf vflip output.mkv```
