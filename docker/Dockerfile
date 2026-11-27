# ClaudeCLangBuild 툴체인 이미지 (Docker 이미지 이름은 소문자만 허용하므로 claudeclangbuild)
# 개발 빌드와 CI(Jenkins) 빌드가 모두 이 이미지를 쓴다.
# 빌드, 단위·통합 테스트, 커버리지, 정적 분석, 복잡도·주석률 측정을 모두 이 이미지 안에서 수행한다.
# 버전을 바꿀 때는 CLAUDE.md "개발 환경" 표도 함께 고친다.
FROM ubuntu:24.04

# Clang/LLVM: 18 이상의 최신 안정 버전을 apt.llvm.org에서 받는다. (MC/DC 측정은 18 이상)
ARG LLVM_VERSION=23
ENV DEBIAN_FRONTEND=noninteractive \
    LANG=C.UTF-8 \
    LC_ALL=C.UTF-8

RUN apt-get update && apt-get install -y --no-install-recommends ca-certificates curl gnupg \
    && curl -fsSL https://apt.llvm.org/llvm-snapshot.gpg.key | gpg --dearmor -o /usr/share/keyrings/llvm.gpg \
    && echo "deb [signed-by=/usr/share/keyrings/llvm.gpg] https://apt.llvm.org/noble/ llvm-toolchain-noble-${LLVM_VERSION} main" \
        > /etc/apt/sources.list.d/llvm.list \
    && rm -rf /var/lib/apt/lists/*

RUN apt-get update && apt-get install -y --no-install-recommends \
        clang-${LLVM_VERSION} \
        clang-format-${LLVM_VERSION} \
        clang-tidy-${LLVM_VERSION} \
        llvm-${LLVM_VERSION} \
        lld-${LLVM_VERSION} \
        libclang-rt-${LLVM_VERSION}-dev \
        cmake \
        ninja-build \
        make \
        git \
        cppcheck \
        cloc \
        doxygen \
        python3-pygments \
        libgtest-dev \
        libgmock-dev \
        python3 \
        python3-venv \
    && rm -rf /var/lib/apt/lists/*

# PICT (페어와이즈 조합 생성기). 테스트 케이스 설계에서 조합을 도출할 때 쓴다.
ARG PICT_VERSION=v3.7.4
RUN git clone --depth 1 --branch ${PICT_VERSION} https://github.com/microsoft/pict.git /tmp/pict \
    && make -C /tmp/pict CXX=clang++-${LLVM_VERSION} -j"$(nproc)" \
    && install -m 0755 /tmp/pict/pict /usr/local/bin/pict \
    && rm -rf /tmp/pict

# 버전 접미사 없는 이름으로 호출할 수 있게 한다 (clang, llvm-cov, llvm-profdata 등).
RUN for t in clang clang++ clang-format clang-tidy llvm-cov llvm-profdata ld.lld; do \
        ln -sf "/usr/bin/${t}-${LLVM_VERSION}" "/usr/local/bin/${t}"; \
    done

# 순환 복잡도·함수 라인수 측정 도구(lizard), 시험 명세 엑셀 검증용 openpyxl. 시스템 Python을 건드리지 않도록 venv에 설치한다.
RUN python3 -m venv /opt/tools \
    && /opt/tools/bin/pip install --no-cache-dir lizard==1.17.13 openpyxl==3.1.5 jinja2==3.1.6
ENV PATH="/opt/tools/bin:${PATH}" \
    CC=clang \
    CXX=clang++

# CI(Jenkins)가 임의의 사용자 ID로 실행해도 도구가 쓸 수 있는 홈 디렉터리
ENV HOME=/tmp
WORKDIR /work
CMD ["bash"]
