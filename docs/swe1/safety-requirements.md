# 기능 안전 요구사항

- 근거 문서: OEM-SWR-001 Rev 1.0 5절 "ASIL B 입력" (OEM-A가 할당. HARA·안전 목표 문서는 제공되지 않음)
- ASIL 분해: 없음
- 면책: 이 문서는 교육용 가상 OEM 입력을 SW 요구로 옮긴 것이며 ISO 26262 준수·인증·ASIL 달성을 주장하지 않는다. 공급자는 HARA와 ASIL 결정 근거를 재구성하지 않는다.
- 용어(갱신, 입력 상태, STALE, INVALID, 마지막 수신 시각, 직전 확정 출력, 유지, 평가)는 [software-requirements.md](software-requirements.md) 0.2절을, 결정 규칙표(SWR-023)와 입력 상태 전이표는 같은 문서 5.4절, 5.5절을 따른다. 평가 주기는 10 ms로 가정한다(A-01). FTTI는 OEM이 준 응답 시간 기준으로 대용한다(질의 Q-10).
- 이터레이션 1 대상: SAF-001, SAF-003, SAF-004, SAF-005. 이터레이션 2 대상: SAF-002. 번호는 OEM 요구 번호를 따르고(SAF-00n ← OEM-SR-00n) 둘로 나눈 OEM-SR-003의 두 번째 요구는 SAF-005다. OEM-SR-002에 대응하는 번호(002)는 이터레이션 2를 위해 예약한다.

| ID | 요구사항 | ASIL | 안전 목표(출처) | 안전 상태 | FTTI | 검증 기준 | 출처 |
|---|---|---|---|---|---|---|---|
| SAF-001 | SW는 충돌 입력의 입력 상태가 VALID이고 판정 값이 CONFIRMED이면 다른 모든 명령보다 우선하여 후석 좌, 우 출력을 RELEASE로 만든다. 입력 상태가 VALID로 CONFIRMED가 유지되는 동안 매 평가에서 RELEASE를 유지한다. | B | 미제공(OEM-A 할당 입력) | 좌, 우 RELEASE | 300 ms | 스냅샷 시각 T에 CONFIRMED를 주입하면 T 이후 처음 호출되는 평가(now_ms ≤ T+10 ms)에서 두 출력이 RELEASE이고 이후 100번의 평가(10 ms 간격, 1 s) 동안 입력 상태가 VALID로 CONFIRMED가 유지되면 매번 RELEASE이다. now_ms − T는 300 ms 이하. T는 스냅샷 시각(timestamp_ms)이다 | OEM-SR-001, 가정 A-04, 질의 Q-17 |
| SAF-002 | SW는 후측방 접근위험 입력이 VALID이고 TRUE인 문의 출력을 LOCK으로 하고, 그 문에 대한 해제 요청을 억제하며, 원인(APPROACH_RISK)을 기록한다. 단 OEM-FR-003의 override가 성립한 문은 예외다(질의 Q-22). | B | 미제공(OEM-A 할당 입력) | 해당 문 LOCK 유지 | 1 평가주기(10 ms) | 좌 접근위험 TRUE를 주입한 첫 평가에서 lock_left=LOCK이고 reason=APPROACH_RISK. 이후 좌 대상 unlock 명령을 평가해도 lock_left=LOCK 유지. 우 문은 영향받지 않는다 | OEM-SR-002, 가정 A-19, 질의 Q-22 |
| SAF-003 | SW는 마지막으로 받아들인 스냅샷 이후 200 ms를 초과해 새 스냅샷을 받지 못하면(모든 입력 STALE) 100 ms 이내에 state를 DEGRADED로 하고 직전 확정 출력을 유지한다. | B | 미제공(OEM-A 할당 입력) | state=DEGRADED, 직전 확정 출력 유지 | 100 ms (STALE이 된 시점부터) | 마지막 수신 시각 T에 대해 now_ms = T+200 ms 평가는 VALID, T+210 ms 평가는 STALE이고 state=DEGRADED, 출력이 직전 확정 출력(LOCK으로 설정해 둔 값)과 같다(T+300 ms 이내) | OEM-SR-003 |
| SAF-004 | SW는 sensor_fault가 VALID이고 TRUE이면 새 명령을 적용하지 않고 직전 확정 출력을 유지하며 FAULT 상태와 경고코드를 제공한다(결정 규칙표 규칙 2). 충돌 해제(SAF-001)는 예외로 우선한다. | B | 미제공(OEM-A 할당 입력) | state=FAULT, 직전 확정 출력 유지 (충돌 CONFIRMED 입력 상태 VALID이면 RELEASE) | 1 평가주기(10 ms) | 직전 확정 출력을 LOCK으로 설정하고 sensor_fault=TRUE인 첫 평가에서 출력이 LOCK 그대로이고 state=FAULT, warning_code=SENSOR_FAULT. 같은 평가에서 충돌 CONFIRMED의 입력 상태가 VALID이면 출력은 RELEASE | OEM-SR-004, 가정 A-03, 질의 Q-16 |
| SAF-005 | SW는 필수 입력의 형식·범위·누락 오류를 평가 전에 검출해 그 값을 판정에 쓰지 않고 새 값이 유효하게 갱신될 때까지 INVALID로 표시하며, state를 DEGRADED(FAULT 조건과 동시이면 FAULT)로 하고 직전 확정 출력을 유지한다. 충돌 해제(SAF-001)는 예외로 우선한다. | B | 미제공(OEM-A 할당 입력) | state=DEGRADED(FAULT 동시이면 FAULT), 직전 확정 출력 유지 (충돌 CONFIRMED 입력 상태 VALID이면 RELEASE) | 1 평가주기(10 ms) | 직전 확정 출력을 LOCK으로 설정하고 입력 하나를 범위 밖 값으로 주입한 첫 평가에서 출력이 LOCK 그대로이고 state=DEGRADED(FAULT 동시이면 FAULT)이며 그 입력은 INVALID로 표시되고 스냅샷 없이 평가해도 유효한 값이 갱신될 때까지 INVALID다. 같은 평가에 충돌 CONFIRMED(입력 상태 VALID)가 있으면 출력은 RELEASE | OEM-SR-003, 가정 A-15 |

## 개정 이력

| 버전 | 일자 | 변경 ID | 사유 | 영향받은 산출물 |
|---|---|---|---|---|
| 0.1 | 2026-11-27 | SAF-001, SAF-003, SAF-004 | 이터레이션 1 최초 작성 | 없음 |
| 0.2 | 2026-11-27 | SAF-001, SAF-003, SAF-004, SAF-005 |  SWE.1 리뷰 1회 조치: SAF-003을 둘로 나눔(SAF-005 신설), 충돌과 sensor_fault 동시 상황 명시, 검증 기준을 관측 가능한 값으로 수정 | 요구사항 명세 |
| 0.3 | 2026-11-27 | SAF-001, SAF-003, SAF-004, SAF-005 | SWE.1 리뷰 2회 조치: 입력 상태·DEGRADED/FAULT 결정 규칙을 요구사항 명세와 일치, 관측 구간(100 평가)과 LOCK 전제 명시 | 요구사항 명세 |
| 0.4 | 2026-11-27 | SAF-001, SAF-003, SAF-004, SAF-005 | SWE.1 리뷰 3회 조치: SAF-003, SAF-005에 충돌 해제 예외 추가, SAF-001 둘째 문장에 입력 상태 조건, 출처에 가정·질의 표시, 용어(fresh, DEGRADED 조건) 정리 | 요구사항 명세 |
| 0.5 | 2026-11-27 | SAF-003 | SWE.1 리뷰 4회 조치: 도달할 수 없는 조합(STALE과 FAULT, STALE과 충돌 해제) 제거, 결정 규칙표 위치(5.4절) 정정, 시간 표기를 ms 정수로 | 요구사항 명세 |
| 0.6 | 2026-11-27 | SAF-002 | 이터레이션 2: OEM-SR-002 접근위험 잠금과 억제 추가 | 요구사항 명세 |
