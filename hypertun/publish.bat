dotnet publish -r linux-arm --self-contained false
wsl rsync -avh ./bin/Debug/net6.0/linux-arm/publish/ pi@raspberrypi:/home/pi/hyperspace/hypertun --delete
