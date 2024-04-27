#!/bin/sh
set -e

cd /soccerwindow2 

bash /soccerwindow2/utils/appimage/build_code.sh
bash /soccerwindow2/utils/appimage/build_appimage.sh