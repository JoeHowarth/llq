#!/bin/sh

# Check if vcpkg is already installed
if [ ! -d "./vcpkg" ]; then
    echo "Cloning vcpkg..."
    git clone https://github.com/microsoft/vcpkg.git
else
    echo "vcpkg already exists, pulling the latest changes..."
    cd vcpkg
    git pull
    cd ..
fi

# Bootstrapping vcpkg
echo "Bootstrapping vcpkg..."
./vcpkg/bootstrap-vcpkg.sh

# Install dependencies from vcpkg.json
echo "Installing dependencies..."
./vcpkg/vcpkg install

echo "vcpkg and dependencies installed successfully!"

# cmake -S . -B build
# cmake --build build
