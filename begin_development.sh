#!/usr/bin/env bash
#
# Combined install script for macOS (Homebrew) and Ubuntu (apt).
# It checks the OS at runtime, then installs the required versions
# of LLVM, Protobuf, and GMP accordingly.

set -e  # Exit immediately on error

detect_macos() {
  # On macOS, 'uname' typically returns 'Darwin'
  [[ "$(uname -s)" == "Darwin" ]]
}

detect_ubuntu() {
  # A simple check: /etc/lsb-release exists, and it's Ubuntu
  [[ -f /etc/lsb-release && "$(grep -i 'ubuntu' /etc/lsb-release)" ]]
}

install_on_macos() {
  echo "Detected macOS. Installing with Homebrew..."
  # 1. LLVM@16
  brew install llvm@16
  brew unlink llvm@16 && brew link --force llvm@16

  # 2. Protobuf@21
  brew install protobuf@21
  brew unlink protobuf@21 && brew link --force protobuf@21

  # 3. GMP
  brew install gmp

  echo "macOS installations complete!"
}

install_on_ubuntu() {
  echo "Detected Ubuntu. Installing via apt..."

  # 1. Update & install prerequisites
  sudo apt-get update
  sudo apt-get install -y wget software-properties-common lsb-release gnupg

  # 2. Install LLVM 16
  wget https://apt.llvm.org/llvm.sh
  chmod +x llvm.sh
  sudo ./llvm.sh 16

  # Register clang-16 & clang++-16 as the default clang/clang++
  sudo update-alternatives --install /usr/bin/clang clang /usr/bin/clang-16 100
  sudo update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-16 100
  sudo update-alternatives --set clang /usr/bin/clang-16
  sudo update-alternatives --set clang++ /usr/bin/clang++-16

  # 3. Install Protobuf (system default)
  sudo apt-get install -y protobuf-compiler libprotobuf-dev

  #    If you truly need Protobuf v21.x, compile from source or use a PPA.
  #    Example source build snippet is commented out below:
  #
  #    sudo apt-get remove -y protobuf-compiler libprotobuf-dev
  #    git clone -b v21.12 https://github.com/protocolbuffers/protobuf.git
  #    cd protobuf
  #    git submodule update --init --recursive
  #    ./autogen.sh
  #    ./configure
  #    make -j "$(nproc)"
  #    sudo make install
  #    sudo ldconfig

  # 4. Install GMP
  sudo apt-get install -y libgmp-dev

  echo "Ubuntu installations complete!"
}

#
# Main entry point — detect OS and run the appropriate steps.
#

if detect_macos; then
  install_on_macos
elif detect_ubuntu; then
  install_on_ubuntu
else
  echo "Unsupported or unknown OS. Exiting..."
  exit 1
fi
