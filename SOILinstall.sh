#!/bin/bash

# 1. Detect OS
OS="$(uname)"
if [ "$OS" == "Linux" ]; then
    if [ -f /etc/arch-release ]; then
        # Arch / CachyOS
        sudo pacman -S --needed base-devel glu glew freetype2 glfw-x11 mesa git
    elif [ -f /etc/lsb-release ] || [ -f /etc/debian_version ]; then
        # Ubuntu / Debian
        sudo apt-get update
        sudo apt-get install -y build-essential libglu1-mesa-dev libglew-dev libfreetype6-dev libglfw3-dev mesa-common-dev git
    elif [ -f /etc/fedora-release ]; then
        # Fedora
        sudo dnf install -y mesa-libGLU-devel glew-devel freetype-devel glfw-devel mesa-libGL-devel git
    fi
elif [ "$OS" == "Darwin" ]; then
    # macOS
    if ! command -v brew &> /dev/null; then
        echo "Homebrew not found. Please install it at https://brew.sh/"
        exit 1
    fi
    brew install glew freetype glfw mesa git
fi

# 2. Setup External Libraries
mkdir -p external && cd external

# 3. Clone and Build SOIL
if [ ! -d "soil" ]; then
    git clone https://github.com/littlstar/soil.git
    cd soil
    mkdir -p obj
    # Note: We won't run 'make' here because we are compiling 
    # SOIL sources directly in our project Makefile as discussed.
    cd ..
fi

echo "Dependencies and SOIL sources are ready for $OS!"