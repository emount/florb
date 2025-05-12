# Dockerfile for Florb
FROM ubuntu:24.04

# Version of Florb
ARG FLORB_VERSION=v1.2

# Label attributes
LABEL title="Florb"
LABEL maintainer="Eldridge M. Mount IV"
LABEL email="eldridge.mount@gmail.com"
LABEL version="${FLORB_VERSION}"

# Fix Ubuntu's insistence on dash
RUN dpkg-reconfigure -p critical dash
RUN rm /bin/sh; ln -s /bin/bash /bin/sh

# Set environment variables intended for use within the container
ENV EDITOR=emacs
ENV TERM=xterm

# Ensure that dpkg doesn't run any "nice" interactive installation front-ends
ENV DEBIAN_FRONTEND=noninteractive

# Specify a US locale for tzdata (one of the otherwise-interactive culprits)
# This can always be changed, even within a container.
ENV TZ="America/New_York"

# Update the package cache and install packages that we will require
ENV DEBIAN_FRONTEND=noninteractive
RUN apt clean
RUN apt update
RUN apt upgrade -y
RUN apt install -y      \
        adduser         \
        automake        \
        autotools-dev   \
        bash-completion \
        clang           \
        emacs           \
        libglew-dev     \
        libglm-dev      \
        libx11-dev      \
        perl            \
        expect          \
        gcc-14          \
        g++-14          \
        git             \
        make            \
        sudo

# Create a symbolic link to g++-14
RUN ln -s /usr/bin/g++-14 /usr/bin/g++

# Define the default username and home directory as overrideable arguments
ARG SHELL_USER=florb
ARG SHELL_UID=1000
ARG SHELL_GID=1000
ENV USER=${SHELL_USER}
ENV SHELL_HOME=/home/${SHELL_USER}

# Delete the default 'ubuntu' account
RUN deluser --remove-home ubuntu

# Add the defined user and their group
RUN addgroup --gid ${SHELL_GID} ${SHELL_USER}
RUN useradd --uid ${SHELL_UID} --gid ${SHELL_GID} -ms /bin/bash ${SHELL_USER}
RUN echo "${SHELL_USER} ALL=(ALL) NOPASSWD: ALL" > /etc/sudoers.d/${SHELL_USER} && \
    chmod 0440 /etc/sudoers.d/${SHELL_USER}

# Provide a bashrc script for the user, as well as a "smoke test" script
# for validating the container
COPY content/user-bashrc ${SHELL_HOME}/.bashrc
RUN chown ${SHELL_UID}:${SHELL_GID} ${SHELL_HOME}/.bashrc
RUN chmod 755 ${SHELL_HOME}/.bashrc

WORKDIR ${SHELL_HOME}

# Fetch and build Florb
RUN git clone https://github.com/emount/florb.git; \
         cd florb; \
    git checkout ${FLORB_VERSION}; \
    make

# Link to the user's image path in their home directory
RUN ln -s ${SHELL_HOME}/host-home/images florb/images

WORKDIR ${SHELL_HOME}/florb

CMD ["/bin/bash", "-c", "${SHELL_HOME}/florb/florb"]

