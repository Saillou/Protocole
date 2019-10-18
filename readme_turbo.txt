# Remove existing jpeg library, you can leave libjpeg62-turbo as it doesn't seem to confict
sudo apt-get remove libjpeg8-dev
sudo apt-get remove libjpeg8

# Get source and extract
wget http://downloads.sourceforge.net/libjpeg-turbo/libjpeg-turbo-1.5.1.tar.gz
tar -xzf libjpeg-turbo-1.5.1.tar.gz libjpeg-turbo-1.5.1/
cd libjpeg-turbo-1.5.1/

# Configure make
# See http://www.linuxfromscratch.org/blfs/view/svn/general/libjpeg.html
# I needed --with-jpeg8 for mjpeg streamer, I'm not sure if the other options are necessary
./configure --prefix=/usr           \
            --mandir=/usr/share/man \
            --with-jpeg8            \
            --disable-static        \
            --docdir=/usr/share/doc/libjpeg-turbo-1.5.1

# Make, takes about 5 minutes
make -j4

# Create a deb so you can install it as a package for easy removal, or you can use a make install
make deb

# Install package, force architecture was required for some reason I cannot explain
sudo dpkg -i --force-architecture libjpeg-turbo_1.5.1_armv7l.deb
