CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Werror
LDFLAGS = -lfuse3

SRC = src/main.cpp src/vfs.cpp
TARGET = kubsh

PKG_DIR = kubsh-package
PKG_BIN = $(PKG_DIR)/usr/bin
PKG_DEBIAN = $(PKG_DIR)/DEBIAN
DEB_NAME = kubsh_1.0_amd64.deb

.PHONY: build run deb clean

# 1) Компиляция из исходников
build: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

# 2) Запуск программы
run: $(TARGET)
	./$(TARGET)

# 3) Сборка deb-пакета
deb: build
	@echo "==> Preparing package directory"
	mkdir -p $(PKG_BIN)
	mkdir -p $(PKG_DEBIAN)
	cp $(TARGET) $(PKG_BIN)/
	chmod 755 $(PKG_BIN)/$(TARGET)
	
	@echo "==> Creating control file"
	echo "Package: kubsh" > $(PKG_DEBIAN)/control
	echo "Version: 1.0" >> $(PKG_DEBIAN)/control
	echo "Architecture: amd64" >> $(PKG_DEBIAN)/control
	echo "Maintainer: Shell Developer <dev@example.com>" >> $(PKG_DEBIAN)/control
	echo "Description: Custom shell with VFS support" >> $(PKG_DEBIAN)/control
	echo " A C++ shell implementation with FUSE VFS for user management." >> $(PKG_DEBIAN)/control

	@echo "==> Building .deb package"
	dpkg-deb --build $(PKG_DIR) $(DEB_NAME)
	@echo "==> Done: $(DEB_NAME)"

# Очистка
clean:
	rm -f $(TARGET)
	rm -f kubsh_*.deb
	rm -rf $(PKG_DIR)
