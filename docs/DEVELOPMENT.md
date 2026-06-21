# 개발 안내

## 개발 원칙

- 작고 읽기 쉬운 변경을 우선한다.
- 기존 구조로 충분하면 새 추상화를 만들지 않는다.
- 공개 심벌과 파일명은 영어, 설명과 학습 문서는 한국어로 쓴다.
- 공개 헤더에는 역할, 소유권, 생명주기, 좌표 단위를 설명한다.
- 구현 파일에는 수학이나 지연 처리처럼 의도가 숨은 부분만 주석을 단다.
- 기능 동작을 바꾸면 테스트와 관련 모듈 문서를 함께 확인한다.

## 새 기능을 넣는 순서

1. 사용자가 보게 될 동작과 완료 조건을 한 문장으로 쓴다.
2. 책임질 기존 모듈을 고른다.
3. 공개 API를 가장 작게 설계한다.
4. headless 테스트로 규칙을 먼저 또는 함께 고정한다.
5. 데모 장면에서 실제 흐름을 연결한다.
6. 관련 `docs/modules/` 문서를 갱신한다.

## C++ 스타일

- C++20 표준 기능을 사용한다.
- 소유권은 가능한 한 `std::unique_ptr`로 드러낸다.
- 관찰용 포인터는 수명을 소유하지 않는다는 전제 아래 raw pointer를 사용한다.
- 거리와 위치는 cm, 회전 UI 값은 degree를 사용한다.
- Tick은 필요한 타입에서만 켠다.
- OpenGL 호출은 `Renderer` 바깥으로 퍼뜨리지 않는다.

## 테스트 선택

| 변경 | 최소 확인 |
| --- | --- |
| Transform/attachment | 부모 변경, World Transform, 순환 거부 |
| 생명주기/Tick | 호출 순서, 그룹, Tick 중 spawn/destroy |
| Reflection | 타입 팩토리, getter/setter, 저장 플래그 |
| 충돌 | Block/Overlap/Ignore, raycast, sweep |
| 렌더 Component | proxy 값과 revision 갱신 횟수 |
| Editor Viewport | 카메라 수학, screen ray, 가장 가까운 proxy 선택 |
| 구조 편집 | snapshot restore, GUID, Controller 참조, Undo/Redo |
| Screenshot | CLI 파싱, PNG signature·크기·상하 방향 |
| PIE | Edit World와 실행 World 격리 |
| 문서 | UTF-8과 내부 링크 검사 |

## 구조 변경

오래 유지할 구조적 결정을 바꿀 때는 `docs/decisions/`에 ADR을 추가한다.
ADR에는 상황, 결정, 이유, 결과를 적고 이전 결정을 대체한다면 그 관계도 남긴다.

## 완료 전 명령

```powershell
cmake --preset windows-debug
cmake --build --preset windows-debug
ctest --preset windows-debug
python tools/check_docs.py
git diff --check
```
