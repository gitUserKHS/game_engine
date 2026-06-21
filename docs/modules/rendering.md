# 렌더링

## v0.7 Texture GPU Resource

`Renderer::textureFor`는 `TextureAsset`의 이미지 파일을 `stb_image`로 읽고,
OpenGL 2D texture와 mipmap을 만든 뒤 GUID 기준 캐시에 보관한다. 같은 texture를
다시 요청하면 GPU 업로드를 반복하지 않고 기존 `TextureGpuResource`를 돌려준다.
이 단계는 “Asset Registry는 메타데이터와 CPU 설명을 읽고, Renderer는 필요할 때
GPU 리소스를 만든다”는 책임 분리를 보여주는 첫 구현이다.

`Renderer::meshFor`는 `StaticMeshAsset`을 `MeshGpuResource`로 바꾼다. 지금은
교육용 단계라 glTF도 Cube primitive로 등록되며 기존 cube VAO를 GUID 캐시에
연결한다. 다음 단계에서 실제 glTF vertex/index buffer를 만들 때도 같은 API를
유지하면 Component 쪽 코드는 크게 바뀌지 않는다.

## v0.9 렌더링 디버그 보기

`Renderer`는 매 Viewport 렌더 후 `RenderPassRecord` 목록을 남긴다. `Engine Debug`
패널은 이 기록을 읽어 Shadow, Opaque, Debug, UI 패스 이름과 draw 수를 보여 준다.
아직 정식 RenderGraph는 없지만, 현재 프레임이 어떤 제출 순서로 구성되는지 익히기
위한 작은 관찰 창이다. Shadow 패스는 `planned`로 남겨 두어 다음 단계에서 무엇이
비어 있는지 바로 볼 수 있게 했다.

## 목표

게임 객체가 OpenGL을 직접 호출하지 않고 화면에 그려지는 데이터 흐름을 이해한다.

## 필요한 이유

게임 로직과 그래픽 API가 섞이면 테스트하기 어렵고 렌더 구조를 바꾸기 힘들다.
Component의 상태를 값 데이터로 복사하면 두 영역의 책임이 선명해진다.

## 핵심 타입

- [`PrimitiveComponent`](../../include/engine/Components.hpp): 렌더 가능한 Component 기반
- [`StaticMeshComponent`](../../include/engine/Components.hpp): 현재 큐브 proxy 생성
- [`RenderProxy`](../../include/engine/RenderTypes.hpp): 렌더러용 불변 값 묶음
- [`RenderScene`](../../include/engine/Systems.hpp): proxy와 revision 관리
- [`RenderPassRecord`](../../include/engine/Renderer.hpp): 마지막 프레임의 패스 기록
- [`Renderer`](../../include/engine/Renderer.hpp): OpenGL 리소스와 draw call
- [`ViewportRenderTarget`](../../include/engine/Renderer.hpp): color/depth framebuffer
- [`DirectionalLightComponent`](../../include/engine/Components.hpp): 방향성 조명 값
- [`CameraView`](../../include/engine/RenderTypes.hpp): view/projection 입력

## 흐름도

```mermaid
flowchart LR
    Component["StaticMeshComponent"] -->|createRenderProxy| Proxy["RenderProxy"]
    Proxy --> Scene["RenderScene::sync"]
    Light["DirectionalLightComponent"] --> Scene
    Scene --> Renderer["Renderer::renderToTarget"]
    Renderer --> Target["ViewportRenderTarget"]
    Target --> Texture["OpenGL color texture"]
    Texture --> ImGui["ImGui::Image"]
```

## 코드 따라가기

1. [`Components.cpp`](../../src/Components.cpp)의
   `StaticMeshComponent::createRenderProxy`를 읽는다.
2. 프로퍼티 setter가 `markRenderStateDirty`를 호출하는지 확인한다.
3. [`Systems.cpp`](../../src/Systems.cpp)의 `RenderScene::sync`에서 revision 비교를
   찾는다.
4. `RenderScene::opaqueProxyCount`와 `debugWireProxyCount`에서 Debug 패널 숫자가
   어떻게 계산되는지 확인한다.
5. [`Renderer.cpp`](../../src/Renderer.cpp)의 `ViewportRenderTarget::resize`에서
   color texture와 depth renderbuffer가 Viewport 크기에 맞춰지는지 확인한다.
6. `RenderScene::sync`에서 `DirectionalLightComponent`가 light proxy로 모이는지
   확인한다.
7. `Renderer::renderToTarget`에서 장면과 선택 wireframe을 off-screen target에
   그리는 순서를 읽는다.
8. [`Application.cpp`](../../src/Application.cpp)의 `drawViewportPanel`에서
   color texture가 `ImGui::Image`로 표시되는지 확인한다.
9. `Renderer::lastPasses`와 `drawDebugPanel`에서 Shadow, Opaque, Debug, UI 패스 기록이
   어떻게 표시되는지 확인한다.
10. [`basic.vert`](../../shaders/basic.vert)와
   [`basic.frag`](../../shaders/basic.frag)에서 GPU 단계의 입력을 확인한다.

## 실험 과제

Viewport 패널 너비를 드래그해 framebuffer 크기가 바뀌는지 확인한다. 그 뒤 큐브
색을 바꾸고 `RenderScene`의 갱신 횟수가 한 번만 늘어나는지 확인한다.

## 흔한 실수

- Component에서 OpenGL 함수를 직접 호출한다.
- 렌더에 영향을 주는 값을 바꾸고 revision을 올리지 않는다.
- RenderProxy에 Component raw pointer를 넣어 수명을 다시 결합한다.
- cm 단위 World 값과 projection의 near/far 값을 맞추지 않는다.
- 선택 wireframe과 grid에도 조명을 적용해 디버그 색이 읽기 어려워진다.
- framebuffer를 다시 화면 framebuffer로 unbind하지 않아 ImGui도 target에 그린다.
- OpenGL 원점이 왼쪽 아래라는 점을 잊어 Viewport나 PNG가 상하로 뒤집힌다.

## 관련 테스트

[`EngineTests.cpp`](../../tests/EngineTests.cpp)의
`testRenderProxyDirtyUpdate`가 변경된 Component의 proxy만 한 번 갱신되는지
확인하고 Opaque/Debug Wire proxy 개수를 검증한다.
`testDirectionalLightProxy`는 조명 값이 렌더 장면에 복사되는지 확인한다.
`testPngWriter`는 RGBA readback의 상하 방향 규칙을 확인한다.
