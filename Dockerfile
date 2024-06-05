FROM ubuntu:mantic
LABEL authors="wejman"

RUN apt-get update && apt-get install -y \
    procps llvm

RUN apt install clang-16 lldb-16 lld-16 \
    libllvm-16-ocaml-dev libllvm16 llvm-16 llvm-16-dev llvm-16-doc  \
    llvm-16-examples llvm-16-runtime build-essential -y



RUN apt-get install valgrind -y

RUN apt-get install leiningen -y

RUN apt-get install cmake -y

RUN apt-get install protobuf-compiler gmpc libgmp3-dev -y

COPY . /app
WORKDIR /app


# compile the code in backend using CMakeLists.txt
RUN cmake ./backend/runtime -DCMAKE_PREFIX_PATH=/usr/bin/cmake -DCMAKE_BUILD_TYPE=Debug
RUN make -j8



# do nothing
ENTRYPOINT ["tail", "-f", "/dev/null"]