dotnet publish -r linux-arm --self-contained false
@REM dotnet publish -r linux-arm /p:PublishSingleFile=true /p:IncludeNativeLibrariesForSelfExtract=true --self-contained true
wsl rsync -avh ./bin/Debug/net6.0/linux-arm/publish/ pi@raspberrypi:/home/pi/hyperspace/border-router --delete
