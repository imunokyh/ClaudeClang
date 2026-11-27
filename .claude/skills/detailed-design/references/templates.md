# 상세 설계서 양식 (docs/swe3/detailed-design.md)

ID: 모듈 `MOD-nnn`, 모듈 인터페이스 `MIF-nnn`, 함수 `FN-nnn`. 구현의 Doxygen `@trace FN-nnn`과 대응한다.

````markdown
# SW 상세 설계서

- 대상 SW: <이름>   버전: <x.y>   일자: YYYY-MM-DD
- 입력: docs/swe2/architecture.md (버전)
- 인터페이스 필수 기준: M1 호출됨 / M2 공개 함수 3개 이상 / M3 안전 요구 구현 / M4 상태 소유 (SKILL.md 기준)

## 1. 모듈
| ID | 모듈 (경로) | 책임 (한 문장) | 소유 데이터 | 인터페이스 필요(근거) | 상위 ID |
|---|---|---|---|---|---|
| MOD-001 | lock_decision (`src/lock_ctrl/lock_decision/`) | 잠금 조건 충족 여부를 판정한다 | 없음 | 예 (M1, M3) | CMP-001 |

## 2. 모듈 인터페이스
| ID | 인터페이스 (헤더) | 제공 모듈 | 사용 모듈 | 구현하는 컴포넌트 인터페이스 | 상위 ID |
|---|---|---|---|---|---|
| MIF-001 | `lock_decision_if.h` | MOD-001 | MOD-002 | IF-001 | CMP-001, IF-001 |

## 3. 함수
| ID | 함수 | 모듈 | 공개 | 목적 (한 문장) | 상위 ID |
|---|---|---|---|---|---|
| FN-001 | `status_t lock_decision_evaluate(uint16_t speed_kph, bool door_closed, bool manual_override, lock_cmd_t *cmd)` | MOD-001 | 예 | 잠금 조건을 판정해 명령을 반환한다 | MOD-001, IF-001 |

### FN-001 `lock_decision_evaluate`
- **매개변수**: `speed_kph` 0~250 km/h (분해능 1), `door_closed`, `manual_override`, `cmd` 출력(NULL 불가)
- **반환값**: `STATUS_OK`, `STATUS_ERR_PARAM`(cmd가 NULL), `STATUS_ERR_RANGE`(speed_kph > 250)
- **전제 조건**: 없음  **사후 조건**: 반환이 `STATUS_OK`일 때만 `*cmd`가 갱신된다
- **부작용**: 없음 (상태를 바꾸지 않는다)
- **알고리즘**: 아래 결정표. 입력 검증 후 판정
- **오류 처리**: 매개변수 오류 시 `*cmd`를 건드리지 않고 오류 코드 반환

**결정표**
| 규칙 | C1 `speed_kph >= 15` | C2 `door_closed` | C3 `!manual_override` | 결과 `*cmd` |
|---|---|---|---|---|
| R1 | T | T | T | LOCK |
| R2 | F | - | - | NONE |
| R3 | T | F | - | NONE |
| R4 | T | T | F | NONE |

## 4. 의존 관계 (모듈 단위)
(표 또는 mermaid. 순환 없음 확인)

## 5. 상수
| 이름 | 값 | 단위 | 근거 ID |
|---|---|---|---|
| `LOCK_SPEED_THRESHOLD_KPH` | 15 | km/h | REQ-F-001 |

## 6. 개정 이력
| 버전 | 일자 | 변경 ID | 사유 | 영향받은 산출물 |
|---|---|---|---|---|
````

- 함수의 **첫 열 ID가 있는 표 행만** 추적 대상이다. `### FN-001` 소제목은 설명용이며 ID 정의로 세지 않는다(정의는 3번 표의 행).
- 함수 행의 상위 ID에는 그 함수가 속한 모듈(`MOD-*`)과, 해당하면 컴포넌트 인터페이스(`IF-*`)를 쓴다. 모듈 행에는 컴포넌트(`CMP-*`)를 쓴다.
- 함수는 모두 상태 코드를 반환한다. 반환할 값이 따로 있으면 출력 매개변수로 내보낸다.
