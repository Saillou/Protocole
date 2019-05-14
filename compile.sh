git pull
g++ -o Server -std=gnu++17 Sources/mainServer.cpp Sources/Device/Device.cpp -lpthread
# g++ -o Client -std=gnu++17 Sources/mainClient.cpp -lpthread

./Server