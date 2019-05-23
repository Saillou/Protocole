echo " ---- Git ----"
git pull

echo " ---- Compile ----"
g++ -o Server -std=gnu++17 Sources/mainServer.cpp Sources/Device/Device.cpp -lpthread

echo " ---- Launch ----"
./Server 640 480