FROM mcr.microsoft.com/dotnet/sdk:8.0 AS publishing
# Build the maintained publishing tool against .NET 8; the SDK's bundled binary
# requires obsolete OpenSSL/ICU versions that modern Linux no longer ships.
ADD https://github.com/OpenOrbis/LibOrbisPkg/archive/92a4e7f8e5914cb06b52ba3ae6b0675da438a051.tar.gz /tmp/publisher.tar.gz
RUN mkdir /src && tar -xzf /tmp/publisher.tar.gz -C /src --strip-components=1 \
    && dotnet publish /src/PkgTool.Core/PkgTool.Core.csproj -c Release \
       -r linux-x64 --self-contained true -p:PublishSingleFile=true -o /publish

FROM ubuntu:24.04

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    ca-certificates clang-18 lld-18 llvm-18 make g++ curl git python3 \
    libcurl4-openssl-dev libicu74 libssl3t64 zlib1g libsdl2-dev libfreetype-dev librsvg2-bin python3-pil && rm -rf /var/lib/apt/lists/*

ENV OO_PS4_TOOLCHAIN=/opt/openorbis
ENV PATH="/usr/lib/llvm-18/bin:/opt/openorbis/bin/linux:${PATH}"
ARG TOOLCHAIN_URL=https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/releases/download/v0.5.4/toolchain-llvm-18.tar.gz
RUN curl -fL --retry 3 "$TOOLCHAIN_URL" -o /tmp/sdk.tar.gz \
    && echo '3c7cd5bb593ca74fa1c13fd59f3938dc0fc07985167f7275063019e63abe4526  /tmp/sdk.tar.gz' | sha256sum -c - \
    && mkdir -p "$OO_PS4_TOOLCHAIN" \
    && tar -xzf /tmp/sdk.tar.gz --strip-components=2 -C "$OO_PS4_TOOLCHAIN" \
    && rm /tmp/sdk.tar.gz

# libjbc supplies the privilege setup expected by BGFT on a homebrew-enabled PS4.
RUN git clone https://github.com/bucanero/ps4-libjbc.git /opt/libjbc \
    && cd /opt/libjbc && git checkout 835fe016ff0ae5dd89b9249f39cc0fe093fd07dd \
    && clang --target=x86_64-pc-freebsd12-elf -fPIC -ffreestanding -mno-red-zone \
       -isysroot "$OO_PS4_TOOLCHAIN" -isystem "$OO_PS4_TOOLCHAIN/include" -c jailbreak.c kernelrw.c utils.c \
    && llvm-ar rcs "$OO_PS4_TOOLCHAIN/lib/libjbc.a" jailbreak.o kernelrw.o utils.o \
    && cp libjbc.h "$OO_PS4_TOOLCHAIN/include/"

WORKDIR /work
COPY --from=publishing /publish/PkgTool.Core /opt/openorbis/bin/linux/PkgTool.Core
ENV PSGI_IN_CONTAINER=1
CMD ["make", "all"]
