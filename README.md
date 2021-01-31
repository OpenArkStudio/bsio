# bsio
better asio (++asio)

# compiler
- gcc 9.3 in tested
- msvc 16 in tested (visual studio 2019)

# build example
```bash
cmake .
make
```
# only install bsio
```bash
cmake . -Dbsio_BUILD_EXAMPLES=OFF
sudo make install
```
# install asio
- Ubuntu
```bash
sudo apt-get install -y libasio-dev
```

- CentOS
```bash
yum install -y asio-devel
```