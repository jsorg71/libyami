#!/bin/sh

INSTALL_PATH=/opt/yami

export PKG_CONFIG_PATH=$INSTALL_PATH/lib/pkgconfig
export NOCONFIGURE=1

rm -r $INSTALL_PATH/*

#rm -f libdrm-2.4.74.tar.gz
#wget https://dri.freedesktop.org/libdrm/libdrm-2.4.74.tar.gz

#rm -f libva-1.7.3.tar.bz2
#rm -f libva-1.7.3.tar
#wget https://www.freedesktop.org/software/vaapi/releases/libva/libva-1.7.3.tar.bz2

#rm -f libva-intel-driver-1.7.3.tar.bz2
#rm -f libva-intel-driver-1.7.3.tar
#wget https://www.freedesktop.org/software/vaapi/releases/libva-intel-driver/libva-intel-driver-1.7.3.tar.bz2

rm -fr libdrm-2.4.74
tar -zxf libdrm-2.4.74.tar.gz
cd libdrm-2.4.74
./autogen.sh
./configure --prefix=$INSTALL_PATH
make
make install
cd ..

rm -fr libva-1.7.3
bunzip2 libva-1.7.3.tar.bz2
tar -xf libva-1.7.3.tar
cd libva-1.7.3
./configure --prefix=$INSTALL_PATH --disable-x11
make
make install
cd ..

rm libva-intel-driver-1.7.3
bunzip2 libva-intel-driver-1.7.3.tar.bz2
tar -xf libva-intel-driver-1.7.3.tar
cd libva-intel-driver-1.7.3
./configure --prefix=$INSTALL_PATH
make
make install
cd ..

git clone https://github.com/jsorg71/libyami.git
cd libyami
git checkout infinte_gop
./autogen.sh
./configure --prefix=$INSTALL_PATH --disable-x11
make
make install
cd ..

