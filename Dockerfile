FROM library/ubuntu

ENV TZ=US/Eastern
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

RUN apt-get update && apt-get install -y build-essential lua5.4 liblua5.4-dev m4 \
        valgrind git curl luarocks zsh neovim apophenia-bin libapophenia libapophenia-dev libgsl-dev libdb-dev \
        sqlite3 libsqlite3-dev gdb \
    && rm -rf /var/lib/apt/lists/*

RUN luarocks install penlight
RUN luarocks install luacheck
#RUN luarocks install debug

RUN mkdir -p /home/@
