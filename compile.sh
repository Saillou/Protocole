echo " ---- Git ----"
git pull

echo " ---- Compile ----"
g++ -o Server -std=gnu++17 \
Sources/mainServer.cpp Sources/Device/Device.cpp  \
-I/opt/lib/libjpeg-turbo/include  \
-L/opt/lib/libjpeg-turbo/lib64  \
-lpthread -lturbojpeg -lopenh264

echo " ---- Launch ----"
./Server