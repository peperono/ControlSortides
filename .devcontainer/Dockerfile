FROM mcr.microsoft.com/devcontainers/base:alpine-3.22

# Install Node.js (LTS) and build tools
RUN apk add --no-cache \
    nodejs \
    npm \
    g++ \
    gcc \
    make \
    cmake \
    musl-dev \
    linux-headers \
    git \
    bash

# Install Claude Code globally
RUN npm install -g @anthropic-ai/claude-code

# Verify installation
RUN claude --version
