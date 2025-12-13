CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Werror
LDFLAGS = -lfuse3

SRC = src/main.cpp src/vfs.cpp
TARGET = kubsh

PKG_DIR = kubsh-package
PKG_BIN = $(PKG_DIR)/usr/bin
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
	cp $(TARGET) $(PKG_BIN)/
	chmod 755 $(PKG_BIN)/$(TARGET)

	@echo "==> Building .deb package"
	dpkg-deb --build $(PKG_DIR) $(DEB_NAME)
	@echo "==> Done: $(DEB_NAME)"

# Очистка
clean:
	rm -f $(TARGET)
	rm -f kubsh_*.deb
