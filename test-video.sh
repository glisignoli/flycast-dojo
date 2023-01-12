#!/bin/bash
./flycast -config dojo:Receiving=yes \
          -config dojo:ActAsServer=no \
          -config dojo:SpectatorIP=ggpo.fightcade.com \
          -config dojo:SpectatorPort=7007 \
          -config dojo:Quark=1673475376481-7706 \
          -config dojo:GameEntry=mvsc2 \
          -config dojo:Enable=yes \
          -config dojo:EnableMatchCode=no \
          -config dojo:RxFrameBuffer=180 \
          -config record:rawaudio=audio.raw \
          -config record:rawvid=video.raw
