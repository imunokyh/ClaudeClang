// ClaudeClang CI 파이프라인.
// 개발과 같은 Docker 이미지(claudeclangbuild)로 빌드·테스트·품질 검사를 하고, 결과를 Jenkins 플러그인으로 보여준다.
// 소스코드 품질 기준(CLAUDE.md)을 하나라도 만족하지 못하면 빌드가 실패한다.
//
// 스테이지는 실패해도 나머지를 끝까지 실행한다(결과를 한 번에 보기 위해). 빌드 결과는 하나라도 실패하면 FAILURE다.
pipeline {
    agent any

    // GitHub Actions(.github/workflows/ci.yml)가 PR 게이트이고, 이 Jenkins는 사내(이 PC)에서 같은 검사를 주기적으로 다시 돌린다.
    // develop 브랜치는 매일 새벽에 실행한다(해시 H로 시각 분산). 다른 브랜치는 정기 실행하지 않는다.
    triggers {
        cron(env.BRANCH_NAME == 'develop' ? 'H 2 * * *' : '')
    }

    options {
        timestamps()
        timeout(time: 60, unit: 'MINUTES')
        buildDiscarder(logRotator(numToKeepStr: '30', artifactNumToKeepStr: '10'))
        disableConcurrentBuilds(abortPrevious: true)
        skipDefaultCheckout(false)
    }

    environment {
        BUILD_IMAGE       = 'claudeclangbuild:latest'
        // 빌드 컨테이너가 이 Jenkins 컨테이너의 볼륨(워크스페이스)을 같은 경로로 공유한다.
        JENKINS_CONTAINER = 'claudeclang-jenkins'
    }

    stages {
        stage('Build image') {
            steps {
                // 개발 환경과 동일한 이미지를 만든다. 변경이 없으면 캐시로 바로 끝난다.
                sh 'docker build -t "$BUILD_IMAGE" docker'
            }
        }

        stage('Self test')    { steps { gate('selftest') } }
        stage('Format')       { steps { gate('format') } }
        stage('Build')        { steps { gate('build') } }
        stage('Rules')        { steps { gate('rules') } }          // 반환 규칙, 인터페이스 경유 접근
        stage('HITL comments') {                                   // AI 작성·사용자 리뷰 Doxygen 주석
            steps { gate('hitl') }
        }
        stage('HITL review completed') {                           // PR 병합 전: 모든 함수의 사용자 리뷰 완료
            when { changeRequest() }
            steps { gate('hitl-reviewed') }
        }
        stage('Doxygen')      { steps { gate('docs') } }
        stage('Test docs')    { steps { gate('testdocs') } }
        stage('Static analysis (Cppcheck)') { steps { gate('static') } }
        stage('Metrics (Lizard)')           { steps { gate('complexity') } }
        stage('Comment ratio (CLOC)')       { steps { gate('cloc') } }
        stage('Unit test (MC/DC)')          { steps { gate('unit') } }
        stage('Integration test (function coverage)') { steps { gate('integration') } }
        stage('TDD test')     { steps { gate('tdd') } }
        stage('Traceability') { steps { gate('trace') } }
        stage('System test (requirement coverage)') { steps { gate('system') } }
    }

    post {
        always {
            // 커버리지 요약(Plot용 CSV)
            sh 'docker run --rm --volumes-from "$JENKINS_CONTAINER" -w "$WORKSPACE" "$BUILD_IMAGE" python3 tools/ci_summary.py || true'

            // 테스트 결과: 단위(unit.*), 통합(integration.*), TDD(tdd.*)가 클래스 이름 접두어로 구분된다.
            junit testResults: 'build/reports/unit/*.xml,build/reports/integration/*.xml,build/reports/tdd/*.xml',
                  allowEmptyResults: true, skipPublishingChecks: true

            // 정적 분석, 컴파일러 경고, 매트릭, Doxygen 경고
            recordIssues enabledForFailure: true, skipPublishingChecks: true, tools: [
                cppCheck(pattern: 'build/reports/cppcheck.xml', id: 'cppcheck', name: 'Cppcheck')
            ]
            recordIssues enabledForFailure: true, skipPublishingChecks: true, tools: [
                clang(pattern: 'build/reports/build-*.log', id: 'clang', name: 'Clang 경고')
            ]
            recordIssues enabledForFailure: true, skipPublishingChecks: true, tools: [
                gcc4(pattern: 'build/reports/lizard-warnings.txt', id: 'lizard', name: 'Lizard (복잡도·라인수)')
            ]
            recordIssues enabledForFailure: true, skipPublishingChecks: true, tools: [
                doxygen(pattern: 'build/reports/doxygen/doxygen.log', id: 'doxygen', name: 'Doxygen')
            ]

            // 커버리지: 단위(MC/DC는 HTML 보고서), 통합(함수), TDD(별도 측정)
            recordCoverage(
                tools: [[parser: 'LCOV', pattern: 'build/reports/unit/coverage.lcov']],
                id: 'coverage-unit', name: '단위 테스트 커버리지', sourceCodeRetention: 'LAST_BUILD',
                skipPublishingChecks: true, enabledForFailure: true)
            recordCoverage(
                tools: [[parser: 'LCOV', pattern: 'build/reports/integration/coverage.lcov']],
                id: 'coverage-integration', name: '통합 테스트 커버리지', sourceCodeRetention: 'LAST_BUILD',
                skipPublishingChecks: true, enabledForFailure: true)
            recordCoverage(
                tools: [[parser: 'LCOV', pattern: 'build/reports/tdd/coverage.lcov']],
                id: 'coverage-tdd', name: 'TDD 테스트 커버리지(별도 측정)', sourceCodeRetention: 'LAST_BUILD',
                skipPublishingChecks: true, enabledForFailure: true)

            // HTML 보고서 (MC/DC는 llvm-cov HTML에서 볼 수 있다)
            publishHTML(target: [reportName: '단위 테스트 — llvm-cov (MC/DC)', reportDir: 'build/reports/unit/html',
                reportFiles: 'index.html', keepAll: true, allowMissing: true, alwaysLinkToLastBuild: true])
            publishHTML(target: [reportName: '통합 테스트 — llvm-cov (함수)', reportDir: 'build/reports/integration/html',
                reportFiles: 'index.html', keepAll: true, allowMissing: true, alwaysLinkToLastBuild: true])
            publishHTML(target: [reportName: 'TDD 테스트 — llvm-cov', reportDir: 'build/reports/tdd/html',
                reportFiles: 'index.html', keepAll: true, allowMissing: true, alwaysLinkToLastBuild: true])
            publishHTML(target: [reportName: 'Cppcheck 보고서', reportDir: 'build/reports/cppcheck-html',
                reportFiles: 'index.html', keepAll: true, allowMissing: true, alwaysLinkToLastBuild: true])
            publishHTML(target: [reportName: 'Lizard 보고서', reportDir: 'build/reports',
                reportFiles: 'lizard.html', keepAll: true, allowMissing: true, alwaysLinkToLastBuild: true])
            publishHTML(target: [reportName: 'Doxygen 문서', reportDir: 'build/reports/doxygen/html',
                reportFiles: 'index.html', keepAll: true, allowMissing: true, alwaysLinkToLastBuild: true])

            // 추이: 주석률(CLOC), 커버리지 요약
            plot csvFileName: 'plot-comment-ratio.csv', group: '코드 품질', title: '주석률 (%)', style: 'line',
                 yaxis: '%', csvSeries: [[file: 'build/reports/cloc-comment-ratio.csv',
                 displayTableFlag: false, inclusionFlag: 'OFF', exclusionValues: '', url: '']]
            plot csvFileName: 'plot-coverage.csv', group: '코드 품질', title: '커버리지 (%)', style: 'line',
                 yaxis: '%', csvSeries: [[file: 'build/reports/coverage-summary.csv',
                 displayTableFlag: false, inclusionFlag: 'OFF', exclusionValues: '', url: '']]

            archiveArtifacts artifacts: 'build/reports/**', allowEmptyArchive: true, fingerprint: false
        }
        cleanup {
            // 워크스페이스는 root 소유 파일이 있어도 지울 수 있다(컨테이너를 root 로 실행함).
            cleanWs(deleteDirs: true, notFailBuild: true)
        }
    }
}

// 빌드 이미지 컨테이너에서 tools/run.sh 명령을 실행한다. 실패하면 이 스테이지만 실패로 표시하고 다음 스테이지를 계속한다.
def gate(String command) {
    catchError(buildResult: 'FAILURE', stageResult: 'FAILURE') {
        sh """
            docker run --rm --volumes-from "\$JENKINS_CONTAINER" -w "\$WORKSPACE" "\$BUILD_IMAGE" bash tools/run.sh ${command}
        """
    }
}
