# ClaudeClang CI용 Jenkins 컨트롤러 이미지 (안정 버전 LTS).
# 빌드·테스트·분석은 이 컨테이너가 아니라 claudeclangbuild 이미지(개발과 동일)에서 실행한다.
# 그래서 Jenkins 컨테이너에는 docker CLI만 있으면 된다 (호스트 Docker 소켓을 마운트해서 쓴다).
FROM docker:cli AS dockercli

FROM jenkins/jenkins:lts-jdk21
USER root
COPY --from=dockercli /usr/local/bin/docker /usr/local/bin/docker
# 마운트된 저장소(소유자가 다른 경로)를 git이 거부하지 않게 한다. 로컬 CI 전용 설정.
RUN git config --system --add safe.directory '*'

# 플러그인은 plugins.txt 의 목록을 설치한다 (Jenkins LTS와 호환되는 최신 버전).
COPY plugins.txt /usr/share/jenkins/ref/plugins.txt
RUN jenkins-plugin-cli --plugin-file /usr/share/jenkins/ref/plugins.txt

COPY casc.yaml /usr/share/jenkins/ref/casc.yaml
ENV CASC_JENKINS_CONFIG=/usr/share/jenkins/ref/casc.yaml \
    JAVA_OPTS="-Djenkins.install.runSetupWizard=false -Dhudson.model.DirectoryBrowserSupport.CSP=\"default-src 'self'; img-src 'self' data:; style-src 'self' 'unsafe-inline'; script-src 'self' 'unsafe-inline'\" -Duser.timezone=Asia/Seoul"
