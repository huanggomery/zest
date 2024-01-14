#!/bin/bash

# Array containing the list of header files to copy
header_base_files=(
    "zest/base/noncopyable.h"
    "zest/base/fix_buffer.h"
    "zest/base/logging.h"
)
header_net_files=(
    "zest/net/tcp_server.h"
    "zest/net/tcp_client.h"
    "zest/net/tcp_connection.h"
    "zest/net/base_addr.h"
    "zest/net/inet_addr.h"
)

# Flag to check if copy operation fails
copy_failed=false

# Run xmake to build the project
xmake

# Check if xmake build succeeded
if [ $? -eq 0 ]; then
    # Check if the user provided a destination path for the library
    if [ $# -eq 0 ]; then
        # Create the directory if it doesn't exist
        sudo mkdir -p /usr/local/lib/
        sudo mkdir -p /usr/local/include/zest/base/
        sudo mkdir -p /usr/local/include/zest/net/

        # If no path is provided, copy the generated static library to the default /usr/local/lib using sudo
        sudo cp ./lib/libzest.a /usr/local/lib/
        echo "Library copied to the default path /usr/local/lib/"

        # If no path is provided, copy the necessary headers to the default /usr/local/include/zest/ using sudo
        for file in "${header_base_files[@]}"; do
            if sudo cp -r "$file" /usr/local/include/zest/base/; then
                echo "Copied $file successfully"
            else
                echo "Failed to Copy $file"
                copy_failed=true
            fi
        done

        for file in "${header_net_files[@]}"; do
            if sudo cp -r "$file" /usr/local/include/zest/net/; then
                echo "Copied $file successfully"
            else
                echo "Failed to Copy $file"
                copy_failed=true
            fi
        done
        echo "Headers copied to the default path /usr/local/include/zest/"
    else
        # Create the directory if it doesn't exist
        sudo mkdir -p "$1/lib/"
        sudo mkdir -p "$1/include/zest/base/"
        sudo mkdir -p "$1/include/zest/net/"

        # If a path is provided, copy the generated static library to the specified path using sudo
        sudo cp ./lib/libzest.a "$1/lib/"
        echo "Library copied to the specified path: $1/lib/"

        # If a path is provided, copy the necessary headers to the specified path using sudo
        for file in "${header_base_files[@]}"; do
            if sudo cp -r "$file" "$1/include/zest/base/"; then
                echo "Copied $file successfully"
            else
                echo "Failed to Copy $file"
                copy_failed=true
            fi
        done

        for file in "${header_net_files[@]}"; do
            if sudo cp -r "$file" "$1/include/zest/net/"; then
                echo "Copied $file successfully"
            else
                echo "Failed to Copy $file"
                copy_failed=true
            fi
        done
        echo "Headers copied to the specified path: $1/include/zest/"
    fi

    if [ "$copy_failed" = true ]; then
        echo "Build failed due to copy errors."
    else
        echo "Build completed successfully!"
    fi
else
    echo "Build failed. Please check the errors."
fi
