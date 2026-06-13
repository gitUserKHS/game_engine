# 기여 안내

이 저장소의 목표는 기능 수보다 **코드를 처음 보는 사람이 배울 수 있는 구조**다.
코드와 문서를 같은 기능의 두 표현으로 취급한다.

## 기본 흐름

1. 작업이 크면 먼저 Issue에 목표와 완료 조건을 적는다.
2. 최신 `main`에서 목적이 하나인 짧은 브랜치를 만든다.
3. 코드, 테스트, 영향을 받는 한국어 문서를 함께 수정한다.
4. 로컬 검사를 통과시킨 뒤 Pull Request를 만든다.
5. CI가 통과하고 대화가 해결되면 squash merge한다.

브랜치 예:

```text
feat/editor-camera
fix/camera-rotation
docs/rendering-guide
refactor/collision-query
test/world-lifecycle
chore/github-setup
```

커밋과 PR 제목 예:

```text
feat: 에디터 카메라 이동 추가
fix: 카메라 회전축 계산 수정
docs: 렌더링 흐름 문서화
test: PIE 월드 격리 테스트 추가
```

## PR에 꼭 적을 것

- 왜 필요한가
- 무엇이 바뀌었나
- 코드가 어떤 순서로 흐르는가
- 이번 변경에서 배울 수 있는 점
- 실행한 테스트
- 함께 바꾼 문서

구조, 사용법, 공개 인터페이스를 바꾸면 관련 `docs/modules/` 문서도 같은 PR에서
갱신한다. 사용자에게 보이는 기능이나 버전이 바뀌면 `README.md`와
`CHANGELOG.md`도 확인한다.

## 로컬 검사

```powershell
cmake --preset windows-debug
cmake --build --preset windows-debug
ctest --preset windows-debug
python tools/check_docs.py
git diff --check
```

더 자세한 규칙은 [개발 안내](docs/DEVELOPMENT.md)와
[Git 작업 흐름](docs/GIT_WORKFLOW.md)에 있다.
