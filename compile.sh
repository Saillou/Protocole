echo " ---- Git ----"
git pull

echo " ---- Compile ----"
g++ -o Server -std=gnu++17 \
Sources/mainServer.cpp Sources/Device/Device.cpp  \
-I/opt/libjpeg-turbo/include  \
-L/opt/libjpeg-turbo/lib64  \
-lpthread -lturbojpeg -lopenh264

echo " ---- Launch ----"
./Server