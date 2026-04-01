#!/bin/bash

# 1. Detect OS
OS="$(uname)"
if [ "$OS" == "Linux" ]; then
    if [ -f /etc/arch-release ] || grep -q "ID=cachyos" /etc/os-release; then
        # Arch / CachyOS
        echo "Detected Arch-based distro (CachyOS)."
        # Adding nlohmann-json here so you don't need to manage the header manually
        sudo pacman -S --needed base-devel glu glew freetype2 glfw-x11 mesa git nlohmann-json soil
    elif [ -f /etc/lsb-release ] || [ -f /etc/debian_version ]; then
        # Ubuntu / Debian
        sudo apt-get update
        sudo apt-get install -y build-essential libglu1-mesa-dev libglew-dev libfreetype6-dev libglfw3-dev mesa-common-dev git nlohmann-json-dev
    elif [ -f /etc/fedora-release ]; then
        # Fedora
        sudo dnf install -y mesa-libGLU-devel glew-devel freetype-devel glfw-devel mesa-libGL-devel git nlohmann-json-devel
    fi
elif [ "$OS" == "Darwin" ]; then
    # macOS
    if ! command -v brew &> /dev/null; then
        echo "Homebrew not found. Please install it at https://brew.sh/"
        exit 1
    fi
    brew install glew freetype glfw mesa git nlohmann-json
fi

# 2. Setup External Libraries (Optional if using system packages, but good for portability)
mkdir -p external && cd external

# 3. Clone SOIL (Only if not found via pacman)
if [ ! -d "soil" ]; then
    git clone https://github.com/littlstar/soil.git
fi

echo "Dependencies are ready for $OS!"